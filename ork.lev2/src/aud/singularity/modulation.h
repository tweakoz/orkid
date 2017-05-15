#pragma once

///////////////////////////////////////////////////////////////////////////////

struct FPARAM;

typedef std::function<float()> controller_t;
typedef std::function<float(float)> mapper_t;
typedef std::function<float(FPARAM& cec)> evalit_t;

///////////////////////////////////////////////////////////////////////////////

struct FPARAM
{
    FPARAM();
    void reset();
    void keyOn( int ikey, int ivel );
    float eval(bool dump=false);

    controller_t _C1;
    controller_t _C2;
    float _coarse = 0.0f;
    float _fine = 0.0f;
    float _keyTrack=0;
    float _velTrack=0;
    float _keyOff = 0.0f;
    float _unitVel = 0.0f;
    int _kstartNote = 60;
    bool _kstartBipolar = false;

    float _s1val = 0.0f;
    float _s2val = 0.0f;
    float _kval = 0.0f;
    float _vval = 0.0f;

    evalit_t _evaluator;
};

