#pragma once

#include "dspblocks.h"

///////////////////////////////////////////////////////////////////////////////
// oscils
///////////////////////////////////////////////////////////////////////////////

struct SAMPLEPB : public DspBlock
{
    SAMPLEPB( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;

    void doKeyOn(const DspKeyOnInfo& koi) final;
    void doKeyOff() final;
    //bool playbackDone() const;
    sampleOsc _spOsc;
    //bool _playbackDone;
    float _filtp;
};

struct SWPLUSSHP : public DspBlock
{
    SWPLUSSHP( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    PolyBLEP _pblep;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SAWPLUS : public DspBlock
{
    SAWPLUS( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    PolyBLEP _pblep;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SINE : public DspBlock
{
    SINE( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    PolyBLEP _pblep;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SAW : public DspBlock
{
    SAW( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    PolyBLEP _pblep;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SQUARE : public DspBlock
{
    SQUARE( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    PolyBLEP _pblep;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SINEPLUS : public DspBlock
{
    SINEPLUS( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    PolyBLEP _pblep;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SHAPEMODOSC : public DspBlock
{
    SHAPEMODOSC( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    PolyBLEP _pblep;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct PLUSSHAPEMODOSC : public DspBlock
{
    PLUSSHAPEMODOSC( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    PolyBLEP _pblep;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SYNCM : public DspBlock
{
    SYNCM( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    float _phase;
    float _phaseInc;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct SYNCS : public DspBlock
{
    SYNCS( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    PolyBLEP _pblep;
    float _prvmaster;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};
struct PWM : public DspBlock
{
    PWM( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    PolyBLEP _pblep;
    void doKeyOn(const DspKeyOnInfo& koi) final;
};

struct fm4syn;

struct FM4 : public DspBlock
{
    FM4( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
    void doKeyOff() final;
    fm4syn* _fm4;
};

struct czsyn;

struct CZX : public DspBlock
{
    CZX( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
    void doKeyOff() final;
    czsyn* _cz;
};

struct NOISE : public DspBlock
{
    NOISE( const DspBlockData& dbd );
    void compute(DspBuffer& dspbuf) final;
    void doKeyOn(const DspKeyOnInfo& koi) final;
    void doKeyOff() final;
};