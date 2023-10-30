#include "index_packet_handler.hh"

IndexPacketHandler::IndexPacketHandler(IndexQueueArray* _targetIndexQueue,
                                       Addr _cacheBlockVaddr, Addr _vaddr,
                                       int _size)
  : targetIndexQueue(_targetIndexQueue), cacheBlockVAddr(_cacheBlockVAddr),
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
