#pragma once

#include "dspblocks.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
// filter blocks
///////////////////////////////////////////////////////////////////////////////

struct BANDPASS_FILT : public DspBlock
{
    BANDPASS_FILT(dspblkdata_constptr_t dbd);
    TrapSVF _filter;
    BiQuad _biquad;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct BAND2 : public DspBlock
{
    BAND2(dspblkdata_constptr_t dbd);
    TrapSVF _filter;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct NOTCH_FILT : public DspBlock
{
    NOTCH_FILT(dspblkdata_constptr_t dbd);
    TrapSVF _filter;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct NOTCH2 : public DspBlock
{
    NOTCH2(dspblkdata_constptr_t dbd);
    TrapSVF _filter1;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct DOUBLE_NOTCH_W_SEP : public DspBlock
{
    DOUBLE_NOTCH_W_SEP(dspblkdata_constptr_t dbd);
    TrapSVF _filter1;
    TrapSVF _filter2;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct TWOPOLE_LOWPASS : public DspBlock
{
    TWOPOLE_LOWPASS(dspblkdata_constptr_t dbd);
    TrapSVF _filter;
    float _smoothFC;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct LOPAS2 : public DspBlock
{   LOPAS2(dspblkdata_constptr_t dbd);
    TrapSVF _filter;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct LP2RES : public DspBlock
{   LP2RES(dspblkdata_constptr_t dbd);
    TrapSVF _filter;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct LPGATE : public DspBlock
{   LPGATE(dspblkdata_constptr_t dbd);
    TrapSVF _filter;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct FOURPOLE_LOPASS_W_SEP : public DspBlock
{
    FOURPOLE_LOPASS_W_SEP(dspblkdata_constptr_t dbd);
    TrapSVF _filter1;
    TrapSVF _filter2;
    float _filtFC;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct FOURPOLE_HIPASS_W_SEP : public DspBlock
{
    FOURPOLE_HIPASS_W_SEP(dspblkdata_constptr_t dbd);
    TrapSVF _filter1;
    TrapSVF _filter2;
    float _filtFC;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct LOPASS : public DspBlock
{   LOPASS(dspblkdata_constptr_t dbd);
    OnePoleLoPass _lpf;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct LPCLIP : public DspBlock
{   LPCLIP(dspblkdata_constptr_t dbd);
    OnePoleLoPass _lpf;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct HIPASS : public DspBlock
{   HIPASS(dspblkdata_constptr_t dbd);
    OnePoleHiPass _hpf;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct ALPASS : public DspBlock
{
    ALPASS(dspblkdata_constptr_t dbd);
    TrapAllpass _filter;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct TWOPOLE_ALLPASS : public DspBlock
{
    TWOPOLE_ALLPASS(dspblkdata_constptr_t dbd);
    TrapAllpass _filterL;
    TrapAllpass _filterH;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct HIFREQ_STIMULATOR : public DspBlock
{
    HIFREQ_STIMULATOR(dspblkdata_constptr_t dbd);
    TrapSVF _filter1;
    TrapSVF _filter2;
    float _smoothFC;
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};

} //namespace ork::audio::singularity {
