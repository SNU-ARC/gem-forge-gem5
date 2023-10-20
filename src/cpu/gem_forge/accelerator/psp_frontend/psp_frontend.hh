/*
 * Editor: Sungjun Jung (K16DIABLO)
 * Top module of PSP Frontend
 */
#ifndef __CPU_GEM_FORGE_ACCELERATOR_PSP_FRONTEND_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_PSP_FRONTEND_HH__

#include "cpu/gem_forge/llvm_insts.hh"
#include "base/statistics.hh"

#include "cpu/gem_forge/accelerator/gem_forge_accelerator.hh"
#include "cpu/gem_forge/gem_forge_packet_handler.hh"
#include "cpu/gem_forge/accelerator/psp_frontend/pattern_table.hh"
#include "cpu/gem_forge/accelerator/psp_frontend/arbiter.hh"

#include "params/PSPFrontend.hh"

class PSPFrontend : public GemForgeAccelerator {
public:
  using Params = PSPFrontendParams;
  PSPFrontend(Params* params);
  ~PSPFrontend() override;

  void tick() override;
  void dump() override;
  void regStats() override;
  void resetStats() override;

  PatternTable* patternTable;
  PatternTable* validPatternTableEntry;
  PatternTableRRArbiter* patternTableArbiter;
//  IndexLoadUnit* indexLoadUnit;
//  RRArbiter* indexArbiter;
//  AddrTransUnit* addrTransUnit;

  // TODO: Define PSP instructions below

  // TODO: Define Stats below
  mutable Stats::Scalar numConfigured;
private:
  unsigned totalPatternTableEntries;
  
};
#endif
