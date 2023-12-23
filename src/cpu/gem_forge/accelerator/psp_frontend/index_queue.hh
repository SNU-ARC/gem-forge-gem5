#ifndef __CPU_GEM_FORGE_ACCELERATOR_INDEX_QUEUE_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_INDEX_QUEUE_HH__

// Editor: Sungjun Jung (miguel92@snu.ac.kr)
// Description: Queue to store index values.
//             Single queue can hold 64B data
//             Number of queue is same as the number of PatternTableEntries

#include <stdlib.h>
#include <string.h>
#include <cassert>
#include <iostream>
#include <vector>
#include "base/statistics.hh"

class IndexQueue {
  public:
    IndexQueue(uint32_t _capacity);
    ~IndexQueue();

    void setConfigured(bool _isConfigured);
    bool getConfigured();
    uint32_t getAllocatedSize();
    uint32_t getSize();
    void setAccessGranularity(uint64_t _accessGranularity);
    uint64_t getAccessGranularity();
    void read(void* _buffer, uint64_t* _seqNum);
    void pop();
    bool canInsert(uint64_t _cacheLineSize, uint64_t _seqNum);
    void allocate(uint64_t _size, uint64_t _seqNum);
    void insert(void* _buffer, uint64_t _size, uint64_t _data_address, uint64_t _seqNum);
    void reset(uint64_t _seqNum);

  private:
    std::vector<void*> data;
    bool isConfigured;
    uint32_t capacity;
    uint32_t front;
    uint32_t allocatedSize;
    uint32_t size;
    uint64_t accessGranularity;
    uint64_t seqNum;
};

class IndexQueueArray {
  public:
    IndexQueueArray(uint32_t _totalIndexQueueEntries, uint32_t _capacity);
    ~IndexQueueArray();

    void setConfigured(const uint64_t _entryId, bool _isConfigured);
    bool getConfigured(const uint64_t _entryId);
    uint32_t getAllocatedSize(const uint64_t _entryId);
    uint32_t getSize(const uint64_t _entryId);
    void setAccessGranularity(const uint64_t _entryId, uint64_t _accessGranularity);
    uint64_t getAccessGranularity(const uint64_t _entryId);
    bool canRead(const uint64_t _entryId);
    void read(const uint64_t _entryId, void* _buffer, uint64_t* _seqNum);
    void pop(const uint64_t _entryId);
    bool canInsert(const uint64_t _entryId, const uint64_t _cacheLineSize, const uint64_t _seqNum);
    void allocate(const uint64_t _entryId, const uint64_t _size, const uint64_t _seqNum);
    void insert(const uint64_t _entryId, void* _buffer, uint64_t _size, uint64_t _data_address, uint64_t _seqNum);
    void reset(const uint64_t _entryId, const uint64_t _seqNum);
    
  private:
    std::vector<IndexQueue> indexQueue;
    uint32_t totalIndexQueueEntries;
    uint32_t capacity;
};
#endif
