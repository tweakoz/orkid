#pragma once

namespace ork::audio::singularity {

struct ShelveEq
{
    void init();
    float SPN;
    float x1l,x2l;
    float x1r,x2r;
    float yl,y1l,y2l;
    float yr,y1r,y2r;
    float a0, a1, a2, b1, b2;
};
struct LoShelveEq : public ShelveEq
{
    void set( float fc, float peakg );
    float compute(float inp);
};
struct HiShelveEq : public ShelveEq
{
    void set( float cf, float peakg );
    float compute(float inp);
};

}
