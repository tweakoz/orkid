#pragma once

#include "alg.h"
#include "controller.h"

struct KeyOnInfo;
struct AsrData;
struct RateLevelEnvData;

///////////////////////////////////////////////////////////////////////////////

struct envframe
{
    std::string _name;
    int _index = 0;
    float _value = 0.0f;
    int _curseg = 0;
    const RateLevelEnvData* _data = nullptr;
};
struct asrframe
{
    int _index = 0;
    float _value = 0.0f;
    int _curseg = 0;
    const AsrData* _data = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct AsrInst : public ControllerInst
{
    AsrInst(synth& syn, const AsrData* data);
    void compute(int inumfr) final;
    void keyOn(const KeyOnInfo& KOI) final;
    void keyOff() final;
    ////////////////////////////
    void initSeg(int iseg);
    bool isValid() const;
    const AsrData* _data;
    int _curseg;
    int _mode;
    float _atkAdjust;
    float _relAdjust;
    float _dstval;
    int _framesrem;
    bool _released;
    bool _ignoreRelease;
    float _curslope_persamp;
};

///////////////////////////////////////////////////////////////////////////////

struct RateLevelEnvInst : public ControllerInst
{
    RateLevelEnvInst(synth& syn, const RateLevelEnvData* data);
    void compute(int inumfr) final;
    void keyOn(const KeyOnInfo& KOI) final;
    void keyOff() final;
    ////////////////////////////
    void initSeg(int iseg);
    bool done() const;
    const RateLevelEnvData* _data;
    layer* _layer;
    int _curseg;
    float _atkAdjust;
    float _decAdjust;
    float _relAdjust;
    float _filtval;
    float _dstval;
    int _framesrem;
    bool _released;
    bool _ignoreRelease;
    float _curslope_persamp;
    bool _ampenv;
    bool _bipolar;
    RlEnvType _envType;

    float _USERAMPENV[1024];
};
