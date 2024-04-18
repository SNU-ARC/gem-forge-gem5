#include "index_queue.hh"

#define INDEX_QUEUE_PANIC(format, args...)                                          \
  panic("%llu-[INDEX_QUEUE%d] " format, cpuDelegator->curCycle(),                   \
        cpuDelegator->cpuId(), ##args)
#define INDEX_QUEUE_DPRINTF(format, args...)                                        \
  DPRINTF(IndexQueue, "%llu-[INDEX_QUEUE%d] " format,                          \
          cpuDelegator->curCycle(), cpuDelegator->cpuId(), ##args)

IndexQueue::IndexQueue(uint32_t _capacity)
  : isConfigured(false), capacity(_capacity), head(0), tail(0), size(0), accessGranularity(0) {
    for (int i = 0; i < _capacity; i++) {
      this->data.emplace_back();
    }
}

IndexQueue::~IndexQueue() {
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

bool
IndexQueue::canRead() {
  return this->data[this->head].valid;
}

void
IndexQueue::read(uint64_t* _buffer, uint64_t* _seqNum) {
  *_buffer = this->data[this->head].data;
  *_seqNum = this->data[this->head].seqNum;
}

void 
IndexQueue::pop() {
  this->data[this->head].valid = false;
  this->data[this->head].seqNum = 0;
  this->head = (this->head + 1) % (this->capacity / this->accessGranularity);
  this->size -= this->accessGranularity;
}

bool
IndexQueue::canInsert(uint64_t _size, uint64_t _seqNum) {
  return (this->size + _size <= this->capacity) && (this->data[this->tail].seqNum == 0);
}

uint32_t
IndexQueue::allocate(uint64_t _size, uint64_t _seqNum) {
  uint32_t id = this->tail;
  uint32_t iter = _size / this->accessGranularity;
  for (uint32_t i = 0; i < iter; i++) {
    this->data[this->tail].seqNum = _seqNum;
    this->tail = (this->tail + 1) % (this->capacity / this->accessGranularity);
  }
//  this->tail = (this->tail + step) % (this->capacity / this->accessGranularity);
  this->size += _size;
  return id;
}

void IndexQueue::insert(uint32_t _id, void* _buffer, uint64_t _size, uint64_t _seqNum) {
  uint32_t iter = _size / this->accessGranularity;
  uint32_t ptr = _id;
  for (int i = 0; i < iter; i++) {
    if (this->data[ptr].seqNum == _seqNum) {
      this->data[ptr].valid = true;
      this->data[ptr].data = 0;
      memcpy(&(this->data[ptr].data), _buffer + i * this->accessGranularity, this->accessGranularity);
    }
//    else {
//      this->size -= this->accessGranularity;
//    }
    ptr = (ptr + 1) % (this->capacity / this->accessGranularity);
  }
}

void
IndexQueue::reset(uint64_t _seqNum) {
  uint32_t idx = this->head;
  uint32_t oldSize = this->size;
  for (uint32_t i = 0; i < oldSize; i++) {
    idx = (idx + 1) % (this->capacity / this->accessGranularity);
    if (this->data[idx].seqNum >= _seqNum) {
      this->data[idx].valid = false;
      this->data[idx].seqNum = 0;
      this->size -= this->accessGranularity;
    }
  }
  this->tail = (this->head + (this->size / this->accessGranularity)) % (this->capacity / this->accessGranularity);
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
//
//uint32_t IndexQueueArray::getAllocatedSize(const uint64_t _entryId) {
//  return this->indexQueue[_entryId].getAllocatedSize();
//}

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
  return this->indexQueue[_entryId].canRead();
//  return this->indexQueue[_entryId].getSize() >= this->indexQueue[_entryId].getAccessGranularity();
}

void IndexQueueArray::read(const uint64_t _entryId, void* _buffer, uint64_t* _seqNum) {
  this->indexQueue[_entryId].read((uint64_t*)_buffer, _seqNum);
//  return this->indexQueue[_entryId].read(_buffer, _seqNum);
}

void IndexQueueArray::pop(const uint64_t _entryId) {
  this->indexQueue[_entryId].pop();
}

bool IndexQueueArray::canInsert(const uint64_t _entryId, const uint64_t _cacheLineSize, const uint64_t _seqNum) {
  return this->indexQueue[_entryId].canInsert(_cacheLineSize, _seqNum);
}

uint32_t
IndexQueueArray::allocate(const uint64_t _entryId, const uint64_t _size, const uint64_t _seqNum) {
  return this->indexQueue[_entryId].allocate(_size, _seqNum);
}

void IndexQueueArray::insert(const uint64_t _entryId, const uint32_t _id, void* _buffer, uint64_t _size, uint64_t _data_address, uint64_t _seqNum) {
  this->indexQueue[_entryId].insert(_id, _buffer, _size, _seqNum);
}

void
IndexQueueArray::reset(const uint64_t _entryId, const uint64_t _seqNum) {
  this->indexQueue[_entryId].reset(_seqNum);
}
