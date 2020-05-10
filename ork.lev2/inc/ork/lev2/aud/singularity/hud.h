#pragma once

#include <ork/lev2/aud/singularity/synth.h>
#include <ork/kernel/svariant.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>
#include <GLFW/glfw3.h>

//#include "drawtext.h"

#include "krzdata.h"
#include "synth.h"
#include "fft.h"

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

typedef ork::svar1024_t svar_t;
void drawtext(const std::string& str, float x, float y, float scale, float r, float g, float b);

///////////////////////////////////////////////////////////////////////////////

struct Rect {
  int X1;
  int Y1;
  int W;
  int H;
  int VPW;
  int VPH;

  void PushOrtho() const;
  void PopOrtho() const;
};

///////////////////////////////////////////////////////////////////////////////

struct ItemDrawReq {
  synth* s;
  int ldi;
  int ienv;
  lyrdata_constptr_t ld;
  const layer* l;
  ork::svar256_t _data;
  Rect rect;

  bool shouldCollectSample() const {
    return ((s->_lnoteframe >> 3) % 3 == 0);
  }
};

///////////////////////////////////////////////////////////////////////////////

struct Op4DrawReq {
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
float FUNH(float vpw, float vph);
float FUNW(float vpw, float vph);
float FUNX(float vpw, float vph);
float ENVW(float vpw, float vph);
float ENVH(float vpw, float vph);
float ENVX(float vpw, float vph);
float DSPW(float vpw, float vph);
float DSPX(float vpw, float vph);
void DrawBorder(int X1, int Y1, int X2, int Y2, int color = 0);

///////////////////////////////////////////////////////////////////////////////

static const float fontscale = 0.40;

} // namespace ork::audio::singularity
