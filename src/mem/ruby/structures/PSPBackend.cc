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
    for (int i = 0; i < p->num_stream_entry; i++) {
        this->streamTable[i].setStreamNum(i);
    }
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
    numHits++;
    StreamEntry * se = getEntry(address);
    DPRINTF(PSPBackend, "** Observed hit for %#x, exist entry %d\n", address, se != NULL);
    this->numValidEntry();
    if (se != NULL) {
        return true;
    }
    return false;
}

bool
PSPBackend::observeMiss(Addr address)
{
    numMisses++;
    StreamEntry * se = getEntry(address);
    DPRINTF(PSPBackend, "** Observed miss for %#x, exist entry %d\n", address, se != NULL);
    this->numValidEntry();
    if (se != NULL) {
        return true;
    }
    return false;
}

bool
PSPBackend::observePfHit(Addr address)
{
    StreamEntry * se = getEntry(address);
    DPRINTF(PSPBackend, "** Observed prefetcher hit for %#x, exist entry %d\n", address, se != NULL);
    this->numValidEntry();
    if (se != NULL) {
        if (se->viewRequest(address)) {
            issuePrefetch(se);
            numPrefetchHits++;
            return true;
        }
    }
    return false;
}

bool
PSPBackend::observePfMiss(Addr address)
{
    StreamEntry * se = getEntry(address);
    DPRINTF(PSPBackend, "** Observed prefetcher miss for %#x, exist entry %d\n", address, se != NULL);
    this->numValidEntry();
    if (se != NULL) {
        if (se->viewRequest(address)) {
            issuePrefetch(se);
            numPrefetchMisses++;
            return true;
        }
    }
    return false;
}

void
PSPBackend::issuePrefetch(StreamEntry *se)
{
    int size = se->numToPrefetch();
    DPRINTF(PSPBackend, "## Issue %d prefetch requests. Num inflight requests : %d\n", size, se->numInflightRequest());
    for (int i = 0; i < size; i++) {
        Addr cur_addr = se->issueRequest();
        m_controller->enqueuePrefetch(cur_addr, RubyRequestType_LD);
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
    DPRINTF(PSPBackend, "## Set Entry %d, address %#x. Num inflight : %d\n", idx, addr, numInflightRequest());

    if (activeEntryIdx == -1) {
        DPRINTF(PSPBackend, "## Activate Entry %d, address %#x.\n", idx, addr);
        activeEntryIdx = idx;
        pb->issuePrefetch(this);
    }
}

Addr
StreamEntry::issueRequest() {
    assert(activeEntryIdx != -1);

    PSPPrefetchEntry *pe = &PSPPrefetchEntryTable[activeEntryIdx];
    assert(pe != NULL);
    Addr addr = pe->issueRequest();
    DPRINTF(PSPBackend, "## Stream : %d, Enqueue prefetch request for address %#x done.\n", stream_num, addr);

    if (pe->doneIssue()) {
        pe->deactivate();
        DPRINTF(PSPBackend, "## Stream : %d, Deactivate Entry %d, address %#x\n", stream_num, activeEntryIdx, pe->getFirstLineAddr());
        activeEntryIdx = -1;
        int toActivate = entryToActivate();
        if (toActivate != -1) {
            activeEntryIdx = toActivate;
            pe = &PSPPrefetchEntryTable[activeEntryIdx];
            pe->activate();
            DPRINTF(PSPBackend, "## Stream : %d, Activate Entry %d, address %#x.\n", stream_num, activeEntryIdx, pe->getFirstLineAddr());
            this->pb->issuePrefetch(this);
        }
    }

    return addr;
}
