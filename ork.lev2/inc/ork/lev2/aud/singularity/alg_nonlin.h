#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// nonlinear blocks
///////////////////////////////////////////////////////////////////////////////

struct SHAPER : public DspBlock
{
    SHAPER( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
};
struct SHAPE2 : public DspBlock
{
    SHAPE2( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
};
struct TWOPARAM_SHAPER : public DspBlock
{
    TWOPARAM_SHAPER( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    float ph1, ph2;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct WRAP : public DspBlock
{
    WRAP( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
};
struct DIST : public DspBlock
{
    DIST( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
};

} //namespace ork::audio::singularity {
