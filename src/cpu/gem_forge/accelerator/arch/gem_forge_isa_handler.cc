#include "gem_forge_isa_handler.hh"

#include "cpu/base.hh"
#include "cpu/exec_context.hh"

// Editor: Sungjun Jung (miguel92@snu.ac.kr)
// Description: Append Programmable Stream Prefetcher instructions 
#define StreamInstCase(stage, xc...)                                           \
  case GemForgeStaticInstOpE::STREAM_CONFIG: {                                 \
    se.stage##StreamConfig(dynInfo, ##xc);                                     \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_INPUT: {                                  \
    se.stage##StreamInput(dynInfo, ##xc);                                      \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_READY: {                                  \
    se.stage##StreamReady(dynInfo, ##xc);                                      \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_END: {                                    \
    se.stage##StreamEnd(dynInfo, ##xc);                                        \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_STEP: {                                   \
    se.stage##StreamStep(dynInfo, ##xc);                                       \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_LOAD:                                     \
  case GemForgeStaticInstOpE::STREAM_FLOAD: {                                  \
    se.stage##StreamLoad(dynInfo, ##xc);                                       \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_STORE: {                                  \
    se.stage##StreamStore(dynInfo, ##xc);                                      \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_CFG_IDX_BASE: {                           \
    psp.stage##StreamConfigIndexBase(dynInfo, ##xc);                           \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_CFG_IDX_GRAN: {                           \
    psp.stage##StreamConfigIndexGranularity(dynInfo, ##xc);                    \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_CFG_VAL_BASE: {                           \
    psp.stage##StreamConfigValueBase(dynInfo, ##xc);                           \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_CFG_VAL_GRAN: {                           \
    psp.stage##StreamConfigValueGranularity(dynInfo, ##xc);                    \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_INPUT_OFFSET_BEGIN: {                     \
    psp.stage##StreamInputBegin(dynInfo, ##xc);                                \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_INPUT_OFFSET_END: {                       \
    psp.stage##StreamInputEnd(dynInfo, ##xc);                                  \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_CFG_READY: {                              \
    psp.stage##StreamConfigReady(dynInfo, ##xc);                              \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_INPUT_READY: {                            \
    psp.stage##StreamInputReady(dynInfo, ##xc);                                \
    break;                                                                     \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_TERMINATE: {                              \
    psp.stage##StreamTerminate(dynInfo, ##xc);                                 \
    break;                                                                     \
  }

#define StreamInstRetCase(stage, xc...)                                        \
  case GemForgeStaticInstOpE::STREAM_CONFIG: {                                 \
    return se.stage##StreamConfig(dynInfo, ##xc);                              \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_INPUT: {                                  \
    return se.stage##StreamInput(dynInfo, ##xc);                               \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_READY: {                                  \
    return se.stage##StreamReady(dynInfo, ##xc);                               \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_END: {                                    \
    return se.stage##StreamEnd(dynInfo, ##xc);                                 \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_STEP: {                                   \
    return se.stage##StreamStep(dynInfo, ##xc);                                \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_LOAD:                                     \
  case GemForgeStaticInstOpE::STREAM_FLOAD: {                                  \
    return se.stage##StreamLoad(dynInfo, ##xc);                                \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_STORE: {                                  \
    return se.stage##StreamStore(dynInfo, ##xc);                               \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_CFG_IDX_BASE: {                           \
    return psp.stage##StreamConfigIndexBase(dynInfo, ##xc);                    \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_CFG_IDX_GRAN: {                           \
    return psp.stage##StreamConfigIndexGranularity(dynInfo, ##xc);             \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_CFG_VAL_BASE: {                           \
    return psp.stage##StreamConfigValueBase(dynInfo, ##xc);                    \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_CFG_VAL_GRAN: {                           \
    return psp.stage##StreamConfigValueGranularity(dynInfo, ##xc);             \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_INPUT_OFFSET_BEGIN: {                     \
    return psp.stage##StreamInputBegin(dynInfo, ##xc);                         \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_INPUT_OFFSET_END: {                       \
    return psp.stage##StreamInputEnd(dynInfo, ##xc);                           \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_CFG_READY: {                              \
    return psp.stage##StreamConfigReady(dynInfo, ##xc);                        \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_INPUT_READY: {                            \
    return psp.stage##StreamInputReady(dynInfo, ##xc);                         \
  }                                                                            \
  case GemForgeStaticInstOpE::STREAM_TERMINATE: {                              \
    return psp.stage##StreamTerminate(dynInfo, ##xc);                          \
  }

bool GemForgeISAHandler::shouldCountInPipeline(
    const GemForgeDynInstInfo &dynInfo) {
  if (!dynInfo.staticInst->isGemForge()) {
    return true;
  }
  auto &staticInstInfo = this->getStaticInstInfo(dynInfo);
  switch (staticInstInfo.op) {
  // Only step and load are considered no overhead in pipeline.
  // Store should still be counted, as it serves as the placehoder.
  case GemForgeStaticInstOpE::STREAM_STEP:
  case GemForgeStaticInstOpE::STREAM_LOAD:
  case GemForgeStaticInstOpE::STREAM_FLOAD: {
    return false;
  }
  default: {
    return true;
  }
  }
}

bool GemForgeISAHandler::canDispatch(const GemForgeDynInstInfo &dynInfo) {
  if (!dynInfo.staticInst->isGemForge()) {
    return true;
  }
  auto &staticInstInfo = this->getStaticInstInfo(dynInfo);
  switch (staticInstInfo.op) {
    StreamInstRetCase(canDispatch);
  default: {
    return true;
  }
  }
}

void GemForgeISAHandler::dispatch(const GemForgeDynInstInfo &dynInfo,
                                  GemForgeLSQCallbackList &extraLSQCallbacks) {
  if (!dynInfo.staticInst->isGemForge()) {
    return;
  }
  auto &staticInstInfo = this->getStaticInstInfo(dynInfo);
  switch (staticInstInfo.op) {
    StreamInstCase(dispatch, extraLSQCallbacks);
  default: {
    break;
  }
  }
}

bool GemForgeISAHandler::canExecute(const GemForgeDynInstInfo &dynInfo) {
  if (!dynInfo.staticInst->isGemForge()) {
    return true;
  }
  auto &staticInstInfo = this->getStaticInstInfo(dynInfo);
  switch (staticInstInfo.op) {
    StreamInstRetCase(canExecute);
  default: {
    return true;
  }
  }
}

void GemForgeISAHandler::execute(const GemForgeDynInstInfo &dynInfo,
                                 ExecContext &xc) {
  if (!dynInfo.staticInst->isGemForge()) {
    return;
  }
  auto &staticInstInfo = this->getStaticInstInfo(dynInfo);
  switch (staticInstInfo.op) {
    StreamInstCase(execute, xc);
  default: {
    break;
  }
  }
}

bool GemForgeISAHandler::canCommit(const GemForgeDynInstInfo &dynInfo) {
  if (!dynInfo.staticInst->isGemForge()) {
    return true;
  }
  auto &staticInstInfo = this->getStaticInstInfo(dynInfo);
  switch (staticInstInfo.op) {
    StreamInstRetCase(canCommit);
  default: {
    return true;
  }
  }
}

void GemForgeISAHandler::commit(const GemForgeDynInstInfo &dynInfo) {
  if (!dynInfo.staticInst->isGemForge()) {
    return;
  }
  auto &staticInstInfo = this->getStaticInstInfo(dynInfo);
  switch (staticInstInfo.op) {
    StreamInstCase(commit);
  default: {
    break;
  }
  }
}

void GemForgeISAHandler::rewind(const GemForgeDynInstInfo &dynInfo) {
  if (!dynInfo.staticInst->isGemForge()) {
    return;
  }
  auto &staticInstInfo = this->getStaticInstInfo(dynInfo);
  switch (staticInstInfo.op) {
    StreamInstCase(rewind);
  default: {
    break;
  }
  }
}

void GemForgeISAHandler::storeTo(InstSeqNum seqNum, Addr vaddr, int size) {
  se.storeTo(seqNum, vaddr, size);
}

GemForgeISAHandler::GemForgeStaticInstInfo &
GemForgeISAHandler::getStaticInstInfo(const GemForgeDynInstInfo &dynInfo) {

  auto pcKey = std::make_pair<Addr, MicroPC>(dynInfo.pc.pc(), dynInfo.pc.upc());

  auto &infoMap = dynInfo.staticInst->isMicroop()
                      ? this->cachedStaticMicroInstInfo
                      : this->cachedStaticMacroInstInfo;

  auto emplaceRet =
      infoMap.emplace(std::piecewise_construct, std::forward_as_tuple(pcKey),
                      std::forward_as_tuple());
  if (emplaceRet.second) {
    // Newly created. Do basic analysis.
    // * Simply use the instruction name may be a bad idea, but it decouples
    // * us from the encoding of the instruction in a specific ISA.
    auto instName = dynInfo.staticInst->getName();
    auto &staticInstInfo = emplaceRet.first->second;

    // Editor: Sungjun Jung (miguel92@snu.ac.kr)
    // Description: Append Programmable Stream Prefetch instructions
    if (instName == "ssp_stream_config") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_CONFIG;
    } else if (instName == "ssp_stream_end") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_END;
    } else if (instName == "ssp_stream_step") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_STEP;
    } else if (instName == "ssp_stream_input") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_INPUT;
    } else if (instName == "ssp_stream_ready") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_READY;
    } else if (instName == "ssp_stream_load") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_LOAD;
    } else if (instName == "ssp_stream_fload") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_LOAD;
    } else if (instName == "ssp_stream_atomic") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_LOAD;
    } else if (instName == "ssp_stream_fatomic") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_LOAD;
    } else if (instName == "ssp_stream_store") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_STORE;
    } else if (instName == "stream_cfg_idx_base") { // From here, PSP instructions
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_CFG_IDX_BASE;
    } else if (instName == "stream_cfg_idx_gran") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_CFG_IDX_GRAN;
    } else if (instName == "stream_cfg_val_base") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_CFG_VAL_BASE;
    } else if (instName == "stream_cfg_val_gran") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_CFG_VAL_GRAN;
    } else if (instName == "stream_input_offset_begin") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_INPUT_OFFSET_BEGIN;
    } else if (instName == "stream_input_offset_end") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_INPUT_OFFSET_END;
    } else if (instName == "stream_terminate") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_TERMINATE;
    } else if (instName == "stream_cfg_ready") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_CFG_READY;
    } else if (instName == "stream_input_ready") {
      staticInstInfo.op = GemForgeStaticInstOpE::STREAM_INPUT_READY;
    }
  }
  return emplaceRet.first->second;
}

void GemForgeISAHandler::takeOverBy(GemForgeCPUDelegator *newDelegator) {
  this->cpuDelegator = newDelegator;
  this->se.takeOverBy(newDelegator);
  this->psp.takeOverBy(newDelegator);
}
