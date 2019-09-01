#pragma once

namespace ork::audio::singularity {

struct KeyOnInfo;
struct DspKeyOnInfo;

///////////////////////////////////////////////////////////////////////////////

struct NatEnv
{
    NatEnv();
    void keyOn(const KeyOnInfo& KOI,const sample* s);
    void keyOff();
    const natenvseg& getCurSeg() const;
    float compute();
    void initSeg(int iseg);

    std::vector<natenvseg> _natenvseg;
    layer* _layer;
    int _curseg;
    int _prvseg;
    int _numseg;
    int _framesrem;
    float _segtime;
    float _curamp;
    float _slopePerSecond;
    float _slopePerSample;
    float _SR;
    bool _ignoreRelease;
    int _state;
    float _atkAdjust;
    float _decAdjust;
    float _relAdjust;
};

///////////////////////////////////////////////////////////////////////////////

struct sampleOsc
{
    sampleOsc();

    void keyOn(const DspKeyOnInfo& koi);
    void keyOff();

    void findRegion(const DspKeyOnInfo& koi);

    void updateFreqRatio();
    void setSrRatio(float r);
    void compute(int inumfr);
    float playNoLoop();
    float playLoopFwd();
    float playLoopBid();
    //bool playbackDone() const;

    //typedef float(sampleOsc::*pbfunc_t)();
    //pbfunc_t _pbFunc = nullptr;

    float(sampleOsc::*_pbFunc)() = nullptr;

    //
    int64_t _blk_start;
    int64_t _blk_alt;
    int64_t _blk_loopstart;
    int64_t _blk_loopend;
    int64_t _blk_end;

    static constexpr float kinv64k = 1.0f/65536.0f;
    static constexpr float kinv32k = 1.0f/32768.0f;

    const sample* _sample;

    int _sampselnote;
    int _sampleRoot;

    float _playbackRate;

    int64_t _pbindex;
    int64_t _pbindexNext;

    float _keyoncents;
    int64_t _pbincrem;

    float _dt;
    float _synsr;
    //bool _isLooped;
    bool _enableNatEnv;
    eLoopMode _loopMode;
    bool _active;
    bool _forwarddir;
    int _loopCounter;
    float _curSampSRratio;
    float _NATENV[1024];
    float _OUTPUT[1024];
    int _keydiff;
    int _kmcents;
    int _pchcents;
    int _curpitchadjx;
    int _curpitchadj;
    int _curcents;
    float _baseCents;
    float _preDSPGAIN;
    int _samppbnote;

    float _curratio;

    NatEnv _natAmpEnv;

    //synth& _syn;
    layer* _lyr;
    bool _released;
    const kmregion* _kmregion;

};

} // namespace ork::audio::singularity {
