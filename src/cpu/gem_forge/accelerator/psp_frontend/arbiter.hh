#ifndef __CPU_GEM_FORGE_ACCELERATOR_ARBITER_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_ARBITER_HH__

#include <iostream>

//#include "psp_frontend.hh"
#include "pattern_table.hh"
#include "index_queue.hh"
#include "pa_queue.hh"

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
  bool getValidEntryId(uint32_t* _entryId, const uint64_t _cacheLineSize);

private:
  PatternTable* patternTable;
  IndexQueueArray* indexQueueArray;
};

class IndexQueueArrayRRArbiter : public RRArbiter {
public:
  IndexQueueArrayRRArbiter(uint32_t _totalIndexQueueArrayEntries, 
                           IndexQueueArray* _indexQueueArray,
                           PAQueueArray* _paQueueArray);
  ~IndexQueueArrayRRArbiter();
  bool getValidEntryId(uint32_t* _entryId);

private:
  IndexQueueArray* indexQueueArray;
  PAQueueArray* paQueueArray;
};
#endif
