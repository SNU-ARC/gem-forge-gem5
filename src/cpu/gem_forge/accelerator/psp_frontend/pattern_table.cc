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
    patternTable = new PatternTableEntry[this->totalPatternTableEntries];
}

PatternTable::~PatternTable() {
  delete[] patternTable;
}

//void PatternTable::configBaseAddr(uint32_t entryId, uint64_t baseAddr, bool isIdx) {
//  patternTable[entryId]->configBaseAddr(baseAddr, isIdx);
//}
//void PatternTable::configAccessGran(uint32_t entryId, uint64_t accessGranularity, bool isIdx) {
//  patternTable[entryId]->configAccessGran(accessGranularity, isIdx);
//}
//void PatternTable::inputOffset(uint32_t entryId, uint64_t inputOffset, bool isIdx) {
//  patternTable[entryId]->inputOffset(inputOffset, isIdx);
//}
bool PatternTable::isValid(const uint32_t _entryId, const bool _isInput) {
  return patternTable[_entryId].isValid(_isInput);
}
bool PatternTable::getConfigInfo(const uint32_t _entryId, uint64_t _idxBaseAddr, uint64_t _idxAccessGranularity, uint64_t _valBaseAddr, uint64_t _valAccessGranularity) {
  return patternTable[_entryId].getConfigInfo(_idxBaseAddr, _idxAccessGranularity, _valBaseAddr, _valAccessGranularity);
}
bool PatternTable::getInputInfo(const uint32_t _entryId, uint64_t _offsetBegin, uint64_t _offsetEnd) {
  return patternTable[_entryId].getInputInfo(_offsetBegin, _offsetEnd);
}
void PatternTable::reset(uint32_t _entryId) {
  patternTable[_entryId].reset();
}
