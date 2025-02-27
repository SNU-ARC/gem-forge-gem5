#ifndef __GEM_FORGE_TRANSLATION_FAULT_HH__
#define __GEM_FORGE_TRANSLATION_FAULT_HH__

#include "sim/faults.hh"

namespace GemForge {
/**
 * Handle the case when there is translation fault on a stream element.
 */
class GemForgeTranslationFault : public FaultBase {
public:
  FaultName name() const override { return "GemForgeTranslationFault"; }
  void
  invoke(ThreadContext *tc,
         const StaticInstPtr &inst = StaticInst::nullStaticInstPtr) override {
    panic("GemForgeTranslationFault should never be invoked.");
  }
};
} // namespace GemForge
#endif