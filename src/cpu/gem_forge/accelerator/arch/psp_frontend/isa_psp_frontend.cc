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
// dispatch in-order, execute not
// canExecuteStreamReady fails if all configurations are not executed
// order of config or input does not matter
// termiante ?

/********************************************************************************
 * StreamConfig/Input Handlers : canDispatch
 *******************************************************************************/
bool ISAPSPFrontend::canDispatchStreamConfigIndexBase(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canDispatchStreamConfigIndexBase\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto iter = this->streamNumToStreamRegionMap.find(streamNum);
  if (iter == this->streamNumToStreamRegionMap.end()) {
    return true;
  }
  return false;
}

bool ISAPSPFrontend::canDispatchStreamConfigIndexGranularity(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canDispatchStreamConfigIndexGranularity\n");
  return true;
}

bool ISAPSPFrontend::canDispatchStreamConfigValueBase(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canDispatchStreamConfigValueBase\n");
  return true;
}

bool ISAPSPFrontend::canDispatchStreamConfigValueGranularity(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canDispatchStreamConfigValueGranularity\n");
  return true;
}

bool ISAPSPFrontend::canDispatchStreamInputBegin(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canDispatchStreamInputBegin\n");
  return true;
}

bool ISAPSPFrontend::canDispatchStreamInputEnd(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canDispatchStreamInputEnd\n");
  return true;
}

/********************************************************************************
 * StreamConfig/Input Handlers : dispatch
 ********************************************************************************/
