#include "se_page_walker.hh"
#include "base/trace.hh"

#include "debug/TLB.hh"

namespace X86ISA {

SEPageWalker::SEPageWalker(const std::string &_name, Cycles _latency,
                           uint32_t _numContext)
    : myName(_name), latency(_latency), numContext(_numContext) {}

void SEPageWalker::regStats() {
  this->accesses.name(this->name() + ".accesses").desc("PageWalker accesses");
  this->hits.name(this->name() + ".hits")
      .desc("PageWalker hits on infly walks");
  this->waits.name(this->name() + ".waits")
      .desc("PageWalker waits on available context");
  this->ptwCycles.name(this->name() + ".ptwCycles")
      .desc("PageWalker spent cycles");
  this->prefetch_accesses.name(this->name() + ".prefetch_accesses").desc("PageWalker prefetch_accesses");
  this->prefetch_hits.name(this->name() + ".prefetch_hits")
      .desc("PageWalker prefetch_hits on infly walks");
  this->prefetch_waits.name(this->name() + ".prefetch_waits")
      .desc("PageWalker prefetch_waits on available context");
  this->prefetch_ptwCycles.name(this->name() + ".prefetch_ptwCycles")
      .desc("PageWalker spent on prefetch cycles");
}

void SEPageWalker::clearReadyStates(Cycles curCycle) {
  for (auto iter = this->inflyState.begin(), end = this->inflyState.end();
       iter != end;) {
    if (iter->readyCycle < curCycle) {
      assert(this->pageVAddrToStateMap.erase(iter->pageVAddr) &&
             "Failed to find the state in PageVAddrToStateMap.");
      iter = this->inflyState.erase(iter);
    } else {
      // This state is not ready yet.
      break;
    }
  }
}

Cycles SEPageWalker::walk(Addr pageVAddr, Cycles curCycle, bool isPrefetch) {
  this->clearReadyStates(curCycle);

  this->accesses++;
  if (isPrefetch) this->prefetch_accesses++;
  DPRINTF(TLB, "Walk PageTable %#x.\n", pageVAddr);

  // Check if we have a parallel miss to the same page.
  auto iter = this->pageVAddrToStateMap.find(pageVAddr);
  if (iter != this->pageVAddrToStateMap.end()) {
    /**
     * Cycle latency for parallel miss.
     */
    this->hits++;
    if (isPrefetch) this->prefetch_hits++;
    iter->second->numAccess++;
    DPRINTF(TLB, "[PTW_walk] Hit %#x, Latency %s.\n", pageVAddr,
            iter->second->readyCycle - curCycle + Cycles(iter->second->numAccess));
    return iter->second->readyCycle - curCycle + Cycles(iter->second->numAccess);
  }

  /**
   * Try to allocate a new state for this miss.
   * First we have to get the available context.
   */
  auto it = this->inflyState.rbegin();
  uint32_t numInflyRequests = 0;
  for (; it != this->inflyState.rend(); it++) {
    numInflyRequests += (it->numAccess + 1);
    DPRINTF(TLB, "inflyState.size %lu, numInflyRequest %u.\n", 
        this->inflyState.size(), numInflyRequests);
    if (this->numContext <= numInflyRequests) {
//      it++;
      break;
    }
  }

  Cycles allocateCycle = curCycle;
  if (this->numContext <= numInflyRequests) {
    // We have to wait until previous translations are done
    this->waits++;
    if (isPrefetch) this->prefetch_waits++;
    allocateCycle = it->readyCycle + Cycles(it->numAccess);

    this->ptwCycles += (this->latency + allocateCycle - this->inflyState.rbegin()->readyCycle);
    if (isPrefetch)
      this->prefetch_ptwCycles += (this->latency + allocateCycle - this->inflyState.rbegin()->readyCycle);
  }
  else if (this->inflyState.size() == 0 || 
      this->inflyState.rbegin()->readyCycle < allocateCycle) {
    this->ptwCycles += this->latency;
    if (isPrefetch) this->prefetch_ptwCycles += this->latency;
  }
  else {
    this->ptwCycles += (this->latency + allocateCycle - this->inflyState.rbegin()->readyCycle);
    if (isPrefetch)
      this->prefetch_ptwCycles += (this->latency + allocateCycle - this->inflyState.rbegin()->readyCycle);
  }

//  auto prevContextIter = this->inflyState.rbegin();
//  auto prevContextEnd = this->inflyState.rend();
//  Cycles allocateCycle = curCycle;
//  if (this->numContext <= this->inflyState.size()) {
//    // We have to wait until previous translations are done
//    this->waits++;
//    if (isPrefetch) this->prefetch_waits++;
//    prevContextIter = this->inflyState.rbegin();
//    for (uint32_t i = 0; i < this->numContext - 1; i++)
//      prevContextIter++;
//    allocateCycle = prevContextIter->readyCycle;
//
//    this->ptwCycles += (this->latency + allocateCycle - this->inflyState.rbegin()->readyCycle);
//    if (isPrefetch)
//      this->prefetch_ptwCycles += (this->latency + allocateCycle - this->inflyState.rbegin()->readyCycle);
//  }
//  else if (this->inflyState.size() == 0 || 
//      this->inflyState.rbegin()->readyCycle < allocateCycle) {
//    this->ptwCycles += this->latency;
//    if (isPrefetch) this->prefetch_ptwCycles += this->latency;
//  }
//  else {
//    this->ptwCycles += (this->latency + allocateCycle - this->inflyState.rbegin()->readyCycle);
//    if (isPrefetch)
//      this->prefetch_ptwCycles += (this->latency + allocateCycle - this->inflyState.rbegin()->readyCycle);
//  }

  // Allocate it.
  auto readyCycle = allocateCycle + this->latency;
  this->inflyState.emplace_back(pageVAddr, readyCycle);
  this->pageVAddrToStateMap.emplace(pageVAddr, &(this->inflyState.back()));

  // We should only return the difference.
  return readyCycle - curCycle;
}

Cycles SEPageWalker::lookup(Addr pageVAddr, Cycles curCycle, bool isPrefetch) {
  this->clearReadyStates(curCycle);

  // Check if we have a parallel miss to the same page.
  auto iter = this->pageVAddrToStateMap.find(pageVAddr);
  if (iter != this->pageVAddrToStateMap.end()) {
    this->accesses++;
    this->hits++;
    if (curCycle + this->latency < iter->second->readyCycle) {
      this->waits++;
    }
    this->ptwCycles += iter->second->readyCycle - curCycle + Cycles(iter->second->numAccess);
    iter->second->numAccess++;

    if (isPrefetch) {
      this->prefetch_accesses++;
      this->prefetch_hits++;
      if (curCycle + this->latency < iter->second->readyCycle) {
        this->prefetch_waits++;
      }
      this->prefetch_ptwCycles += iter->second->readyCycle - curCycle + Cycles(iter->second->numAccess);
    }
    DPRINTF(TLB, "[PTW_lookup] Hit %#x, Latency %s.\n", pageVAddr,
            iter->second->readyCycle - curCycle + Cycles(iter->second->numAccess));
    return iter->second->readyCycle - curCycle + Cycles(iter->second->numAccess);
  }
  // If no match, it is L1 hit
  auto it = this->inflyState.begin();
  uint32_t numInflyRequests = 0;
  for (; it != this->inflyState.end(); it++) {
    if (it->readyCycle - this->latency <= curCycle &&
        curCycle < it->readyCycle) {
      numInflyRequests += (it->numAccess + 1);
    }
    else {
      break;
    }
  }
  if (numInflyRequests < this->numContext) {
    return Cycles(0);
  }
  else {
    return it->readyCycle;
  }
}

} // namespace X86ISA
