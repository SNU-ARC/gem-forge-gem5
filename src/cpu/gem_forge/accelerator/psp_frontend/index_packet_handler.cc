#include "index_packet_handler.hh"

IndexPacketHandler::IndexPacketHandler(uint64_t _entryId,
                                       Addr _cacheBlockVaddr, Addr _vaddr,
                                       int _size)
  : entryId(_entryId), cacheBlockVAddr(_cacheBlockVAddr),
    vaddr(_vaddr), size(_size) {
}

void IndexPacketHandler::handlePacketResponse(GemForgeCPUDelegator* cpuDelegator,
                                              PacketPtr pkt) {
  this->targetIndexQueue->handlePacketResponse(this, pkt);

  delete pkt;
  delete this;
}

void IndexPacketHandler::issueToMemoryCallback(GemForgeCPUDelegator* cpuDelegator) {
}
