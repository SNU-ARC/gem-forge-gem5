#include "psp_frontend.hh"

#include "debug/PSPFrontend.hh"

PSPFrontend::PSPFrontend(Params* params)
  : GemForgeAccelerator(params) {
    this->totalPatternTableEntries = params->totalPatternTableEntries;
    patternTable = new PatternTable(params->totalPatternTableEntries);
}

PSPFrontend::~PSPFrontend() {
  delete[] patternTable;
}

void PSPFrontend::tick() {
}

void PSPFrontend::dump() {
}

void PSPFrontend::regStats() {
  GemForgeAccelerator::regStats();

#define scalar(stat, describe)                                                 \
  this->stat.name(this->manager->name() + (".psp." #stat))                      \
      .desc(describe)                                                          \
      .prereq(this->stat)

  scalar(numConfigured, "Number of streams configured.");
#undef scalar
}

void PSPFrontend::resetStats() {
}

PSPFrontend* PSPFrontendParams::create() { return new PSPFrontend(this); }
