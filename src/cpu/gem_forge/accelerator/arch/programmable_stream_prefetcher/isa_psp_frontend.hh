#ifndef __GEM_FORGE_PSP_FRONTEND_H__
#define __GEM_FORGE_PSP_FRONTEND_H__

/**
 * Editor: Sungjun Jung (miguel92@snu.ac.kr)
 * Description: An interface between the psp instructions in a ISA
 * and the programmable stream prefetcher.
 */

#include "cpu/gem_forge/gem_forge_dyn_inst_info.hh"
#include "cpu/gem_forge/gem_forge_lsq_callback.hh"

//#include "cpu/gem_forge/accelerator/arch/exec_func.hh"
//#include "cpu/gem_forge/accelerator/stream/StreamMessage.pb.h"

#include <array>
#include <unordered_map>

class PSPFrontend;
class GemForgeCPUDelegator;

class ISAPSPFrontend {
public:
  ISAPSPFrontend(GemForgeCPUDelegator *_cpuDelegator)
      : cpuDelegator(_cpuDelegator) {}

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

  DeclareStreamInstHandler(ConfigIndexBase);
  DeclareStreamInstHandler(ConfigIndexGranularity);
  DeclareStreamInstHandler(ConfigValueBase);
  DeclareStreamInstHandler(ConfigValueGranularity);
  DeclareStreamInstHandler(InputOffsetBegin);
  DeclareStreamInstHandler(InputOffsetEnd);
  DeclareStreamInstHandler(Terminate);

#undef DeclareStreamInstHandler

  void storeTo(InstSeqNum seqNum, Addr vaddr, int size);

private:
  ::GemForgeCPUDelegator *cpuDelegator;

  ::PSPFrontend* PSP_FE = nullptr;
  ::PSPFrontend* getPSPFrontend();

  template <typename T> T extractImm(const StaticInst *staticInst) const;

  /**
   * Memorize the AllStreamRegions.
   */
  mutable std::unique_ptr<::LLVM::TDG::AllStreamRegions> allStreamRegions;
  const std::string &getRelativePath(int configIdx) const;

  /**
   * Memorize the StreamConfigureInfo.
   */
  mutable std::unordered_map<uint64_t, const ::LLVM::TDG::StreamRegion *>
      memorizedStreamRegionIdMap;
  mutable std::unordered_map<std::string, ::LLVM::TDG::StreamRegion>
      memorizedStreamRegionRelativePathMap;
  const ::LLVM::TDG::StreamRegion &getStreamRegion(uint64_t configIdx) const;
  const ::LLVM::TDG::StreamRegion &
  getStreamRegion(const std::string &relativePath) const;

  /**
   * Collect all StreamInfo in the region. This includes nest streams.
   */
  std::vector<const ::LLVM::TDG::StreamInfo *>
  collectStreamInfoInRegion(const ::LLVM::TDG::StreamRegion &region) const;

  /**
   * Since the stream engine uses the full stream id,
   * we want to translate the regional stream id to it.
   * This is performed to reduce the complexity of the stream engine.
   * Also we maintain a stack of RegionStreamIdTable to support inter-procedure
   * stream configuration/termination.
   */
  static constexpr int MaxNumRegionStreams = 128;
  struct RegionStreamIdTable {
    const uint64_t configIdx;
    std::array<uint64_t, MaxNumRegionStreams> table;
    RegionStreamIdTable(uint64_t _configIdx) : configIdx(_configIdx) {
      this->table.fill(InvalidStreamId);
    }
    uint64_t &at(int regionStreamId) { return this->table.at(regionStreamId); }
    const uint64_t &at(int regionStreamId) const {
      return this->table.at(regionStreamId);
    }
  };
  std::vector<RegionStreamIdTable> regionStreamIdTableStack;
  static constexpr uint64_t InvalidStreamId = 0;
  void insertRegionStreamIds(uint64_t configIdx,
                             const ::LLVM::TDG::StreamRegion &region);
  bool hasRecursiveRegion(uint64_t configIdx);
  bool canSetRegionStreamIds(const ::LLVM::TDG::StreamRegion &region);
  bool canRemoveRegionStreamIds(const ::LLVM::TDG::StreamRegion &region);
  void removeRegionStreamIds(uint64_t configIdx,
                             const ::LLVM::TDG::StreamRegion &region);
  uint64_t searchRegionStreamId(int regionStreamId) const;
  uint64_t lookupRegionStreamId(int regionStreamId) const;
  bool isValidRegionStreamId(int regionStreamId) const;

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

