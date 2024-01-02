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
                                       int _size, uint64_t _seqNum, bool _isIndex, bool _isDataPrefetchOnly, int _additional_delay)
  : pspFrontend(_pspFrontend), entryId(_entryId), cacheBlockVAddr(_cacheBlockVAddr),
    vaddr(_vaddr), size(_size), seqNum(_seqNum), isIndex(_isIndex), isDataPrefetchOnly(_isDataPrefetchOnly), additionalDelay(_additional_delay) {
}

void 
IndexPacketHandler::handlePacketResponse(GemForgeCPUDelegator* cpuDelegator,
                                              PacketPtr pkt) {
  if (this->additionalDelay != 0) {
    PSP_FE_DPRINTF("PacketResposne with additional delay. Reschedule after %d cycle.\n",
        this->additionalDelay);
    auto responseEvent = new ResponseEvent(cpuDelegator, this, pkt, "handlePacketResponse");
    cpuDelegator->schedule(responseEvent, Cycles(this->additionalDelay));
    this->additionalDelay = 0;
    return;
  }
  
//  if (this->isDataPrefetchOnly) {
//    delete pkt;
//    delete this;
//    return;
//  }
  this->pspFrontend->handlePacketResponse(this, pkt);

  delete pkt;
  delete this;
}

void 
IndexPacketHandler::handleAddressTranslateResponse(GemForgeCPUDelegator* cpuDelegator,
                                              PacketPtr pkt) {
  if (this->additionalDelay != 0) {
    PSP_FE_DPRINTF("PacketResposne with additional delay. Reschedule after %d cycle.\n",
        this->additionalDelay);
    auto responseEvent = new ResponseEvent(cpuDelegator, this, pkt, "handleAddressTranslateResponse");
    cpuDelegator->schedule(responseEvent, Cycles(this->additionalDelay));
    this->additionalDelay = 0;
    return;
  }
  this->pspFrontend->handleAddressTranslateResponse(this, pkt);

//  if (!this->isDataPrefetchOnly) {
    delete pkt;
    delete this;
//  }
}

void
IndexPacketHandler::issueToMemoryCallback(GemForgeCPUDelegator* cpuDelegator) {
}

