#ifndef __CPU_GEM_FORGE_ACCELERATOR_PATTERN_TABLE_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_PATTERN_TABLE_HH__

#include "base/statistics.hh"

#include <vector>
#include <list>
#include <iostream>

struct TableConfigEntry {
  bool valid;
  uint64_t idxBaseAddr;
  uint64_t idxAccessGranularity;
  uint64_t valBaseAddr;
  uint64_t valAccessGranularity;
  uint64_t valCurrentSize;
 
  TableConfigEntry() 
    : valid(false), idxBaseAddr(0), idxAccessGranularity(0),
      valBaseAddr(0), valAccessGranularity(0) {}

  bool isValid() { return this->valid; }
  void setConfigInfo(const uint64_t _idxBaseAddr, const uint64_t _idxAccessGranularity,
                     const uint64_t _valBaseAddr, const uint64_t _valAccessGranularity) {
    this->valid = true;
    this->idxBaseAddr = _idxBaseAddr;
    this->idxAccessGranularity = _idxAccessGranularity;
    this->valBaseAddr = _valBaseAddr;
    this->valAccessGranularity = _valAccessGranularity;
  }
  bool getConfigInfo(uint64_t* _idxBaseAddr, uint64_t* _idxAccessGranularity,
                     uint64_t* _valBaseAddr, uint64_t* _valAccessGranularity) {
    *_idxBaseAddr = this->idxBaseAddr;
    *_idxAccessGranularity = this->idxAccessGranularity;
    *_valBaseAddr = this->valBaseAddr;
    *_valAccessGranularity = this->valAccessGranularity;
    return this->valid;
  }
  void reset() {
    this->valid = false;
  }
  void resetUndo() {
    this->valid = true;
  }
};

struct TableInputEntry {
  bool valid;
  uint64_t offsetBegin;
  uint64_t offsetEnd;
  uint64_t seqNum;

  TableInputEntry(bool _valid = false, uint64_t _offsetBegin = 0, uint64_t _offsetEnd = 0,
                  uint64_t _seqNum = 0)
    : valid(_valid), offsetBegin(_offsetBegin), offsetEnd(_offsetEnd), seqNum(_seqNum) {}

  bool isValid() { return this->valid; }
  void setInputInfo(const uint64_t _offsetBegin, const uint64_t _offsetEnd) {
    this->valid = true;
    this->offsetBegin = _offsetBegin;
    this->offsetEnd = _offsetEnd;
  }
  bool getInputInfo(uint64_t* _offsetBegin, uint64_t* _offsetEnd, uint64_t* _seqNum) {
    *_offsetBegin = this->offsetBegin;
    *_offsetEnd = this->offsetEnd;
    *_seqNum = this->seqNum;
    return this->valid;
  }
  void reset() {
    this->valid = false;
  }
  void resetUndo() {
    this->valid = true;
  }
};

struct PatternTableEntry {
  TableConfigEntry tableConfigEntry;
  std::list<TableInputEntry> tableInputEntry;

  PatternTableEntry() : tableConfigEntry(), tableInputEntry() {}

  bool isConfigInfoValid() {
    return this->tableConfigEntry.isValid();
  }
  bool isInputInfoValid() {
    return this->tableInputEntry.front().isValid() && this->tableInputEntry.size() > 0;
  }
  void setConfigInfo(const uint64_t _idxBaseAddr, const uint64_t _idxAccessGranularity,
                     const uint64_t _valBaseAddr, const uint64_t _valAccessGranularity) {
    this->tableConfigEntry.setConfigInfo(_idxBaseAddr, _idxAccessGranularity,
                                         _valBaseAddr, _valAccessGranularity);
  }
  bool getConfigInfo(uint64_t* _idxBaseAddr, uint64_t* _idxAccessGranularity, 
                     uint64_t* _valBaseAddr, uint64_t* _valAccessGranularity) {
    return tableConfigEntry.getConfigInfo(_idxBaseAddr, _idxAccessGranularity,
                                          _valBaseAddr, _valAccessGranularity);
  }
  bool canPushInputInfo() {
    return this->tableInputEntry.size() <= 2;
  }
  void pushInputInfo(const uint64_t _offsetBegin, const uint64_t _offsetEnd, const uint64_t _seqNum) {
    this->tableInputEntry.emplace_back(true, _offsetBegin, _offsetEnd, _seqNum);
  }
  void setInputInfo(const uint64_t _offsetBegin, uint64_t _offsetEnd) {
    this->tableInputEntry.front().setInputInfo(_offsetBegin, _offsetEnd);
  }
  bool getInputInfo(uint64_t* _offsetBegin, uint64_t* _offsetEnd, uint64_t* _seqNum) {
    return tableInputEntry.front().getInputInfo(_offsetBegin, _offsetEnd, _seqNum);
  }
  void resetConfig() {
    this->tableConfigEntry.reset();
  }
  void resetConfigUndo() {
    this->tableConfigEntry.resetUndo();
  }
  void popInput() {
    this->tableInputEntry.pop_front();
  }
  void resetInput() {
    this->tableInputEntry.pop_back();
  }
  void resetInputUndo() {
    this->tableInputEntry.front().resetUndo();
  }
  void resetAll() {
    this->tableConfigEntry.reset();
    this->tableInputEntry.front().reset();
  }
  void resetUndoAll() {
    this->tableConfigEntry.resetUndo();
    this->tableInputEntry.front().resetUndo();
  }
  size_t getInputSize() {
    return this->tableInputEntry.size();
  }
};

class PatternTable {
  public:
    PatternTable(uint32_t _totalPatternTableEntries);
    ~PatternTable();

    uint32_t size();
    bool isConfigInfoValid(const uint64_t _entryId);
    bool isInputInfoValid(const uint64_t _entryId);
    void setConfigInfo(const uint64_t _entryId, 
                       const uint64_t _idxBaseAddr, const uint64_t _idxAccessGranularity,
                       const uint64_t _valBaseAddr, const uint64_t _valAccessGranularity);
    bool getConfigInfo(const uint64_t _entryId, 
                       uint64_t* _idxBaseAddr, uint64_t* _idxAccessGranularity, 
                       uint64_t* _valBaseAddr, uint64_t* _valAccessGranularity);
    bool canPushInputInfo(const uint64_t _entryId);
    void pushInputInfo(const uint64_t _entryId, 
                      const uint64_t _offsetBegin, const uint64_t _offsetEnd,
                      const uint64_t _seqNum);
    void setInputInfo(const uint64_t _entryId, 
                      const uint64_t _offsetBegin, const uint64_t _offsetEnd);
    bool getInputInfo(const uint64_t _entryId, uint64_t* _offsetBegin, uint64_t* _offsetEnd, uint64_t* _seqNum);
    void resetConfig(uint64_t entryId);
    void resetConfigUndo(uint64_t entryId);
    void popInput(uint64_t entryId);
    void resetInput(uint64_t entryId);
    void resetInputUndo(uint64_t entryId);
    size_t getInputSize(uint64_t entryId);

  private:
    uint32_t totalPatternTableEntries;
    std::vector<PatternTableEntry> patternTable;
};

#endif
