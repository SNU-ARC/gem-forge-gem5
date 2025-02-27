#ifndef __CPU_GEM_FORGE_ACCELERATOR_PHYSICAL_ADDRESS_QUEUE_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_PHYSICAL_ADDRESS_QUEUE_HH__

// Editor: Sungjun Jung (miguel92@snu.ac.kr)
// Description: Queue to store value prefetch patterns.

#include <stdlib.h>
#include <string.h>
#include <cassert>
#include <iostream>
#include <queue>
#include <vector>

class PhysicalAddressQueue {
  public:
    PhysicalAddressQueue(uint32_t _capacity);
    ~PhysicalAddressQueue();

    struct PhysicalAddressArgs {
      bool valid;
      uint64_t entryId;
      uint64_t pAddr;
      uint64_t size;
      uint64_t seqNum;

      PhysicalAddressArgs(bool _valid = false, uint64_t _entryId = 0, uint64_t _pAddr = 0, uint64_t _size = 0, uint64_t _seqNum = 0)
        : valid(_valid), entryId(_entryId), pAddr(_pAddr), size(_size), seqNum(_seqNum) {}
    };

    uint32_t getSize();
    bool canRead();
    void read(PhysicalAddressArgs* _pkt);
    void pop();
    bool canInsert();
    uint32_t allocate(uint64_t _seqNum);
    void insert(uint32_t _id, PhysicalAddressArgs _pkt);
    void reset(uint64_t _seqNum);

  private:
    std::vector<PhysicalAddressArgs> pkt;
    uint32_t head;
    uint32_t tail;
    uint32_t size;
    uint32_t capacity;
};

class PAQueueArray {
  public:
    PAQueueArray(uint32_t _totalValueQueueEntries, uint32_t _capacity);
    ~PAQueueArray();

    uint32_t getSize(const uint64_t _entryId);
    bool canRead(const uint64_t _entryId);
    void read(const uint64_t _entryId, PhysicalAddressQueue::PhysicalAddressArgs* _pkt);
    void pop(const uint64_t _entryId);
    bool canInsert(const uint64_t _entryId);
    uint32_t allocate(const uint64_t _entryId, const uint64_t _seqNum);
    void insert(const uint64_t _entryId, const uint32_t _id, PhysicalAddressQueue::PhysicalAddressArgs _pkt);
    void reset(const uint64_t _entryId, const uint64_t _seqNum);
    
  private:
    std::vector<PhysicalAddressQueue> paQueue;
    uint32_t totalPAQueueEntries;
    uint32_t capacity;
};
#endif
