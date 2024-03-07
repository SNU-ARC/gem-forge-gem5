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

  if (!isPrefetch)
    this->accesses++;
  DPRINTF(TLB, "Walk PageTable %#x.\n", pageVAddr);

  // Check if we have a parallel miss to the same page.
  auto iter = this->pageVAddrToStateMap.find(pageVAddr);
  if (iter != this->pageVAddrToStateMap.end()) {
    /**
     * Cycle latency for parallel miss.
     */
    if (!isPrefetch) {
      this->hits++;
      iter->second->numAccess++;
    }
    DPRINTF(TLB, "Hit %#x, Latency %s.\n", pageVAddr,
            iter->second->readyCycle - curCycle + Cycles(iter->second->numAccess));
    return iter->second->readyCycle - curCycle + Cycles(iter->second->numAccess);
  }

  /**
   * Try to allocate a new state for this miss.
   * First we have to get the available context.
   */
  auto prevContextIter = this->inflyState.rbegin();
  auto prevContextEnd = this->inflyState.rend();
  Cycles allocateCycle = curCycle;
  if (this->numContext <= this->inflyState.size()) {
    // We have to wait until previous translations are done
    if (!isPrefetch) this->waits++;
    prevContextIter = this->inflyState.rbegin();
    for (uint32_t i = 0; i < this->numContext - 1; i++)
      prevContextIter++;
    allocateCycle = prevContextIter->readyCycle;

    if (!isPrefetch)
      this->ptwCycles += (this->latency + allocateCycle - this->inflyState.rbegin()->readyCycle);
  }
  else if (this->inflyState.size() == 0 || 
      this->inflyState.rbegin()->readyCycle < allocateCycle) {
    if (!isPrefetch)
      this->ptwCycles += this->latency;
  }
  else {
    if (!isPrefetch)
      this->ptwCycles += (this->latency + allocateCycle - this->inflyState.rbegin()->readyCycle);
  }

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
    if (!isPrefetch) {
      this->accesses++;
      this->hits++;
      iter->second->numAccess++;
      if (curCycle + this->latency < iter->second->readyCycle) {
        this->waits++;
      }
    }

    DPRINTF(TLB, "Hit %#x, Latency %s.\n", pageVAddr,
            iter->second->readyCycle - curCycle + Cycles(iter->second->numAccess));
    return iter->second->readyCycle - curCycle + Cycles(iter->second->numAccess);
  }
  // If no match, it is L1 hit
  return Cycles(0);
}

} // namespace X86ISA
