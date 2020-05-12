#pragma once

#include <ork/lev2/aud/singularity/synth.h>
#include <ork/kernel/svariant.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include "krzdata.h"
#include "synth.h"
#include "fft.h"
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

using vtx_t        = lev2::SVtxV16T16C16;
using vtxbuf_t     = lev2::DynamicVertexBuffer<vtx_t>;
using vtxbuf_ptr_t = std::shared_ptr<vtxbuf_t>;
vtxbuf_ptr_t get_vertexbuffer(lev2::Context* context);
lev2::freestyle_mtl_ptr_t hud_material(lev2::Context* context);

///////////////////////////////////////////////////////////////////////////////

typedef ork::svar1024_t svar_t;
void drawtext(
    lev2::Context* ctx, //
    const std::string& str,
    float x,
    float y,
    float scale,
    float r,
    float g,
    float b);

struct HudLine {

  fvec2 _from;
  fvec2 _to;
  fvec3 _color;
};
using hudlines_t = std::vector<HudLine>;

void drawHudLines(lev2::Context* context, const hudlines_t& lines);

///////////////////////////////////////////////////////////////////////////////

struct Rect {
  int X1;
  int Y1;
  int W;
  int H;
  int VPW;
  int VPH;

  void PushOrtho(lev2::Context* context) const;
  void PopOrtho(lev2::Context* context) const;
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

void DrawOscope(
    lev2::Context* context, //
    const hudaframe& HAF,
    const float* samples,
    fvec2 xy,
    fvec2 wh);

void DrawSpectra(
    lev2::Context* context, //
    const hudaframe& HAF,
    const float* samples,
    fvec2 xy,
    fvec2 wh);

void DrawEnv(lev2::Context* context, const ItemDrawReq& EDR);
void DrawAsr(lev2::Context* context, const ItemDrawReq& EDR);
void DrawLfo(lev2::Context* context, const ItemDrawReq& EDR);
void DrawFun(lev2::Context* context, const ItemDrawReq& EDR);
void DrawOp4(lev2::Context* context, const Op4DrawReq& OPR);
float FUNH(float vpw, float vph);
float FUNW(float vpw, float vph);
float FUNX(float vpw, float vph);
float ENVW(float vpw, float vph);
float ENVH(float vpw, float vph);
float ENVX(float vpw, float vph);
float DSPW(float vpw, float vph);
float DSPX(float vpw, float vph);
void DrawBorder(lev2::Context* context, int X1, int Y1, int X2, int Y2, int color = 0);

///////////////////////////////////////////////////////////////////////////////

static const float fontscale = 0.40;

} // namespace ork::audio::singularity
