#ifndef __CPU_GEM_FORGE_ACCELERATOR_PSP_MEM_ACCESS_HH__
#define __CPU_GEM_FORGE_ACCELERATOR_PSP_MEM_ACCESS_HH__

// Editor: Sungjun Jung (miguel92@snu.ac.kr)
// Description: Interface to handle memory access packets
//              Connected to LSQ of core via GemForgePacketHandler

#include "cpu/gem_forge/gem_forge_packet_handler.hh"
#include "index_queue.hh"

#include <stdlib.h>
#include <string.h>
#include <cassert>
#include <iostream>
#include <vector>

class IndexPacketHandler final : public GemForgePacketHandler {
public:
  IndexPacketHandler(IndexQueueArray* _targetIndexQueue,
                     Addr _cacheBlockVAddr, Addr _vaddr,
                     int _size);
  virtual ~IndexPacketHandler() {}
  void handlePacketResponse(GemForgeCPUDelegator* cpuDelegator,
                            PacketPtr packet);
  void issueToMemoryCallback(GemForgeCPUDelegator* cpuDelegator);

  struct ResponseEvent : public Event {
  public:
    GemForgeCPUDelegator *cpuDelegator;
    IndexPacketHandler *memAccess;
    PacketPtr pkt;
    std::string n;
    ResponseEvent(GemForgeCPUDelegator *_cpuDelegator,
                  IndexPacketHandler *_memAccess, PacketPtr _pkt)
        : cpuDelegator(_cpuDelegator), memAccess(_memAccess), pkt(_pkt),
          n("IndexPacketHandlerResponseEvent") {
      this->setFlags(EventBase::AutoDelete);
    }
    void process() override {
      this->memAccess->handlePacketResponse(this->cpuDelegator, this->pkt);
    }

    const char *description() const { return "IndexPacketHandlerResponseEvent"; }

    const std::string name() const { return this->n; }
  };

  Addr cacheBlockVAddr;
  Addr vaddr;
  int size;
  IndexQueueArray* targetIndexQueueEntry;
};
#endif
