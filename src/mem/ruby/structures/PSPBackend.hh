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

#ifndef __MEM_RUBY_STRUCTURES_PSPBackend_HH__
#define __MEM_RUBY_STRUCTURES_PSPBackend_HH__

#include "base/statistics.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/network/MessageBuffer.hh"
#include "mem/ruby/slicc_interface/AbstractController.hh"
#include "mem/ruby/slicc_interface/RubyRequest.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "params/PSPBackend.hh"
#include "sim/sim_object.hh"
#include "sim/system.hh"
#include "cpu/gem_forge/accelerator/psp_frontend/psp_frontend.hh"
#include "mem/ruby/protocol/RubyRequestType.hh"
#include "mem/ruby/structures/RubyPrefetcher.hh"
#include "debug/PSPBackend.hh"

#include <math.h>

class PSPPrefetchEntry {
  private:
    bool valid;
    Addr baseAddr; // Line address
    Addr endAddr; // Line address
    int size;

  public:
    void setEntry(Addr _baseAddr, int _size) {
      this->valid = true;
      this->baseAddr = makeLineAddress(_baseAddr);
      this->endAddr = makeLineAddress(_baseAddr) + _size;
      this->size = _size;
    }

    bool isValid() { return this->valid; }
    void invalidate() { this->valid = false; }
    Addr getBaseAddr() { return this->baseAddr; }
    void incrementBaseAddr(int _size) { 
      this->baseAddr += _size;
      this->size -= _size;
    }
    Addr getEndAddr() { return this->endAddr; }
    int getSize() { return this->size; }
    bool isHit(Addr _addr) {
      return (this->valid && (_addr >= this->baseAddr) && (_addr < this->endAddr));
    }
};

class StreamEntry {
  private : 
    std::vector<PSPPrefetchEntry> prefetchQueue;
    // Tag Information
    Addr tagAddr;
    int tagSize;
    std::vector<int> accumulatedSize;

    // Prefetch Information
    bool prefetchEnabled;
    Addr nextPrefetchAddr;
    int nextPrefetchSize;
    int incrementSize;
    int incrementAccumSize;

    // Prefetch Queue Managing
    int headEntryIdx;
    int headEntryForward;
    int tailEntryIdx;
    int count;
    int prefetchEntryIdx;
    int totalSize;

    // Parameters
    int numQueueEntry;
    int prefetchDistance;

  public : 
    StreamEntry(int _numQueueEntry, int _prefetchDistance);

    bool isValid() { return this->prefetchQueue[headEntryIdx].isValid(); }
    bool hasEntry(Addr _addr);
    bool canInsertEntry() { return this->count < this->numQueueEntry; }
    void insertEntry(Addr _addr, int _size);
    void popEntry(int _entryIdx);
    bool isPrefetchEnabled() { return this->prefetchEnabled; }
    Addr getPrefetchAddr() { return this->nextPrefetchAddr; }
    void incrementTagAddr(Addr _snoopAddr);
    void incrementNextPrefetchAddr(Addr _snoopAddr);
    void printStatus();
    int getTotalSize();
};

class PSPBackend : public SimObject {
  private:
    bool enabled;
    bool tlbPrefetchOnly;
    bool dataPrefetchOnly;
    int numStreams;
    int prefetchDistance;
    AbstractController *mController;
    std::vector<StreamEntry> streamTable;

    Stats::Scalar numPrefetchHits;
    Stats::Scalar numPrefetchMisses;
    Stats::Scalar numHits;
    Stats::Scalar numMisses;

  public:
    typedef PSPBackendParams Params;
    PSPBackend(const Params *p);
    void regStats();
    void setController(AbstractController *_ctrl) { mController = _ctrl; }

    StreamEntry* getEntry(Addr _addr);
    bool canInsertEntry(uint64_t _entryId);
    void insertEntry(uint64_t _entryId, uint64_t _pAddr, uint64_t _size);

    // For UVE-proxy
    bool isDataPrefetchOnly() { return this->dataPrefetchOnly; }

    // For Ruby State Machine
    bool isEnabled() const { return this->enabled; }
    void issuePrefetch(StreamEntry *se, Addr address);
    bool observePfInCache(Addr address);
    bool observePfHit(Addr address);
    bool observePfMiss(Addr address);
    bool observeHit(Addr address);
    bool observeMiss(Addr address);
    void popResponse(Addr address);

    void printStatus();
    int getTotalSize(uint64_t _entryId);
};

#endif // __MEM_RUBY_STRUCTURES_PSPBackend_HH__
