/*
 * Copyright (c) 2020 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 1999-2012 Mark D. Hill and David A. Wood
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mem/ruby/structures/PSPBackend.hh"

#include "base/bitfield.hh"
#include "mem/ruby/slicc_interface/RubySlicc_ComponentMapping.hh"

StreamEntry::StreamEntry(int _numQueueEntry, int _prefetchDistance)
  : numQueueEntry(_numQueueEntry), prefetchDistance(_prefetchDistance) {
  this->tagAddr = 0;
  this->tagSize = 0;
  this->accumulatedSize.resize(_numQueueEntry);
  for (int i = 0; i < _numQueueEntry; i++) {
    this->accumulatedSize[i] = 0;
  }
  this->prefetchEnabled = false;
  this->nextPrefetchAddr = 0;
  this->nextPrefetchSize = 0;
  this->headEntryIdx = 0;
  this->tailEntryIdx = 0;
  this->count = 0;
  this->prefetchEntryIdx = 0;
  this->prefetchQueue.resize(_numQueueEntry);
  for (int i = 0; i < _numQueueEntry; i++) {
    this->prefetchQueue[i].invalidate();
  }
  this->totalSize = 0;
  this->headEntryForward = 0;
}

bool
StreamEntry::hasEntry(Addr _addr) {
  for (int i = 0; i < this->numQueueEntry; i++) {
      if (this->prefetchQueue[i].isHit(_addr)) return true;
  }
  return false;
}

void 
StreamEntry::insertEntry(Addr _addr, int _size) {
  // Must have invalid entry for insertion when call this function 
  assert(this->count != this->numQueueEntry && "## No free entry\n");

  // Enable prefetch if it was disabled
  int prefetchDistanceBytes = this->prefetchDistance * RubySystem::getBlockSizeBytes();
  if (this->prefetchEnabled == false && prefetchDistanceBytes < this->totalSize + _size) {
    this->prefetchEnabled = true;
    this->prefetchEntryIdx = this->tailEntryIdx;
    this->nextPrefetchAddr = _addr + prefetchDistanceBytes - this->accumulatedSize[this->tailEntryIdx];
    this->nextPrefetchSize = (_size - prefetchDistanceBytes);
  }

  // Set inserted packet as Tag Address if not set
  DPRINTF(PSPBackend, "## Activate Entry %d, address %#x.\n", this->tailEntryIdx, _addr);

  PSPPrefetchEntry* p = &prefetchQueue[this->tailEntryIdx];
  p->setEntry(_addr, _size);
  DPRINTF(PSPBackend, "## Set Entry %d, address %#x, totalSize %d.\n", this->tailEntryIdx, _addr, this->totalSize);
  this->totalSize += _size;
  this->accumulatedSize[this->tailEntryIdx] = this->totalSize;
  this->tailEntryIdx = (this->tailEntryIdx + 1) % this->numQueueEntry;
  this->count++;
}

void
StreamEntry::popEntry(int _entryIdx) {
  PSPPrefetchEntry* queue = &this->prefetchQueue[_entryIdx];
  DPRINTF(PSPBackend, "## Invalidate Entry %#x %#x.\n", queue->getBaseAddr(), queue->getEndAddr());
  DPRINTF(PSPBackend, "##############################################################\n");
  queue->invalidate();
  
  if (this->headEntryIdx == _entryIdx) {
    this->headEntryIdx = (this->headEntryIdx + this->headEntryForward + 1) % this->numQueueEntry;
    this->count -= (this->headEntryForward + 1);
    this->headEntryForward = 0;
  }
  else {
    this->headEntryForward++;
  }
}

void
StreamEntry::incrementTagAddr(Addr _snoopAddr) {
  int hitIdx = 0;
  int incrementSize = 0;
  // Get which index is hit and increment baseAddr
  for (int i = 0; i < this->numQueueEntry; i++) {
    if (this->prefetchQueue[i].isHit(_snoopAddr)) {
      incrementSize = 
        _snoopAddr - this->prefetchQueue[i].getBaseAddr() + RubySystem::getBlockSizeBytes();
      this->prefetchQueue[i].incrementBaseAddr(incrementSize);
      hitIdx = i;
    }
  }

  for (int i = 0; i < this->numQueueEntry; i++) {
    this->accumulatedSize[i] -= incrementSize;
    if (this->prefetchQueue[i].isValid()) {
      if (_snoopAddr == this->prefetchQueue[i].getEndAddr()) {
        this->popEntry(i);
      }
    }
  }
  this->totalSize -= incrementSize;
}

void
StreamEntry::incrementNextPrefetchAddr(Addr _snoopAddr) {
  PSPPrefetchEntry* prefetchHead = &this->prefetchQueue[this->prefetchEntryIdx];
  int prefetchDistanceBytes = this->prefetchDistance * RubySystem::getBlockSizeBytes();
//  for (int i = 0; i < this->count; i++) {
//    int iterator = (this->headEntryIdx + i) % this->numQueueEntry;
//    int iteratorNext = (this->headEntryIdx + i + 1) % this->numQueueEntry;
//    if (this->accumulatedSize[iterator] < prefetchDistanceBytes &&
//        prefetchDistanceBytes <= this->accumulatedSize[iteratorNext]) {
//      this->prefetchEnabled = true;
//      this->nextPrefetchAddr = this->prefetchQueue[iteratorNext].getBaseAddr() +
//        prefetchDistanceBytes - this->accumulatedSize[iterator];
//    }
//  }
  int prefetchReach = this->accumulatedSize[this->prefetchEntryIdx] - this->nextPrefetchSize + prefetchDistanceBytes;
  if (this->totalSize < prefetchReach) { 
    this->prefetchEnabled = false;
    DPRINTF(PSPBackend, "## Disable prefetching (Reach Prefetch Distance Limit)\n");
  }
  else if (this->nextPrefetchAddr == prefetchHead->getEndAddr()) {
    int nextPrefetchEntryIdx = (this->prefetchEntryIdx + 1) % this->numQueueEntry;
    PSPPrefetchEntry* nextPrefetchHead = &this->prefetchQueue[nextPrefetchEntryIdx];
    this->prefetchEntryIdx = nextPrefetchEntryIdx;
    this->nextPrefetchAddr = nextPrefetchHead->getBaseAddr();
    this->nextPrefetchSize = nextPrefetchHead->getSize();
  }
  else {
    this->nextPrefetchAddr += RubySystem::getBlockSizeBytes();
    this->nextPrefetchSize -= RubySystem::getBlockSizeBytes();
  }
}

void
StreamEntry::printStatus() {
  if (this->count > 0) {
    DPRINTF(PSPBackend, "## HeadEntryIdx %d TailEntryIdx %d PrefetchEnabled %d PrefetchEntryIdx %d PrefetchAddr %#x PrefetchSize %d\n",
        this->headEntryIdx, this->tailEntryIdx, this->prefetchEnabled, this->prefetchEntryIdx, this->nextPrefetchAddr, this->nextPrefetchSize);
    for (int i = 0; i < this->numQueueEntry; i++) {
      int iterator = (this->headEntryIdx + i) % this->numQueueEntry;
      PSPPrefetchEntry* p = &this->prefetchQueue[iterator];
      if (p->isValid()) {
        DPRINTF(PSPBackend, "## entry %d : valid ? %d addr %#x %#x %d %d\n",
            iterator, p->isValid(), p->getBaseAddr(),
            p->getEndAddr(), p->getSize(), accumulatedSize[iterator]);
      }
    }
  }
}

PSPBackend::PSPBackend(const Params *p)
  : SimObject(p), enabled(p->enabled), tlbPrefetchOnly(p->tlb_prefetch_only),
    numStreams(p->num_streams), prefetchDistance(p->prefetch_distance) { 
  prefetchedDistance = new int[p->num_streams];
  for (int i = 0; i < p->num_streams; i++) {
    prefetchedDistance[i] = 0;
  }
  this->streamTable.resize(p->num_streams, StreamEntry(p->num_stream_entry, p->prefetch_distance));
  numPrefetchHits = 0;
  numPrefetchMisses = 0;
  numHits = 0;
  numMisses = 0;
}

void
PSPBackend::regStats() {
  SimObject::regStats();

  numPrefetchHits
    .name(name() + "numPrefetchHits");

  numPrefetchMisses
    .name(name() + "numPrefetchMisses");

  numHits
    .name(name() + "numHits");

  numMisses
    .name(name() + "numMisses");
}

StreamEntry*
PSPBackend::getEntry(Addr _addr) {
  for (auto& se : this->streamTable) {
    if (se.hasEntry(_addr)) {
      return &se;
    }
  }
  return NULL;
}

bool
PSPBackend::canInsertEntry(uint64_t _entryId) {
  assert(_entryId < this->streamTable.size());
  return this->streamTable[_entryId].canInsertEntry();
}

void
PSPBackend::insertEntry(uint64_t _entryId, Addr _pAddr, uint64_t _size) {
  this->streamTable[_entryId].insertEntry(_pAddr, _size); 
}

void
PSPBackend::issuePrefetch(StreamEntry *se, Addr _address) {
  Addr cur_addr = se->getPrefetchAddr();
  if (se->isPrefetchEnabled()) {
    mController->enqueuePrefetch(cur_addr, RubyRequestType_LD);
    DPRINTF(PSPBackend, "## Enqueue prefetch request for address %#x done.\n", cur_addr);
  }
  se->incrementNextPrefetchAddr(_address);
}

bool
PSPBackend::observeHit(Addr address) {
  StreamEntry * se = getEntry(address);
  if (se != NULL) {
    // This is for freeEntry only
    numHits++;
    DPRINTF(PSPBackend, "** Observed hit for %#x\n", address);
    se->incrementTagAddr(address);
    if (!this->tlbPrefetchOnly) {
      issuePrefetch(se, address);
      return true;
    }
    return false;
  }
  return false;
}

bool
PSPBackend::observeMiss(Addr address)
{
  StreamEntry * se = getEntry(address);
  if (se != NULL) {
    // This should have been hit, but miss because of invalidation
    numMisses++;
    DPRINTF(PSPBackend, "** Observed miss for %#x\n", address);
    se->incrementTagAddr(address);
    if (!this->tlbPrefetchOnly) {
      issuePrefetch(se, address);
    }
    return true;
  }
  return false;
}

bool
PSPBackend::observePfInCache(Addr address)
{
  StreamEntry * se = getEntry(address);
  if (se != NULL) {
    // Prefetch later than demand load or data reused.
    // Prefetch further
    numPrefetchHits++;
    DPRINTF(PSPBackend, "** Observed PSP prefetcher already in Cache or MSHR for %#x\n", address);
    se->incrementTagAddr(address);
    if (!this->tlbPrefetchOnly) {
      issuePrefetch(se, address);
      return true;
    }
  }
  //DPRINTF(PSPBackend, "** Observed OTHER prefetcher hit for %#x\n", address);
  return false;
}

bool
PSPBackend::observePfHit(Addr address)
{
  StreamEntry * se = getEntry(address);
  if (se != NULL) {
    numPrefetchHits++;
    DPRINTF(PSPBackend, "** Observed PSP prefetcher hit for %#x\n", address);
    se->incrementTagAddr(address);
    if (!this->tlbPrefetchOnly) {
      issuePrefetch(se, address);
      return true;
    }
    return false;
  }
  //DPRINTF(PSPBackend, "** Observed OTHER prefetcher hit for %#x\n", address);
  return false;
}

bool
PSPBackend::observePfMiss(Addr address)
{
  StreamEntry * se = getEntry(address);
  if (se != NULL) {
    // This is miss for cache, but hit by MSHR
    numPrefetchMisses++;
    DPRINTF(PSPBackend, "** Observed PSP prefetcher partial miss for %#x\n", address);
    se->incrementTagAddr(address);
    if (!this->tlbPrefetchOnly) {
      issuePrefetch(se, address);
      return true;
    }
    return false;
  }
  //DPRINTF(PSPBackend, "** Observed OTHER prefetcher miss for %#x\n", address);
  return false;
}

void
PSPBackend::printStatus() {
  for (int i = 0; i < this->numStreams; i++) {
    DPRINTF(PSPBackend, "## stream : %d\n", i);
    this->streamTable[i].printStatus();
  }
}

PSPBackend*
PSPBackendParams::create()
{
  return new PSPBackend(this);
}
