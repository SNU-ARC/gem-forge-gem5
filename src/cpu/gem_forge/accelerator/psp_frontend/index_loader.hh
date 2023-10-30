#ifndef __CPU_GEM_FORGE_ACCELERATOR_INDEX_LOAD_UNIT_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_INDEX_LOAD_UNIT_HH__

// Editor: Sungjun Jung (miguel92@snu.ac.kr)
// Description: Generate load requests for index

#include <stdlib.h>
#include <string.h>
#include <cassert>
#include <iostream>
#include <vector>

#include "pattern_table.hh"
#include "index_queue.hh"

class IndexLoadUnit {
  public:
    IndexLoadUnit(PatternTable* _patternTable,
                  IndexQueueArray* _indexQueueArray);
    ~IndexLoadUnit();

    void tick();
    void setCacheLineSize(uint32_t _cacheLineSize);
    bool issueLoadIndex(uint64_t _validEntryId);
    
  private:
    uint32_t cacheLineSize;
    PatternTable* patternTable;
    IndexQueueArray* indexQueueArray;
};
#endif
