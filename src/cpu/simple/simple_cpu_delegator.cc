#include "simple_cpu_delegator.hh"

#include "exec_context.hh"
#include "minimal_data_move_machine.hh"

class SimpleCPUDelegator::Impl {
public:
  Impl(BaseSimpleCPU *_cpu, SimpleCPUDelegator *_cpuDelegator)
      : cpu(_cpu), state(StateE::BEFORE_DISPATCH), curSeqNum(1),
        minDataMachine(_cpu->name() + ".min_data", _cpu->cpuId(), false),
        minDataMachineFix(_cpu->name() + ".min_data_fix", _cpu->cpuId(), true) {
  }

  BaseSimpleCPU *cpu;
  /**
   * Sanity check state.
   */
  enum StateE {
    BEFORE_DISPATCH,
    BEFORE_EXECUTE,
    BEFORE_COMMIT,
  };
  StateE state;
  /**
   * For SimpleCPU, it is trivial to maintain a sequence number.
   * This starts from 1, as 0 is reserved for invalid.
   */
  uint64_t curSeqNum;

  /**
   * Some extra runtime information for the current instruction.
   */
  struct InstRunTimeInfo {
    Addr paddr = 0;
    GemForgeLSQCallbackPtr callback = nullptr;
    void clear() {
      this->paddr = 0;
      this->callback = nullptr;
    }
  };
  InstRunTimeInfo curInstRunTimeInfo;

  MinimalDataMoveMachine minDataMachine;
  MinimalDataMoveMachine minDataMachineFix;

  std::string traceExtraFolder;

  Process *getProcess() {
    assert(this->cpu->activeThreads.size() == 1 &&
           "SMT not supported in GemForge yet.");
    // Crazy oracle access chain.
    auto threadInfo =
        this->cpu->threadInfo.at(this->cpu->activeThreads.front());
    auto thread = threadInfo->thread;
    auto process = thread->getProcessPtr();
    return process;
  }
};

/**************************************************************************
 * SimpleCPUDelegator.
 *************************************************************************/

SimpleCPUDelegator::SimpleCPUDelegator(CPUTypeE _cpuType, BaseSimpleCPU *_cpu)
    : GemForgeCPUDelegator(_cpuType, _cpu), pimpl(new Impl(_cpu, this)) {}
SimpleCPUDelegator::~SimpleCPUDelegator() = default;

bool SimpleCPUDelegator::canDispatch(StaticInstPtr staticInst,
                                     ExecContext &xc) {
  assert(pimpl->state == Impl::StateE::BEFORE_DISPATCH);
  GemForgeDynInstInfo dynInfo(pimpl->curSeqNum, xc.pcState(), staticInst.get(),
                              xc.tcBase());
  return isaHandler->canDispatch(dynInfo);
}

void SimpleCPUDelegator::dispatch(StaticInstPtr staticInst, ExecContext &xc) {
  assert(pimpl->state == Impl::StateE::BEFORE_DISPATCH);
  GemForgeDynInstInfo dynInfo(pimpl->curSeqNum, xc.pcState(), staticInst.get(),
                              xc.tcBase());
  /**
   * SimpleTimingCPU never really all cause a RAW misspeculation in LSQ,
   * so we ignore any extra LSQCallbacks.
   *
   * TODO: Correctly handle the StreamLoad, which is now a MemRef inst.
   */
  GemForgeLSQCallbackList extraLSQCallbacks;
  isaHandler->dispatch(dynInfo, extraLSQCallbacks);
  if (extraLSQCallbacks.front()) {
    assert(extraLSQCallbacks.front()->getType() ==
               GemForgeLSQCallback::Type::LOAD &&
           "StreamStore is not implemented.");
    pimpl->curInstRunTimeInfo.callback = std::move(extraLSQCallbacks.front());
  }
  pimpl->state = Impl::StateE::BEFORE_EXECUTE;
}

bool SimpleCPUDelegator::canExecute(StaticInstPtr staticInst, ExecContext &xc) {
  assert(pimpl->state == Impl::StateE::BEFORE_EXECUTE);
  GemForgeDynInstInfo dynInfo(pimpl->curSeqNum, xc.pcState(), staticInst.get(),
                              xc.tcBase());
  return isaHandler->canExecute(dynInfo);
}

