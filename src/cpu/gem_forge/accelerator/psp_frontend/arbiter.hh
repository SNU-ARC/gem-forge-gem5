#ifndef __CPU_GEM_FORGE_ACCELERATOR_ARBITER_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_ARBITER_HH__

#include <iostream>

//#include "psp_frontend.hh"
#include "pattern_table.hh"
#include "index_queue.hh"

class RRArbiter {
public:
  RRArbiter(uint32_t _totalPatternTableEntries);
  ~RRArbiter();

  uint32_t getTotalPatternTableEntries();
  uint32_t getLastChosenEntryId();
  void setLastChosenEntryId(uint32_t _chosenEntryId);

private:
  uint32_t totalPatternTableEntries;
  uint32_t lastChosenEntryId;
};

class PatternTableRRArbiter : public RRArbiter {
public:
  PatternTableRRArbiter(uint32_t _totalPatternTableEntries, 
                        PatternTable* _patternTable,
                        IndexQueueArray* _indexQueueArray);
  ~PatternTableRRArbiter();
  bool getValidEntryId(uint32_t* _entryId);
  void selectEntryId(uint32_t _entryId);

private:
  PatternTable* patternTable;
  IndexQueueArray* indexQueueArray;
};
#endif
