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

bool PatternTableRRArbiter::getValidEntryId(uint32_t _entryId) {
  for (uint32_t i = 0; i < this->getTotalPatternTableEntries(); i++) {
    uint32_t entryId = (this->getLastChosenEntryId() + i) % this->getTotalPatternTableEntries();
    if (this->patternTable->isConfigInfoValid(entryId) &&
        this->patternTable->isInputInfoValid(entryId) &&
        this->indexQueueArray->canInsert(entryId)) {
      _entryId = entryId;
      return true;
    }
  }
  return false;
}

void PatternTableRRArbiter::selectEntryId(uint32_t _entryId) {
  this->setLastChosenEntryId(_entryId);
}
