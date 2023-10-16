/*
 * Editor: Sungjun Jung (K16DIABLO)
 * Top module of PSP
 */
#ifndef __CPU_GEM_FORGE_ACCELERATOR_PROGRAMMABLE_STREAM_PREFETCHER_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_PROGRAMMABLE_STREAM_PREFETCHER_HH__

#include "cpu/gem_forge/accelerator/gem_forge_accelerator.hh"
#include "cpu/gem_forge/gem_forge_packet_handler.hh"

#include "params/ProgrammableStreamPrefetcher.hh"

class ProgrammableStreamPrefetcher : public GemForgeAccelerator,
                                     public GemForgePacketHandler {
public:
  using Params = ProgrammableStreamPrefetcherParams;
  ProgrammableStreamPrefetcher(Params* params);
  ~ProgrammableStreamPrefetcher() override;

  void tick() override;
  void regStats() override;
  void resetStats() override;

  // TODO: Define PSP instructions below

  // TODO: Define Stats below
  // mutable Stats::Scalar numInstructions;
private:
  
};
#endif
