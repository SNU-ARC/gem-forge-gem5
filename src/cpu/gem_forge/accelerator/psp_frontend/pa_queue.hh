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
      uint64_t entryId;
      uint64_t pAddr;
      uint64_t size;

      PhysicalAddressArgs(uint64_t _entryId, uint64_t _pAddr, uint64_t _size)
        : entryId(_entryId), pAddr(_pAddr), size(_size) {}
    };

    uint32_t getSize();
    bool canRead();
    void read(PhysicalAddressArgs* _pkt);
    void pop();
    bool canInsert();
    void insert(PhysicalAddressArgs* _pkt);

  private:
    std::queue<PhysicalAddressArgs> pkt;
    uint32_t capacity;
};

class PAQueueArray {
  public:
    PAQueueArray(uint32_t _totalValueQueueEntries, uint32_t _capacity);
    ~PAQueueArray();

    uint32_t* numInflightTranslations;

    uint32_t getSize(const uint64_t _entryId);
    bool canRead(const uint64_t _entryId);
    void read(const uint64_t _entryId, PhysicalAddressQueue::PhysicalAddressArgs* _pkt);
    void pop(const uint64_t _entryId);
    bool canInsert(const uint64_t _entryId);
    void insert(const uint64_t _entryId, PhysicalAddressQueue::PhysicalAddressArgs* _pkt);
    
  private:
    std::vector<PhysicalAddressQueue> paQueue;
    uint32_t totalPAQueueEntries;
    uint32_t capacity;
};
#endif