  static std::string
  mustBeMisspeculatedString(MustBeMisspeculatedReason reason);

  /**
   * StreamEngine is configured through a sequence of instructions:
   * ssp.stream.config
   * ssp.stream.input*
   * ssp.stream.ready
   * We hide this detail from the StreamEngine. When dispatched, all these
   * instructions will
   * be marked with the current DynStreamRegionInfo.
   * 1. When ssp.stream.ready dispatches, we call StreamEngine::canStreamConfig
   * and StreamEngine::dispatchStreamConfig.
   * 2. When all the instructions are executed, we inform the
   * StreamEngine::executeStreamConfig.
   * 3. When ssp.stream.ready commits, we call StreamEngine::commitStreamConfig.
   *
   * ssp.stream.config may set MustMisspeculated, which will be propagated to
   * later instructions.
   */
  struct DynStreamRegionInfo {
    using StreamInputValue = TheISA::ExecFunc::RegisterValue;
    const std::string infoRelativePath;
    bool streamReadyDispatched = false;
    uint64_t streamReadySeqNum = 0;
    int numDispatchedInsts = 0;
    int numExecutedInsts = 0;
    bool mustBeMisspeculated = false;
    std::unordered_map<uint64_t, std::vector<StreamInputValue>> inputMap;

    // Mainly used for misspeculation recover.
    std::shared_ptr<DynStreamRegionInfo> prevRegion = nullptr;
    DynStreamRegionInfo(const std::string &_infoRelativePath,
                        std::shared_ptr<DynStreamRegionInfo> _prevRegion)
        : infoRelativePath(_infoRelativePath), prevRegion(_prevRegion) {}

    std::vector<StreamInputValue> &getInputVec(uint64_t streamId);
  };

  /**
   * Store the current stream region info being used at dispatch stage.
   * We need a shared_ptr as it will be stored in DynStreamInstInfo and used
   * later in execution stage, etc.
   */
  std::shared_ptr<DynStreamRegionInfo> curStreamRegionInfo;

  /**
   * We need some extra information for each dynamic stream information.
   * ssp.stream.config
   * ssp.stream.input
   * ssp.stream.ready
   *   --> They require DynStreamRegionInfo.
   * ssp.stream.step
   *   --> Need the translated StreamId.
   */
  struct DynStreamConfigInstInfo {
    std::shared_ptr<DynStreamRegionInfo> dynStreamRegionInfo;
  };

  struct DynStreamInputInstInfo {
    uint64_t translatedStreamId = InvalidStreamId;
    int inputIdx = -1;
  };

  struct DynStreamStepInstInfo {
    uint64_t translatedStreamId = InvalidStreamId;
  };

  struct DynStreamUserInstInfo {
    static constexpr int MaxUsedStreams = 2;
    std::array<uint64_t, MaxUsedStreams> translatedUsedStreamIds;
  };

  /**
   * We also remember the translated regionStreamId for every dynamic
   * instruction.
   */
  struct DynStreamInstInfo {
    /**
     * Maybe we can use a union to save the storage, but union is
     * painful to use when the member is not POD and I don't care.
     */
    DynStreamConfigInstInfo configInfo;
    DynStreamInputInstInfo inputInfo;
    DynStreamStepInstInfo stepInfo;
    DynStreamUserInstInfo userInfo;
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
