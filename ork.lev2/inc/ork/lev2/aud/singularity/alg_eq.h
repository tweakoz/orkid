#pragma once

#include "alg.h"
#include "dspblocks.h"
#include "shelveeq.h"

namespace ork::audio::singularity {

struct PARABASS : public DspBlock
{
    PARABASS(dspblkdata_constptr_t dbd);
    LoShelveEq _lsq;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct STEEP_RESONANT_BASS : public DspBlock
{
    STEEP_RESONANT_BASS(dspblkdata_constptr_t dbd);
    LoShelveEq _lsq;
    TrapSVF _svf;
    float _filtFC;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct PARAMID : public DspBlock
{
    PARAMID(dspblkdata_constptr_t dbd);
    BiQuad _biquad;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct PARATREBLE : public DspBlock
{
    PARATREBLE(dspblkdata_constptr_t dbd);
    HiShelveEq _lsq;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct PARAMETRIC_EQ : public DspBlock
{
    PARAMETRIC_EQ(dspblkdata_constptr_t dbd);
    BiQuad _biquad;
    Fil4Paramsect _peq;
    ParaOne _peq1;
    float _smoothFC;
    float _smoothW;
    float _smoothG;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};

} //namespace ork::audio::singularity {
