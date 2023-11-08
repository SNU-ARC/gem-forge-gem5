#include "psp_frontend.hh"

#include "debug/PSPFrontend.hh"

#define PSP_FE_PANIC(format, args...)                                          \
  panic("%llu-[PSP_FE%d] " format, cpuDelegator->curCycle(),                   \
        cpuDelegator->cpuId(), ##args)
#define PSP_FE_DPRINTF(format, args...)                                        \
  DPRINTF(PSPFrontend, "%llu-[PSP_FE%d] " format,                          \
          cpuDelegator->curCycle(), cpuDelegator->cpuId(), ##args)

IndexPacketHandler::IndexPacketHandler(PSPFrontend* _pspFrontend, uint64_t _entryId,
                                       Addr _cacheBlockVAddr, Addr _vaddr,
                                       int _size)
  : pspFrontend(_pspFrontend), entryId(_entryId), cacheBlockVAddr(_cacheBlockVAddr),
    vaddr(_vaddr), size(_size) {
}

void IndexPacketHandler::handlePacketResponse(GemForgeCPUDelegator* cpuDelegator,
                                              PacketPtr pkt) {
  this->pspFrontend->handlePacketResponse(this, pkt);

  delete pkt;
  delete this;
}

void IndexPacketHandler::issueToMemoryCallback(GemForgeCPUDelegator* cpuDelegator) {
}

PSPFrontend::PSPFrontend(Params* params)
  : GemForgeAccelerator(params), totalPatternTableEntries(params->totalPatternTableEntries) {
    valCurrentSize = new uint64_t[params->totalPatternTableEntries]();
    patternTable = new PatternTable(params->totalPatternTableEntries); 
    indexQueueArray = new IndexQueueArray(params->totalPatternTableEntries,
                                          params->indexQueueCapacity);
    paQueueArray = new PAQueueArray(params->totalPatternTableEntries,
                                    params->paQueueCapacity);
    patternTableArbiter = new PatternTableRRArbiter(params->totalPatternTableEntries,
                                                    patternTable, indexQueueArray);
    indexQueueArrayArbiter = new IndexQueueArrayRRArbiter(params->totalPatternTableEntries,
                                                          indexQueueArray,
                                                          paQueueArray);
}

PSPFrontend::~PSPFrontend() {
  delete[] valCurrentSize;
  delete patternTable;
  delete indexQueueArray;
}

// Take over PSP_Frontend from initial_cpu to future_cpu
void PSPFrontend::takeOverBy(GemForgeCPUDelegator *newCpuDelegator,
                              GemForgeAcceleratorManager *newManager) {
  GemForgeAccelerator::takeOverBy(newCpuDelegator, newManager);
  translationBuffer = new PSPTranslationBuffer<void*>(this->cpuDelegator->getDataTLB(),
      [this](PacketPtr pkt, ThreadContext* tc, void* ) -> void {
      this->cpuDelegator->sendRequest(pkt); },
      [this](PacketPtr pkt, ThreadContext* tc, void* indexPacketHandler) -> void {
      this->handleAddressTranslateResponse((IndexPacketHandler*)indexPacketHandler, pkt); },
      false /* AccessLastLevelTLBOnly */, true /* MustDoneInOrder */);
  for (auto so : SimObject::getSimObjectList()) {
    if (so->name() == "system.ruby.l0_cntrl0.pspbackend") {
      pspBackend = dynamic_cast<PSPBackend*>(so);
      PSP_FE_DPRINTF("MATCH!\n");
    }
  }
 }

void PSPFrontend::dump() {
}

void PSPFrontend::regStats() {
  GemForgeAccelerator::regStats();

#define scalar(stat, describe)                                                 \
  this->stat.name(this->manager->name() + (".psp." #stat))                      \
      .desc(describe)                                                          \
      .prereq(this->stat)

  scalar(numConfigured, "Number of streams configured.");
#undef scalar
}

void PSPFrontend::resetStats() {
}

void PSPFrontend::tick() {
  if (this->patternTable->size() > 0)
    this->manager->scheduleTickNextCycle();

  /* Issue load index  */
  uint32_t validEntryId;
  uint64_t cacheLineSize = this->cpuDelegator->cacheLineSize();
  if (this->patternTableArbiter->getValidEntryId(&validEntryId, cacheLineSize)) {
    PSP_FE_DPRINTF("ValidEntryId: %d\n", validEntryId);
    this->issueLoadIndex(validEntryId);
  }

  /* Issue feature vector address translation */
  uint32_t validIQEntryId;
  
  // TODO: Should I check the availability of TLB? (e.g., pending translation by PTW)
  if (this->indexQueueArrayArbiter->getValidEntryId(&validIQEntryId)) {
    this->issueTranslateValueAddress(validIQEntryId);
  }

  /* Issue PA packets to PSP Backend*/
  // Temporal code to prevent deadlock
  // TODO: Replace with implement for offloading packets to backend
  for (uint32_t i = 0; i < this->totalPatternTableEntries; i++) {
    //PSP_FE_DPRINTF("PSPBackend_canInsert: %d / %d\n", this->pspBackend->canInsertEntry(i), this->totalPatternTableEntries);
    if (this->paQueueArray->canRead(i) && this->pspBackend->canInsertEntry(i)) {
      PhysicalAddressQueue::PhysicalAddressArgs validPAQEntry;
      this->paQueueArray->read(i, &validPAQEntry);
      this->pspBackend->insertEntry(validPAQEntry.entryId, validPAQEntry.pAddr, validPAQEntry.size);
      PSP_FE_DPRINTF("paQueueEntryId: %lu, pAddr: %x, size: %lu, PSPBackend_canRead: %d\n",
          validPAQEntry.entryId, validPAQEntry.pAddr, validPAQEntry.size, this->paQueueArray->canRead(i));
      this->paQueueArray->pop(i);
    }
  }
}

void PSPFrontend::issueLoadIndex(uint64_t _validEntryId) {
  // TODO: Implement function to get value infos (e.g., baseAddr, accessGranularity)
  uint64_t idxBaseAddr, idxAccessGranularity, valBaseAddr, valAccessGranularity;
  this->patternTable->getConfigInfo(_validEntryId, &idxBaseAddr, &idxAccessGranularity,
                                    &valBaseAddr, &valAccessGranularity);
  PSP_FE_DPRINTF("EntryId: %lu, IndexBaseAddress: %x, IndexAccessGranularity: %lu\n",
      _validEntryId, idxBaseAddr, idxAccessGranularity);

  uint64_t offsetBegin, offsetEnd;
  this->patternTable->getInputInfo(_validEntryId, &offsetBegin, &offsetEnd);
  
  if (offsetBegin >= offsetEnd) {
    this->patternTable->resetInput(_validEntryId);
    return;
  }

  PSP_FE_DPRINTF("EntryId: %lu, OffsetBegin: %lu, OffsetEnd: %lu\n",
      _validEntryId, offsetBegin, offsetEnd);
  assert(offsetBegin < offsetEnd);

  // Generate cacheline-aware memory requests
  uint64_t currentVAddr = idxBaseAddr + offsetBegin * idxAccessGranularity;
  uint64_t currentSize = (offsetEnd - offsetBegin) * idxAccessGranularity;
  uint64_t cacheLineSize = this->cpuDelegator->cacheLineSize();

  if (((currentVAddr % cacheLineSize) + currentSize) > cacheLineSize) {
    currentSize = cacheLineSize - (currentVAddr % cacheLineSize);
  }

  uint64_t cacheBlockVAddr = currentVAddr & (~(cacheLineSize - 1));

  // Address Translation (Does not consume tick)
  Addr cacheBlockPAddr;
  assert(this->cpuDelegator->translateVAddrOracle(cacheBlockVAddr, cacheBlockPAddr) && 
      "Page Fault is not we intend");
  assert((cacheBlockPAddr % cacheLineSize == 0) && "Not cacheline aligned");

  // Send load index request
  IndexPacketHandler* indexPacketHandler = new IndexPacketHandler(this, _validEntryId,
      cacheBlockVAddr, currentVAddr, currentSize);
  Request::Flags flags;
  PacketPtr pkt = GemForgePacketHandler::createGemForgePacket(
      cacheBlockPAddr, cacheLineSize, indexPacketHandler, nullptr /* Data */,
      cpuDelegator->dataMasterId(), 0 /* ContextId */, 0 /* PC */, flags);
  pkt->req->setVirt(cacheBlockVAddr);
  PSP_FE_DPRINTF("Prefetch for %luth entryId. VA: %x PA: %x Size: %d.\n", _validEntryId,
      cacheBlockVAddr, pkt->getAddr(), pkt->getSize());
 
  if (cpuDelegator->cpuType == GemForgeCPUDelegator::ATOMIC_SIMPLE) {
    // Directly send to memory for atomic cpu.
    this->cpuDelegator->sendRequest(pkt);
  } 
  else {
    // Send the pkt to translation. (Translation consume tick)
    this->translationBuffer->addTranslation(
        pkt, cpuDelegator->getSingleThreadContext(), nullptr);
  }
  
  // Update PatternTable
  uint32_t numInflightBytes = currentSize / idxAccessGranularity;
  offsetBegin += numInflightBytes;
  
  if (offsetBegin < offsetEnd) {
    this->patternTable->setInputInfo(_validEntryId, offsetBegin, offsetEnd);
  }
  else {
    this->patternTable->resetInput(_validEntryId);
  }
  this->patternTableArbiter->setLastChosenEntryId(_validEntryId);

  // Update IndexQueueArray(num of inflight load requests)
  this->indexQueueArray->numInflightBytes[_validEntryId] += currentSize;
  PSP_FE_DPRINTF("%luth IndexQueue with numInflightBytes: %d.\n", _validEntryId,
      this->indexQueueArray->numInflightBytes[_validEntryId]);
}

void PSPFrontend::issueTranslateValueAddress(uint64_t _validEntryId) {
  uint64_t index;
  this->indexQueueArray->read(_validEntryId, &index);
  PSP_FE_DPRINTF("EntryId: %lu, Index: %lu\n", _validEntryId, index);

  // TODO: Implement function to get value infos (e.g., baseAddr, accessGranularity)
  uint64_t idxBaseAddr, idxAccessGranularity, valBaseAddr, valAccessGranularity;
  this->patternTable->getConfigInfo(_validEntryId, &idxBaseAddr, &idxAccessGranularity,
                                    &valBaseAddr, &valAccessGranularity);

  // Generate 4KB aligned address translation requests
  uint64_t currentVAddr = valBaseAddr + (index + 1) * valAccessGranularity - this->valCurrentSize[_validEntryId];
  uint64_t currentSize = this->valCurrentSize[_validEntryId];
  uint64_t cacheLineSize = this->cpuDelegator->cacheLineSize();
  uint64_t pageSize = 1 << 12;

  if (((currentVAddr % pageSize) + currentSize) > pageSize) {
    // If current value is cross-page, split address translate
    currentSize = (currentVAddr + currentSize) % pageSize;
    this->valCurrentSize[_validEntryId] = currentSize;
  }
  else {
    // Proceed to next index
    this->indexQueueArray->pop(_validEntryId);
    this->valCurrentSize[_validEntryId] = valAccessGranularity;
  }
  uint64_t cacheBlockVAddr = currentVAddr & (~(cacheLineSize - 1));
  uint64_t cacheBlockSize = (currentSize + cacheLineSize) & (~(cacheLineSize - 1));

  // Address translation (Does not consume tick)
  Addr cacheBlockPAddr;
  assert(this->cpuDelegator->translateVAddrOracle(cacheBlockVAddr, cacheBlockPAddr) && 
      "Page Fault is not we intend");

  // VA to PA
  IndexPacketHandler* indexPacketHandler = new IndexPacketHandler(this, _validEntryId,
      cacheBlockVAddr, currentVAddr, currentSize);
  Request::Flags flags;
  PacketPtr pkt = GemForgePacketHandler::createGemForgePacket(
      cacheBlockPAddr, currentSize, indexPacketHandler, nullptr /* Data */,
      cpuDelegator->dataMasterId(), 0 /* ContextId */, 0 /* PC */, flags);
  pkt->req->setVirt(cacheBlockVAddr);
  PSP_FE_DPRINTF("Address translation for %luth entryId. VA: %x PA: %x Size: %d.\n", _validEntryId,
      cacheBlockVAddr, pkt->getAddr(), pkt->getSize());
 
  if (cpuDelegator->cpuType == GemForgeCPUDelegator::ATOMIC_SIMPLE) {
    // No requests sent to memory for atomic cpu.
  } 
  else {
    // Send the pkt to translation. (Translation consume tick)
    this->translationBuffer->addTranslation(
        pkt, cpuDelegator->getSingleThreadContext(), (void*)indexPacketHandler,
        true /* VA to PA only */);
  }
  
  // Update PAQueueArray(num of inflight load requests)
  this->paQueueArray->numInflightTranslations[_validEntryId] += 1;
  PSP_FE_DPRINTF("%luth IndexQueue with numInflightTranslations: %d.\n", _validEntryId,
      this->paQueueArray->numInflightTranslations[_validEntryId]);
}

void PSPFrontend::handlePacketResponse(IndexPacketHandler* indexPacketHandler,
                                       PacketPtr pkt) {
  uint64_t entryId = indexPacketHandler->entryId;
  Addr cacheBlockVAddr = indexPacketHandler->cacheBlockVAddr;
  Addr vaddr = indexPacketHandler->vaddr;
  void* data = pkt->getPtr<void>();
  uint64_t packetSize = pkt->getSize();
  uint64_t inputSize = indexPacketHandler->size;
  if (cacheBlockVAddr < vaddr) { 
    data += (packetSize - inputSize); 
  }
  this->indexQueueArray->insert(entryId, data, inputSize);
  this->indexQueueArray->numInflightBytes[entryId] -= inputSize;
  PSP_FE_DPRINTF("%luth IndexQueue filled with %lu data size.\n", entryId, inputSize);
}

void PSPFrontend::handleAddressTranslateResponse(IndexPacketHandler* _indexPacketHandler,
                                                 PacketPtr _pkt) {
  uint64_t entryId = _indexPacketHandler->entryId;
  Addr pAddr = _pkt->getAddr();
  uint64_t size = _pkt->getSize();
  this->paQueueArray->numInflightTranslations[entryId] -= 1;

  PhysicalAddressQueue::PhysicalAddressArgs args(entryId, pAddr, size);
  this->paQueueArray->insert(entryId, &args);
  
  PSP_FE_DPRINTF("Address translation for %luth entryId done. PA: %x, Size: %lu, numInflight: %lu.\n", entryId, pAddr, size, this->paQueueArray->numInflightTranslations[entryId]);
}

/********************************************************************************
 * StreamConfig Handlers.
 *******************************************************************************/

bool PSPFrontend::canDispatchStreamConfig(const StreamConfigArgs &args) {
  uint64_t entryId = args.entryId;
  return !(this->patternTable->isConfigInfoValid(entryId)
      && this->indexQueueArray->getConfigured(entryId));
}

void PSPFrontend::dispatchStreamConfig(const StreamConfigArgs &args) {
}

bool PSPFrontend::canExecuteStreamConfig(const StreamConfigArgs &args) {
  return true;
}

void PSPFrontend::executeStreamConfig(const StreamConfigArgs &args) {
  uint64_t entryId = args.entryId;
  uint64_t idxBaseAddr = args.config.at(0);
  uint64_t idxAccessGranularity = args.config.at(1);
  uint64_t valBaseAddr = args.config.at(2);
  uint64_t valAccessGranularity = args.config.at(3);

  this->valCurrentSize[entryId] = valAccessGranularity;
  this->patternTable->setConfigInfo(entryId,
                                    idxBaseAddr, idxAccessGranularity,
                                    valBaseAddr, valAccessGranularity);
  this->indexQueueArray->setConfigured(entryId, true);
  this->indexQueueArray->setAccessGranularity(entryId, idxAccessGranularity);
}

bool PSPFrontend::canCommitStreamConfig(const StreamConfigArgs &args) {
  uint64_t entryId = args.entryId;
  return this->patternTable->isConfigInfoValid(entryId) &&
    this->indexQueueArray->getConfigured(entryId);
}

void PSPFrontend::commitStreamConfig(const StreamConfigArgs &args) {

}

void PSPFrontend::rewindStreamConfig(const StreamConfigArgs &args) {
  uint64_t entryId = args.entryId;
  this->patternTable->resetConfig(entryId);
  this->indexQueueArray->setConfigured(entryId, false);
}

/********************************************************************************
 * StreamInput Handlers.
 *******************************************************************************/

bool PSPFrontend::canDispatchStreamInput(const StreamInputArgs &args) {
  uint64_t entryId = args.entryId;
  return !(this->patternTable->isInputInfoValid(entryId));
}

void PSPFrontend::dispatchStreamInput(const StreamInputArgs &args) {
}

bool PSPFrontend::canExecuteStreamInput(const StreamInputArgs &args) {
  return true;
}

void PSPFrontend::executeStreamInput(const StreamInputArgs &args) {
  uint64_t entryId = args.entryId;
  uint64_t offsetBegin = args.input.at(0);
  uint64_t offsetEnd = args.input.at(1);

  this->patternTable->setInputInfo(entryId, offsetBegin, offsetEnd);
}

bool PSPFrontend::canCommitStreamInput(const StreamInputArgs &args) {
  uint64_t entryId = args.entryId;
  return this->patternTable->isInputInfoValid(entryId);
}

void PSPFrontend::commitStreamInput(const StreamInputArgs &args) {
  this->manager->scheduleTickNextCycle();
//  uint64_t entryId = args.entryId;
//  this->patternTable->resetInput(entryId);
}

void PSPFrontend::rewindStreamInput(const StreamInputArgs &args) {
  uint64_t entryId = args.entryId;
  this->patternTable->resetInput(entryId);
}

/********************************************************************************
 * StreamTerminate Handlers.
 *******************************************************************************/

bool PSPFrontend::canDispatchStreamTerminate(const StreamTerminateArgs &args) {
  uint64_t entryId = args.entryId;
  bool validConfig = this->patternTable->isConfigInfoValid(entryId) &&
    this->indexQueueArray->getConfigured(entryId);
  bool inputAllConsumed = !(this->patternTable->isInputInfoValid(entryId));
  return validConfig && inputAllConsumed;
}

void PSPFrontend::dispatchStreamTerminate(const StreamTerminateArgs &args) {

}

bool PSPFrontend::canExecuteStreamTerminate(const StreamTerminateArgs &args) {
  return true;
}

void PSPFrontend::executeStreamTerminate(const StreamTerminateArgs &args) {
  uint64_t entryId = args.entryId;
  this->patternTable->resetConfig(entryId);
  this->indexQueueArray->getConfigured(entryId);
}

bool PSPFrontend::canCommitStreamTerminate(const StreamTerminateArgs &args) {
  uint64_t entryId = args.entryId;
  bool invalidConfig = !(this->patternTable->isConfigInfoValid(entryId) &&
      this->indexQueueArray->getConfigured(entryId));
  bool invalidInput = !(this->patternTable->isInputInfoValid(entryId));
  return invalidConfig && invalidInput;
}

void PSPFrontend::commitStreamTerminate(const StreamTerminateArgs &args) {

}

void PSPFrontend::rewindStreamTerminate(const StreamTerminateArgs &args) {
  this->manager->scheduleTickNextCycle();
  uint64_t entryId = args.entryId;
  this->patternTable->resetConfigUndo(entryId);
  this->indexQueueArray->setConfigured(entryId, true);
}

PSPFrontend *PSPFrontendParams::create() { return new PSPFrontend(this); }
