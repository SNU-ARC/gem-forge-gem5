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

uint32_t PatternTable::size() {
  return patternTable.size();
}

bool PatternTable::isConfigInfoValid(const uint64_t _entryId) {
  return patternTable[_entryId].isConfigInfoValid();
}
bool PatternTable::isInputInfoValid(const uint64_t _entryId) {
  return patternTable[_entryId].isInputInfoValid();
}
void PatternTable::setConfigInfo(const uint64_t _entryId,
                                 const uint64_t _idxBaseAddr, const uint64_t _idxAccessGranularity,
                                 const uint64_t _valBaseAddr, const uint64_t _valAccessGranularity) {
  this->patternTable[_entryId].setConfigInfo(_idxBaseAddr, _idxAccessGranularity,
                                             _valBaseAddr, _valAccessGranularity);
}
bool PatternTable::getConfigInfo(const uint64_t _entryId,
                                 uint64_t _idxBaseAddr, uint64_t _idxAccessGranularity,
                                 uint64_t _valBaseAddr, uint64_t _valAccessGranularity) {
  return patternTable[_entryId].getConfigInfo(_idxBaseAddr, _idxAccessGranularity,
                                              _valBaseAddr, _valAccessGranularity);
}
void PatternTable::setInputInfo(const uint64_t _entryId,
                                const uint64_t _offsetBegin, const uint64_t _offsetEnd) {
  this->patternTable[_entryId].setInputInfo(_offsetBegin, _offsetEnd);
}
bool PatternTable::getInputInfo(const uint64_t _entryId, uint64_t _offsetBegin, uint64_t _offsetEnd) {
  return patternTable[_entryId].getInputInfo(_offsetBegin, _offsetEnd);
}
void PatternTable::reset(uint64_t _entryId) {
  patternTable[_entryId].reset();
}
