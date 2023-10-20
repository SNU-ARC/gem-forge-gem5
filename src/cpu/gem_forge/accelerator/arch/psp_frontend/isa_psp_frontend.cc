#include "isa_psp_frontend.hh"

#include "cpu/base.hh"
#include "cpu/exec_context.hh"
#include "cpu/gem_forge/accelerator/psp_frontend/psp_frontend.hh"
#include "debug/ISAPSPFrontend.hh"
#include "proto/protoio.hh"

#if THE_ISA == RISCV_ISA
#include "arch/riscv/insts/standard.hh"
#endif

#define ISA_PSP_FE_PANIC(format, args...)                                          \
  panic("%llu-[ISA_PSP_FE%d] " format, cpuDelegator->curCycle(),                   \
        cpuDelegator->cpuId(), ##args)
#define ISA_PSP_FE_DPRINTF(format, args...)                                        \
  DPRINTF(ISAPSPFrontend, "%llu-[ISA_PSP_FE%d] " format,                          \
          cpuDelegator->curCycle(), cpuDelegator->cpuId(), ##args)
#define DYN_INST_DPRINTF(format, args...)                                      \
  ISA_PSP_FE_DPRINTF("%llu %s " format, dynInfo.seqNum, dynInfo.pc, ##args)
#define DYN_INST_PANIC(format, args...)                                        \
  ISA_PSP_FE_PANIC("%llu %s " format, dynInfo.seqNum, dynInfo.pc, ##args)

//constexpr uint64_t ISAPSPFrontend::InvalidStreamId;

/********************************************************************************
 * StreamConfig Handlers.
 *******************************************************************************/

bool ISAPSPFrontend::canDispatchStreamConfig(
    const GemForgeDynInstInfo &dynInfo) {
  return true;
}

void ISAPSPFrontend::dispatchStreamConfig(
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {

  auto streamId = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &instInfo = this->createDynStreamInstInfo(dynInfo.seqNum);
  auto &regionInfo = this->curStreamRegionInfo;
  
  if (regionInfo == NULL) {
    // _prevRegion as input to DynStreamRegionInfo constructor
    regionInfo = instInfo.dynStreamRegionInfo = this->curStreamRegionInfo = std::make_shared<DynStreamRegionInfo>(regionInfo);

    DYN_INST_DPRINTF("[dispatch] %s %llu\n", regionInfo->getStageName(), streamId);

    this->curStreamRegionInfo->numDispatchedInsts++;

    if (regionInfo->prevRegion) {
      DYN_INST_DPRINTF("[dispatch] MustMisspeculated StreamConfig %llu : Has previous region.\n", streamId);
      instInfo.mustBeMisspeculated = true;
      instInfo.mustBeMisspeculatedReason = CONFIG_HAS_PREV_REGION;
      regionInfo->mustBeMisspeculated = true;
      return;
    }
  } else {
    instInfo.dynStreamRegionInfo = regionInfo;

    DYN_INST_DPRINTF("[dispatch] %s %llu.\n", regionInfo->getStageName(), streamId);

    this->curStreamRegionInfo->numDispatchedInsts++;

    // Check if the previous instruction is misspeculated.
    if (regionInfo->mustBeMisspeculated) {
      DYN_INST_DPRINTF("[dispatch] MustMisspeculated %s.\n", regionInfo->getStageName());
      instInfo.mustBeMisspeculated = true;
      return;
    }
  }
}

bool ISAPSPFrontend::canExecuteStreamConfig(
    const GemForgeDynInstInfo &dynInfo) {
  return true;
}

void ISAPSPFrontend::executeStreamConfig(const GemForgeDynInstInfo &dynInfo,
                                         ExecContext &xc) {
  auto &instInfo = this->getDynStreamInstInfo(dynInfo.seqNum);
  auto &regionInfo = this->curStreamRegionInfo;
  std::string stageName = regionInfo->getStageName();

  if (instInfo.mustBeMisspeculated) {
    DYN_INST_DPRINTF("[execute] MustMisspeculated %s.\n", stageName);
    return;
  }

  auto streamId = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &inputVec = regionInfo->getInputVec();

  for (int srcIdx = 0; srcIdx < dynInfo.staticInst->numSrcRegs(); ++srcIdx) {
    const auto &regId = dynInfo.staticInst->srcRegIdx(srcIdx);
    assert(regId.isIntReg());
    RegVal regValue = xc.readIntRegOperand(dynInfo.staticInst, srcIdx);
    inputVec.push_back(regValue);
  }

  DYN_INST_DPRINTF(
      "[execute] %s StreamId %llu Dispatched %d Executed %d.\n",
      stageName, streamId, 
      regionInfo->numDispatchedInsts,
      regionInfo->numExecutedInsts + 1);
  instInfo.executed = true;

  this->increamentStreamRegionInfoNumExecutedInsts(*regionInfo);
}

bool ISAPSPFrontend::canCommitStreamConfig(const GemForgeDynInstInfo &dynInfo) {
  return true;
}

void ISAPSPFrontend::commitStreamConfig(const GemForgeDynInstInfo &dynInfo) {
  // Release the InstInfo.
  auto &instInfo = this->getDynStreamInstInfo(dynInfo.seqNum);
  auto &regionInfo = this->curStreamRegionInfo;
  std::string stageName = regionInfo->getStageName();

  if (instInfo.mustBeMisspeculated) {
    panic("[commit] MustMisspeculated %s.\n", stageName);
  }
  this->seqNumToDynInfoMap.erase(dynInfo.seqNum);
  regionInfo->incrementStage();
}

void ISAPSPFrontend::rewindStreamConfig(const GemForgeDynInstInfo &dynInfo) {
  auto &instInfo = this->getDynStreamInstInfo(dynInfo.seqNum);
  auto &regionInfo = this->curStreamRegionInfo;
  std::string stageName = regionInfo->getStageName();

  if (instInfo.mustBeMisspeculated) {
    DYN_INST_DPRINTF("[rewind] MustMisspeculated %s.\n", stageName);
    this->seqNumToDynInfoMap.erase(dynInfo.seqNum);

    if (regionInfo->stage == 0) {
      this->curStreamRegionInfo = regionInfo->prevRegion;  
      assert(!this->curStreamRegionInfo && "Has previous stream region?");
    }
    return;
  }

  // Check if I executed.
  if (instInfo.executed) {
    regionInfo->numExecutedInsts--;
  }

  // Decrease numDispatchedInst.
  regionInfo->numDispatchedInsts--;

  // Rollback to previous region. 
  if (regionInfo->stage == 0) {
    this->curStreamRegionInfo = regionInfo->prevRegion;  
    assert(!this->curStreamRegionInfo && "Has previous stream region?");
  } else {
    regionInfo->inputMap.pop_back();
  }

  // Release the InstInfo.
  this->seqNumToDynInfoMap.erase(dynInfo.seqNum);
  regionInfo->decrementStage();
}

/********************************************************************************
 * StreamReady Handlers.
 *******************************************************************************/

bool ISAPSPFrontend::canDispatchStreamReady(const GemForgeDynInstInfo &dynInfo) {
  assert(this->curStreamRegionInfo && "Missing DynStreamRegionInfo.");

  auto &regionInfo = this->curStreamRegionInfo;
  std::string stageName = regionInfo->getStageName();

  if (this->curStreamRegionInfo->mustBeMisspeculated) {
    DYN_INST_DPRINTF("[canDispatch] MustMisspeculated %s.\n", stageName);
    return true;
  }

  auto psp = this->getPSPFrontend();
  bool canReady = false;
  if (regionInfo->isConfigStage()) {
    ::PSPFrontend::StreamConfigArgs args(dynInfo.seqNum, regionInfo->getInputVec());
    canReady = psp->canDispatchStreamConfig(args);
  } else {
    ::PSPFrontend::StreamInputArgs args(dynInfo.seqNum, regionInfo->getInputVec());
    canReady = psp->canDispatchStreamInput(args);
  }

  if (canReady) {
    DYN_INST_DPRINTF("[canDispatch] %s\n", stageName);
    return true;
  } else {
    DYN_INST_DPRINTF("[can X Dispatch] %s\n", stageName);
    return false;
  }
}

void ISAPSPFrontend::dispatchStreamReady(
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {
  assert(this->curStreamRegionInfo && "Missing DynStreamRegionInfo.");

  auto &instInfo = this->createDynStreamInstInfo(dynInfo.seqNum);
  auto &regionInfo = instInfo.dynStreamRegionInfo = this->curStreamRegionInfo;
  std::string stageName = regionInfo->getStageName();

  if (regionInfo->mustBeMisspeculated) {
    // Handle must be misspeculated.
    DYN_INST_DPRINTF("[dispatch] MustMisspeculated %s.\n", stageName);
    instInfo.mustBeMisspeculated = true;
    // Release the current DynStreamRegionInfo.
    instInfo.dynStreamRegionInfo = nullptr;
    this->curStreamRegionInfo = nullptr;
    return;
  }

  auto psp = this->getPSPFrontend();
  if (regionInfo->isConfigStage()) {
    ::PSPFrontend::StreamConfigArgs args(dynInfo.seqNum, regionInfo->getInputVec(), dynInfo.tc);
    psp->dispatchStreamConfig(args);
    regionInfo->streamConfigReadyDispatched = true;
  } else {
    ::PSPFrontend::StreamInputArgs args(dynInfo.seqNum, regionInfo->getInputVec(), dynInfo.tc);
    psp->dispatchStreamInput(args);
    regionInfo->streamInputReadyDispatched = true;
  }

  regionInfo->numDispatchedInsts++;
  regionInfo->streamReadySeqNum = dynInfo.seqNum;

  DYN_INST_DPRINTF("[dispatch] %s Dispatched %d Executed %d.\n",
                   stageName,
                   this->curStreamRegionInfo->numDispatchedInsts,
                   this->curStreamRegionInfo->numExecutedInsts);

  // Release the current DynStreamRegionInfo.
  this->curStreamRegionInfo = nullptr;
}

bool ISAPSPFrontend::canExecuteStreamReady(
    const GemForgeDynInstInfo &dynInfo) {
  return true;
}

void ISAPSPFrontend::executeStreamReady(const GemForgeDynInstInfo &dynInfo,
                                         ExecContext &xc) {

  auto &instInfo = this->getDynStreamInstInfo(dynInfo.seqNum);
  auto &regionInfo = this->curStreamRegionInfo;
  std::string stageName = regionInfo->getStageName();
                                        
  if (instInfo.mustBeMisspeculated) {
    DYN_INST_DPRINTF("[execute] MustMisspeculated %s.\n", stageName);
    return;
  }

  DYN_INST_DPRINTF("[execute] %s Dispatched %d Executed %d.\n",
                   stageName,
                   regionInfo->numDispatchedInsts,
                   regionInfo->numExecutedInsts + 1);
  instInfo.executed = true;

  DYN_INST_DPRINTF("[execute] %s : ", stageName);
  for (auto content : regionInfo->getInputVec()) {
    DYN_INST_DPRINTF("%d ", content);
  }
  DYN_INST_DPRINTF("\n");

  this->increamentStreamRegionInfoNumExecutedInsts(*regionInfo);
}

bool ISAPSPFrontend::canCommitStreamReady(const GemForgeDynInstInfo &dynInfo) {
  return true;
}

void ISAPSPFrontend::commitStreamReady(const GemForgeDynInstInfo &dynInfo) {
  auto &instInfo = this->getDynStreamInstInfo(dynInfo.seqNum);
  auto &regionInfo = this->curStreamRegionInfo;
  std::string stageName = regionInfo->getStageName();

  if (instInfo.mustBeMisspeculated) {
    panic("[commit] MustMisspeculated %s.\n", stageName);
  }

  auto psp = this->getPSPFrontend();
  if (regionInfo->isConfigStage()) {
    ::PSPFrontend::StreamConfigArgs args(dynInfo.seqNum, regionInfo->getInputVec());
    psp->commitStreamConfig(args);
  } else {
    ::PSPFrontend::StreamInputArgs args(dynInfo.seqNum, regionInfo->getInputVec());
    psp->commitStreamInput(args);
  }

  DYN_INST_DPRINTF("[commit] %s.\n", stageName);

  // Release the InstInfo.
  this->seqNumToDynInfoMap.erase(dynInfo.seqNum);
  regionInfo->incrementStage();
}

void ISAPSPFrontend::rewindStreamReady(const GemForgeDynInstInfo &dynInfo) {
  auto &instInfo = this->getDynStreamInstInfo(dynInfo.seqNum);
  auto &regionInfo = this->curStreamRegionInfo;
  std::string stageName = regionInfo->getStageName();

  if (instInfo.mustBeMisspeculated) {
    DYN_INST_DPRINTF("[rewind] MustMisspeculated %s.\n", stageName);

    // Restore the currentStreamRegion.
    this->curStreamRegionInfo = instInfo.dynStreamRegionInfo;
    // Release the InstInfo.
    this->seqNumToDynInfoMap.erase(dynInfo.seqNum);
    return;
  }

  assert((regionInfo->streamConfigReadyDispatched || regionInfo->streamInputReadyDispatched) && "StreamReady must be dispatched.");

  // Check if the StreamReady is actually executed.
  if (instInfo.executed) {
    regionInfo->numExecutedInsts--;
  }

  auto psp = this->getPSPFrontend();
  if (regionInfo->isConfigStage()) {
    ::PSPFrontend::StreamConfigArgs args(dynInfo.seqNum, regionInfo->getInputVec(), dynInfo.tc);
    psp->rewindStreamConfig(args);
    regionInfo->streamConfigReadyDispatched = false;
  } else {
    ::PSPFrontend::StreamInputArgs args(dynInfo.seqNum, regionInfo->getInputVec(), dynInfo.tc);
    psp->rewindStreamInput(args);
    regionInfo->streamInputReadyDispatched = false;
  }

  // Decrease numDispatchedInst.
  regionInfo->numDispatchedInsts--;

  DYN_INST_DPRINTF("[rewind] %s Dispatched %d Executed %d %s.\n",
                   stageName, regionInfo->numDispatchedInsts, regionInfo->numExecutedInsts);

  // Restore the currentStreamRegion.
  this->curStreamRegionInfo = instInfo.dynStreamRegionInfo;
  // Release the InstInfo.
  this->seqNumToDynInfoMap.erase(dynInfo.seqNum);
  regionInfo->decrementStage();
}

/********************************************************************************
 * StreamTerminate Handlers.
 *******************************************************************************/

bool ISAPSPFrontend::canDispatchStreamTerminate(const GemForgeDynInstInfo &dynInfo) {
  auto streamId = this->extractImm<uint64_t>(dynInfo.staticInst);

  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum);
  auto psp = this->getPSPFrontend();
  if (psp->canDispatchStreamTerminate(args)) {
    DYN_INST_DPRINTF("[CanDispatch] StreamTerminate %llu, ..\n", streamId);
    return true;
  } else {
    DYN_INST_DPRINTF("[Can X Dispatch] StreamTerminate %llu: No UnsteppedElement.\n", streamId);
    return false;
  }
}

void ISAPSPFrontend::dispatchStreamTerminate(
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {
  auto streamId = this->extractImm<uint64_t>(dynInfo.staticInst);

  DYN_INST_DPRINTF("[dispatch] StreamTerminate %llu.\n", streamId);

  this->createDynStreamInstInfo(dynInfo.seqNum);

  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum);
  auto psp = this->getPSPFrontend();
  assert(psp->canDispatchStreamTerminate(args) && "CanNot Dispatch StreamTerminate.");

  psp->dispatchStreamTerminate(args);
}

bool ISAPSPFrontend::canExecuteStreamTerminate(const GemForgeDynInstInfo &dynInfo) {
  auto &instInfo = this->getDynStreamInstInfo(dynInfo.seqNum);
  if (instInfo.mustBeMisspeculated) {
    return true;
  }
  auto streamId = this->extractImm<uint64_t>(dynInfo.staticInst);

  DYN_INST_DPRINTF("[canExecute] StreamTerminate %llu.\n", streamId);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum);
  return psp->canExecuteStreamTerminate(args);
}

void ISAPSPFrontend::executeStreamTerminate(const GemForgeDynInstInfo &dynInfo,
                                       ExecContext &xc) {
  auto &instInfo = this->getDynStreamInstInfo(dynInfo.seqNum);

  if (instInfo.mustBeMisspeculated) {
    DYN_INST_DPRINTF("[execute] MustMisspeculated StreamTerminate.\n");
    return;
  }

  auto streamId = this->extractImm<uint64_t>(dynInfo.staticInst);
  DYN_INST_DPRINTF("[execute] StreamTerminate %llu.\n", streamId);
  instInfo.executed = true;

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum);
  psp->executeStreamTerminate(args);  
}

bool ISAPSPFrontend::canCommitStreamTerminate(const GemForgeDynInstInfo &dynInfo) {
  auto &instInfo = this->getDynStreamInstInfo(dynInfo.seqNum);
  if (instInfo.mustBeMisspeculated) {
    return true;
  }

  auto streamId = this->extractImm<uint64_t>(dynInfo.staticInst);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum);
  auto canCommit = psp->canCommitStreamTerminate(args);
  DYN_INST_DPRINTF("[canCommit] StreamTerminate %llu, CanCommit? %d.\n", streamId, canCommit);

  // Release the info.
  return canCommit;
}

void ISAPSPFrontend::commitStreamTerminate(const GemForgeDynInstInfo &dynInfo) {
  auto &instInfo = this->getDynStreamInstInfo(dynInfo.seqNum);
  assert(!instInfo.mustBeMisspeculated &&
         "Try to commit a MustBeMisspeculated inst.");

  auto streamId = this->extractImm<uint64_t>(dynInfo.staticInst);

  DYN_INST_DPRINTF("[commit] StreamTerminate %llu.\n", streamId);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum);
  psp->commitStreamTerminate(args);

  // Release the info.
  this->seqNumToDynInfoMap.erase(dynInfo.seqNum);
  instInfo.dynStreamRegionInfo->incrementStage();
}

void ISAPSPFrontend::rewindStreamTerminate(const GemForgeDynInstInfo &dynInfo) {
  auto &instInfo = this->getDynStreamInstInfo(dynInfo.seqNum);
  if (!instInfo.mustBeMisspeculated) {
    // Really rewind the StreamTerminate.
    auto streamId = this->extractImm<uint64_t>(dynInfo.staticInst);

    auto psp = this->getPSPFrontend();
    ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum);
    psp->rewindStreamTerminate(args);
  }

  // Release the info.
  this->seqNumToDynInfoMap.erase(dynInfo.seqNum);
  instInfo.dynStreamRegionInfo->decrementStage();
}

