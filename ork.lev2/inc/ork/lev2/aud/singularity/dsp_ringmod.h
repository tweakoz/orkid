////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {

struct RingModData final : public DspBlockData {
  RingModData(std::string name);
  dspblk_ptr_t createInstance() const override;
};
struct RingModSumAData final : public DspBlockData {
  RingModSumAData(std::string name);
  dspblk_ptr_t createInstance() const override;
};

struct RingMod : public DspBlock {
  using dataclass_t = RingModData;
  RingMod(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct RingModSumA : public DspBlock {
  using dataclass_t = RingModSumAData;
  RingModSumA(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
} // namespace ork::audio::singularity
