////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// nonlinear blocks
///////////////////////////////////////////////////////////////////////////////
struct SHAPER_DATA : public DspBlockData {
  DeclareConcreteX(SHAPER_DATA,DspBlockData);
  SHAPER_DATA(std::string name="X");
  dspblk_ptr_t createInstance() const override;
};

struct SHAPER : public DspBlock {
  using dataclass_t = SHAPER_DATA;
  SHAPER(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct SHAPE2_DATA : public DspBlockData {
  DeclareConcreteX(SHAPE2_DATA,DspBlockData);
  SHAPE2_DATA(std::string name="X");
  dspblk_ptr_t createInstance() const override;
};
struct SHAPE2 : public DspBlock {
  using dataclass_t = SHAPE2_DATA;
  SHAPE2(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct TWOPARAM_SHAPER_DATA : public DspBlockData {
  DeclareConcreteX(TWOPARAM_SHAPER_DATA,DspBlockData);
  TWOPARAM_SHAPER_DATA(std::string name="X");
  dspblk_ptr_t createInstance() const override;
};
struct TWOPARAM_SHAPER : public DspBlock {
  using dataclass_t = TWOPARAM_SHAPER_DATA;
  TWOPARAM_SHAPER(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
  float ph1, ph2;
  void doKeyOn(const KeyOnInfo& koi) final;
};
//
struct WrapData : public DspBlockData {
  DeclareConcreteX(WrapData,DspBlockData);
  WrapData(std::string name="X");
  dspblk_ptr_t createInstance() const override;
};
struct Wrap : public DspBlock {
  using dataclass_t = WrapData;
  Wrap(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};
struct DistortionData : public DspBlockData {
  DeclareConcreteX(DistortionData,DspBlockData);
  DistortionData(std::string name="X");
  dspblk_ptr_t createInstance() const override;
};
struct Distortion : public DspBlock {
  using dataclass_t = DistortionData;
  Distortion(const DspBlockData* dbd);
  void compute(DspBuffer& dspbuf) final;
};

} // namespace ork::audio::singularity
