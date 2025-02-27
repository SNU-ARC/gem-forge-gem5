#ifndef __GEM_FORGE_STREAM_ADDRESS_GENERATE_CALLBACK_HH__
#define __GEM_FORGE_STREAM_ADDRESS_GENERATE_CALLBACK_HH__

#include "cpu/gem_forge/accelerator/arch/exec_func.hh"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

using StreamValue = TheISA::ExecFunc::RegisterValue;
using DynamicStreamParamV = std::vector<StreamValue>;
using GetStreamValueFunc = std::function<StreamValue(uint64_t)>;
using ExecFuncPtr = std::shared_ptr<TheISA::ExecFunc>;

struct DynamicStreamFormalParam {
  StreamValue invariant;
  uint64_t baseStreamId;
  bool isInvariant;
};
using DynamicStreamFormalParamV = std::vector<DynamicStreamFormalParam>;

/**
 * Use if you expect no base stream.
 */
StreamValue getStreamValueFail(uint64_t streamId);

/**
 * Use if you expect only one base stream.
 */
struct GetSingleStreamValue {
  const uint64_t streamId;
  const StreamValue streamValue;
  GetSingleStreamValue(uint64_t _streamId, const StreamValue &_streamValue)
      : streamId(_streamId), streamValue(_streamValue) {}
  StreamValue operator()(uint64_t streamId) const;
};

DynamicStreamParamV
convertFormalParamToParam(const DynamicStreamFormalParamV &formalParams,
                          GetStreamValueFunc getStreamValue);

/**
 * This represent a stateless addr_gen function.
 */
struct AddrGenCallback {
  virtual StreamValue genAddr(uint64_t idx,
                              const DynamicStreamParamV &params) = 0;

  /**
   * This is a helper function to actually call the address callback.
   * Given a callback to collect base stream value.
   */
  StreamValue genAddr(uint64_t idx,
                      const DynamicStreamFormalParamV &formalParams,
                      GetStreamValueFunc getStreamValue);
          
  virtual Cycles getEstimatedLatency() const = 0;
};

using AddrGenCallbackPtr = std::shared_ptr<AddrGenCallback>;

struct LinearAddrGenCallback : public AddrGenCallback {
  StreamValue genAddr(uint64_t idx, const DynamicStreamParamV &params) override;

  Cycles getEstimatedLatency() const override {
    return Cycles(1);
  }

  bool isContinuous(const DynamicStreamFormalParamV &params,
                    int32_t elementSize);
  /**
   * Get the inner most stride.
   */
  int64_t getInnerStride(const DynamicStreamFormalParamV &params);
  /**
   * Only valid when continuous.
   */
  uint64_t getStartAddr(const DynamicStreamFormalParamV &params);
  /**
   * Compute the first elementIdx that will touch the address.
   */
  uint64_t getFirstElementForAddr(const DynamicStreamFormalParamV &params,
                                  int32_t elementSize, uint64_t addr);
  /**
   * Estimate memory footprint and reuse count.
   * @return success, reuse footprint, reuse count.
   */
  bool estimateReuse(const DynamicStreamFormalParamV &params,
                     uint64_t elementSize, uint64_t &reuseFootprint,
                     uint64_t &reuseCount);

  /**
   * Get nested trip count.
   */
  uint64_t getNestTripCount(const DynamicStreamFormalParamV &params,
                            int nestLevel);
};

class FuncAddrGenCallback : public AddrGenCallback {
public:
  FuncAddrGenCallback(ExecFuncPtr _execFunc) : execFunc(_execFunc) {}

  StreamValue genAddr(uint64_t idx,
                      const DynamicStreamParamV &params) override {
    // We ignore the idx.
    return this->execFunc->invoke(params);
  }

  Cycles getEstimatedLatency() const override {
    return this->execFunc->getEstimatedLatency();
  }

  const ExecFuncPtr &getExecFunc() const {
    return this->execFunc;
  }

private:
  ExecFuncPtr execFunc;
};

#endif