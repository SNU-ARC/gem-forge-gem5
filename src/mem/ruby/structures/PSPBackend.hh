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

#include <math.h>

/*
class PrefetchEntry
{
    private:
        // Base physical address from stream prefetching
        Addr m_addr;

        // Line index to be prefetched
        int m_index;

        // Size of stream prefetching
        int m_size;

        // Valid bit for each stream
        bool m_valid;

        // Execute bit for each stream
        bool m_execute;

        RubyRequestType m_type;

    public:
        void setPrefetchEntry(Addr addr, int index, int size, RubyRequestType type) {
            m_addr = makeLineAddress(addr);
            m_index = index;
            m_size = ceil((double)size / RubySystem::getBlockSizeBytes());
            m_type = type;
            m_is_valid = false;
            m_is_execute = false;
        }

        bool isLastElement(Addr addr) {
            return addr == m_addr + m_size - 1;
        }

        bool inRange(Addr addr) {
            return addr >= m_addr && addr < m_addr + m_size;
        }

        void invalidate() {
            m_valid = false;
        }

        void reverseExecute() {
            m_execute = !m_execute;
        }

        void setExecute() {
            m_execute = true;
        }

        bool isExecute() {
            return m_execute;
        }

        Addr getNextAddr() {
            return m_addr + m_index;
        }

        RubyRequestType getMemRequestType() {
            return m_type;
        }
};

class StreamEntry
{
    private : 
        std::vector<PrefetchEntry>* prefetchEntryTable;

    public : 
        StreamEntry() {
            prefetchEntryTable = new std::vector<PrefetchEntry>(2);
        }

        ~StreamEntry() {
            delete prefetchEntryTable;
        }

        PrefetchEntry* changeStream() {
            PrefetchEntry& p1 = prefetchEntryTable[0];
            PrefetchEntry& p2 = prefetchEntryTable[1];

            assert(p1.isExecute() || p2.isExecute());

            p1.reverseExecute();
            p2.reverseExecute();

            if (p1.isExecute()) {
                return &p1;
            } else {
                return &p2;
            }
        }

        bool hasEntry(Addr addr) {
            for (auto& pe : prefetchEntryTable) {
                if (pe.inRange(addr))
                    return true;
            }
            return false;
        }

        PrefetchEntry* getEntry(Addr addr) {
            for (auto& pe : prefetchEntryTable) {
                if (pe.inRange(addr)) {
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
        ~PSPBackend();

        void issueNextPrefetch(Addr address, PrefetchEntry *stream);

        void observePfHit(Addr address);
        void observePfMiss(Addr address);
        void observeHit(Addr address, const RubyRequestType& type);
        void observeMiss(Addr address, const RubyRequestType& type);

        void print(std::ostream& out) const;
        void setController(AbstractController *_ctrl)
        { m_controller = _ctrl; }
        void regStats();

    private:
        std::vector<StreamEntry>* streamTable;
        int num_streams;
        int prefetch_distance;
        AbstractController *m_controller;
        PSPFrontend *pf;

        PrefetchEntry* getStream(Addr addr);

        Stats::Scalar numPrefetchHits;
        Stats::Scalar numPrefetchMisses;
        Stats::Scalar numTotalMemoryAccesses;
        Stats::Scalar numAllocatedStreams;
        Stats::Scalar numInPrefetchDistance;
        Stats::Scalar numNotInPrefetchDistance;
};
*/

class PSPBackend : public SimObject
{
    public:
        PSPBackend(const Params *p);
    private:
        PSPFrontend *pf;
};

#endif // __MEM_RUBY_STRUCTURES_PSPBackend_HH__
