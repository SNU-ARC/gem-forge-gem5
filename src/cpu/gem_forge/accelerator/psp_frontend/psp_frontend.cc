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
    patternTable = new PatternTable(params->totalPatternTableEntries); 
    indexQueueArray = new IndexQueueArray(params->totalPatternTableEntries,
                                          params->indexQueueCapacity);
    patternTableArbiter = new PatternTableRRArbiter(params->totalPatternTableEntries,
                                                    patternTable, indexQueueArray);
}

PSPFrontend::~PSPFrontend() {
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
      false /* AccessLastLevelTLBOnly */, true /* MustDoneInOrder */);
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

  uint32_t validEntryId;
  uint64_t cacheLineSize = this->cpuDelegator->cacheLineSize();
  if (!this->patternTableArbiter->getValidEntryId(&validEntryId, cacheLineSize)) {
    return;
  }
  PSP_FE_DPRINTF("ValidEntryId: %d\n", validEntryId);
  this->issueLoadIndex(validEntryId);
}

void PSPFrontend::issueLoadIndex(uint64_t _validEntryId) {
  uint64_t idxBaseAddr, idxAccessGranularity, valBaseAddr, valAccessGranularity;
  this->patternTable->getConfigInfo(_validEntryId, &idxBaseAddr, &idxAccessGranularity,
                                    &valBaseAddr, &valAccessGranularity);
  PSP_FE_DPRINTF("EntryId: %lu, IndexBaseAddress: %x, IndexAccessGranularity: %lu\n",
      _validEntryId, idxBaseAddr, idxAccessGranularity);

  uint64_t offsetBegin, offsetEnd;
  this->patternTable->getInputInfo(_validEntryId, &offsetBegin, &offsetEnd);
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

  // Update IndexQueueArray(num of inflight load requests)
  this->indexQueueArray->numInflightBytes[_validEntryId] += currentSize;
  PSP_FE_DPRINTF("%luth IndexQueue with numInflightBytes: %d.\n", _validEntryId,
      this->indexQueueArray->numInflightBytes[_validEntryId]);
}

void PSPFrontend::handlePacketResponse(IndexPacketHandler* indexPacketHandler,
                                       PacketPtr pkt) {
  uint64_t entryId = indexPacketHandler->entryId;
  void* data = pkt->getPtr<void>();
  uint64_t packetSize = pkt->getSize();
  uint64_t inputSize = indexPacketHandler->size;
  data += (packetSize - inputSize);
  this->indexQueueArray->insert(entryId, data, inputSize);
  this->indexQueueArray->numInflightBytes[entryId] -= inputSize;
  PSP_FE_DPRINTF("%luth IndexQueue filled with %lu data size.\n", entryId, inputSize);

//  uint64_t elemSize = this->indexQueueArray->getAccessGranularity(entryId);
//  uint64_t iterator = inputSize / elemSize;
//  uint64_t* print_data = (uint64_t*)data;
//  for (uint64_t i = 0; i < iterator; i++) {
//    PSP_FE_DPRINTF("%lu\n", print_data[i]);
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
