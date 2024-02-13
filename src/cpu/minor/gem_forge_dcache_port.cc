#include "lsq.hh"

#include "cpu/gem_forge/gem_forge_packet_handler.hh"

/*********************************************************************
 * Implementation of the GemForgeDcachePort in the LSQ.
 ********************************************************************/

namespace Minor {

bool LSQ::GemForgeDcachePort::recvTimingResp(PacketPtr pkt) {
  // Intercept the GemForgePackets.
  if (GemForgePacketHandler::isGemForgePacket(pkt)) {
    LSQ::GemForgeDcachePort::decrementNumAccessesInMemorySystem();
    GemForgePacketHandler::handleGemForgePacketResponse(cpu.getCPUDelegator(),
                                                        pkt);
    return true;
  }
  // Normally call base handler.
  return DcachePort::recvTimingResp(pkt);
}

} // namespace Minor
