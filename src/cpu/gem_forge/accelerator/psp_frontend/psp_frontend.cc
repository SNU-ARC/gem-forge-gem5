#include "psp_frontend.hh"

#include "debug/PSPFrontend.hh"

#define PSP_FE_PANIC(format, args...)                                          \
  panic("%llu-[PSP_FE%d] " format, cpuDelegator->curCycle(),                   \
        cpuDelegator->cpuId(), ##args)
#define PSP_FE_DPRINTF(format, args...)                                        \
  DPRINTF(PSPFrontend, "%llu-[PSP_FE%d] " format,                          \
          cpuDelegator->curCycle(), cpuDelegator->cpuId(), ##args)

PSPFrontend::PSPFrontend(Params* params)
  : GemForgeAccelerator(params) {
    this->totalPatternTableEntries = params->totalPatternTableEntries;
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
//  this->patternTableArbiter->getValidEntry(validPatternTableEntry);
//  if (validPatternTableEntry != nullptr) {
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
  uint64_t entryId = args.entryId;
  this->patternTable->resetInput(entryId);
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
  uint64_t entryId = args.entryId;
  this->patternTable->resetConfigUndo(entryId);
  this->indexQueueArray->setConfigured(entryId, true);
}

PSPFrontend *PSPFrontendParams::create() { return new PSPFrontend(this); }