void SimpleCPUDelegator::execute(StaticInstPtr staticInst, ExecContext &xc) {
  assert(pimpl->state == Impl::StateE::BEFORE_EXECUTE);
  GemForgeDynInstInfo dynInfo(pimpl->curSeqNum, xc.pcState(), staticInst.get(),
                              xc.tcBase());
  isaHandler->execute(dynInfo, xc);
  pimpl->state = Impl::StateE::BEFORE_COMMIT;
}

bool SimpleCPUDelegator::canCommit(StaticInstPtr staticInst, ExecContext &xc) {
  assert(pimpl->state == Impl::StateE::BEFORE_COMMIT);
  GemForgeDynInstInfo dynInfo(pimpl->curSeqNum, xc.pcState(), staticInst.get(),
                              xc.tcBase());
  return isaHandler->canCommit(dynInfo);
}

void SimpleCPUDelegator::commit(StaticInstPtr staticInst, ExecContext &xc) {
  assert(pimpl->state == Impl::StateE::BEFORE_COMMIT);
  GemForgeDynInstInfo dynInfo(pimpl->curSeqNum, xc.pcState(), staticInst.get(),
                              xc.tcBase());

  /**
   * Notify the MinimalDataMoveMachine.
   * ! So far we are not using this to model ideal data movement.
   */
  const bool enableMinimalDataMoveMachien = false;
  if (enableMinimalDataMoveMachien) {
    const auto &pc = xc.pcState();
    bool isStream = pimpl->curInstRunTimeInfo.callback != nullptr;
    Addr paddr = pimpl->curInstRunTimeInfo.paddr;
    if (isStream) {
      auto vaddr = pimpl->curInstRunTimeInfo.callback->getAddr();
      assert(this->translateVAddrOracle(vaddr, paddr));
    }
    pimpl->minDataMachine.commit(staticInst, pc, paddr, isStream);
    pimpl->minDataMachineFix.commit(staticInst, pc, paddr, isStream);
  }

  isaHandler->commit(dynInfo);
  pimpl->state = Impl::StateE::BEFORE_DISPATCH;
  pimpl->curSeqNum++;
  pimpl->curInstRunTimeInfo.clear();
}

void SimpleCPUDelegator::storeTo(Addr vaddr, int size) {
  isaHandler->storeTo(pimpl->curSeqNum, vaddr, size);
}

void SimpleCPUDelegator::regStats() {
  pimpl->minDataMachine.regStats();
  pimpl->minDataMachineFix.regStats();
}

void SimpleCPUDelegator::recordPAddr(Addr paddr) {
  pimpl->curInstRunTimeInfo.paddr = paddr;
}

const std::string &SimpleCPUDelegator::getTraceExtraFolder() const {
  // Always assume that the binary is in the TraceExtraFolder.
  if (pimpl->traceExtraFolder.empty()) {
    auto process = pimpl->getProcess();
    const auto &executable = process->executable;
    auto sepPos = executable.rfind('/');
    if (sepPos == std::string::npos) {
      // Not found.
      pimpl->traceExtraFolder = ".";
    } else {
      pimpl->traceExtraFolder = executable.substr(0, sepPos);
    }
  }
  return pimpl->traceExtraFolder;
}

bool SimpleCPUDelegator::translateVAddrOracle(Addr vaddr, Addr &paddr) {
  auto process = pimpl->getProcess();
  auto pTable = process->pTable;
  if (!pTable->translate(vaddr, paddr)) {
    // Due to the new MemState class and lazy allocation, it's possible
    // that this page has not allocated. However, we want to simplify
    // our life as before, so try to fix it?
    if (process->fixupFault(vaddr)) {
      // Try again.
      return pTable->translate(vaddr, paddr);
    }
    return false;
  }
  return true;
}

InstSeqNum SimpleCPUDelegator::getInstSeqNum() const {
  return pimpl->curSeqNum;
}

void SimpleCPUDelegator::setInstSeqNum(InstSeqNum seqNum) {
  pimpl->curSeqNum = seqNum;
}

void SimpleCPUDelegator::recordStatsForFakeExecutedInst(
    const StaticInstPtr &inst) {
  // So far we do nothing.
}