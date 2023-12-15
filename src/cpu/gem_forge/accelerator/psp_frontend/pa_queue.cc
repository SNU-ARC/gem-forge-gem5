#include "pa_queue.hh"

PhysicalAddressQueue::PhysicalAddressQueue(uint32_t _capacity)
  : head(0), tail(0), size(0), capacity(_capacity) {
  for (int i = 0; i < _capacity; i++) {
    this->pkt.emplace_back();
  }
}

PhysicalAddressQueue::~PhysicalAddressQueue() {
}

uint32_t
PhysicalAddressQueue::getSize() {
  return this->size;
}

bool
PhysicalAddressQueue::canRead() {
  return this->pkt[this->head].valid;
}

void 
PhysicalAddressQueue::read(PhysicalAddressArgs* _pkt) {
  *_pkt = this->pkt[this->head];
}

void
PhysicalAddressQueue::pop() {
  this->pkt[this->head].valid = false;
  this->pkt[this->head].seqNum = 0;
  this->head = (this->head + 1) % this->capacity;
  this->size--;
}

bool
PhysicalAddressQueue::canInsert() {
  return (this->size + 1 < this->capacity) && (this->pkt[this->tail].seqNum == 0);
}

uint32_t
PhysicalAddressQueue::allocate(uint64_t _seqNum) {
  uint32_t allocated = this->tail;
  this->pkt[allocated].seqNum = _seqNum;
  this->tail = (this->tail + 1) % this->capacity;
  this->size++;
  return allocated;
}

void
PhysicalAddressQueue::reset(uint64_t _seqNum) {
  uint32_t idx = this->head;
  uint32_t oldSize = this->size;
  for (uint32_t i = 0; i < oldSize; i++) {
    idx = (idx + 1) % this->capacity;
    if (this->pkt[idx].seqNum >= _seqNum) {
      this->pkt[idx].valid = false;
      this->pkt[idx].seqNum = 0;
      this->size--;
    }
  }
  this->tail = (this->head + this->size) % this->capacity;
}

void
PhysicalAddressQueue::insert(uint32_t _id, PhysicalAddressArgs _pkt) {
  if (this->pkt[_id].seqNum == _pkt.seqNum) {
    this->pkt[_id] = _pkt;
  }
  else {
    this->size--;
  }
}

PAQueueArray::PAQueueArray(uint32_t _totalPAQueueEntries, uint32_t _capacity) 
  : totalPAQueueEntries(_totalPAQueueEntries), capacity(_capacity) {
    for (uint32_t i = 0; i < _totalPAQueueEntries; i++) {
      this->paQueue.emplace_back(_capacity);
    }
}

PAQueueArray::~PAQueueArray() {
  this->paQueue.erase(this->paQueue.begin(), this->paQueue.end());
}

uint32_t
PAQueueArray::getSize(const uint64_t _entryId) {
  return this->paQueue[_entryId].getSize();
}

bool
PAQueueArray::canRead(const uint64_t _entryId) {
  return this->paQueue[_entryId].canRead();
}

void
PAQueueArray::read(const uint64_t _entryId, 
                   PhysicalAddressQueue::PhysicalAddressArgs* _pkt) {
  return this->paQueue[_entryId].read(_pkt);
}

void
PAQueueArray::pop(const uint64_t _entryId) {
  return this->paQueue[_entryId].pop();
}

bool
PAQueueArray::canInsert(const uint64_t _entryId) {
  return this->paQueue[_entryId].canInsert();
}

uint32_t
PAQueueArray::allocate(const uint64_t _entryId, const uint64_t _seqNum) {
  return this->paQueue[_entryId].allocate(_seqNum);
}

void 
PAQueueArray::insert(const uint64_t _entryId, const uint32_t _id,
                     PhysicalAddressQueue::PhysicalAddressArgs _pkt) {
  this->paQueue[_entryId].insert(_id, _pkt);
}

void
PAQueueArray::reset(const uint64_t _entryId, const uint64_t _seqNum) {
  this->paQueue[_entryId].reset(_seqNum);
}
