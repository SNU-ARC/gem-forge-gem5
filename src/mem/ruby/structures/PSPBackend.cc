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

PSPBackend*
PSPBackendParams::create()
{
    return new PSPBackend(this);
}

PSPBackend::PSPBackend(const Params *p)
    : SimObject(p), enabled(p->enabled), num_streams(p->num_streams), prefetch_distance(p->prefetch_distance)
{ 
    this->streamTable.resize(p->num_streams, StreamEntry(this, p->num_stream_entry, p->prefetch_distance));
    numPrefetchHits = 0;
    numPrefetchMisses = 0;
    numHits = 0;
    numMisses = 0;
}

void
PSPBackend::regStats()
{
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
PSPBackend::getEntry(Addr addr) {
    for (auto& se : streamTable) {
        if (se.hasEntry(addr)) {
            return &se;
        }
    }
    return NULL;
}

bool
PSPBackend::observeHit(Addr address)
{
    StreamEntry * se = getEntry(address);
    if (se != NULL) {
        DPRINTF(PSPBackend, "** Observed hit for %#x\n", address);
        se->freeEntry(address);
        se->prefetchRequestCompleted();
        issuePrefetch(se);
        return true;
    }
    return false;
}

bool
PSPBackend::observeMiss(Addr address)
{
    StreamEntry * se = getEntry(address);
    if (se != NULL) {
        DPRINTF(PSPBackend, "** Observed miss for %#x\n", address);
        se->freeEntry(address);
        se->prefetchRequestCompleted();
        issuePrefetch(se);
        return true;
    }
    return false;
}

bool
PSPBackend::observePfHit(Addr address)
{
    StreamEntry * se = getEntry(address);
    if (se != NULL) {
        DPRINTF(PSPBackend, "** Observed PSP prefetcher hit for %#x\n", address);
        se->freeEntry(address); // Delete already used entry if possible
        se->prefetchRequestCompleted();
        issuePrefetch(se);
        numPrefetchHits++;
        return true;
    }
    //DPRINTF(PSPBackend, "** Observed OTHER prefetcher hit for %#x\n", address);
    return false;
}

bool
PSPBackend::observePfMiss(Addr address)
{
    StreamEntry * se = getEntry(address);
    if (se != NULL) {
        DPRINTF(PSPBackend, "** Observed PSP prefetcher miss for %#x\n", address);
        se->freeEntry(address); // Delete already used entry if possible
        se->prefetchRequestCompleted();
        issuePrefetch(se);
        numPrefetchMisses++;
        return true;
    }
    //DPRINTF(PSPBackend, "** Observed OTHER prefetcher miss for %#x\n", address);
    return false;
}

void
PSPBackend::issuePrefetch(StreamEntry *se)
{
    int size = se->numToPrefetch();
    for (int i = 0; i < size; i++) {
        if (se->existNextLineAddr()) {
            Addr cur_addr = se->getNextLineAddr();
            m_controller->enqueuePrefetch(cur_addr, RubyRequestType_LD);
            DPRINTF(PSPBackend, "## Enqueue prefetch request for address %#x done.\n", cur_addr);
            se->incrementLineAddr();
        }
    }
}

void 
StreamEntry::insertEntry(Addr addr, int size) {
    int idx = entryToValidate();
    // Must have invalid entry for insertion when call this function 
    assert(idx != -1 && "## No free entry\n");

    // Merge duplicated entry
    if (hasEntry(addr)) {
      DPRINTF(PSPBackend, "## Merge Address %#x.\n", addr);
      return;
    }

    PSPPrefetchEntry& p = PSPPrefetchEntryTable[idx];
    p.setEntry(addr, size, getPriority());
    DPRINTF(PSPBackend, "## Set Entry %d, address %#x.\n", idx, addr);

    if (activeEntryIdx == -1) {
        DPRINTF(PSPBackend, "## Activate Entry %d, address %#x.\n", idx, addr);
        activeEntryIdx = idx;
        pb->issuePrefetch(this);
    }
}
