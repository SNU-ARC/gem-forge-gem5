
#include "stream_float_policy.hh"

#include "mem/ruby/structures/CacheMemory.hh"
#include "sim/stream_nuca/stream_nuca_manager.hh"
#include "stream_engine.hh"

#include "debug/StreamFloatPolicy.hh"
#define DEBUG_TYPE StreamFloatPolicy
#include "stream_log.hh"

void StreamFloatPolicy::setFloatPlanManual(DynamicStream &dynS) {

  /**
   * Manually check for the stream name.
   * Default to L2 cache.
   */
  auto S = dynS.stream;
  const auto &streamName = S->getStreamName();

  auto &floatPlan = dynS.getFloatPlan();
  uint64_t firstElementIdx = 0;

  static const std::unordered_set<std::string> manualFloatToMemSet = {
      "rodinia.pathfinder.wall.ld",
      "rodinia.hotspot.power.ld",
      "rodinia.hotspot3D.power.ld",
      "gap.pr_push.atomic.out_v.ld",
  };

  if (manualFloatToMemSet.count(streamName)) {
    floatPlan.addFloatChangePoint(firstElementIdx, MachineType_Directory);
    return;
  }

  if (streamName.find("rodinia.srad_v2.") == 0) {
    /**
     * For srad_v2, we want to split them at iterations.
     */
    if (!dynS.hasTotalTripCount()) {
      DYN_S_PANIC(dynS.dynamicStreamId,
                  "Missing TotalTripCount for rodinia.srad_v2.");
    }

    auto linearAddrGen =
        std::dynamic_pointer_cast<LinearAddrGenCallback>(dynS.addrGenCallback);
    if (!linearAddrGen) {
      // They should have linear address pattern.
      DYN_S_PANIC(dynS.dynamicStreamId,
                  "Non-LinearAddrGen for rodinia.srad_v2.");
    }

    auto totalTripCount = dynS.getTotalTripCount();
    auto rowTripCount =
        linearAddrGen->getNestTripCount(dynS.addrGenFormalParams, 1);

    // Take min to handle the coalesced stream.
    auto elementSize = std::min(S->getMemElementSize(), 64);
    auto totalThreads =
        S->getCPUDelegator()->getSingleThreadContext()->getThreadGroupSize();
    auto totalArrays = 6;

    auto totalLLCBytes = this->getSharedLLCCapacity();
    auto myLLCBytes = totalLLCBytes / totalThreads;
    auto rowDataBytes = rowTripCount * elementSize * totalArrays;
    auto myLLCRows = myLLCBytes / rowDataBytes;

    auto myRows = totalTripCount / rowTripCount;
    hack("TotalTripCount %d RowTripCount %d MyRows %d MyLLCRows %d.\n",
         totalTripCount, rowTripCount, myRows, myLLCRows);

    if (myRows <= myLLCRows) {
      // I should be able to fit in LLC.
      floatPlan.addFloatChangePoint(firstElementIdx, MachineType_L2Cache);
      return;
    }

    /**
     * For now we start with LLC and then migrate to Mem.
     * And we take care of reuse across rows.
     */
    if (streamName == "rodinia.srad_v2.Jn.ld") {
      myLLCRows++;
    } else if (streamName == "rodinia.srad_v2.Js.ld") {
      myLLCRows--;
    } else if (streamName == "rodinia.srad_v2.cN.ld") {
      myLLCRows++;
    } else if (streamName == "rodinia.srad_v2.cS.ld") {
      myLLCRows--;
    }
    auto llcTripCount = myLLCRows * rowTripCount;
    floatPlan.addFloatChangePoint(firstElementIdx, MachineType_L2Cache);
    floatPlan.addFloatChangePoint(llcTripCount, MachineType_Directory);
    return;
  }

  // Default just offload to LLC.
  floatPlan.addFloatChangePoint(firstElementIdx, MachineType_L2Cache);
  return;
}

