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
#include "debug/PSPBackend.hh"
#include "mem/ruby/slicc_interface/RubySlicc_ComponentMapping.hh"
#include "mem/ruby/system/RubySystem.hh"

PSPBackend*
PSPBackendParams::create()
{
    return new PSPBackend(this);
}

PSPBackend::PSPBackend(const Params *p)
    : SimObject(p)
{ 
    /*
    num_streams = p->num_streams;
    prefetch_distance = p->prefetch_distance;
    streamTable = new std::vector<StreamEntry>(num_streams);

    PSPFrontend * pf = m_controller->getCPUSequencer()->getThreadContext()->getCpuPtr()->getAccelManager()->getPSPFrontend();
    this->pf = pf;
    pf->setPSPBackend(this);
    */
}

/*
PSPBackend::~PSPBackend()
{
    delete streamTable;
}

void
PSPBackend::regStats()
{
    SimObject::regStats();

    numPrefetchHits
        .name(name() + "numPrefetchHits");

    numPrefetchMisses
        .name(name() + "numPrefetchMisses");
    
    numTotalMemoryAccesses
        .name(name() + "numTotalMemoryAccesses");

    numAllocatedStreams
        .name(name() + "numAllocatedStreams");

    numInPrefetchDistance
        .name(name() + "numInPrefetchDistance");

    numNotInPrefetchDistance
        .name(name() + "numNotInPrefetchDistance");
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

void
PSPBackend::observeHit(Addr address, const RubyRequestType& type)
{
    numTotalAccesses++;
    DPRINTF(PSPBackend, "Observed hit for %#x\n", address);
}

void
PSPBackend::observeMiss(Addr address, const RubyRequestType& type)
{
    numTotalAccesses++;
    DPRINTF(PSPBackend, "Observed miss for %#x\n", address);
}

void
PSPBackend::observePfMiss(Addr address)
{
    numPrefetchMisses++;
    DPRINTF(PSPBackend, "Observed prefetch miss for %#x\n", address);
}

void
PSPBackend::observePfHit(Addr address)
{
    // FIXME, not addresss but use line address
    numPrefetchHits++;
    DPRINTF(PSPBackend, "Observed prefetch hit for %#x\n", address);
    StreamEntry * se = getEntry(address);
    PrefetchEntry * pe = se.getEntry(address);

    assert(se);
    assert(pe);

    Addr line_addr = makeLineAddress(address);

    Addr nextPrefetchAddr = pe->getNextAddr();

    if (pe->isLastElement(nextPrefetchAddr)) {
        pe = se->changeStream();
        nextPrefetchAddr = pe->getNextAddr();
    } else if (pe->isLastElement(address)) {
        // POP from PAQ and set new stream
    }

    if (nextPrefetchAddr - address <= prefetch_distance) {
        numInPrefetchDistance++;
        issueNextPrefetch(nextPrefetchAddr, pe->getMemRequestType());
    } else {
        numNotInPrefetchDistance++;
    }
}

void
PSPBackend::issueNextPrefetch(Addr address, RubyRequestType type)
{
    m_controller->enqueuePrefetch(address, type);
}
*/
