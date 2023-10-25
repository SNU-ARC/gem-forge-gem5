#include "index_queue.hh"

#define INDEX_QUEUE_PANIC(format, args...)                                          \
  panic("%llu-[INDEX_QUEUE%d] " format, cpuDelegator->curCycle(),                   \
        cpuDelegator->cpuId(), ##args)
#define INDEX_QUEUE_DPRINTF(format, args...)                                        \
  DPRINTF(IndexQueue, "%llu-[INDEX_QUEUE%d] " format,                          \
          cpuDelegator->curCycle(), cpuDelegator->cpuId(), ##args)
#define DYN_INST_DPRINTF(format, args...)                                      \
  INDEX_QUEUE_DPRINTF("%llu %s " format, dynInfo.seqNum, dynInfo.pc, ##args)
#define DYN_INST_PANIC(format, args...)                                        \
  INDEX_QUEUE_PANIC("%llu %s " format, dynInfo.seqNum, dynInfo.pc, ##args)

IndexQueue::IndexQueue(uint32_t _capacity)
  : isConfigured(false), capacity(_capacity), front(0), size(0), accessGranularity(0) {
  data = malloc(_capacity);
}

IndexQueue::~IndexQueue() {
  free(data);
}

void IndexQueue::setConfigured(bool _isConfigured) {
  this->isConfigured = _isConfigured;
}

bool IndexQueue::getConfigured() {
  return this->isConfigured;
}

uint32_t IndexQueue::getSize() {
  return this->size;
}

void IndexQueue::setAccessGranularity(uint64_t _accessGranularity) {
  this->accessGranularity = _accessGranularity;
}

uint64_t IndexQueue::getAccessGranularity() {
  return this->accessGranularity;
}

void IndexQueue::pop(void* _buffer) {
  this->size -= this->accessGranularity;
  this->front = (this->front + this->accessGranularity) % this->capacity;
}

void IndexQueue::insert(void* _buffer) {
  uint64_t back = (this->front + this->size) % this->capacity;
  if (this->capacity - back >= 64) {
    memcpy(data + back, _buffer, 64);
  }
  else {
    uint64_t remainder = this->capacity - back;
    memcpy(data + back, _buffer, remainder);
    memcpy(data, _buffer + remainder, 64 - remainder);
  }
}

IndexQueueArray::IndexQueueArray(uint32_t _totalIndexQueueEntries, uint32_t _capacity) 
  : totalIndexQueueEntries(_totalIndexQueueEntries), capacity(_capacity) {
    for (uint32_t i = 0; i < _totalIndexQueueEntries; i++) {
      this->indexQueue.emplace_back(_capacity);
    }
    this->numInflightRequests = new uint32_t[this->totalIndexQueueEntries];
}

IndexQueueArray::~IndexQueueArray() {
  this->indexQueue.erase(this->indexQueue.begin(), this->indexQueue.end());
  delete[] this->numInflightRequests;
}

void IndexQueueArray::setConfigured(const uint32_t _entryId, bool _isConfigured) {
  this->indexQueue[_entryId].setConfigured(_isConfigured);
}

bool IndexQueueArray::getConfigured(const uint32_t _entryId) {
  return this->indexQueue[_entryId].getConfigured();
}

uint32_t IndexQueueArray::getSize(const uint32_t _entryId) {
  return this->indexQueue[_entryId].getSize();
}

void IndexQueueArray::setAccessGranularity(const uint32_t _entryId, uint64_t _accessGranularity) {
  switch (_accessGranularity) {
    case sizeof(uint8_t):
    case sizeof(uint16_t):
    case sizeof(uint32_t):
    case sizeof(uint64_t):
      this->indexQueue[_entryId].setAccessGranularity(_accessGranularity);
      break;
    default:
      assert(true && "Invalid Index access granularity.");
  }
}

uint64_t IndexQueueArray::getAccessGranularity(const uint32_t _entryId) {
  return this->indexQueue[_entryId].getAccessGranularity();
}

void IndexQueueArray::pop(const uint32_t _entryId, void* _buffer) {
  return this->indexQueue[_entryId].pop(_buffer);
}

bool IndexQueueArray::canInsert(const uint32_t _entryId) {
  return (this->indexQueue[_entryId].getSize() + this->numInflightRequests[_entryId]) > this->capacity; 
}

void IndexQueueArray::insert(const uint32_t _entryId, void* _buffer) {
  this->indexQueue[_entryId].insert(_buffer);
}
