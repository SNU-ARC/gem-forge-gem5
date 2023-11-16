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

class PSPPrefetchEntry
{
    private:
        Addr m_addr; // Line address
        int m_index;
        int m_size;
        bool m_valid;
        bool m_use;
        int m_priority;

    public:
        void setEntry(Addr addr, int size, int priority) {
            m_addr = makeLineAddress(addr);
            m_index = 0;
            m_size = ceil((double)size / RubySystem::getBlockSizeBytes()) * RubySystem::getBlockSizeBytes();
            m_valid = true;
            m_use = false;
            m_priority = priority;
        }

        int get_size() { return m_size; }
        int get_index() { return m_index; }
        int get_priority() { return m_priority; }

        void activate() { m_use = true; }
        void deactivate() { m_use = false; }
        bool isActive() { return m_use; }
        void invalidate() { m_valid = false; }
        bool isValid() { return m_valid; }

        Addr getFirstLineAddr() { return m_addr; }
        Addr getNextLineAddr() { return m_addr + m_index; }
        Addr getLastLineAddr() { return m_addr + m_size - RubySystem::getBlockSizeBytes(); }
        void incrementLineAddr() {
            m_index = m_index + RubySystem::getBlockSizeBytes();  
        }

        bool isDone() { return m_index == m_size; }
        //bool isEntry(Addr addr) { return addr == getNextLineAddr() && m_valid; }
        bool isEntry(Addr addr) { return (addr >= m_addr) && (addr < m_addr + m_size) && m_valid; }
        void incrementPriority() { if (m_priority >= 0) m_priority--; }
};

class StreamEntry
{
    private : 
        std::vector<PSPPrefetchEntry> PSPPrefetchEntryTable;
        int activeEntryIdx;
        PSPBackend *pb;
        int num_stream_entry;
        int prefetch_distance;
        int num_requested_prefetch;

    public : 
        StreamEntry(PSPBackend *pb, int num_stream_entry, int prefetch_distance) {
            activeEntryIdx = -1;
            this->PSPPrefetchEntryTable.resize(num_stream_entry);
            for (int i = 0; i < this->num_stream_entry; i++) {
                this->PSPPrefetchEntryTable[i].invalidate(); 
                this->PSPPrefetchEntryTable[i].deactivate(); // Just in case
            }
            this->pb = pb;
            this->num_stream_entry = num_stream_entry;
            this->prefetch_distance = prefetch_distance;
            this->num_requested_prefetch = 0;
        }

        void freeEntry(Addr snoopAddr) {
            for (int i = 0; i < this->num_stream_entry; i++) {
                PSPPrefetchEntry& p = PSPPrefetchEntryTable[i];
                if (p.getLastLineAddr() == snoopAddr && p.isDone()) {
                    DPRINTF(PSPBackend, "## Invalidate Entry %d, address %#x.\n", i, p.getFirstLineAddr());
                    DPRINTF(PSPBackend, "##############################################################\n");
                    p.deactivate(); // Just in case
                    p.invalidate();
                    return;
                }
            }
        }

        void printStatus()
        {
            for (int i = 0; i < this->num_stream_entry; i++) {
                PSPPrefetchEntry& p = PSPPrefetchEntryTable[i];
                if (p.isValid()) {
                    DPRINTF(PSPBackend, "## entry %d : %d / %d active ? %d addr %#x %#x priority %d\n", i, p.get_index(), p.get_size(), p.isActive(), p.getFirstLineAddr(), p.getLastLineAddr(), p.get_priority());
                }
            }
        }

        int entryToActivate() {
            int entryId = -1;
            for (int i = 0; i < this->num_stream_entry; i++) {
                PSPPrefetchEntry& p = PSPPrefetchEntryTable[i];
                p.incrementPriority();
                if (p.isValid() && !p.isActive() && !p.isDone() && p.get_priority() == 0) {
                    entryId = i;
                }
            }
            return entryId;
        }

        int entryToValidate() {
            for (int i = 0; i < this->num_stream_entry; i++) {
                PSPPrefetchEntry& p = PSPPrefetchEntryTable[i];
                if (!p.isValid()) {
                    return i;
                }
            }
            return -1;
        }

        int getPriority() {
            int priority = 0;
            for (int i = 0; i < this->num_stream_entry; i++) {
                PSPPrefetchEntry& p = PSPPrefetchEntryTable[i];
                if (p.isValid()) {
                    priority = priority > p.get_priority() ? priority : p.get_priority() + 1;
                }
            }
            return priority;
        }

        void incrementLineAddr() {
            assert(activeEntryIdx != -1);

            PSPPrefetchEntry *pe = &PSPPrefetchEntryTable[activeEntryIdx];
            pe->incrementLineAddr();
            num_requested_prefetch++;

            //DPRINTF(PSPBackend, "@@ %d %d %d\n", pe->get_index(), pe->get_size(), pe->isDone());

            if (pe->isDone()) {
                pe->deactivate();
                DPRINTF(PSPBackend, "## Deactivate Entry %d, address %#x\n", activeEntryIdx, pe->getFirstLineAddr());
                activeEntryIdx = -1;
                int toActivate = entryToActivate();
                if (toActivate != -1) {
                    activeEntryIdx = toActivate;
                    pe = &PSPPrefetchEntryTable[activeEntryIdx];
                    pe->activate();
                    DPRINTF(PSPBackend, "## Activate Entry %d, address %#x.\n", activeEntryIdx, pe->getFirstLineAddr());
                }
            }
        }

        void prefetchRequestCompleted() {
            num_requested_prefetch--;
        }

        int numToPrefetch() {
            return std::max(prefetch_distance - num_requested_prefetch, 0);
        }

        bool existNextLineAddr() {
            return activeEntryIdx != -1;
        }

        Addr getNextLineAddr() {
            PSPPrefetchEntry *pe = &PSPPrefetchEntryTable[activeEntryIdx];
            return pe->getNextLineAddr();
        }

        void insertEntry(Addr addr, int size);

        bool hasEntry(Addr addr) {
            for (auto& pe : PSPPrefetchEntryTable) {
                if (pe.isEntry(addr))
                    return true;
            }
            return false;
        }
};

class PSPBackend : public SimObject
{
    public:
        typedef PSPBackendParams Params;
        PSPBackend(const Params *p);

        void issuePrefetch(StreamEntry *se);
        bool isEnabled() const { return this->enabled; }
        bool observePfHit(Addr address);
        bool observePfMiss(Addr address);
        bool observeHit(Addr address);
        bool observeMiss(Addr address);

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
          assert(entryId < streamTable.size());
          return streamTable[entryId].entryToValidate() != -1;
        }
        void printStatus() {
            for (int i = 0; i < num_streams; i++) {
                DPRINTF(PSPBackend, "## stream : %d\n", i);
                streamTable[i].printStatus();
            }
        }
        

    private:
        bool enabled;
        int num_streams;
        int prefetch_distance;
        PSPFrontend *pf;
        AbstractController *m_controller;
        std::vector<StreamEntry> streamTable;

        Stats::Scalar numPrefetchHits;
        Stats::Scalar numPrefetchMisses;
        Stats::Scalar numHits;
        Stats::Scalar numMisses;
};

#endif // __MEM_RUBY_STRUCTURES_PSPBackend_HH__
