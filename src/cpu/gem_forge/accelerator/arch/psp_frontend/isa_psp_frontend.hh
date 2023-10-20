#ifndef __GEM_FORGE_PSP_FRONTEND_H__
#define __GEM_FORGE_PSP_FRONTEND_H__

/**
 * Editor: Sungjun Jung (miguel92@snu.ac.kr)
 * Description: An interface between the psp instructions in a ISA
 * and the programmable stream prefetcher.
 */

#include "cpu/gem_forge/gem_forge_dyn_inst_info.hh"
#include "cpu/gem_forge/gem_forge_lsq_callback.hh"

#include "config/have_protobuf.hh"
#ifndef HAVE_PROTOBUF
#error "Require protobuf to parse stream info."
#endif

#include "cpu/gem_forge/accelerator/arch/exec_func.hh"
#include "cpu/gem_forge/accelerator/stream/StreamMessage.pb.h"

#include <array>
#include <unordered_map>

class GemForgeCPUDelegator;
class PSPFrontend;

class ISAPSPFrontend {
public:
  ISAPSPFrontend(GemForgeCPUDelegator *_cpuDelegator)
      : cpuDelegator(_cpuDelegator) {}

  void takeOverBy(GemForgeCPUDelegator *newDelegator);

  /**
   * Reset the stream state, e.g. RegionStreamIdTable.
   */
  void reset();

#define DeclareStreamInstHandler(Inst)                                         \
  bool canDispatchStream##Inst(const GemForgeDynInstInfo &dynInfo);            \
  void dispatchStream##Inst(const GemForgeDynInstInfo &dynInfo,                \
                            GemForgeLSQCallbackList &extraLSQCallbacks);       \
  bool canExecuteStream##Inst(const GemForgeDynInstInfo &dynInfo);             \
  void executeStream##Inst(const GemForgeDynInstInfo &dynInfo,                 \
                           ExecContext &xc);                                   \
  bool canCommitStream##Inst(const GemForgeDynInstInfo &dynInfo);              \
  void commitStream##Inst(const GemForgeDynInstInfo &dynInfo);                 \
  void rewindStream##Inst(const GemForgeDynInstInfo &dynInfo);

  DeclareStreamInstHandler(Config);
  DeclareStreamInstHandler(Ready);
  DeclareStreamInstHandler(Terminate);
#undef DeclareStreamInstHandler

  void storeTo(InstSeqNum seqNum, Addr vaddr, int size);

private:
  ::GemForgeCPUDelegator *cpuDelegator;

  ::PSPFrontend* PSP_FE = nullptr;
  bool PSP_FE_Memorized = false;
  ::PSPFrontend* getPSPFrontend();

  template <typename T> T extractImm(const StaticInst *staticInst) const;

  /**
   * Remembers the mustBeMisspeculatedReason.
   */
  enum MustBeMisspeculatedReason {
    CONFIG_HAS_PREV_REGION = 0,
    CONFIG_RECURSIVE,
    CONFIG_CANNOT_SET_REGION_ID,
    STEP_INVALID_REGION_ID,
    USER_INVALID_REGION_ID,
    USER_USING_LAST_ELEMENT,
    USER_SE_CANNOT_DISPATCH,
  };

  static std::string mustBeMisspeculatedString(MustBeMisspeculatedReason reason);

  /**
   * StreamEngine is configured through a sequence of instructions:
   * stream.cfg.idx.base
   * stream.cfg.idx.gran
   * stream.cfg.val.base
   * stream.cfg.val.gran
   * stream.cfg.ready
   * stream.input.offset.begin
   * stream.input.offset.end
   * stream.input.ready
   * We hide this detail from the StreamEngine. 
   * When dispatched, all these instructions will be marked with the current DynStreamRegionInfo.
   * 
   * 1. When stream.cfg.ready dispatches, we call StreamEngine::dispatchStreamConfig.
   * 2. When all the instructions are executed, we call StreamEngine::executeStreamConfig.
   * 3. When ssp.stream.ready commits, we call StreamEngine::commitStreamConfig.
   */
  struct DynStreamRegionInfo {
    bool streamConfigReadyDispatched = false;
    bool streamInputReadyDispatched = false;
    uint64_t streamReadySeqNum = 0;
    int numDispatchedInsts = 0;
    int numExecutedInsts = 0;
    bool mustBeMisspeculated = false;
    int stage = 0;
    std::vector<RegVal> inputMap;

    // Mainly used for misspeculation recover.
    std::shared_ptr<DynStreamRegionInfo> prevRegion = nullptr;
    DynStreamRegionInfo(std::shared_ptr<DynStreamRegionInfo> _prevRegion) : prevRegion(_prevRegion) {}

    std::vector<RegVal> &getInputVec();
    std::string &getStageName();
    bool isConfigStage();
    void incrementStage();
    void decrementStage();
  };

  /**
   * Store the current stream region info being used at dispatch stage.
   * We need a shared_ptr as it will be stored in DynStreamInstInfo and used
   * later in execution stage, etc.
   */
  std::shared_ptr<DynStreamRegionInfo> curStreamRegionInfo;

  struct DynStreamInstInfo {
    /**
     * Maybe we can use a union to save the storage, but union is
     * painful to use when the member is not POD and I don't care.
     */
    std::shared_ptr<DynStreamRegionInfo> dynStreamRegionInfo;

    /**
     * Sometimes it is for sure this instruction is misspeculated.
     */
    bool mustBeMisspeculated = false;
    /**
     * Whether this instruction has been executed.
     * Only valid if mustBeMisspeculated is false.
     */
    bool executed = false;
    MustBeMisspeculatedReason mustBeMisspeculatedReason;
  };
  std::unordered_map<uint64_t, DynStreamInstInfo> seqNumToDynInfoMap;

  DynStreamInstInfo &createDynStreamInstInfo(uint64_t seqNum);
  DynStreamInstInfo &getOrCreateDynStreamInstInfo(uint64_t seqNum);
  DynStreamInstInfo &getDynStreamInstInfo(uint64_t seqNum);

  /**
   * Mark one stream config inst executed.
   * If all executed, will call StreamEngine::executeStreamConfig.
   */
  void increamentStreamRegionInfoNumExecutedInsts(
      DynStreamRegionInfo &dynStreamRegionInfo);
};

#endif