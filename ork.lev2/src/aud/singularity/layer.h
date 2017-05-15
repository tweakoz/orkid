#pragma once

#include <mutex>
#include "alg.h"
#include "sampleOsc.h"
#include "envelope.h"
#include "konoff.h"

///////////////////////////////////////////////////////////////////////////////

static const int koscopelength = 4096;

struct lfoframe
{
    int _index = 0;
    float _value = 0.0f;
    float _currate = 1.0f;
    const LfoData* _data = nullptr;
};
struct funframe
{
    int _index = 0;
    float _value = 0.0f;
    float _a = 0.0f;
    float _b = 0.0f;
    const FunData* _data = nullptr;
};
struct op4frame
{   
    float _envout = 0.0f;
    int _envph = 0;
    float _mi = 0.0f;
    float _r = 0.0f;
    float _f = 0.0f;
    int _ar = 0;
    int _d1r = 0;
    int _d1l = 0;
    int _d2r = 0;
    int _rr = 0;
    int _egshift = 0;
    int _wav = 0;
    int _olev = 0;
};
struct hudaframe
{
    hudaframe() : _oscopebuffer(256) {}
    std::vector<float> _oscopebuffer;

    std::vector<ork::svar256_t> _items;

    op4frame _op4frame[4];
};
struct hudkframe
{
    int _note = 0;
    int _vel = 0;
    int _layerIndex = -1;
    const layerData* _layerdata = nullptr;
    const kmregion* _kmregion = nullptr;
    Alg* _alg = nullptr;
    bool _useFm4 = false;
    
    std::string _miscText;

};

///////////////////////////////////////////////////////////////////////////////

struct layer
{
    layer(synth& syn);
    ~layer();

    void compute(outputBuffer& obuf);
    void keyOn( int note, int vel, const layerData* ld );
    void keyOff();
    void reset();
    controller_t getController(const std::string& n) const;

    controller_t getSRC1(const BlockModulationData& mods);
    controller_t getSRC2(const BlockModulationData& mods);

    void updateSampSRRatio();

    void retain();
    void release();
    bool isDone() const { return _keepalive<=0; }
    bool isHudLayer() const;
    
    std::mutex _mutex;

    synth& _syn;
    int _curnote;
    int _curvel;
    int _ldindex;
    float _layerGain;
    float _curPitchOffsetInCents;
    float _centsPerKey;
    int _lyrPhase;
    bool _useNatEnv;
    bool _ignoreRelease;

    int _curCentsOSC;

    float _pchc1;
    float _pchc2;
    float _sinrepPH = 0.0f;
    bool _doNoise;
    float _masterGain = 0.0f;
    float* _AENV;
    float _USERAMPENV[1024];
    float _layerTime;

    ControlBlockInst* _ctrlBlock[kmaxctrlblocks] = {0,0,0,0,0,0,0,0};

    std::map<std::string,ControllerInst*> _controlMap;
    Alg* _alg;
    
    outputBuffer _layerObuf;

    hudkframe _HKF;
    hudaframe _HAF;
    int _HAF_nenvseg;

    const layerData* _layerData;

private:

    int _keepalive;

};