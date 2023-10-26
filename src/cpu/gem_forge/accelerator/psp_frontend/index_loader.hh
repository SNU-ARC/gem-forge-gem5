#ifndef __CPU_GEM_FORGE_ACCELERATOR_INDEX_LOAD_UNIT_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_INDEX_LOAD_UNIT_HH__

// Editor: Sungjun Jung (miguel92@snu.ac.kr)
// Description: Generate load requests for index

#include <stdlib.h>
#include <string.h>
#include <cassert>
#include <iostream>
#include <vector>

class IndexLoadUnit {
  public:
    IndexLoadUnit(PatternTable* _patternTable,
                  indexQueueArray* _indexQueueArray);
    ~IndexLoadUnit();

    void tick();
    
  private:
    PatternTable* patternTable;
    IndexQueueArray* indexQueueArray;
};
#endif
