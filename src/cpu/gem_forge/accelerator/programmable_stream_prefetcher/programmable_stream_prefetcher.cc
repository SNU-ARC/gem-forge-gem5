#include "programmable_stream_prefetcher.hh"

#include "debug/programmable_stream_prefetcher.hh"

ProgrammableStreamPrefetcher::ProgrammableStreamPrefetcher(Params* params)
  : GemForgeAccelerator(params) {
}

void ProgrammableStreamPrefetcher::regStats() {
  GemForgeAccelerator::regStats();

#define scalar(stat, describe)                                                 \
  this->stat.name(this->manager->name() + (".psp." #stat))                      \
      .desc(describe)                                                          \
      .prereq(this->stat)

  scalar(numConfigured, "Number of streams configured.");
#undef scalar
}

void ProgrammableStreamPrefetcher::tick() {
}