void StreamFloatPolicy::setFloatPlanManual2(DynamicStream &dynS) {

  /**
   * Manually check for the stream name.
   * Default to L2 cache.
   */
  auto S = dynS.stream;
  const auto &streamName = S->getStreamName();

  auto &floatPlan = dynS.getFloatPlan();
  uint64_t firstElementIdx = 0;

  static const std::unordered_set<std::string> manualFloatToMemSet = {
      "rodinia.pathfinder.wall.ld",
      "rodinia.hotspot3D.power.ld",
      "gap.pr_push.atomic.out_v.ld",
  };

  if (manualFloatToMemSet.count(streamName)) {
    floatPlan.addFloatChangePoint(firstElementIdx, MachineType_Directory);
    return;
  }

  /**
   * Split streams at iterations:
   * rodinia.srad_v2
   * rodinia.hotspot
   */
  if (streamName.find("rodinia.srad_v2.") == 0 ||
      streamName.find("rodinia.hotspot.") == 0) {
    if (!dynS.hasTotalTripCount()) {
      DYN_S_PANIC(dynS.dynamicStreamId,
                  "Missing TotalTripCount for iter-based floating plan..");
    }

    auto linearAddrGen =
        std::dynamic_pointer_cast<LinearAddrGenCallback>(dynS.addrGenCallback);
    if (!linearAddrGen) {
      // They should have linear address pattern.
      DYN_S_PANIC(dynS.dynamicStreamId,
                  "Non-LinearAddrGen for iter-based floating plan.");
    }

    auto totalTripCount = dynS.getTotalTripCount();
    auto rowTripCount =
        linearAddrGen->getNestTripCount(dynS.addrGenFormalParams, 1);

    // Take min to handle the coalesced stream.
    auto elementSize = std::min(S->getMemElementSize(), 64);

    auto myStartVAddr = linearAddrGen->getStartAddr(dynS.addrGenFormalParams);
    // ! This only considers the case when the address pattern is increasing.
    auto myEndVAddr = myStartVAddr + totalTripCount * elementSize;

    auto threadContext = S->getCPUDelegator()->getSingleThreadContext();
    auto streamNUCAManager = threadContext->getStreamNUCAManager();

    auto totalArrays = streamNUCAManager->getNumStreamRegions();

    const auto &streamNUCARegion =
        streamNUCAManager->getContainingStreamRegion(myStartVAddr);

    auto totalLLCBytes = this->getSharedLLCCapacity();
    auto rowDataBytes = rowTripCount * elementSize * totalArrays;
    auto totalLLCRows = totalLLCBytes / rowDataBytes;

    if (streamName.find("rodinia.hotspot3D.") == 0) {
      /**
       * For hotspot3D, since the first and last layer of Power is not used,
       * we take that into account.
       */
      auto &tbDynS = S->getSE()
                         ->getStream("rodinia.hotspot3D.tb.ld")
                         ->getDynamicStream(dynS.configSeqNum);
      auto &ttDynS = S->getSE()
                         ->getStream("rodinia.hotspot3D.tt.ld")
                         ->getDynamicStream(dynS.configSeqNum);
      auto tbStartVAddr =
          tbDynS.addrGenCallback
              ->genAddr(0, tbDynS.addrGenFormalParams, getStreamValueFail)
              .uint64();
      auto ttStartVAddr =
          ttDynS.addrGenCallback
              ->genAddr(0, ttDynS.addrGenFormalParams, getStreamValueFail)
              .uint64();

      auto layerDataBytes = (tbStartVAddr - ttStartVAddr) / 2;
      auto additionalRows = layerDataBytes / rowDataBytes;
      DYN_S_DPRINTF(dynS.dynamicStreamId,
                    "Rodinia.Hotspot3D Adjust Unused Power LayerBytes %dkB "
                    "LLCRows %d = %d + %d.\n",
                    layerDataBytes / 1024, totalLLCRows + additionalRows,
                    totalLLCRows, additionalRows);
      totalLLCRows += additionalRows;
    }

    auto totalLLCRowDataBytes = totalLLCRows * rowDataBytes;

    auto llcEndVAddr =
        streamNUCARegion.vaddr + totalLLCRows * rowTripCount * elementSize;

    DYN_S_DPRINTF(
        dynS.dynamicStreamId,
        "TotalTripCount %d RowTripCount %d TotalLLCRows %d LLCEndVAddr "
        "%#x = %#x + %d MyEndVAddr %#x = %#x + %d MyEnd %s LLCEnd.\n",
        totalTripCount, rowTripCount, totalLLCRows, llcEndVAddr,
        streamNUCARegion.vaddr, totalLLCRowDataBytes, myEndVAddr, myStartVAddr,
        totalTripCount * elementSize, myEndVAddr < llcEndVAddr ? "<" : ">=");

    if (myEndVAddr <= llcEndVAddr) {
      // We are accessing rows cached in LLC.
      floatPlan.addFloatChangePoint(firstElementIdx, MachineType_L2Cache);
      return;
    }

    if (myStartVAddr >= llcEndVAddr) {
      // We accessing rows not cached in LLC.
      floatPlan.addFloatChangePoint(firstElementIdx, MachineType_Directory);
      return;
    }

    // We are accessing mixing rows in LLC and Mem.
    auto myLLCTripCount = (llcEndVAddr - myStartVAddr) / elementSize;
    floatPlan.addFloatChangePoint(firstElementIdx, MachineType_L2Cache);
    floatPlan.addFloatChangePoint(myLLCTripCount, MachineType_Directory);
    return;
  }

  // Default just offload to LLC.
  floatPlan.addFloatChangePoint(firstElementIdx, MachineType_L2Cache);
  return;
}