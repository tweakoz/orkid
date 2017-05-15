#pragma once

#include <ork/kernel/svariant.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <GLFW/glfw3.h>
#include "drawtext.h"

#include "krzdata.h"
#include "synth.h"
#include "fft.h"

///////////////////////////////////////////////////////////////////////////////

typedef ork::svar1024_t svar_t;

///////////////////////////////////////////////////////////////////////////////

struct Rect
{
    int X1;
    int Y1;
    int W;
    int H;
    int VPW;
    int VPH;

    inline void PushOrtho() const
    {
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0,VPW,VPH,0,0,1);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

    }
    inline void PopOrtho() const
    {
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
    }
};

///////////////////////////////////////////////////////////////////////////////

struct ItemDrawReq
{
    synth* s;
    int ldi;
    int ienv;
    const layerData* ld;
    const layer* l;
    ork::svar256_t _data;
    Rect rect;

    bool shouldCollectSample() const
    {
        return ( (s->_lnoteframe>>3)%3 == 0 );
    }
};

///////////////////////////////////////////////////////////////////////////////

struct Op4DrawReq
{
    synth* s;
    int iop;
    Rect rect;
};

///////////////////////////////////////////////////////////////////////////////

void DrawEnv(const ItemDrawReq& EDR);
void DrawAsr(const ItemDrawReq& EDR);
void DrawLfo(const ItemDrawReq& EDR);
void DrawFun(const ItemDrawReq& EDR);
void DrawOp4(const Op4DrawReq& OPR);
void PushOrtho(float VPW, float VPH);
void PopOrtho();
float FUNH(float vpw,float vph);
float FUNW(float vpw,float vph);
float FUNX(float vpw,float vph);
float ENVW(float vpw,float vph);
float ENVH(float vpw,float vph);
float ENVX(float vpw,float vph);
float DSPW(float vpw,float vph);
float DSPX(float vpw,float vph);
void DrawBorder(int X1, int Y1, int X2, int Y2, int color=0);

///////////////////////////////////////////////////////////////////////////////

static const float fontscale = 0.40;
