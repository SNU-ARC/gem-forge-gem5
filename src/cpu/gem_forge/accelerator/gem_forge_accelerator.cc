
#include "gem_forge_accelerator.hh"

// For the DPRINTF function.
#include "base/trace.hh"
#include "cpu/gem_forge/llvm_insts.hh"

// Include the accelerators.
#include "speculative_precomputation/speculative_precomputation_manager.hh"
#include "stream/stream_engine.hh"
#include "psp_frontend/psp_frontend.hh"

void GemForgeAccelerator::handshake(GemForgeCPUDelegator *_cpuDelegator,
                                    GemForgeAcceleratorManager *_manager) {
  this->cpuDelegator = _cpuDelegator;
  this->manager = _manager;
}

void GemForgeAccelerator::takeOverBy(GemForgeCPUDelegator *newCpuDelegator,
                                     GemForgeAcceleratorManager *newManager) {
  this->cpuDelegator = newCpuDelegator;
  this->manager = newManager;
}

GemForgeAcceleratorManager::GemForgeAcceleratorManager(
    GemForgeAcceleratorManagerParams *params)
    : SimObject(params), accelerators(params->accelerators),
      cpuDelegator(nullptr), tickEvent([this] { this->tick(); }, name()) {}

GemForgeAcceleratorManager::~GemForgeAcceleratorManager() {}

void GemForgeAcceleratorManager::addAccelerator(
    GemForgeAccelerator *accelerator) {
  this->accelerators.push_back(accelerator);
}

void GemForgeAcceleratorManager::handshake(
    GemForgeCPUDelegator *_cpuDelegator) {
  cpuDelegator = _cpuDelegator;
  for (auto accelerator : this->accelerators) {
    accelerator->handshake(_cpuDelegator, this);
  }
}

void GemForgeAcceleratorManager::handle(LLVMDynamicInst *inst) {
  for (auto accelerator : this->accelerators) {
    if (accelerator->handle(inst)) {
      return;
    }
  }
  panic("Unable to handle accelerator instruction id %u.", inst->getId());
}

void GemForgeAcceleratorManager::tick() {
  for (auto accelerator : this->accelerators) {
    accelerator->tick();
  }
}

void GemForgeAcceleratorManager::scheduleTickNextCycle() {
  if (!this->tickEvent.scheduled()) {
    // Schedule the next tick event.
    cpuDelegator->schedule(&tickEvent, Cycles(1));
  }
}

void GemForgeAcceleratorManager::dump() {
  for (auto accelerator : this->accelerators) {
    accelerator->dump();
  }
}

bool GemForgeAcceleratorManager::checkProgress() {
  bool hasProgress = false;
  for (auto accelerator : this->accelerators) {
    if (accelerator->checkProgress()) {
      hasProgress = true;
    }
  }
  return hasProgress;
}

void GemForgeAcceleratorManager::regStats() { SimObject::regStats(); }

StreamEngine *GemForgeAcceleratorManager::getStreamEngine() {
  for (auto accelerator : this->accelerators) {
    if (auto se = dynamic_cast<StreamEngine *>(accelerator)) {
      return se;
    }
  }
  return nullptr;
}

PSPFrontend *GemForgeAcceleratorManager::getPSPFrontend() {
  for (auto accelerator : this->accelerators) {
    if (auto psp_fe = dynamic_cast<PSPFrontend *>(accelerator)) {
      return psp_fe;
    }
  }
  return nullptr;
}

SpeculativePrecomputationManager *
GemForgeAcceleratorManager::getSpeculativePrecomputationManager() {
  for (auto accelerator : this->accelerators) {
    if (auto spm =
            dynamic_cast<SpeculativePrecomputationManager *>(accelerator)) {
      return spm;
    }
  }
  panic("Failed to find the SpeculativePrecomputationManager.");
}

void GemForgeAcceleratorManager::takeOverFrom(
    GemForgeAcceleratorManager *oldManager) {
  assert(this->accelerators.empty() && "I already have accelerators.");
  this->accelerators.insert(this->accelerators.begin(),
                            oldManager->accelerators.begin(),
                            oldManager->accelerators.end());
  oldManager->accelerators.clear();

  // Reset the cpuDelegator for all accelerators.
  for (auto accelerator : this->accelerators) {
    accelerator->takeOverBy(this->cpuDelegator, this);
  }

  // Take care of schedule tick event.
  if (oldManager->tickEvent.scheduled()) {
    // Simply schedule for myself.
    this->scheduleTickNextCycle();
    oldManager->cpuDelegator->deschedule(&oldManager->tickEvent);
  }
}

GemForgeAcceleratorManager *GemForgeAcceleratorManagerParams::create() {
  return new GemForgeAcceleratorManager(this);
}

void GemForgeAcceleratorManager::exitDump() {
  if (auto se = this->getStreamEngine()) {
    se->exitDump();
  }

  /*
  if (auto psp = this->getPSPFrontend()) {
    psp->exitDump();
  }
  */
}

void GemForgeAcceleratorManager::resetStats() {
  for (auto accelerator : this->accelerators) {
    accelerator->resetStats();
  }
}