/********************************************************************************
 * APIs related to misspeculation handling.
 *******************************************************************************/
/*
void ISAPSPFrontend::storeTo(InstSeqNum seqNum, Addr vaddr, int size) {
  auto psp = this->getPSPFrontend();
  if (psp) {
    psp->cpuStoreTo(seqNum, vaddr, size);
  }
}
*/

/********************************************************************************
 * StreamEngine Helpers.
 *******************************************************************************/

::PSPFrontend *ISAPSPFrontend::getPSPFrontend() {
  if (!this->PSP_FE_Memorized) {
    this->PSP_FE =
        this->cpuDelegator->baseCPU->getAccelManager()->getPSPFrontend();
    this->PSP_FE_Memorized = true;
  }
  return this->PSP_FE;
}

template <typename T>
T ISAPSPFrontend::extractImm(const StaticInst *staticInst) const {
#if THE_ISA == RISCV_ISA
  auto immOp = dynamic_cast<const RiscvISA::ImmOp<T> *>(staticInst);
  assert(immOp && "Invalid ImmOp.");
  return immOp->getImm();
#elif THE_ISA == X86_ISA
  auto machineInst = staticInst->machInst;
  return machineInst.immediate;
#else
  panic("ISA stream engine is not supported.");
#endif
}

ISAPSPFrontend::DynStreamInstInfo &
ISAPSPFrontend::createDynStreamInstInfo(uint64_t seqNum) {
  auto emplaceRet = this->seqNumToDynInfoMap.emplace(
      std::piecewise_construct, std::forward_as_tuple(seqNum),
      std::forward_as_tuple());
  assert(emplaceRet.second && "StreamInstInfo already there.");
  return emplaceRet.first->second;
}