void ISAPSPFrontend::dispatchStreamConfigIndexBase(
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {
  ISA_PSP_FE_DPRINTF("dispatchStreamConfigIndexBase\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->createStreamRegionInfo(streamNum);
  
  regionInfo.configInfo.dispatched[0] = 1;

  DYN_INST_DPRINTF("[dispatch] StreamConfigIndexBase %llu\n", streamNum);
}

void ISAPSPFrontend::dispatchStreamConfigIndexGranularity(    
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {
  ISA_PSP_FE_DPRINTF("dispatchStreamConfigIndexGranularity\n");  
  
  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);
  
  regionInfo.configInfo.dispatched[1] = 1;

  DYN_INST_DPRINTF("[dispatch] StreamConfigIndexGranularity %llu\n", streamNum);
}

void ISAPSPFrontend::dispatchStreamConfigValueBase(    
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {
  ISA_PSP_FE_DPRINTF("dispatchStreamConfigValueBase\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);
  
  regionInfo.configInfo.dispatched[2] = 1;

  DYN_INST_DPRINTF("[dispatch] StreamConfigValueBase %llu\n", streamNum);
}

void ISAPSPFrontend::dispatchStreamConfigValueGranularity(    
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {
  ISA_PSP_FE_DPRINTF("dispatchStreamConfigValueGranularity\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);
  
  regionInfo.configInfo.dispatched[3] = 1;

  DYN_INST_DPRINTF("[dispatch] StreamConfigValueGranularity %llu\n", streamNum);
}

void ISAPSPFrontend::dispatchStreamInputBegin(    
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {
  ISA_PSP_FE_DPRINTF("dispatchStreamInputBegin\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);
  
  regionInfo.inputInfo.dispatched[0] = 1;

  DYN_INST_DPRINTF("[dispatch] StreamInputBegin %llu\n", streamNum);
}

void ISAPSPFrontend::dispatchStreamInputEnd(    
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {
  ISA_PSP_FE_DPRINTF("dispatchStreamInputEnd\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  regionInfo.inputInfo.dispatched[1] = 1;

  DYN_INST_DPRINTF("[dispatch] StreamInputEnd %llu\n", streamNum);
}

/********************************************************************************
 * StreamConfig/Input Handlers : canExecute
 *******************************************************************************/
bool ISAPSPFrontend::canExecuteStreamConfigIndexBase(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canExecuteStreamConfigIndexBase\n");
  return true;
}

bool ISAPSPFrontend::canExecuteStreamConfigIndexGranularity(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canExecuteStreamConfigIndexGranularity\n");
  return true;
}

bool ISAPSPFrontend::canExecuteStreamConfigValueBase(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canExecuteStreamConfigValueBase\n");
  return true;
}

bool ISAPSPFrontend::canExecuteStreamConfigValueGranularity(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canExecuteStreamConfigValueGranularity\n");
  return true;
}

bool ISAPSPFrontend::canExecuteStreamInputBegin(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canExecuteStreamInputBegin\n");
  return true;
}

bool ISAPSPFrontend::canExecuteStreamInputEnd(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canExecuteStreamInputEnd\n");
  return true;
}

/********************************************************************************
 * StreamConfig/Input Handlers : execute
 *******************************************************************************/
void ISAPSPFrontend::executeStreamConfigIndexBase(const GemForgeDynInstInfo &dynInfo, ExecContext &xc) {
  ISA_PSP_FE_DPRINTF("executeStreamConfigIndexBase\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);
  auto &configInfo = regionInfo.configInfo;

  const auto &regId = dynInfo.staticInst->srcRegIdx(0);
  assert(regId.isIntReg());
  RegVal regValue = xc.readIntRegOperand(dynInfo.staticInst, 0);
  configInfo.inputContents[0] = regValue;
  configInfo.executed[0] = 1;

  DYN_INST_DPRINTF("[execute] executeStreamConfigIndexBase, StreamNum %llu\n", streamNum);
}

void ISAPSPFrontend::executeStreamConfigIndexGranularity(const GemForgeDynInstInfo &dynInfo, ExecContext &xc) {
  ISA_PSP_FE_DPRINTF("executeStreamConfigIndexGranularity\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);
  auto &configInfo = regionInfo.configInfo;

  const auto &regId = dynInfo.staticInst->srcRegIdx(0);
  assert(regId.isIntReg());
  RegVal regValue = xc.readIntRegOperand(dynInfo.staticInst, 0);
  configInfo.inputContents[1] = regValue;
  configInfo.executed[1] = 1;

  DYN_INST_DPRINTF("[execute] executeStreamConfigIndexGranularity, StreamNum %llu\n", streamNum);
}

void ISAPSPFrontend::executeStreamConfigValueBase(const GemForgeDynInstInfo &dynInfo, ExecContext &xc) {
  ISA_PSP_FE_DPRINTF("executeStreamConfigValueBase\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);
  auto &configInfo = regionInfo.configInfo;

  const auto &regId = dynInfo.staticInst->srcRegIdx(0);
  assert(regId.isIntReg());
  RegVal regValue = xc.readIntRegOperand(dynInfo.staticInst, 0);
  configInfo.inputContents[2] = regValue;
  configInfo.executed[2] = 1;

  DYN_INST_DPRINTF("[execute] executeStreamConfigValueBase, StreamNum %llu\n", streamNum);
}

void ISAPSPFrontend::executeStreamConfigValueGranularity(const GemForgeDynInstInfo &dynInfo, ExecContext &xc) {
  ISA_PSP_FE_DPRINTF("executeStreamConfigValueGranularity\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);
  auto &configInfo = regionInfo.configInfo;

  const auto &regId = dynInfo.staticInst->srcRegIdx(0);
  assert(regId.isIntReg());
  RegVal regValue = xc.readIntRegOperand(dynInfo.staticInst, 0);
  configInfo.inputContents[3] = regValue;
  configInfo.executed[3] = 1;

  DYN_INST_DPRINTF("[execute] executeStreamConfigValueGranularity, StreamNum %llu\n", streamNum);
}

void ISAPSPFrontend::executeStreamInputBegin(const GemForgeDynInstInfo &dynInfo, ExecContext &xc) {
  ISA_PSP_FE_DPRINTF("executeStreamInputBegin\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);
  auto &inputInfo = regionInfo.inputInfo;

  const auto &regId = dynInfo.staticInst->srcRegIdx(0);
  assert(regId.isIntReg());
  RegVal regValue = xc.readIntRegOperand(dynInfo.staticInst, 0);
  inputInfo.inputContents[0] = regValue;
  inputInfo.executed[0] = 1;

  DYN_INST_DPRINTF("[execute] executeStreamInputBegin, StreamNum %llu\n", streamNum);
}

void ISAPSPFrontend::executeStreamInputEnd(const GemForgeDynInstInfo &dynInfo, ExecContext &xc) {
  ISA_PSP_FE_DPRINTF("executeStreamInputEnd\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);
  auto &inputInfo = regionInfo.inputInfo;

  const auto &regId = dynInfo.staticInst->srcRegIdx(0);
  assert(regId.isIntReg());
  RegVal regValue = xc.readIntRegOperand(dynInfo.staticInst, 0);
  inputInfo.inputContents[1] = regValue;
  inputInfo.executed[1] = 1;

  DYN_INST_DPRINTF("[execute] executeStreamInputEnd, StreamNum %llu\n", streamNum);
}

/********************************************************************************
 * StreamConfig/Input Handlers : canCommit
 *******************************************************************************/

bool ISAPSPFrontend::canCommitStreamConfigIndexBase(const GemForgeDynInstInfo &dynInfo) {
  return true;
}

bool ISAPSPFrontend::canCommitStreamConfigIndexGranularity(const GemForgeDynInstInfo &dynInfo) {
  return true;
}

bool ISAPSPFrontend::canCommitStreamConfigValueBase(const GemForgeDynInstInfo &dynInfo) {
  return true;
}

bool ISAPSPFrontend::canCommitStreamConfigValueGranularity(const GemForgeDynInstInfo &dynInfo) {
  return true;
}

bool ISAPSPFrontend::canCommitStreamInputBegin(const GemForgeDynInstInfo &dynInfo) {
  return true;
}

bool ISAPSPFrontend::canCommitStreamInputEnd(const GemForgeDynInstInfo &dynInfo) {
  return true;
}

/********************************************************************************
 * StreamConfig/Input Handlers : commit
 *******************************************************************************/

void ISAPSPFrontend::commitStreamConfigIndexBase(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("commitStreamConfigIndexBase\n");
}

void ISAPSPFrontend::commitStreamConfigIndexGranularity(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("commitStreamConfigIndexGranularity\n");
}

void ISAPSPFrontend::commitStreamConfigValueBase(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("commitStreamConfigValueBase\n");
}

void ISAPSPFrontend::commitStreamConfigValueGranularity(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("commitStreamConfigValueGranularity\n");
}

void ISAPSPFrontend::commitStreamInputBegin(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("commitStreamInputBegin\n");
}

void ISAPSPFrontend::commitStreamInputEnd(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("commitStreamInputEnd\n");
}

/********************************************************************************
 * StreamConfig/Input Handlers : rewind
 *******************************************************************************/

void ISAPSPFrontend::rewindStreamConfigIndexBase(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("rewindStreamConfigIndexBase\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  regionInfo.configInfo.dispatched[0] = 0;
  regionInfo.configInfo.executed[0] = 0;

  this->removeStreamRegionInfo(streamNum);
}

void ISAPSPFrontend::rewindStreamConfigIndexGranularity(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("rewindStreamConfigIndexGranularity\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  regionInfo.configInfo.dispatched[1] = 0;
  regionInfo.configInfo.executed[1] = 0;
}

void ISAPSPFrontend::rewindStreamConfigValueBase(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("rewindStreamConfigValueBase\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  regionInfo.configInfo.dispatched[2] = 0;
  regionInfo.configInfo.executed[2] = 0;
}

void ISAPSPFrontend::rewindStreamConfigValueGranularity(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("rewindStreamConfigValueGranularity\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  regionInfo.configInfo.dispatched[3] = 0;
  regionInfo.configInfo.executed[3] = 0;
}

void ISAPSPFrontend::rewindStreamInputBegin(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("rewindStreamInputBegin\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  regionInfo.inputInfo.dispatched[0] = 0;
  regionInfo.inputInfo.executed[0] = 0;
}

void ISAPSPFrontend::rewindStreamInputEnd(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("rewindStreamInputEnd\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  regionInfo.inputInfo.dispatched[1] = 0;
  regionInfo.inputInfo.executed[1] = 0;
}

/********************************************************************************
 * Stream[Config/Input]Ready Handlers : canDispatch
 *******************************************************************************/

bool ISAPSPFrontend::canDispatchStreamConfigReady(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canDispatchStreamConfigReady\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamConfigArgs args(dynInfo.seqNum, streamNum, regionInfo.configInfo.inputContents);
  ISA_PSP_FE_DPRINTF("args addr: %x\n", &args);
  bool canReady = psp->canDispatchStreamConfig(args);

  if (canReady) {
    DYN_INST_DPRINTF("[canDispatch] canDispatchStreamConfigReady\n");
    return true;
  } else {
    DYN_INST_DPRINTF("[canDispatch FAIL] canDispatchStreamConfigReady\n");
    return false;
  }
}

bool ISAPSPFrontend::canDispatchStreamInputReady(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canDispatchStreamInputReady\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamInputArgs args(dynInfo.seqNum, streamNum, regionInfo.inputInfo.inputContents);
  bool canReady = psp->canDispatchStreamInput(args);

  if (canReady) {
    DYN_INST_DPRINTF("[canDispatch] canDispatchStreamInputReady\n");
    return true;
  } else {
    DYN_INST_DPRINTF("[canDispatch FAIL] canDispatchStreamInputReady\n");
    return false;
  }
}

/********************************************************************************
 * Stream[Config/Input]Ready Handlers : dispatch
 *******************************************************************************/

void ISAPSPFrontend::dispatchStreamConfigReady(
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {
  ISA_PSP_FE_DPRINTF("dispatchStreamConfigReady\n");

  DYN_INST_DPRINTF("[dispatch] dispatchStreamConfigReady\n");
}

void ISAPSPFrontend::dispatchStreamInputReady(
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {
  ISA_PSP_FE_DPRINTF("dispatchStreamInputReady\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);
  
  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamInputArgs args(dynInfo.seqNum, streamNum, regionInfo.inputInfo.inputContents);
  psp->dispatchStreamInput(args);

  DYN_INST_DPRINTF("[dispatch] dispatchStreamInputReady\n");
}

/********************************************************************************
 * Stream[Config/Input]Ready Handlers : canExecute
 *******************************************************************************/

bool ISAPSPFrontend::canExecuteStreamConfigReady(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canExecuteStreamConfigReady\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  auto &v = regionInfo.configInfo.executed;
  return std::find(std::begin(v), std::end(v), false) == std::end(v);
}

bool ISAPSPFrontend::canExecuteStreamInputReady(
    const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canExecuteStreamInputReady\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  auto &v = regionInfo.inputInfo.executed;
  return std::find(std::begin(v), std::end(v), false) == std::end(v);
}

/********************************************************************************
 * Stream[Config/Input]Ready Handlers : execute
 *******************************************************************************/

void ISAPSPFrontend::executeStreamConfigReady(const GemForgeDynInstInfo &dynInfo,
                                         ExecContext &xc) {
  ISA_PSP_FE_DPRINTF("executeStreamConfigReady\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

   auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamConfigArgs args(dynInfo.seqNum, streamNum, regionInfo.configInfo.inputContents);
  psp->executeStreamConfig(args);

  DYN_INST_DPRINTF("[execute] executeStreamConfigReady %x %lu %x %lu\n", \
    regionInfo.configInfo.inputContents[0], regionInfo.configInfo.inputContents[1], regionInfo.configInfo.inputContents[2], regionInfo.configInfo.inputContents[3]);
}

void ISAPSPFrontend::executeStreamInputReady(const GemForgeDynInstInfo &dynInfo,
                                         ExecContext &xc) {
  ISA_PSP_FE_DPRINTF("executeStreamInputReady\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

   auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamInputArgs args(dynInfo.seqNum, streamNum, regionInfo.inputInfo.inputContents);
  psp->executeStreamInput(args);

  DYN_INST_DPRINTF("[execute] executeStreamInputReady %d %d\n", \
    regionInfo.inputInfo.inputContents[0], regionInfo.inputInfo.inputContents[1]);
}

/********************************************************************************
 * Stream[Config/Input]Ready Handlers : canCommit
 *******************************************************************************/

bool ISAPSPFrontend::canCommitStreamConfigReady(const GemForgeDynInstInfo &dynInfo) {
  return true;
}

bool ISAPSPFrontend::canCommitStreamInputReady(const GemForgeDynInstInfo &dynInfo) {
  return true;
}

/********************************************************************************
 * Stream[Config/Input]Ready Handlers : commit
 *******************************************************************************/

void ISAPSPFrontend::commitStreamConfigReady(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("commitStreamConfigReady\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamConfigArgs args(dynInfo.seqNum, streamNum, regionInfo.configInfo.inputContents);
  psp->commitStreamConfig(args);

  DYN_INST_DPRINTF("[commit] commitStreamConfigReady\n");
}

void ISAPSPFrontend::commitStreamInputReady(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("commitStreamInputReady\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamInputArgs args(dynInfo.seqNum, streamNum, regionInfo.inputInfo.inputContents);
  psp->commitStreamInput(args);

  DYN_INST_DPRINTF("[commit] commitStreamInputReady\n");
}

/********************************************************************************
 * Stream[Config/Input]Ready Handlers : rewind
 *******************************************************************************/

void ISAPSPFrontend::rewindStreamConfigReady(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("rewindStreamConfigReady\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamConfigArgs args(dynInfo.seqNum, streamNum, regionInfo.configInfo.inputContents);
  psp->rewindStreamConfig(args);

  DYN_INST_DPRINTF("[rewind] rewindStreamConfigReady\n");
}

void ISAPSPFrontend::rewindStreamInputReady(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("rewindStreamInputReady\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);
  auto &regionInfo = this->getStreamRegionInfo(streamNum);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamInputArgs args(dynInfo.seqNum, streamNum, regionInfo.inputInfo.inputContents);
  psp->rewindStreamInput(args);

  DYN_INST_DPRINTF("[rewind] rewindStreamInputReady\n");
}

/********************************************************************************
 * StreamTerminate Handlers.
 *******************************************************************************/

bool ISAPSPFrontend::canDispatchStreamTerminate(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canDispatchStreamTerminate\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum, streamNum);
  
  if (psp->canDispatchStreamTerminate(args)) {
    DYN_INST_DPRINTF("[canDispatch] StreamTerminate %llu\n", streamNum);
    return true;
  } else {
    DYN_INST_DPRINTF("[canDispatch FAIL] StreamTerminate %llu\n", streamNum);
    return false;
  }
}

void ISAPSPFrontend::dispatchStreamTerminate(
    const GemForgeDynInstInfo &dynInfo,
    GemForgeLSQCallbackList &extraLSQCallbacks) {
  ISA_PSP_FE_DPRINTF("dispatchStreamTerminate\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum, streamNum);
  psp->dispatchStreamTerminate(args);

  DYN_INST_DPRINTF("[dispatch] StreamTerminate %llu.\n", streamNum);
}

bool ISAPSPFrontend::canExecuteStreamTerminate(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canExecuteStreamTerminate\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum, streamNum);
  
  if (psp->canExecuteStreamTerminate(args)) {
    DYN_INST_DPRINTF("[canExecute] StreamTerminate %llu.\n", streamNum);
    return true;
  } else {
    DYN_INST_DPRINTF("[canExecute FAIL] StreamTerminate %llu.\n", streamNum);
    return false;
  }
}

void ISAPSPFrontend::executeStreamTerminate(const GemForgeDynInstInfo &dynInfo,
                                       ExecContext &xc) {
  ISA_PSP_FE_DPRINTF("executeStreamTerminate\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum, streamNum);
  psp->executeStreamTerminate(args);  

  DYN_INST_DPRINTF("[execute] StreamTerminate %llu.\n", streamNum);
}

bool ISAPSPFrontend::canCommitStreamTerminate(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("canCommitStreamTerminate\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum, streamNum);

  if (psp->canCommitStreamTerminate(args)) {
    DYN_INST_DPRINTF("[canCommit] StreamTerminate %llu.\n", streamNum);
    return true;
  } else {
    DYN_INST_DPRINTF("[canCommit FAIL] StreamTerminate %llu.\n", streamNum);
    return false;
  }
}

void ISAPSPFrontend::commitStreamTerminate(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("commitStreamTerminate\n");

  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum, streamNum);
  psp->commitStreamTerminate(args);

  this->removeStreamRegionInfo(streamNum);

  DYN_INST_DPRINTF("[commit] StreamTerminate %llu.\n", streamNum);
}

void ISAPSPFrontend::rewindStreamTerminate(const GemForgeDynInstInfo &dynInfo) {
  ISA_PSP_FE_DPRINTF("rewindStreamTerminate\n");
  
  auto streamNum = this->extractImm<uint64_t>(dynInfo.staticInst);

  auto psp = this->getPSPFrontend();
  ::PSPFrontend::StreamTerminateArgs args(dynInfo.seqNum, streamNum);
  psp->rewindStreamTerminate(args);
  
  DYN_INST_DPRINTF("[rewind] StreamTerminate %llu.\n", streamNum);
}

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

ISAPSPFrontend::StreamRegionInfo &
ISAPSPFrontend::createStreamRegionInfo(uint64_t streamNum) {
  auto emplaceRet = this->streamNumToStreamRegionMap.emplace(
      std::piecewise_construct, std::forward_as_tuple(streamNum),
      std::forward_as_tuple());
  assert(emplaceRet.second && "StreamRegionInfo already there.");
  return emplaceRet.first->second;
}

ISAPSPFrontend::StreamRegionInfo &
ISAPSPFrontend::getStreamRegionInfo(uint64_t streamNum) {
  auto iter = this->streamNumToStreamRegionMap.find(streamNum);
  if (iter == this->streamNumToStreamRegionMap.end()) {
    inform("Failed to get StreamRegionInfo for %llu.", streamNum);
    assert(false && "Failed to get StreamRegionInfo.");
  }
  return iter->second;
}

void ISAPSPFrontend::removeStreamRegionInfo(uint64_t streamNum) {
  this->streamNumToStreamRegionMap.erase(streamNum);
}

void ISAPSPFrontend::takeOverBy(GemForgeCPUDelegator *newDelegator) {
  this->cpuDelegator = newDelegator;
  // Clear memorized StreamEngine, even though by our design this should not
  // change.
  this->PSP_FE = nullptr;
  this->PSP_FE_Memorized = false;
}

void ISAPSPFrontend::reset() {
  this->streamNumToStreamRegionMap.clear();
}