PSPFrontend::PSPFrontend(Params* params)
  : GemForgeAccelerator(params), totalPatternTableEntries(params->totalPatternTableEntries),
    isPSPBackendEnabled(params->isPSPBackendEnabled), 
    isTLBPrefetchOnly(params->isTLBPrefetchOnly),
    isDataPrefetchOnly(params->isDataPrefetchOnly),
    paQueueCapacity(params->paQueueCapacity),
    seqNum(0) {
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
void 
PSPFrontend::takeOverBy(GemForgeCPUDelegator *newCpuDelegator,
                        GemForgeAcceleratorManager *newManager) {
  GemForgeAccelerator::takeOverBy(newCpuDelegator, newManager);
  translationBuffer = new PSPTranslationBuffer<void*>(this->cpuDelegator->getDataTLB(),
      [this](PacketPtr pkt, ThreadContext* tc, void* ) -> void {
      this->cpuDelegator->sendRequest(pkt); },
      [this](PacketPtr pkt, ThreadContext* tc, void* indexPacketHandler) -> void {
      ((IndexPacketHandler*)indexPacketHandler)->handleAddressTranslateResponse(this->cpuDelegator, pkt); },
      false /* AccessLastLevelTLBOnly */, true /* MustDoneInOrder */);

  char pspbackend_name[100] = "system.ruby.l0_cntrl";
  char cpuId = (this->cpuDelegator->cpuId() + '0');
  strncat(pspbackend_name, &cpuId, 1);
  strcat(pspbackend_name, ".pspbackend");
  PSP_FE_DPRINTF("Matching %s...\n", pspbackend_name);
  for (auto so : SimObject::getSimObjectList()) {
    if (so->name() == pspbackend_name) {
      pspBackend = dynamic_cast<PSPBackend*>(so);
      PSP_FE_DPRINTF("MATCH!\n");
    }
  }
}

void
PSPFrontend::dump() {
}

void
PSPFrontend::regStats() {
  GemForgeAccelerator::regStats();

#define scalar(stat, describe)                                                 \
  this->stat.name(this->manager->name() + (".psp." #stat))                      \
      .desc(describe)                                                          \
      .prereq(this->stat)

  scalar(numConfigured, "Number of streams configured.");
#undef scalar
}

void 
PSPFrontend::resetStats() {
}

void
PSPFrontend::tick() {
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
  if (this->indexQueueArrayArbiter->getValidEntryId(&validIQEntryId, false)) {
//  if (this->indexQueueArrayArbiter->getValidEntryId(&validIQEntryId, this->isDataPrefetchOnly)) {
//    if (this->isDataPrefetchOnly) {
//      if (this->paQueueArray->getSize(validIQEntryId) + this->pspBackend->getTotalSize(validIQEntryId) < this->paQueueCapacity &&
//          this->inflightTranslations.size() < 4) {
//        this->issueLoadValue(validIQEntryId);
//      }
//      else { // If cannot issue prefetch, pass chance to next entry
//        this->indexQueueArrayArbiter->setLastChosenEntryId(validIQEntryId);
//      }
//    }
//    else {
      this->issueTranslateValueAddress(validIQEntryId);
//    }
  }

  /* Issue PA packets to PSP Backend*/
  for (uint32_t i = 0; i < this->totalPatternTableEntries; i++) {
    if (this->paQueueArray->canRead(i)) {
      if (this->isPSPBackendEnabled) {
        if (this->pspBackend->canInsertEntry(i)) {
          PhysicalAddressQueue::PhysicalAddressArgs validPAQEntry;
          this->paQueueArray->read(i, &validPAQEntry);
          if (this->seqNum < validPAQEntry.seqNum) continue;
          this->pspBackend->insertEntry(validPAQEntry.entryId, validPAQEntry.pAddr, validPAQEntry.size);
          PSP_FE_DPRINTF("paQueueEntryId: %lu, pAddr: %#x, size: %lu, seqNum: %lu, PSPBackend_canRead: %d\n",
              validPAQEntry.entryId, validPAQEntry.pAddr, validPAQEntry.size, validPAQEntry.seqNum,
              this->paQueueArray->canRead(i));
          this->paQueueArray->pop(i);
        }
      }
      else {
        // Deadlock prevent implementation to measure impact of TLB prefetch
        this->paQueueArray->pop(i);
      }
    }
  }
  this->pspBackend->printStatus();
}

void PSPFrontend::issueLoadIndex(uint64_t _validEntryId) {
  // TODO: Implement function to get value infos (e.g., baseAddr, accessGranularity)
  uint64_t idxBaseAddr, idxAccessGranularity, valBaseAddr, valAccessGranularity;
  this->patternTable->getConfigInfo(_validEntryId, &idxBaseAddr, &idxAccessGranularity,
                                    &valBaseAddr, &valAccessGranularity);
  PSP_FE_DPRINTF("EntryId: %lu, IndexBaseAddress: %x, IndexAccessGranularity: %lu\n",
      _validEntryId, idxBaseAddr, idxAccessGranularity);

  uint64_t offsetBegin, offsetEnd, seqNum;
  this->patternTable->getInputInfo(_validEntryId, &offsetBegin, &offsetEnd, &seqNum);

  PSP_FE_DPRINTF("EntryId: %lu, OffsetBegin: %lu, OffsetEnd: %lu, SeqNum: %lu\n",
      _validEntryId, offsetBegin, offsetEnd, seqNum);
  if (offsetBegin >= offsetEnd) {
    if (this->seqNum > seqNum) {
      PSP_FE_DPRINTF("Waiting for prediction result\n");
      return;
    }
    else {
      PSP_FE_DPRINTF("Pop Entry Id: %lu, Size: %lu\n", _validEntryId, this->patternTable->getInputSize(_validEntryId));
      this->patternTable->popInput(_validEntryId);
      return;
    }
  }

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
      cacheBlockVAddr, currentVAddr, currentSize, seqNum, true /* isIndex? */, false /* isDataPrefetchOnly */, 1 /* Latency for address compute */);
  Request::Flags flags;
  PacketPtr pkt = GemForgePacketHandler::createGemForgePacket(
      cacheBlockPAddr, cacheLineSize, indexPacketHandler, nullptr /* Data */,
      cpuDelegator->dataMasterId(), 0 /* ContextId */, 0 /* PC */, flags);
  pkt->req->setVirt(cacheBlockVAddr);
  PSP_FE_DPRINTF("Prefetch for %luth entryId. VA: %#x PA: %#x Size: %d.\n", _validEntryId,
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
  
  if (offsetBegin < offsetEnd || seqNum > this->seqNum) {
    this->patternTable->setInputInfo(_validEntryId, offsetBegin, offsetEnd);
    PSP_FE_DPRINTF("EntryId: %lu, isValid: %d, NewOffsetBegin: %lu, OffsetEnd: %lu\n",
        _validEntryId, this->patternTable->isInputInfoValid(_validEntryId), offsetBegin, offsetEnd);
  }
  else {
    PSP_FE_DPRINTF("Pop Entry Id: %lu, Size: %lu\n", _validEntryId, this->patternTable->getInputSize(_validEntryId));
    this->patternTable->popInput(_validEntryId);
  }
  this->patternTableArbiter->setLastChosenEntryId(_validEntryId);

  // Update IndexQueueArray
  this->indexQueueArray->allocate(_validEntryId, currentSize, seqNum);
  PSP_FE_DPRINTF("Index Queue EntryId: %lu, AllocatedSize: %lu, Size: %lu\n", 
      _validEntryId, this->indexQueueArray->getAllocatedSize(_validEntryId), this->indexQueueArray->getSize(_validEntryId));
}

void
PSPFrontend::issueLoadValue(uint64_t _validEntryId) {
  uint64_t index, seqNum;
  this->indexQueueArray->read(_validEntryId, &index, &seqNum);
  PSP_FE_DPRINTF("EntryId: %lu, Index: %lu, SeqNum: %lu %lu\n", _validEntryId, index, seqNum, this->indexQueueArray->getSize(_validEntryId));

  // TODO: Implement function to get value infos (e.g., baseAddr, accessGranularity)
  uint64_t idxBaseAddr, idxAccessGranularity, valBaseAddr, valAccessGranularity;
  this->patternTable->getConfigInfo(_validEntryId, &idxBaseAddr, &idxAccessGranularity,
                                    &valBaseAddr, &valAccessGranularity);

  // Generate address translation requests
  uint64_t currentVAddrBegin = valBaseAddr + (index + 1) * valAccessGranularity - this->valCurrentSize[_validEntryId];
  uint64_t currentVAddrEnd = valBaseAddr + (index + 1) * valAccessGranularity;
  uint64_t currentSize = this->valCurrentSize[_validEntryId];
  uint64_t cacheLineSize = this->cpuDelegator->cacheLineSize();

  uint64_t cacheBlockVAddr = currentVAddrBegin & (~(cacheLineSize - 1));
  uint64_t cacheBlockSize;

  // Address translation (Does not consume tick)
  Addr cacheBlockPAddr;
  assert(this->cpuDelegator->translateVAddrOracle(cacheBlockVAddr, cacheBlockPAddr) && 
      "Page Fault is not we intend");

  if (currentSize <= cacheLineSize) {
    // Proceed to next index
    this->indexQueueArray->pop(_validEntryId);
    this->valCurrentSize[_validEntryId] = valAccessGranularity;
    cacheBlockSize = cacheLineSize;
  }
  else {
    // If current value is cross-page, split address translate
    cacheBlockSize = cacheLineSize;
    currentSize -= cacheLineSize;
    this->valCurrentSize[_validEntryId] = currentSize;
  }

  // VA to PA
  IndexPacketHandler* indexPacketHandler = new IndexPacketHandler(this, _validEntryId,
      cacheBlockVAddr, currentVAddrBegin, currentSize, seqNum, false /* isIndex? */, true /* isDataPrefetchOnly? */, 1 /* Latency for address compute */);
  Request::Flags flags;
  PacketPtr pkt = GemForgePacketHandler::createGemForgePacket(
      cacheBlockPAddr, cacheBlockSize, indexPacketHandler, nullptr /* Data */,
      cpuDelegator->dataMasterId(), 0 /* ContextId */, 0 /* PC */, flags);
  pkt->req->setVirt(cacheBlockVAddr);
  PSP_FE_DPRINTF("Prefetch Value for %luth entryId (size: %lu). VA: %#x, BlockVA: %#x PA: %#x Size: %d.\n", _validEntryId,
      this->indexQueueArray->getSize(_validEntryId), currentVAddrBegin, cacheBlockVAddr, pkt->getAddr(), pkt->getSize());
 
  if (cpuDelegator->cpuType == GemForgeCPUDelegator::ATOMIC_SIMPLE) {
    // No requests sent to memory for atomic cpu.
    this->cpuDelegator->sendRequest(pkt);
  } 
  else {
    // Send the pkt to translation. (Translation consume tick)
    this->translationBuffer->addTranslation(
        pkt, cpuDelegator->getSingleThreadContext(), (void*)indexPacketHandler, true /* VA to PA only */);
  }
  this->indexQueueArrayArbiter->setLastChosenEntryId(_validEntryId);
  
  // Update PAQueueArray(num of inflight load requests)
  uint32_t paQueueId = this->paQueueArray->allocate(_validEntryId, seqNum);
  this->inflightTranslations.emplace(cacheBlockPAddr, paQueueId);
  PSP_FE_DPRINTF("%luth IndexQueue with numInflightLoadRequests: %d paQueueId: %d.\n", _validEntryId,
      this->inflightTranslations.size(), paQueueId);
}

void PSPFrontend::issueTranslateValueAddress(uint64_t _validEntryId) {
  uint64_t index, seqNum;
  this->indexQueueArray->read(_validEntryId, &index, &seqNum);
  PSP_FE_DPRINTF("EntryId: %lu, Index: %lu, SeqNum: %lu %lu\n", _validEntryId, index, seqNum, this->indexQueueArray->getSize(_validEntryId));

  // TODO: Implement function to get value infos (e.g., baseAddr, accessGranularity)
  uint64_t idxBaseAddr, idxAccessGranularity, valBaseAddr, valAccessGranularity;
  this->patternTable->getConfigInfo(_validEntryId, &idxBaseAddr, &idxAccessGranularity,
                                    &valBaseAddr, &valAccessGranularity);

  // Generate 4KB aligned address translation requests
  uint64_t cacheLineSize = this->cpuDelegator->cacheLineSize();
  uint64_t currentVAddrBegin = valBaseAddr + (index + 1) * valAccessGranularity - this->valCurrentSize[_validEntryId];
  uint64_t currentVAddrEnd = valBaseAddr + (index + 1) * valAccessGranularity;
  uint64_t currentSize = (this->valCurrentSize[_validEntryId] + (cacheLineSize - 1)) & (~(cacheLineSize - 1));
  uint64_t pageSize = this->isDataPrefetchOnly ? cacheLineSize : TheISA::PageBytes;

  uint64_t cacheBlockVAddr = currentVAddrBegin & (~(cacheLineSize - 1));
//  currentSize += (currentVAddrBegin - cacheBlockVAddr);
//  // This is hack for cacheline granularity size
//  if (currentSize % cacheLineSize > 0)
//    currentSize += (cacheLineSize - currentSize % cacheLineSize);

  uint64_t cacheBlockSize;// = currentSize; 

  // Address translation (Does not consume tick)
  Addr cacheBlockPAddr;
  assert(this->cpuDelegator->translateVAddrOracle(cacheBlockVAddr, cacheBlockPAddr) && 
      "Page Fault is not we intend");

  uint64_t pageRemain = pageSize - (cacheBlockPAddr % pageSize);
  if (currentSize <= pageRemain) {
    // Proceed to next index
    this->indexQueueArray->pop(_validEntryId);
    this->valCurrentSize[_validEntryId] = valAccessGranularity;
    cacheBlockSize = currentSize;
  }
  else {
    // If current value is cross-page, split address translate
    cacheBlockSize = pageRemain;
    currentSize -= pageRemain;
    this->valCurrentSize[_validEntryId] = currentSize;
  }

  // VA to PA
  IndexPacketHandler* indexPacketHandler = new IndexPacketHandler(this, _validEntryId,
      cacheBlockVAddr, currentVAddrBegin, currentSize, seqNum, false /* isIndex? */, false /* isDataPrefetchOnly */, 1 /* Latency for address compute */);
  Request::Flags flags;
  PacketPtr pkt = GemForgePacketHandler::createGemForgePacket(
      cacheBlockPAddr, cacheBlockSize, indexPacketHandler, nullptr /* Data */,
      cpuDelegator->dataMasterId(), 0 /* ContextId */, 0 /* PC */, flags);
  pkt->req->setVirt(cacheBlockVAddr);
  PSP_FE_DPRINTF("Address translation for %luth entryId (size: %lu). VA: %#x, BlockVA: %#x PA: %#x Size: %d, PageSize: %lu.\n", _validEntryId,
      this->indexQueueArray->getSize(_validEntryId), currentVAddrBegin, cacheBlockVAddr, pkt->getAddr(), pkt->getSize(), pageSize);
 
  if (cpuDelegator->cpuType == GemForgeCPUDelegator::ATOMIC_SIMPLE) {
    // No requests sent to memory for atomic cpu.
  } 
  else {
    // Send the pkt to translation. (Translation consume tick)
    this->translationBuffer->addTranslation(
        pkt, cpuDelegator->getSingleThreadContext(), (void*)indexPacketHandler,
        true /* VA to PA only */);
  }
  this->indexQueueArrayArbiter->setLastChosenEntryId(_validEntryId);
  
  // Update PAQueueArray(num of inflight load requests)
  uint32_t paQueueId = this->paQueueArray->allocate(_validEntryId, seqNum);
  this->inflightTranslations.emplace(cacheBlockPAddr, paQueueId);
  PSP_FE_DPRINTF("%luth IndexQueue with numInflightTranslations: %d paQueueId: %d.\n", _validEntryId,
      this->inflightTranslations.size(), paQueueId);
}

void PSPFrontend::handlePacketResponse(IndexPacketHandler* indexPacketHandler,
                                       PacketPtr pkt) {
  uint64_t entryId = indexPacketHandler->entryId;
  Addr cacheBlockVAddr = indexPacketHandler->cacheBlockVAddr;
  Addr vaddr = indexPacketHandler->vaddr;
  void* data = pkt->getPtr<void>();
  uint64_t packetSize = pkt->getSize();
  uint64_t inputSize = indexPacketHandler->size;
  uint64_t seqNum = indexPacketHandler->seqNum;
  data += (vaddr - cacheBlockVAddr);
  uint64_t debug_address;
  if (indexPacketHandler->isIndex) {
    this->indexQueueArray->insert(entryId, data, inputSize, debug_address, seqNum);
    PSP_FE_DPRINTF("%luth IndexQueue filled with blockVA: %#x, VA: %#x, size: %lu, debug_address, %#x, SeqNum: %lu %lu\n", entryId,
        cacheBlockVAddr, vaddr, inputSize, debug_address, seqNum, this->seqNum);
    for (int i = 0; i < inputSize / 8; i++) {
      PSP_FE_DPRINTF("data[%d]: %lu\n", i, ((uint64_t*)data)[i]);
    }
    PSP_FE_DPRINTF("\n");
  }
//  else {
//    Addr pAddr = pkt->getAddr();
//    uint32_t paQueueId = this->inflightTranslations.find(pAddr)->second;
//    PhysicalAddressQueue::PhysicalAddressArgs args(true, entryId, pAddr, packetSize, seqNum);
//    this->paQueueArray->insert(entryId, paQueueId, args);
//    this->inflightTranslations.erase(this->inflightTranslations.find(pAddr));
//    PSP_FE_DPRINTF("%luth packet to PSPBackend: PA: %#x, size: %lu, SeqNum: %lu %lu.\n", entryId, pAddr, packetSize,
//        seqNum, this->seqNum);
//  }
  this->manager->scheduleTickNextCycle();
}

void PSPFrontend::handleAddressTranslateResponse(IndexPacketHandler* _indexPacketHandler,
                                                 PacketPtr _pkt) {
  uint64_t entryId = _indexPacketHandler->entryId;
  Addr pAddr = _pkt->getAddr();
  uint64_t size = _pkt->getSize();
  uint64_t seqNum = _indexPacketHandler->seqNum;
  uint32_t paQueueId = this->inflightTranslations.find(pAddr)->second;

  PhysicalAddressQueue::PhysicalAddressArgs args(true, entryId, pAddr, size, seqNum);
  this->paQueueArray->insert(entryId, paQueueId, args);
  this->inflightTranslations.erase(this->inflightTranslations.find(pAddr));
  this->manager->scheduleTickNextCycle();
  
  PSP_FE_DPRINTF("Address translation for %luth entryId done. PA: %#x, Size: %lu, numInflightTranslations: %lu, SeqNum: %lu %lu.\n",
      entryId, pAddr, size, this->inflightTranslations.size(), seqNum, this->seqNum);
  PSP_FE_DPRINTF("PAQeueuArray_canRead(%lu): %d, PSPBackend_canInsertEntry(%lu): %d\n", entryId, this->paQueueArray->canRead(entryId), entryId, this->pspBackend->canInsertEntry(entryId));

//  if (this->isDataPrefetchOnly) {
//    this->cpuDelegator->sendRequest(_pkt);
//  }
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
  return this->patternTable->canPushInputInfo(entryId);
}

void PSPFrontend::dispatchStreamInput(const StreamInputArgs &args) {
}

bool PSPFrontend::canExecuteStreamInput(const StreamInputArgs &args) {
  uint64_t entryId = args.entryId;
  return this->patternTable->canPushInputInfo(entryId);
}

void PSPFrontend::executeStreamInput(const StreamInputArgs &args) {
  uint64_t entryId = args.entryId;
  uint64_t offsetBegin = args.input.at(0);
  uint64_t offsetEnd = args.input.at(1);
  uint64_t seqNum = args.seqNum;

  this->patternTable->pushInputInfo(entryId, offsetBegin, offsetEnd, seqNum);
  this->manager->scheduleTickNextCycle();
  PSP_FE_DPRINTF("executeStreamInput...Schedule next tick %lu %lu %lu %lu %lu %lu %lu\n",
      entryId, offsetBegin, offsetEnd, seqNum, this->indexQueueArray->getAllocatedSize(entryId),
      this->indexQueueArray->getSize(entryId), this->paQueueArray->getSize(entryId));
//  this->pspBackend->printStatus();
}

bool PSPFrontend::canCommitStreamInput(const StreamInputArgs &args) {
  return true;
}

void PSPFrontend::commitStreamInput(const StreamInputArgs &args) {
  this->seqNum = this->seqNum < args.seqNum ? args.seqNum : this->seqNum;
  PSP_FE_DPRINTF("commitStreamInput\n");
}

void PSPFrontend::rewindStreamInput(const StreamInputArgs &args) {
  uint64_t entryId = args.entryId;
  uint64_t offsetBegin, offsetEnd, seqNum;
  this->patternTable->getInputInfo(entryId, &offsetBegin, &offsetEnd, &seqNum);
  if (seqNum == args.seqNum) {
    this->patternTable->resetInput(entryId);
  }
  this->indexQueueArray->reset(entryId, seqNum);
  this->paQueueArray->reset(entryId, seqNum);
  PSP_FE_DPRINTF("rewindStreamInput %lu %lu %lu\n", 
      this->patternTable->getInputSize(entryId), this->indexQueueArray->getSize(entryId),
      this->paQueueArray->getSize(entryId));
}

/********************************************************************************
 * StreamTerminate Handlers.
 *******************************************************************************/

bool PSPFrontend::canDispatchStreamTerminate(const StreamTerminateArgs &args) {
  uint64_t entryId = args.entryId;
  bool validConfig = this->patternTable->isConfigInfoValid(entryId) &&
    this->indexQueueArray->getConfigured(entryId);
  bool inputAllConsumed = !(this->patternTable->isInputInfoValid(entryId));
  uint64_t offsetBegin, offsetEnd, seqNum;
  this->patternTable->getInputInfo(entryId, &offsetBegin, &offsetEnd, &seqNum);
  PSP_FE_DPRINTF("%lu %lu %lu %lu\n", offsetBegin, offsetEnd, seqNum, this->seqNum);

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
  this->indexQueueArray->reset(entryId, seqNum);
  this->paQueueArray->reset(entryId, seqNum);
//  this->indexQueueArray->getConfigured(entryId);
}

bool PSPFrontend::canCommitStreamTerminate(const StreamTerminateArgs &args) {
  uint64_t entryId = args.entryId;
  bool validConfig = (this->patternTable->isConfigInfoValid(entryId) &&
      this->indexQueueArray->getConfigured(entryId));
  bool invalidInput = !(this->patternTable->isInputInfoValid(entryId));
  return validConfig && invalidInput;
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
