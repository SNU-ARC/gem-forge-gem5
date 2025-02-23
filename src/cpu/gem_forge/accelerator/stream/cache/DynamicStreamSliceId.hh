#ifndef __CPU_TDG_ACCELERATOR_DYNAMIC_STREAM_SLICE_ID_HH__
#define __CPU_TDG_ACCELERATOR_DYNAMIC_STREAM_SLICE_ID_HH__

#include "DynamicStreamElementRangeId.hh"

/**
 * The core stream engine manages stream at granularity of element.
 * However, this is not ideal for cache stream engine, as we want to
 * coalesce continuous elements to the same cache line. Things get
 * more complicated when there is overlapping between elements and
 * one element can span across multiple cache lines.
 *
 * This represent the basic unit how the cache system manages streams.
 * A slice is a piece of continuous memory, and does not span across
 * cache lines. It also remembers elements within this slice,
 * [lhsElementIdx, rhsElementIdx).
 *
 * Notice that one element may span across multiple cache lines, and
 * thus the data in a slice may only be a portion of the whole element.
 */
struct DynamicStreamSliceId {
  DynamicStreamElementRangeId elementRange;
  /**
   * Hack: This is element vaddr for indirect streams,
   * but line vaddr for direct sliced streams.
   * TODO: Fix this.
   */
  Addr vaddr;
  int size;

  DynamicStreamSliceId() : elementRange(), vaddr(0), size(0) {}

  bool isValid() const { return this->elementRange.isValid(); }
  void clear() {
    this->elementRange.clear();
    this->vaddr = 0;
    this->size = 0;
  }

  DynamicStreamId &getDynStreamId() { return this->elementRange.streamId; }
  const DynamicStreamId &getDynStreamId() const {
    return this->elementRange.streamId;
  }

  const uint64_t &getStartIdx() const {
    return this->elementRange.getLHSElementIdx();
  }
  uint64_t &getStartIdx() { return this->elementRange.getLHSElementIdx(); }

  const uint64_t &getEndIdx() const { return this->elementRange.rhsElementIdx; }
  uint64_t &getEndIdx() { return this->elementRange.rhsElementIdx; }

  uint64_t getNumElements() const {
    return this->elementRange.getNumElements();
  }
  int getSize() const { return this->size; }

  bool operator==(const DynamicStreamSliceId &other) const {
    return this->elementRange == other.elementRange;
  }

  bool operator!=(const DynamicStreamSliceId &other) const {
    return !(this->operator==(other));
  }
};

std::ostream &operator<<(std::ostream &os, const DynamicStreamSliceId &id);

struct DynamicStreamSliceIdHasher {
  std::size_t operator()(const DynamicStreamSliceId &key) const {
    return (DynamicStreamElementRangeIdHasher()(key.elementRange)) ^
           std::hash<uint64_t>()(key.vaddr);
  }
};

#endif