#include "pa_queue.hh"

PhysicalAddressQueue::PhysicalAddressQueue(uint32_t _capacity)
  : capacity(_capacity) {
}

PhysicalAddressQueue::~PhysicalAddressQueue() {
}

uint32_t PhysicalAddressQueue::getSize() {
  return this->pkt.size();
}

bool PhysicalAddressQueue::canRead() {
  return this->pkt.size() > 0;
}

void PhysicalAddressQueue::read(PhysicalAddressArgs* _pkt) {
  *_pkt = pkt.front();
}

void PhysicalAddressQueue::pop() {
  pkt.pop();
}

bool PhysicalAddressQueue::canInsert() {
  return this->pkt.size() <= this->capacity;
}

void PhysicalAddressQueue::insert(PhysicalAddressArgs* _pkt) {
  pkt.push(*_pkt);
}

PAQueueArray::PAQueueArray(uint32_t _totalPAQueueEntries, uint32_t _capacity) 
  : totalPAQueueEntries(_totalPAQueueEntries), capacity(_capacity) {
    for (uint32_t i = 0; i < _totalPAQueueEntries; i++) {
      this->paQueue.emplace_back(_capacity);
    }
    this->numInflightTranslations = new uint32_t[this->totalPAQueueEntries];
    for (uint64_t i = 0; i < _totalPAQueueEntries; i++)
      this->numInflightTranslations[i] = 0;
}

PAQueueArray::~PAQueueArray() {
  this->paQueue.erase(this->paQueue.begin(), this->paQueue.end());
  delete[] this->numInflightTranslations;
}

uint32_t PAQueueArray::getSize(const uint64_t _entryId) {
  return this->paQueue[_entryId].getSize();
}

bool PAQueueArray::canRead(const uint64_t _entryId) {
  return this->paQueue[_entryId].canRead();
}

void PAQueueArray::read(const uint64_t _entryId, 
                       PhysicalAddressQueue::PhysicalAddressArgs* _pkt) {
  return this->paQueue[_entryId].read(_pkt);
}

void PAQueueArray::pop(const uint64_t _entryId) {
  return this->paQueue[_entryId].pop();
}

bool PAQueueArray::canInsert(const uint64_t _entryId) {
  return (this->paQueue[_entryId].getSize() + this->numInflightTranslations[_entryId]) < this->capacity; 
}

void PAQueueArray::insert(const uint64_t _entryId, 
                             PhysicalAddressQueue::PhysicalAddressArgs* _pkt) {
  this->paQueue[_entryId].insert(_pkt);
}
