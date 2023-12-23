#include "index_queue.hh"

#define INDEX_QUEUE_PANIC(format, args...)                                          \
  panic("%llu-[INDEX_QUEUE%d] " format, cpuDelegator->curCycle(),                   \
        cpuDelegator->cpuId(), ##args)
#define INDEX_QUEUE_DPRINTF(format, args...)                                        \
  DPRINTF(IndexQueue, "%llu-[INDEX_QUEUE%d] " format,                          \
          cpuDelegator->curCycle(), cpuDelegator->cpuId(), ##args)

IndexQueue::IndexQueue(uint32_t _capacity)
  : isConfigured(false), capacity(_capacity), front(0), allocatedSize(0), size(0), accessGranularity(0) {
  this->data.emplace_back();
  data[0] = malloc(_capacity);
//  data = malloc(_capacity);
}

IndexQueue::~IndexQueue() {
//  free(data);
}

void IndexQueue::setConfigured(bool _isConfigured) {
  this->isConfigured = _isConfigured;
}

bool IndexQueue::getConfigured() {
  return this->isConfigured;
}

uint32_t IndexQueue::getAllocatedSize() {
  return this->allocatedSize;
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

void IndexQueue::read(void* _buffer, uint64_t* _seqNum) {
  memcpy(_buffer, data[0] + this->front, this->accessGranularity);
  *_seqNum = this->seqNum;
}

void IndexQueue::pop() {
  this->allocatedSize -= this->accessGranularity;
  this->size -= this->accessGranularity;
  this->front = (this->front + this->accessGranularity) % this->capacity;
}

bool
IndexQueue::canInsert(uint64_t _cacheLineSize, uint64_t _seqNum) {
  if (this->size > 0 && this->seqNum != _seqNum)
    return false;
  else 
    return ((this->allocatedSize + _cacheLineSize) <= this->capacity);
}

void
IndexQueue::allocate(uint64_t _size, uint64_t _seqNum) {
  this->allocatedSize += _size;
  this->seqNum = _seqNum;
}

void IndexQueue::insert(void* _buffer, uint64_t _size, uint64_t _data_address, uint64_t _seqNum) {
  if (this->seqNum != _seqNum) {
    return;
  }
  else {
    uint64_t back = (this->front + this->size) % this->capacity;
    _data_address = (uint64_t)data[0] + back;
    if (this->capacity - back >= _size) {
      memcpy(data[0] + back, _buffer, _size);
    }
    else {
      uint64_t remainder = this->capacity - back;
      memcpy(data[0] + back, _buffer, remainder);
      memcpy(data[0], _buffer + remainder, _size - remainder);
    }
    this->size += _size;
  }
}

void
IndexQueue::reset(uint64_t _seqNum) {
  if (this->seqNum == _seqNum) {
    front = 0;
    allocatedSize = 0;
    size = 0;
    this->seqNum = 0;
  }
}

IndexQueueArray::IndexQueueArray(uint32_t _totalIndexQueueEntries, uint32_t _capacity) 
  : totalIndexQueueEntries(_totalIndexQueueEntries), capacity(_capacity) {
    for (uint32_t i = 0; i < _totalIndexQueueEntries; i++) {
      this->indexQueue.emplace_back(_capacity);
    }
}

IndexQueueArray::~IndexQueueArray() {
  this->indexQueue.erase(this->indexQueue.begin(), this->indexQueue.end());
}

void IndexQueueArray::setConfigured(const uint64_t _entryId, bool _isConfigured) {
  this->indexQueue[_entryId].setConfigured(_isConfigured);
}

bool IndexQueueArray::getConfigured(const uint64_t _entryId) {
  return this->indexQueue[_entryId].getConfigured();
}

uint32_t IndexQueueArray::getAllocatedSize(const uint64_t _entryId) {
  return this->indexQueue[_entryId].getAllocatedSize();
}

uint32_t IndexQueueArray::getSize(const uint64_t _entryId) {
  return this->indexQueue[_entryId].getSize();
}

void IndexQueueArray::setAccessGranularity(const uint64_t _entryId, uint64_t _accessGranularity) {
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

uint64_t IndexQueueArray::getAccessGranularity(const uint64_t _entryId) {
  return this->indexQueue[_entryId].getAccessGranularity();
}

bool IndexQueueArray::canRead(const uint64_t _entryId) {
  return this->indexQueue[_entryId].getSize() >= this->indexQueue[_entryId].getAccessGranularity();
}

void IndexQueueArray::read(const uint64_t _entryId, void* _buffer, uint64_t* _seqNum) {
  return this->indexQueue[_entryId].read(_buffer, _seqNum);
}

void IndexQueueArray::pop(const uint64_t _entryId) {
  this->indexQueue[_entryId].pop();
}

bool IndexQueueArray::canInsert(const uint64_t _entryId, const uint64_t _cacheLineSize, const uint64_t _seqNum) {
  return this->indexQueue[_entryId].canInsert(_cacheLineSize, _seqNum);
}

void
IndexQueueArray::allocate(const uint64_t _entryId, const uint64_t _size, const uint64_t _seqNum) {
  this->indexQueue[_entryId].allocate(_size, _seqNum);
}

void IndexQueueArray::insert(const uint64_t _entryId, void* _buffer, uint64_t _size, uint64_t _data_address, uint64_t _seqNum) {
  this->indexQueue[_entryId].insert(_buffer, _size, _data_address, _seqNum);
}

void
IndexQueueArray::reset(const uint64_t _entryId, const uint64_t _seqNum) {
  this->indexQueue[_entryId].reset(_seqNum);
}