ISAPSPFrontend::DynStreamInstInfo &
ISAPSPFrontend::getOrCreateDynStreamInstInfo(uint64_t seqNum) {
  auto emplaceRet = this->seqNumToDynInfoMap.emplace(
      std::piecewise_construct, std::forward_as_tuple(seqNum),
      std::forward_as_tuple());
  return emplaceRet.first->second;
}

ISAPSPFrontend::DynStreamInstInfo &
ISAPSPFrontend::getDynStreamInstInfo(uint64_t seqNum) {
  auto iter = this->seqNumToDynInfoMap.find(seqNum);
  if (iter == this->seqNumToDynInfoMap.end()) {
    inform("Failed to get DynStreamInstInfo for %llu.", seqNum);
    assert(false && "Failed to get DynStreamInstInfo.");
  }
  return iter->second;
}

void ISAPSPFrontend::increamentStreamRegionInfoNumExecutedInsts(DynStreamRegionInfo &dynStreamRegionInfo) {
  dynStreamRegionInfo.numExecutedInsts++;

  if (dynStreamRegionInfo.numExecutedInsts == dynStreamRegionInfo.numDispatchedInsts) {
    if (dynStreamRegionInfo.streamConfigReadyDispatched) {
      ::PSPFrontend::StreamConfigArgs args(dynStreamRegionInfo.streamReadySeqNum,
                                            dynStreamRegionInfo.getInputVec());
      auto psp = this->getPSPFrontend();
      psp->executeStreamConfig(args);
    } else if (dynStreamRegionInfo.streamInputReadyDispatched) {
      ::PSPFrontend::StreamInputArgs args(dynStreamRegionInfo.streamReadySeqNum,
                                            dynStreamRegionInfo.getInputVec());
      auto psp = this->getPSPFrontend();
      psp->executeStreamInput(args);
    }
  }
}

