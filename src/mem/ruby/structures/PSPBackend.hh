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

#include <math.h>

class PSPPrefetchEntry
{
    private:
        Addr m_addr; // Line address
        int m_index;
        int m_size;
        bool m_valid;
        bool m_use;

    public:
        void setEntry(Addr addr, int size) {
            m_addr = makeLineAddress(addr);
            m_index = 0;
            m_size = ceil((double)size / RubySystem::getBlockSizeBytes());
            m_valid = true;
            m_use = false;
        }

        void activate() { m_use = true; }
        void deactivate() { m_use = false; }
        bool isActive() { return m_use; }
        void invalidate() { m_valid = false; }
        bool isValid() { return m_valid; }
        
        Addr getNextLineAddr() { return m_addr + m_index; }
        Addr getLastLineAddr() { return m_addr + m_size - 1; }
        void incrementLineAddr() { m_addr = m_addr + 1; }

        bool isDone() { return m_index == m_size; }
        bool isEntry(Addr addr) { return addr >= m_addr && addr < m_addr + m_size; }    
};

class StreamEntry
{
    private : 
        std::vector<PSPPrefetchEntry> PSPPrefetchEntryTable;
        int activeEntryIdx;
        bool bulkPrefetch;

    public : 
        StreamEntry() {
            activeEntryIdx = -1;
            bulkPrefetch = false;
            // TODO: Modify codes below
            this->PSPPrefetchEntryTable.emplace_back(); 
            this->PSPPrefetchEntryTable.emplace_back(), 
            this->PSPPrefetchEntryTable.at(0).invalidate(); 
            this->PSPPrefetchEntryTable.at(1).invalidate(); 
        }

        Addr getNextLineAddr(Addr snoopAddr) {            
            assert(activeEntryIdx != -1);

            PSPPrefetchEntry *pe = &PSPPrefetchEntryTable[activeEntryIdx];
            if (pe->isDone()) {
                pe->deactivate();
                activeEntryIdx = 1 - activeEntryIdx;
                pe = &PSPPrefetchEntryTable[activeEntryIdx];
                pe->activate();
                bulkPrefetch = true;
            }
            if (PSPPrefetchEntryTable[1 - activeEntryIdx].getLastLineAddr() == snoopAddr) {
                PSPPrefetchEntryTable[1 - activeEntryIdx].invalidate();
            }
            Addr addr = pe->getNextLineAddr();
            pe->incrementLineAddr();
            return addr;
        }

        bool hasInvalidEntry() {
            PSPPrefetchEntry& p1 = PSPPrefetchEntryTable[0];
            PSPPrefetchEntry& p2 = PSPPrefetchEntryTable[1];

            return !p1.isValid() || !p2.isValid();
        }

        void insertEntry(Addr addr, int size) {
            PSPPrefetchEntry& p1 = PSPPrefetchEntryTable[0];
            PSPPrefetchEntry& p2 = PSPPrefetchEntryTable[1];

            bool p1_valid = p1.isValid();
            bool p2_valid = p2.isValid();

            assert(!p1_valid || !p2_valid);

            if (p1_valid) {
                p2.setEntry(addr, size);
                if (activeEntryIdx == -1) { // When this is first entry inserting
                    p2.activate();
                    activeEntryIdx = 1;
                    bulkPrefetch = true;
                }
            } else {
                p1.setEntry(addr, size);
                if (activeEntryIdx == -1) {
                    p1.activate();
                    activeEntryIdx = 0;
                    bulkPrefetch = true;
                }
            }
        }

        bool isBulk() {
            return bulkPrefetch;
        }

        void turnOffBulk() {
            bulkPrefetch = false;
        }

        bool hasEntry(Addr addr) {
            for (auto& pe : PSPPrefetchEntryTable) {
                if (pe.isEntry(addr))
                    return true;
            }
            return false;
        }

        PSPPrefetchEntry* getEntry(Addr addr) {
            for (auto& pe : PSPPrefetchEntryTable) {
                if (pe.isEntry(addr)) {
                    return &pe;
                }
            }
            return NULL;
        }
};

class PSPBackend : public SimObject
{
    public:
        typedef PSPBackendParams Params;
        PSPBackend(const Params *p);

        void issuePrefetch(Addr address);

        void observePfHit(Addr address);
        void observePfMiss(Addr address);
        void observeHit(Addr address, const RubyRequestType& type);
        void observeMiss(Addr address, const RubyRequestType& type);

        StreamEntry* getEntry(Addr addr);

        void regStats();
        void setController(AbstractController *_ctrl)
        { m_controller = _ctrl; }
        void setPSPFrontend(PSPFrontend *pf)
        { this->pf = pf; }
        void insertEntry(uint64_t entryId, uint64_t pAddr, uint64_t size) {
            streamTable[entryId].insertEntry(pAddr, size); 
        }
        bool canInsertEntry(uint64_t entryId) {
          return streamTable[entryId].hasInvalidEntry();
        }
        
    private:
        int num_streams;
        int prefetch_distance;
        PSPFrontend *pf;
        AbstractController *m_controller;
        std::vector<StreamEntry> streamTable;

        Stats::Scalar numPrefetchHits;
        Stats::Scalar numPrefetchMisses;
        Stats::Scalar numTotalMemoryAccesses;
        Stats::Scalar numInPrefetchDistance;
        Stats::Scalar numNotInPrefetchDistance;
};

#endif // __MEM_RUBY_STRUCTURES_PSPBackend_HH__
