#include "arbiter.hh"

RRArbiter::RRArbiter(uint32_t _totalPatternTableEntries)
  : totalPatternTableEntries(_totalPatternTableEntries), lastChosenEntryId(0) {};
 
RRArbiter::~RRArbiter() {};

uint32_t RRArbiter::getTotalPatternTableEntries() {
  return this->totalPatternTableEntries;
}

uint32_t RRArbiter::getLastChosenEntryId() { 
  return this->lastChosenEntryId; 
}

void RRArbiter::setLastChosenEntryId(uint32_t _chosenEntryId) {
  this->lastChosenEntryId = _chosenEntryId;
}

PatternTableRRArbiter::PatternTableRRArbiter(uint32_t _totalPatternTableEntries,
    PatternTable* _patternTable, IndexQueueArray* _indexQueueArray)
  : RRArbiter(_totalPatternTableEntries), patternTable(_patternTable), 
    indexQueueArray(_indexQueueArray) {
}

PatternTableRRArbiter::~PatternTableRRArbiter() {
}

bool PatternTableRRArbiter::getValidEntryId(uint32_t* _entryId, const uint64_t _cacheLineSize) {
  for (uint32_t i = 1; i < this->getTotalPatternTableEntries() + 1; i++) {
    uint32_t entryId = (this->getLastChosenEntryId() + i) % this->getTotalPatternTableEntries();
    if (this->patternTable->isConfigInfoValid(entryId) &&
        this->patternTable->isInputInfoValid(entryId)) {
      uint64_t offsetBegin, offsetEnd, seqNum;
      this->patternTable->getInputInfo(entryId, &offsetBegin, &offsetEnd, &seqNum);
      if (this->indexQueueArray->canInsert(entryId, _cacheLineSize, seqNum)) {
        *_entryId = entryId;
        return true;
      }
    }
  }
  return false;
}

IndexQueueArrayRRArbiter::IndexQueueArrayRRArbiter(uint32_t _totalIndexQueueArrayEntries,
    IndexQueueArray* _indexQueueArray, PAQueueArray* _paQueueArray)
  : RRArbiter(_totalIndexQueueArrayEntries), indexQueueArray(_indexQueueArray),
    paQueueArray(_paQueueArray) {
}

IndexQueueArrayRRArbiter::~IndexQueueArrayRRArbiter() {
}

bool IndexQueueArrayRRArbiter::getValidEntryId(uint32_t* _entryId, bool _bypassPAQueue) {
  for (uint32_t i = 1; i < this->getTotalPatternTableEntries() + 1; i++) {
    uint32_t entryId = (this->getLastChosenEntryId() + i) % this->getTotalPatternTableEntries();
    if (this->indexQueueArray->getConfigured(entryId) &&
        this->indexQueueArray->canRead(entryId) &&
        (this->paQueueArray->canInsert(entryId))) {
      *_entryId = entryId;
      return true;
    }
  }
  return false;
}
