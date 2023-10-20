#ifndef __CPU_GEM_FORGE_ACCELERATOR_PATTERN_TABLE_HH__
#define __CPU_GEM_FROGE_ACCELERATOR_PATTERN_TABLE_HH__

#include <iostream>
#include <unordered_map>

struct TableConfigEntry {
  bool valid;
  uint64_t baseAddr;
  uint64_t accessGranularity;
 
  TableConfigEntry() : valid(false), baseAddr(0), accessGranularity(0) {}

//  void configBaseAddr(const uint64_t _baseAddr) {
//    this->numConfigured++;
//    this->baseAddr = _baseAddr;
//    if (this->numConfigured == 2)
//      this->valid = true;
//  }
//  void configAccessGran(const uint64_t _accessGranularity) {
//    this->numConfigured++;
//    this->accessGranularity = _accessGranularity;
//    if (this->numConfigured == 2)
//      this->valid = true;
//  }
  bool getConfigInfo(uint64_t _baseAddr, uint64_t _accessGranularity) {
    _baseAddr = this->baseAddr;
    _accessGranularity = this->accessGranularity;
    return this->valid;
  }
  void reset() {
    this->valid = false;
  }
};

struct TableInputEntry {
  bool valid;
  uint64_t offsetBegin;
  uint64_t offsetEnd;

  TableInputEntry() : valid(false), offsetBegin(0), offsetEnd(0) {}

//  void inputOffsetBegin(const uint64_t _inputOffset) {
//    this->numInput++;
//    this->offsetBegin = _inputOffset;
//    if (this->numInput == 2)
//      this->valid = true;
//  }
//  void inputOffsetEnd(const uint64_t _inputOffset) {
//    this->numInput++;
//    this->offsetEnd = _inputOffset;
//    if (this->numInput == 2)
//      this->valid = true;
//  }
  bool getInputInfo(uint64_t _offsetBegin, uint64_t _offsetEnd) {
    _offsetBegin = this->offsetBegin;
    _offsetEnd = this->offsetEnd;
    return this->valid;
  }
  void reset() {
    this->valid = false;
  }
};

struct PatternTableEntry {
  TableConfigEntry idxTableConfigEntry;
  TableConfigEntry valTableConfigEntry;
  TableInputEntry idxTableInputEntry;

  PatternTableEntry() : idxTableConfigEntry(), valTableConfigEntry(), idxTableInputEntry() {}

//  void configBaseAddr(const uint64_t _baseAddr, bool isIdx) {
//    if (isIdx) 
//      this->idxTableConfigEntry.configBaseAddr(_baseAddr);
//    else 
//      this->valTableConfigEntry.configBaseAddr(_baseAddr);
//  }
//  void configAccessGran(const uint64_t _accessGranularity, bool isIdx) {
//    if (isIdx) 
//      this->idxTableConfigEntry.configAccessGran(_accessGranularity);
//    else 
//      this->valTableConfigEntry.configAccessGran(_accessGranularity);
//  }
//  void InputOffset(const uint64_t _inputOffset, bool isBegin) {
//    if (isBegin)
//      this->idxTableInputEntry.inputOffsetBegin(_inputOffset);
//    else
//      this->idxTableInputEntry.inputOffsetEnd(_inputOffset);
//  }
  bool getConfigInfo(uint64_t _idxBaseAddr, uint64_t _idxAccessGranularity, uint64_t _valBaseAddr, uint64_t _valAccessGranularity) {
    bool idxValid = idxTableConfigEntry.getConfigInfo(_idxBaseAddr, _idxAccessGranularity);
    bool valValid = valTableConfigEntry.getConfigInfo(_valBaseAddr, _valAccessGranularity);
    return idxValid && valValid;
  }
  bool getInputInfo(uint64_t _offsetBegin, uint64_t _offsetEnd) {
    bool inputValid = idxTableInputEntry.getInputInfo(_offsetBegin, _offsetEnd);
    return inputValid;
  }
  void reset() {
    this->idxTableConfigEntry.reset();
    this->valTableConfigEntry.reset();
  }
};

class PatternTable {
  public:
    PatternTable(uint32_t _totalPatternTableEntries);
    ~PatternTable();

//    void configBaseAddr(uint32_t streamId, uint64_t baseAddr, bool isIdx);
//    void configAccessGran(uint32_t streamId, uint64_t accessGranularity, bool isIdx);
//    void inputOffset(uint32_t streamId, uint64_t inputOffset, bool isBegin);
    bool getConfigInfo(const uint32_t _streamId, uint64_t _idxBaseAddr, uint64_t _idxAccessGranularity, uint64_t _valBaseAddr, uint64_t _valAccessGranularity);
    bool getInputInfo(const uint32_t _streamId, uint64_t _offsetBegin, uint64_t _offsetEnd);
    void reset(uint32_t streamId);

  private:
    uint32_t totalPatternTableEntries;
    PatternTableEntry* patternTable;
};

#endif