std::vector<RegVal>&
ISAPSPFrontend::DynStreamRegionInfo::getInputVec() {
  return this->inputMap;
}

std::string&
ISAPSPFrontend::DynStreamRegionInfo::getStageName() {
  std::string stageName;
  switch(this->stage) {
  case 0 :
    stageName = "StreamConfigIndexBase";
    break;
  case 1 :
    stageName = "StreamConfigIndexGranularity";
    break;
  case 2 :
    stageName = "StreamConfigValueBase";
    break;
  case 3 :
    stageName = "StreamConfigValueGranularity";
    break;
  case 4 :
    stageName = "StageConfigReady";
    break;
  case 5 : 
    stageName = "StreamInputBegin";
    break;
  case 6 : 
    stageName = "StreamInputEnd";
    break;
  case 7 :
    stageName = "StreamInput";
    break;
  case 8 : 
    stageName = "StreamTerminate";
    break;
  default : 
    stageName = "StreamStageNameError";
  }
}

void ISAPSPFrontend::DynStreamRegionInfo::incrementStage() {
  this->stage = (this->stage + 1) % 9;
}

void ISAPSPFrontend::DynStreamRegionInfo::decrementStage() {
  this->stage = (this->stage - 1) % 9;
}

bool ISAPSPFrontend::DynStreamRegionInfo::isConfigStage() {
  return this->stage <= 4;
}

std::string
ISAPSPFrontend::mustBeMisspeculatedString(MustBeMisspeculatedReason reason) {
#define CASE_REASON(reason)                                                    \
  case reason:                                                                 \
    return #reason
  switch (reason) {
    CASE_REASON(CONFIG_HAS_PREV_REGION);
    CASE_REASON(CONFIG_RECURSIVE);
    CASE_REASON(CONFIG_CANNOT_SET_REGION_ID);
    CASE_REASON(STEP_INVALID_REGION_ID);
    CASE_REASON(USER_INVALID_REGION_ID);
    CASE_REASON(USER_USING_LAST_ELEMENT);
    CASE_REASON(USER_SE_CANNOT_DISPATCH);
  default:
    return "UNKNOWN_REASON";
  }
#undef CASE_REASON
}

void ISAPSPFrontend::takeOverBy(GemForgeCPUDelegator *newDelegator) {
  this->cpuDelegator = newDelegator;
  // Clear memorized StreamEngine, even though by our design this should not
  // change.
  this->PSP_FE = nullptr;
  this->PSP_FE_Memorized = false;
}

void ISAPSPFrontend::reset() {
  this->seqNumToDynInfoMap.clear();
  this->curStreamRegionInfo = nullptr;
}
