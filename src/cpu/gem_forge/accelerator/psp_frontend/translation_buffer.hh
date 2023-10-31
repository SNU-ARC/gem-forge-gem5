#ifndef __CPU_TDG_ACCELERATOR_PSP_TRANSLATION_BUFFER_HH__
#define __CPU_TDG_ACCELERATOR_PSP_TRANSLATION_BUFFER_HH__

#include "arch/generic/tlb.hh"
//#include "cpu/gem_forge/accelerator/stream/stream.hh"

#include <functional>
#include <queue>

/**
 * Represent a simple translation buffer for each stream.
 */

template <typename T> class PSPTranslationBuffer {
public:
  using TranslationDoneCallback =
      std::function<void(PacketPtr, ThreadContext *, T)>;

  PSPTranslationBuffer(BaseTLB *_tlb, TranslationDoneCallback _doneCallback,
                          bool _accessLastLevelTLBOnly,
                          bool _mustDoneInOrder = false)
      : tlb(_tlb), doneCallback(_doneCallback),
        accessLastLevelTLBOnly(_accessLastLevelTLBOnly),
        mustDoneInOrder(_mustDoneInOrder) {}

  void addTranslation(PacketPtr pkt, ThreadContext *tc, T data) {
    auto translation = new PSPTranslation(pkt, tc, data, this);
    this->inflyTranslationQueue.push(translation);

    // Start translation.
    this->startTranslation(translation);
  }

private:
  BaseTLB *tlb;
  TranslationDoneCallback doneCallback;
  /**
   * Whether we only go to last level TLB.
   */
  bool accessLastLevelTLBOnly;
  /**
   * Whether the doneCallback has to be called in order.
   */
  bool mustDoneInOrder;
  struct PSPTranslation;

  void startTranslation(PSPTranslation *translation) {
    assert(translation->state == PSPTranslation::State::INITIATED);
    translation->state = PSPTranslation::State::STARTED;
    BaseTLB::Mode mode = BaseTLB::Mode::Read;
    if (this->accessLastLevelTLBOnly) {
      this->tlb->translateTimingAtLastLevel(translation->pkt->req,
                                            translation->tc, translation, mode);
    } else {
      this->tlb->translateTiming(translation->pkt->req, translation->tc,
                                 translation, mode);
    }
  }
  void finishTranslation(PSPTranslation *translation) {
    assert(translation->state == PSPTranslation::State::STARTED);
    translation->state = PSPTranslation::State::TRANSLATED;
    if (!this->mustDoneInOrder) {
      this->doneTranslation(translation);
    }
    this->releaseTranslationQueue();
  }
  void doneTranslation(PSPTranslation *translation) {
    assert(translation->state == PSPTranslation::State::TRANSLATED);
    auto pkt = translation->pkt;
    auto tc = translation->tc;
    auto data = translation->data;
    this->doneCallback(pkt, tc, data);
    translation->state = PSPTranslation::State::DONE;
  }
  void releaseTranslationQueue() {
    while (!this->inflyTranslationQueue.empty()) {
      auto translation = this->inflyTranslationQueue.front();
      if (translation->state == PSPTranslation::State::DONE) {
        delete translation;
        this->inflyTranslationQueue.pop();
      } else if (translation->state == PSPTranslation::State::TRANSLATED) {
        this->doneTranslation(translation);
        delete translation;
        this->inflyTranslationQueue.pop();
      } else {
        break;
      }
    }
  }

  struct PSPTranslation : public BaseTLB::Translation {
    PacketPtr pkt;
    ThreadContext *tc;
    T data;
    PSPTranslationBuffer *buffer;
    enum State {
      INITIATED,
      STARTED,
      TRANSLATED,
      DONE,
    };
    State state = INITIATED;
    PSPTranslation(PacketPtr _pkt, ThreadContext *_tc, T _data,
                      PSPTranslationBuffer *_buffer)
        : pkt(_pkt), tc(_tc), data(_data), buffer(_buffer) {}

    /**
     * Implement translation interface.
     */
    void markDelayed() override {
      // No need to do anything.
    }

    void finish(const Fault &fault, const RequestPtr &req, ThreadContext *tc,
                BaseTLB::Mode mode) override {
      assert(fault == NoFault && "Fault for StreamTranslation.");
      this->buffer->finishTranslation(this);
    }

    bool squashed() const override {
      // So far we do not support squashing.
      return false;
    }
  };

  std::queue<PSPTranslation *> inflyTranslationQueue;
};

#endif
