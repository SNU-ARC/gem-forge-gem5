#include "index_loader.hh"

#include "debug/IndexLoadUnit.hh"

IndexLoadUnit::IndexLoadUnit(PatternTable* _patternTable, 
                             IndexQueueArray* _indexQueueArray)
  : cacheLineSize(64), patternTable(_patternTable), indexQueueArray(_indexQueueArray) {
}

IndexLoadUnit::~IndexLoadUnit() {
}

void IndexLoadUnit::tick() {
}

void IndexLoadUnit::setCacheLineSize(uint32_t _cacheLineSize) {
  this->cacheLineSize = _cacheLineSize;
}

bool IndexLoadUnit::issueLoadIndex(uint64_t _validEntryId) {
  uint64_t idxBaseAddr, idxAccessGranularity, valBaseAddr, valAccessGranularity;
  this->patternTable->getConfigInfo(_validEntryId, &idxBaseAddr, &idxAccessGranularity,
                                    &valBaseAddr, &valAccessGranularity);
//  warn("EntryId: %lu, IndexBaseAddress: %x, IndexAccessGranularity: %lu",
//      _validEntryId, idxBaseAddr, idxAccessGranularity);

  uint64_t offsetBegin, offsetEnd;
  this->patternTable->getInputInfo(_validEntryId, &offsetBegin, &offsetEnd);
//  warn("EntryId: %lu, OffsetBegin: %lu, OffsetEnd: %lu\n",
//      _validEntryId, offsetBegin, offsetEnd);
  assert(offsetBegin < offsetEnd);

  // Generate cacheline-aware memory requests
  uint64_t currentAddr = idxBaseAddr + offsetBegin * idxAccessGranularity;
  uint64_t currentSize = (offsetEnd - offsetBegin) * idxAccessGranularity;

  if (((currentAddr % this->cacheLineSize) + currentSize) > this->cacheLineSize) {
    currentSize = this->cacheLineSize - (currentAddr % this->cacheLineSize);
  }

  uint64_t cacheBlockAddr = currentAddr & (~(this->cacheLineSize - 1));

  // Send load index request
  
  // Update PatternTable
  offsetBegin += (currentSize / idxAccessGranularity);
  
  if (offsetBegin < offsetEnd) {
    this->patternTable->setInputInfo(_validEntryId, offsetBegin, offsetEnd);
  }
  else {
    this->patternTable->resetInput(_validEntryId);
  }

  // Update IndexQueueArray(num of inflight load requests)
  
  return true;
}
