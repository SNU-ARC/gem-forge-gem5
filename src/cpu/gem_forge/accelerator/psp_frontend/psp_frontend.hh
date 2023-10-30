/*
 * Editor: Sungjun Jung (K16DIABLO)
 * Top module of PSP Frontend
 */
#ifndef __CPU_GEM_FORGE_ACCELERATOR_PSP_FRONTEND_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_PSP_FRONTEND_HH__

#include "cpu/gem_forge/llvm_insts.hh"
#include "base/statistics.hh"

#include "cpu/gem_forge/accelerator/gem_forge_accelerator.hh"
#include "cpu/gem_forge/gem_forge_packet_handler.hh"
#include "pattern_table.hh"
#include "arbiter.hh"
#include "index_queue.hh"
#include "index_loader.hh"

#include "params/PSPFrontend.hh"

class PSPFrontend : public GemForgeAccelerator {
public:
  using Params = PSPFrontendParams;
  PSPFrontend(Params* params);
  ~PSPFrontend(); // override;

  void takeOverBy(GemForgeCPUDelegator* _cpuDelegator,
                  GemForgeAcceleratorManager* _manager) override;

  void tick() override;
  void dump() override;
  void regStats() override;
  void resetStats() override;

  PatternTable* patternTable;
  IndexQueueArray* indexQueueArray;
  PatternTableRRArbiter* patternTableArbiter;
  IndexLoadUnit* indexLoadUnit;
//  RRArbiter* indexArbiter;
//  AddrTransUnit* addrTransUnit;

  // TODO: Define PSP instructions below

  // Arguments from ISAStreamEngine
  struct StreamConfigArgs {
    uint64_t seqNum;
    uint64_t entryId;
    std::vector<RegVal>& config;
    ThreadContext *const tc;

    StreamConfigArgs(uint64_t _seqNum, uint64_t _entryId, 
                     std::vector<RegVal>& _config,
                     ThreadContext *_tc = nullptr)
        : seqNum(_seqNum), entryId(_entryId), config(_config), tc(_tc) {}
  };
  bool canDispatchStreamConfig(const StreamConfigArgs &args);
  void dispatchStreamConfig(const StreamConfigArgs &args);
  bool canExecuteStreamConfig(const StreamConfigArgs &args);
  void executeStreamConfig(const StreamConfigArgs &args);
  bool canCommitStreamConfig(const StreamConfigArgs &args);
  void commitStreamConfig(const StreamConfigArgs &args);
  void rewindStreamConfig(const StreamConfigArgs &args);

  struct StreamInputArgs {
    uint64_t seqNum;
    uint64_t entryId;
    std::vector<RegVal>& input;
    ThreadContext *const tc;

    StreamInputArgs(uint64_t _seqNum, uint64_t _entryId, 
                     std::vector<RegVal>& _input,
                     ThreadContext *_tc = nullptr)
        : seqNum(_seqNum), entryId(_entryId), input(_input), tc(_tc) {}
  };
  bool canDispatchStreamInput(const StreamInputArgs &args);
  void dispatchStreamInput(const StreamInputArgs &args);
  bool canExecuteStreamInput(const StreamInputArgs &args);
  void executeStreamInput(const StreamInputArgs &args);
  bool canCommitStreamInput(const StreamInputArgs &args);
  void commitStreamInput(const StreamInputArgs &args);
  void rewindStreamInput(const StreamInputArgs &args);

  struct StreamTerminateArgs {
    uint64_t seqNum;
    uint64_t entryId;
    StreamTerminateArgs(uint64_t _seqNum, uint64_t _entryId)
      : seqNum(_seqNum), entryId(_entryId) {}
  };
  bool canDispatchStreamTerminate(const StreamTerminateArgs &args);
  void dispatchStreamTerminate(const StreamTerminateArgs &args);
  bool canExecuteStreamTerminate(const StreamTerminateArgs &args);
  void executeStreamTerminate(const StreamTerminateArgs &args);
  bool canCommitStreamTerminate(const StreamTerminateArgs &args);
  void commitStreamTerminate(const StreamTerminateArgs &args);
  void rewindStreamTerminate(const StreamTerminateArgs &args);

  // TODO: Define Stats below
  mutable Stats::Scalar numConfigured;
private:
  unsigned totalPatternTableEntries;
};
#endif
