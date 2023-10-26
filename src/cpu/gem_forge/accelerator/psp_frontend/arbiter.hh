#ifndef __CPU_GEM_FORGE_ACCELERATOR_ARBITER_HH__
#define __CPU_GEM_FROGE_ACCELERATOR_ARBITER_HH__

#include <iostream>
#include <unordered_map>

class RRArbiter {
public:
  RRArbiter(uint32_t _totalPatternTableEntries)
    : totalPatternTableEntries(_totalPatternTableEntries), lastChosenEntryId(0) {};
  ~RRArbiter() {};

  uint32_t getTotalPatternTableEntries() { return this->totalPatternTableEntries; }
  uint32_t getLastChosenEntryId() { return this->lastChosenEntryId; }
  void setLastChosenEntryId(uint32_t _chosenEntryId) { this->lastChosenEntryId = _chosenEntryId; }

private:
  uint32_t totalPatternTableEntries;
  uint32_t lastChosenEntryId;
};

class PatternTableRRArbiter : public RRArbiter {
public:
  PatternTableRRArbiter(uint32_t _totalPatternTableEntries)
    : RRArbiter(_totalPatternTableEntries) {};
  ~PatternTableRRArbiter() {};
  PatternTable* getValidEntry(PatternTable* _patternTable) {
    uint32_t patternTableIter = (this->getLastChosenEntryId() + 1) % this->getTotalPatternTableEntries();
    for (uint32_t i = 0; i < this->getTotalPatternTableEntries(); i++) {
      if (_patternTable->isConfigInfoValid(patternTableIter)) {
        this->setLastChosenEntryId(patternTableIter);
        return &_patternTable[patternTableIter];
      }
      patternTableIter = (patternTableIter + 1) % this->getTotalPatternTableEntries();
    }
    return nullptr;
  }
};
#endif
