#include "pattern_table.hh"

#define PATTERN_TABLE_PANIC(format, args...)                                          \
  panic("%llu-[PATTERN_TABLE%d] " format, cpuDelegator->curCycle(),                   \
        cpuDelegator->cpuId(), ##args)
#define PATTERN_TABLE_DPRINTF(format, args...)                                        \
  DPRINTF(PatternTable, "%llu-[PATTERN_TABLE%d] " format,                          \
          cpuDelegator->curCycle(), cpuDelegator->cpuId(), ##args)
#define DYN_INST_DPRINTF(format, args...)                                      \
  PATTERN_TABLE_DPRINTF("%llu %s " format, dynInfo.seqNum, dynInfo.pc, ##args)
#define DYN_INST_PANIC(format, args...)                                        \
  PATTERN_TABLE_PANIC("%llu %s " format, dynInfo.seqNum, dynInfo.pc, ##args)

PatternTable::PatternTable(uint32_t _totalPatternTableEntries) {
    this->totalPatternTableEntries = _totalPatternTableEntries;
    for (uint32_t i = 0; i < _totalPatternTableEntries; i++) {
      patternTable.emplace_back(PatternTableEntry());
    }
}

PatternTable::~PatternTable() {
  patternTable.clear();
}

uint32_t
PatternTable::size() {
  uint32_t real_size = 0;
  for (uint32_t i = 0; i < patternTable.size(); i++) {
    real_size += this->isConfigInfoValid(i);
  }
  return real_size;
}

bool
PatternTable::isConfigInfoValid(const uint64_t _entryId) {
  return patternTable[_entryId].isConfigInfoValid();
}

bool
PatternTable::isInputInfoValid(const uint64_t _entryId) {
  return patternTable[_entryId].isInputInfoValid();
}

void
PatternTable::setConfigInfo(const uint64_t _entryId,
                                 const uint64_t _idxBaseAddr, const uint64_t _idxAccessGranularity,
                                 const uint64_t _valBaseAddr, const uint64_t _valAccessGranularity) {
  this->patternTable[_entryId].setConfigInfo(_idxBaseAddr, _idxAccessGranularity,
                                             _valBaseAddr, _valAccessGranularity);
}

bool
PatternTable::getConfigInfo(const uint64_t _entryId,
                                 uint64_t* _idxBaseAddr, uint64_t* _idxAccessGranularity,
                                 uint64_t* _valBaseAddr, uint64_t* _valAccessGranularity) {
  return patternTable[_entryId].getConfigInfo(_idxBaseAddr, _idxAccessGranularity,
                                              _valBaseAddr, _valAccessGranularity);
}

bool
PatternTable::canPushInputInfo(const uint64_t _entryId) {
  return this->patternTable[_entryId].canPushInputInfo();
}

void
PatternTable::pushInputInfo(const uint64_t _entryId,
                            const uint64_t _offsetBegin, const uint64_t _offsetEnd,
                            const uint64_t _seqNum) {
  this->patternTable[_entryId].pushInputInfo(_offsetBegin, _offsetEnd, _seqNum);
}

void
PatternTable::setInputInfo(const uint64_t _entryId,
                                const uint64_t _offsetBegin, const uint64_t _offsetEnd) {
  this->patternTable[_entryId].setInputInfo(_offsetBegin, _offsetEnd);
}

bool PatternTable::getInputInfo(const uint64_t _entryId, uint64_t* _offsetBegin, uint64_t* _offsetEnd, uint64_t* _seqNum) {
  return patternTable[_entryId].getInputInfo(_offsetBegin, _offsetEnd, _seqNum);
}

void
PatternTable::commitInputInfo(const uint64_t _entryId, const uint64_t _seqNum) {
  this->patternTable[_entryId].commitInputInfo(_seqNum);
}

void PatternTable::resetConfig(uint64_t _entryId) {
  patternTable[_entryId].resetConfig();
}

void PatternTable::resetConfigUndo(uint64_t _entryId) {
  patternTable[_entryId].resetConfigUndo();
}

void PatternTable::popInput(uint64_t _entryId) {
  patternTable[_entryId].popInput();
}

void PatternTable::resetInput(uint64_t _entryId) {
  patternTable[_entryId].resetInput();
}

void PatternTable::resetInputUndo(uint64_t _entryId) {
  patternTable[_entryId].resetInputUndo();
}

size_t 
PatternTable::getInputSize(uint64_t _entryId) {
  return patternTable[_entryId].getInputSize();
}
