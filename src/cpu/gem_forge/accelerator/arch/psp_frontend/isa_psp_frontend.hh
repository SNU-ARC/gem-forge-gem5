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

  DeclareStreamInstHandler(ConfigIndexBase);
  DeclareStreamInstHandler(ConfigIndexGranularity);
  DeclareStreamInstHandler(ConfigValueBase);
  DeclareStreamInstHandler(ConfigValueGranularity);
  DeclareStreamInstHandler(InputBegin);
  DeclareStreamInstHandler(InputEnd);
  DeclareStreamInstHandler(ConfigReady);
  DeclareStreamInstHandler(InputReady);
  DeclareStreamInstHandler(Terminate);
#undef DeclareStreamInstHandler

private:
  ::GemForgeCPUDelegator *cpuDelegator;

  ::PSPFrontend* PSP_FE = nullptr;
  bool PSP_FE_Memorized = false;
  ::PSPFrontend* getPSPFrontend();

  template <typename T> T extractImm(const StaticInst *staticInst) const;

  struct StreamConfig {
    std::vector<bool> dispatched = std::vector<bool>(4);
    std::vector<bool> executed = std::vector<bool>(4);
    std::vector<RegVal> inputContents = std::vector<RegVal>(4);
  };

  struct StreamInput {
    std::vector<bool> dispatched = std::vector<bool>(2);
    std::vector<bool> executed = std::vector<bool>(2);
    std::vector<RegVal> inputContents = std::vector<RegVal>(2);
  };

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
  struct StreamRegionInfo {
    StreamConfig configInfo;
    StreamInput inputInfo;
  };

  std::unordered_map<uint64_t, StreamRegionInfo> streamNumToStreamRegionMap;

  StreamRegionInfo &createStreamRegionInfo(uint64_t streamNum);
  StreamRegionInfo &getStreamRegionInfo(uint64_t streamNum);
};

#endif