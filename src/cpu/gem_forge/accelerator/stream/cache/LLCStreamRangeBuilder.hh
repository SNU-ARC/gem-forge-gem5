#ifndef __CPU_GEM_FORGE_ACCELERATOR_LLC_STREAM_RANGE_BUILDER_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_LLC_STREAM_RANGE_BUILDER_HH__

#include "DynamicStreamAddressRange.hh"
#include "LLCDynamicStream.hh"

#include <list>

class LLCStreamRangeBuilder {
public:
  LLCStreamRangeBuilder(LLCDynamicStream *_stream, int64_t _totalTripCount);

  void addElementAddress(uint64_t elementIdx, Addr vaddr, Addr paddr, int size);

  bool hasReadyRanges() const;
  DynamicStreamAddressRangePtr popReadyRange();

  /**
   * Push the next range tail element idx.
   */
  void pushNextRangeTailElementIdx(uint64_t nextRangeTailElementIdx);

  /**
   * Cut total trip count due to StreamLoopBound.
   */
  void receiveLoopBoundRet(int64_t totalTripCount);

private:
  LLCDynamicStream *stream;
  std::list<uint64_t> nextRangeTailElementIdxQueue;
  static constexpr int64_t InvalidTotalTripCount =
      CacheStreamConfigureData::InvalidTotalTripCount;
  int64_t totalTripCount;
  uint64_t nextElementIdx = 0;
  uint64_t prevBuiltElementIdx = 0;
  uint64_t prevNextRangeTailElementIdx = 0;

  AddressRange vaddrRange;
  AddressRange paddrRange;
  std::list<DynamicStreamAddressRangePtr> readyRanges;

  void tryBuildRange();

  int curRemoteBank() const { return this->stream->curRemoteBank(); }
  const char *curRemoteMachineType() const {
    return this->stream->curRemoteMachineType();
  }
};

#endif