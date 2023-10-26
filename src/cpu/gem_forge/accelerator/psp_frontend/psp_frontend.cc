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
    warn("Sipal!!");
    this->totalPatternTableEntries = params->totalPatternTableEntries;
//    patternTable.reserve(params->totalPatternTableEntries);
    for (uint32_t i = 0; i < params->totalPatternTableEntries; i++) {
      PatternTableEntry* entry = new PatternTableEntry;
      patternTable.emplace_back(entry);
    }
    warn("patternTable size: %d.", patternTable.size());
    patternTableArbiter = new PatternTableRRArbiter(params->totalPatternTableEntries);
}

PSPFrontend::~PSPFrontend() {
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
  warn("patternTable size: %d.", patternTable.size());
  bool value = patternTable[entryId]->isConfigInfoValid();
  warn("Return value: %d.", value);
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

//  this->patternTable.setConfigInfo(entryId,
//                                   idxBaseAddr, idxAccessGranularity,
//                                   valBaseAddr, valAccessGranularity);
}

bool PSPFrontend::canCommitStreamConfig(const StreamConfigArgs &args) {
  return true;
}

void PSPFrontend::commitStreamConfig(const StreamConfigArgs &args) {

}

void PSPFrontend::rewindStreamConfig(const StreamConfigArgs &args) {

}

/********************************************************************************
 * StreamInput Handlers.
 *******************************************************************************/

bool PSPFrontend::canDispatchStreamInput(const StreamInputArgs &args) {
  auto entryId = args.entryId;
  return true;
//  return this->patternTable.isInputInfoValid(entryId);
}

void PSPFrontend::dispatchStreamInput(const StreamInputArgs &args) {

}

bool PSPFrontend::canExecuteStreamInput(const StreamInputArgs &args) {
  return true;
}

void PSPFrontend::executeStreamInput(const StreamInputArgs &args) {

}

bool PSPFrontend::canCommitStreamInput(const StreamInputArgs &args) {
  return true;
}

void PSPFrontend::commitStreamInput(const StreamInputArgs &args) {

}

void PSPFrontend::rewindStreamInput(const StreamInputArgs &args) {

}

/********************************************************************************
 * StreamTerminate Handlers.
 *******************************************************************************/

bool PSPFrontend::canDispatchStreamTerminate(const StreamTerminateArgs &args) {
  return true;
}

void PSPFrontend::dispatchStreamTerminate(const StreamTerminateArgs &args) {

}

bool PSPFrontend::canExecuteStreamTerminate(const StreamTerminateArgs &args) {
  return true;
}

void PSPFrontend::executeStreamTerminate(const StreamTerminateArgs &args) {

}

bool PSPFrontend::canCommitStreamTerminate(const StreamTerminateArgs &args) {
  return true;
}

void PSPFrontend::commitStreamTerminate(const StreamTerminateArgs &args) {

}

void PSPFrontend::rewindStreamTerminate(const StreamTerminateArgs &args) {

}

PSPFrontend *PSPFrontendParams::create() { return new PSPFrontend(this); }
