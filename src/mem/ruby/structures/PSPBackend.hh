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
#include "mem/ruby/system/Sequencer.hh"

#include <math.h>

#include <vector>

class PSPPrefetchEntry
{
    private:
        Addr m_addr; // Line address
        int m_size;
        bool m_valid;
        bool m_use;
        int m_priority;
        
        std::vector<bool> issued;
        std::vector<bool> received;

    public:
        void setEntry(Addr addr, int size, int priority) {
            m_addr = makeLineAddress(addr);
            m_size = ceil((double)size / RubySystem::getBlockSizeBytes());
            m_valid = true;
            m_use = false;
            m_priority = priority;

            issued.clear();
            received.clear();
            issued.resize(m_size, false);
            received.resize(m_size, false);
        }

        void activate() { m_use = true; }
        void deactivate() { m_use = false; }
        bool isActive() { return m_use; }
        void invalidate() { m_valid = false; }
        bool isValid() { return m_valid; }
        int get_priority() { return m_priority; }
        Addr getFirstLineAddr() { return m_addr; }

        int numToIssue() {
            int count = 0;
            for (int i = 0; i < m_size; i++) {
                if (!issued[i]) {
                    count++;
                }
            }
            return count;
        }

        int numInflightRequest() {
            std::stringstream ss;
            std::string print;
            int count = 0;
            for (int i = 0; i < m_size; i++) {
                if (issued[i] && !received[i]) {
                    ss.str("");
                    ss << std::hex << m_addr + i * RubySystem::getBlockSizeBytes();
                    print = print + std::string(" ") + ss.str();
                    count++;
                }
            }
            DPRINTF(PSPBackend, "Inflight addr : %s, total : %d\n", print, count);
            return count;
        }

        Addr issueRequest() {
            for (int i = 0; i < m_size; i++) {
                if (!issued[i]) {
                    issued[i] = true;
                    return m_addr + i * RubySystem::getBlockSizeBytes();
                }
            }
            assert(false);
        }

        bool viewRequest(Addr addr) {
            for (int i = 0; i < m_size; i++) {
                if (m_addr + i * RubySystem::getBlockSizeBytes() == addr) {
                    if (issued[i] && !received[i]) {
                        received[i] = true;
                        return true;
                    }
                    return false;
                }
            }
            assert(false);
        }

        bool doneIssue() {
            for (int i = 0; i < m_size; i++) {
                if (!issued[i]) {
                    return false;
                }
            }
            return true;
        }

        bool donePrefetch() {
            for (int i = 0; i < m_size; i++) {
                if (!received[i]) {
                    return false;
                }
            }
            return true;
        }

        bool isEntry(Addr addr) {
            return (addr >= m_addr) && (addr < m_addr + m_size * RubySystem::getBlockSizeBytes()) && m_valid; 
        }

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
        int stream_num;

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
        }

        void setStreamNum(int stream_num) {
            this->stream_num = stream_num;
        }

        int numInflightRequest() {
            int total = 0;
            for (int i = 0; i < this->num_stream_entry; i++) {
                if (this->PSPPrefetchEntryTable[i].isValid()) {
                    total = total + this->PSPPrefetchEntryTable[i].numInflightRequest();
                }
            }
            DPRINTF(PSPBackend, "Stream %d, total inflight request %d\n", stream_num, total);
            return total;
        }

        std::string numValidEntry() {
            int total = 0;
            for (int i = 0; i < this->num_stream_entry; i++) {
                if (this->PSPPrefetchEntryTable[i].isValid()) {
                    total++;
                }
            }
            return std::string(", Stream ") + std::to_string(stream_num) + std::string(" : ") + std::to_string(total);
        }

        int entryToActivate() {
            int entryId = -1;
            for (int i = 0; i < this->num_stream_entry; i++) {
                PSPPrefetchEntry& p = PSPPrefetchEntryTable[i];
                p.incrementPriority();
                if (p.isValid() && !p.isActive() && !p.doneIssue() && p.get_priority() == 0) {
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

        bool viewRequest(Addr snoopAddr) {
            for (int i = 0; i < this->num_stream_entry; i++) {
                PSPPrefetchEntry& p = PSPPrefetchEntryTable[i];
                if (p.isEntry(snoopAddr)) {
                    bool ret = p.viewRequest(snoopAddr);
                    if (p.doneIssue() && p.donePrefetch()) {
                        DPRINTF(PSPBackend, "## Stream : %d, Invalidate Entry %d, address %#x.\n", stream_num, i, p.getFirstLineAddr());
                        //DPRINTF(PSPBackend, "##############################################################\n");
                        p.deactivate(); // Just in case
                        p.invalidate();
                        
                    }
                    return ret;
                }
            }
        }

        int numToPrefetch() {
            if (activeEntryIdx == -1) {
                return 0;
            }
            PSPPrefetchEntry *pe = &PSPPrefetchEntryTable[activeEntryIdx];
            int n = std::min(prefetch_distance - this->numInflightRequest(), pe->numToIssue());
            assert(n >= 0);
            return n;
        }

        void insertEntry(Addr addr, int size);
        Addr issueRequest();

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

        void numValidEntry() {
            std::string print;
            for (int i = 0; i < this->num_streams; i++) {
                print += streamTable[i].numValidEntry();
            }
            DPRINTF(PSPBackend, "Valid entries %s\n", print);
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
