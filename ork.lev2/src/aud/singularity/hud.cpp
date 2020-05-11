#include <ork/lev2/aud/singularity/hud.h>
#include <ork/math/cvector3.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/material_freestyle.h>

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
using vtx_t        = SVtxV16T16C16;
using vtxbuf_t     = DynamicVertexBuffer<vtx_t>;
using vtxbuf_ptr_t = std::shared_ptr<vtxbuf_t>;
static vtxbuf_ptr_t create_vertexbuffer(Context* context) {
  auto vb = std::make_shared<vtxbuf_t>(16 << 20, 0, EPrimitiveType::NONE); // ~800 MB
  vb->SetRingLock(true);
  return vb;
}
static vtxbuf_ptr_t get_vertexbuffer(Context* context) {
  static auto vb = create_vertexbuffer(context);
  return vb;
}
///////////////////////////////////////////////////////////////////////////////
static freestyle_mtl_ptr_t create_hud_material(Context* context) {
  auto mtl = std::make_shared<FreestyleMaterial>();
  mtl->gpuInit(context, "orkshader://solid");
  return mtl;
}
static freestyle_mtl_ptr_t hud_material(Context* context) {
  static auto mtl = create_hud_material(context);
  return mtl;
}
///////////////////////////////////////////////////////////////////////////////
void Rect::PushOrtho(Context* context) const {
  int w = context->mainSurfaceWidth();
  int h = context->mainSurfaceHeight();
  context->MTXI()->PushUIMatrix(w, h);
}
void Rect::PopOrtho(Context* context) const {
  context->MTXI()->PopUIMatrix();
}

void drawtext(
    Context* context, //
    const std::string& str,
    float x,
    float y,
    float scale,
    float r,
    float g,
    float b) {

  int w = context->mainSurfaceWidth();
  int h = context->mainSurfaceHeight();
  context->MTXI()->PushUIMatrix(w, h);

  auto fontman = FontMan::instance();

  fontman->SetCurrentFont("i16");
  context->PushModColor(fcolor4(r, g, b, 1));

  fontman->beginTextBlock(context, str.length());
  fontman->DrawText(context, x, y, "%s", str.c_str());
  fontman->endTextBlock(context);
  context->PopModColor();

  context->MTXI()->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////

void drawHudLines(Context* context, const hudlines_t& lines) {

  if (lines.size() == 0)
    return;

  auto mtl = hud_material(context);
  auto tek = mtl->technique("vtxcolor");
  RenderContextFrameData RCFD(context);
  auto vbuf = get_vertexbuffer(context);
  auto mtxi = context->MTXI();
  auto gbi  = context->GBI();

  auto par_mvp = mtl->param("MatMVP");

  VtxWriter<SVtxV16T16C16> vw;
  vw.Lock(context, vbuf.get(), lines.size() * 2);
  for (auto& l : lines) {
    const auto& p1 = l._from;
    const auto& p2 = l._to;
    const auto& c  = l._color;
    vw.AddVertex(SVtxV16T16C16(fvec3(p1), fvec4(), c));
    vw.AddVertex(SVtxV16T16C16(fvec3(p2), fvec4(), c));
  }
  vw.UnLock(context);

  int w = context->mainSurfaceWidth();
  int h = context->mainSurfaceHeight();
  mtxi->PushUIMatrix(w, h);
  mtl->begin(tek, RCFD);
  mtl->bindParamMatrix(par_mvp, mtxi->RefMVPMatrix());
  mtl->_rasterstate.SetBlending(EBLENDING_OFF);
  gbi->DrawPrimitiveEML(vw, EPrimitiveType::LINES);
  mtl->end(RCFD);
  mtxi->PopUIMatrix();
}

///////////////////////////////////////////////////////////////////////////////

float FUNH(float vpw, float vph) {
  return (vph / 6);
}
float FUNW(float vpw, float vph) {
  float fh   = FUNH(vpw, vph);
  float funw = fh * 1.5;
  return funw;
}
float FUNX(float vpw, float vph) {
  return vpw - FUNW(vpw, vph);
}
float ENVW(float vpw, float vph) {
  return FUNW(vpw, vph) * 1.5;
}
float ENVH(float vpw, float vph) {
  return (vph / 5);
}
float ENVX(float vpw, float vph) {
  return FUNX(vpw, vph) - ENVW(vpw, vph) - 16;
}
float DSPW(float vpw, float vph) {
  return FUNW(vpw, vph);
}
float DSPX(float vpw, float vph) {
  return ENVX(vpw, vph) - (FUNW(vpw, vph) + 16);
}

///////////////////////////////////////////////////////////////////////////////

void synth::onDrawHud(Context* context, float width, float height) {

  if (_clearhuddata) {
    _hudsample_map.clear();
    _clearhuddata = false;
  }

  svar_t hdata;

  while (_hudbuf.try_pop(hdata)) {
    if (hdata.IsA<hudkframe>()) {
      _curhud_kframe = hdata.Get<hudkframe>();

      for (int i = 0; i < koscopelength; i++) {
        _oscopebuffer[i] = 0.0f;
      }
      for (int i = 0; i < koscopelength / 2; i++) {
        _fftbuffer[i] = 0.0f;
      }

    }

    else if (auto try_aframe = hdata.TryAs<hudaframe>()) {
      //////////////////////////////
      // OSCOPE INPUT
      //////////////////////////////

      auto& AFIN  = try_aframe.value();
      int inumfin = AFIN._oscopebuffer.size();

      int tailbegin = koscopelength - inumfin;

      memcpy(_oscopebuffer, _oscopebuffer + inumfin, tailbegin * 4);

      float* tailb = _oscopebuffer + tailbegin;
      for (int i = 0; i < inumfin; i++) {
        float inp = AFIN._oscopebuffer[i];
        tailb[i]  = inp;
      }
      _curhud_aframe._items.clear();
      _curhud_aframe = AFIN;
      AFIN._items.clear();
    }
  }

  if (_hudpage == 0) {
    this->onDrawHudPage2(context, width, height);
    this->onDrawHudPage3(context, width, height);
  } else if (_hudpage == 1)
    this->onDrawHudPage1(context, width, height);
}

///////////////////////////////////////////////////////////////////////////////

void synth::onDrawHudPage1(Context* context, float width, float height) {
}

///////////////////////////////////////////////////////////////////////////////

void synth::onDrawHudPage2(Context* context, float width, float height) {
  auto hudl = _hudLayer;

  if (false == (hudl && hudl->_LayerData))
    return;

  std::lock_guard<std::mutex> lock(hudl->_mutex);

  auto layd = _hudLayer->_LayerData;

  const hudaframe& HAF = _curhud_aframe;

  const auto& ENVCT = layd->_envCtrlData;
  bool useNENV      = ENVCT._useNatEnv;

  float fh   = FUNH(width, height);
  float envh = ENVH(width, height);
  float funW = FUNW(width, height);
  float funX = FUNX(width, height);
  float envX = ENVX(width, height);
  float envW = ENVW(width, height);

  ItemDrawReq EDR;
  Op4DrawReq OPR;
  auto& R  = EDR.rect;
  auto& R2 = OPR.rect;
  {
    R.VPW  = width;
    R.VPH  = height;
    R.X1   = envX;
    R.W    = envW;
    R.H    = envh;
    R2.VPW = width;
    R2.VPH = height;
    R2.X1  = envX;
    R2.W   = envW;
    R2.H   = envh;

    EDR.s  = this;
    OPR.s  = this;
    EDR.l  = hudl;
    EDR.ld = layd;

    int numitems = HAF._items.size();
    // printf( "HAF numitems<%d>\n", numitems);
    for (auto& item : HAF._items) {
      if (auto as_env = item.TryAs<envframe>()) {
        auto E    = as_env.value();
        R.X1      = envX;
        R.W       = envW;
        R.H       = envh;
        R.Y1      = envh * E._index;
        EDR._data = E;
        EDR.ienv  = E._index;
        DrawEnv(context, EDR);
      } else if (auto as_asr = item.TryAs<asrframe>()) {
        auto A    = as_asr.value();
        R.X1      = envX;
        R.W       = envW;
        R.H       = envh;
        R.Y1      = envh * (3 + A._index);
        EDR._data = A;
        EDR.ienv  = A._index;
        DrawAsr(context, EDR);
      } else if (auto as_lfo = item.TryAs<lfoframe>()) {
        auto L    = as_lfo.value();
        R.X1      = funX;
        R.W       = funW;
        R.H       = fh;
        R.Y1      = fh * L._index;
        EDR._data = L;
        EDR.ienv  = L._index;
        DrawLfo(context, EDR);
      } else if (auto as_fun = item.TryAs<funframe>()) {
        auto F    = as_fun.value();
        R.X1      = funX;
        R.W       = funW;
        R.H       = fh;
        R.Y1      = fh * (F._index + 2);
        EDR._data = F;
        EDR.ienv  = F._index;
        DrawFun(context, EDR);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void DrawBorder(Context* context, int X1, int Y1, int X2, int Y2, int color) {
  hudlines_t lines;
  fvec3 vcolor(1, 1, 1);
  switch (color) {
    case 0:
      vcolor = fvec3(0.6, 0.3, 0.6);
      break;
    case 1:
      vcolor = fvec3(0.0, 0.0, 0.0);
      break;
    case 2:
      vcolor = fvec3(0.9, 0.0, 0.0);
      break;
  }
  auto addline = [&lines, &vcolor](float xa, float ya, float xb, float yb) { //
    lines.push_back(HudLine{fvec2{xa, ya}, fvec2{xb, yb}, vcolor});
  };
  addline(X1, Y1, X2, Y1);
  addline(X2, Y1, X2, Y2);
  addline(X2, Y2, X1, Y2);
  addline(X1, Y2, X1, Y1);
  drawHudLines(context, lines);
}

///////////////////////////////////////////////////////////////////////////////

void synth::onDrawHudPage3(Context* context, float width, float height) {

  auto MTXI = context->MTXI();

  const hudkframe& HKF = _curhud_kframe;
  const hudaframe& HAF = _curhud_aframe;

  if (nullptr == HKF._layerdata)
    return;
  // if( nullptr == HKF._kmregion )
  //   return;
  if (nullptr == HKF._alg)
    return;

  auto layd = HKF._layerdata;

  float DSPx = DSPX(width, height);
  float FUNw = FUNW(width, height);
  float INSX = 100;
  float INSW = DSPx - (INSX + 16);

  drawtext(context, FormatString("ostrack<%g>", _ostrack), 100, 250, fontscale, 1, 1, 0);

  _ostrackPH += _ostrack;

  int inumframes = koscopelength;

  if (_ostrack > 0 and _ostrackPH > inumframes)
    _ostrackPH = 0;
  if (_ostrack < 0 and _ostrackPH < -inumframes)
    _ostrackPH = 0;

  const float* ldata = _oscopebuffer;

  const float OSC_X1 = 100;
  const float OSC_Y1 = 250;
  const float OSC_W  = INSW;
  const float OSC_H  = 500;
  const float OSC_HH = OSC_H * 0.5;
  const float OSC_CY = OSC_Y1 + OSC_HH;
  const float OSC_X2 = OSC_X1 + OSC_W;
  const float OSC_Y2 = OSC_Y1 + OSC_H;

  const float ANA_X1 = 100;
  const float ANA_Y1 = 800;
  const float ANA_W  = INSW;
  const float ANA_H  = 600;
  const float ANA_HH = ANA_H * 0.5;
  const float ANA_CY = ANA_Y1 + ANA_HH;
  const float ANA_X2 = ANA_X1 + ANA_W;
  const float ANA_Y2 = ANA_Y1 + ANA_H;

  MTXI->PushUIMatrix(width, height);

  const size_t fftSize = inumframes; // Needs to be power of 2!

  std::vector<float> input(fftSize, 0.0f);
  std::vector<float> re(audiofft::AudioFFT::ComplexSize(fftSize));
  std::vector<float> im(audiofft::AudioFFT::ComplexSize(fftSize));
  std::vector<float> output(fftSize);

  /////////////////////////////////////////////
  // oscilloscope centerline
  /////////////////////////////////////////////

  hudlines_t lines;
  lines.push_back(HudLine{fvec2(OSC_X1, OSC_H), fvec2(OSC_X2, OSC_H), fvec3(1, 1, 1)});

  /////////////////////////////////////////////
  // oscilloscope trace
  /////////////////////////////////////////////

  int i     = 0;
  auto mapI = [&](int i) -> int {
    int j = i + _ostrackPH;

    if (j < 0)
      j += inumframes;
    else if (j >= inumframes)
      j -= inumframes;

    assert(j >= 0);
    assert(j < inumframes);
    return j;
  };

  float x1 = OSC_X1;
  float y1 = OSC_Y1 + OSC_HH + ldata[mapI(i)] * OSC_HH;
  float x2, y2;

  const int koscfr = inumframes / 4;

  for (i = 0; i < inumframes; i++) {
    int j = mapI(i);

    float s = ldata[j];

    float win_num = pi2 * float(i);
    float win_den = inumframes - 1;
    float win     = 0.5f * (1 - cosf(win_num / win_den));
    // printf( "win<%d> : %f\n", i, win );
    float s2 = ldata[i];
    input[i] = s2 * win;

    float x = OSC_W * float(i) / float(koscfr);
    float y = OSC_HH - s * OSC_HH;

    x2 = x + OSC_X1;
    y2 = y + OSC_HH;

    if (i < koscfr)
      lines.push_back(HudLine{fvec2(x1, y1), fvec2(x2, y2), fvec3(.3, 1, .3)});

    x1 = x2;
    y1 = y2;
  }

  //////////////////////////////
  // do the FFT
  //////////////////////////////

  audiofft::AudioFFT fft;
  fft.init(fftSize);
  fft.fft(input.data(), re.data(), im.data());

  //////////////////////////////
  // map bin -> Y
  //////////////////////////////

  auto mapDecibels = [&](float re, float im) -> float {
    float mag = re * re + im * im;
    float dB  = 10.0f * log_base(10.0f, mag) - 6.0f;
    return dB;
  };
  auto mapFFTY = [&](float dbin) -> float {
    float dbY = (dbin + 96.0f) / 132.0f;
    float y   = ANA_Y2 - dbY * ANA_H;
    if (y > ANA_Y2)
      y = ANA_Y2;
    return y;
  };

  //////////////////////////////
  // draw grid
  //////////////////////////////

  for (int dB = 36; dB >= -96; dB -= 12) {
    lines.push_back(HudLine{
        fvec2(ANA_X1, mapFFTY(dB)), //
        fvec2(ANA_X2, mapFFTY(dB)),
        fvec3(.2, .1, .3)}); // dB grid
  }

  constexpr int KMAXNOTE     = 12 * 11;
  constexpr float KFIMAXNOTE = 1.0f / float(KMAXNOTE);
  for (int note = 0; note <= KMAXNOTE; note += 12) {
    float x = ANA_X1 + ANA_W * float(note) * KFIMAXNOTE;
    lines.push_back(HudLine{
        fvec2(x, ANA_Y1), //
        fvec2(x, ANA_Y2),
        fvec3(.2, .1, .3)}); // vertical grid
  }

  //////////////////////////////
  // spectral plot
  //////////////////////////////

  float dB = mapDecibels(re[0], im[0]);
  x1       = ANA_X1; // + 500 + ANA_W * float(0) / float(inumframes);
  y1       = mapFFTY(dB);
  for (int i = 0; i < inumframes / 2; i++) {

    float dB = mapDecibels(re[i], im[i]);
    _fftbuffer[i] += dB * 0.03 + 0.0001f;
    _fftbuffer[i] *= 0.97f;
    dB = _fftbuffer[i];

    // printf( "dB<%f>\n", dB);
    float fi = float(i) / float(inumframes);

    float frq      = fi * getSampleRate();
    float midinote = frequency_to_midi_note(frq);
    // printf("i<%d> frq<%g> midinote<%g>\n", i, frq, midinote);
    float x2 = ANA_X1 + ANA_W * midinote * KFIMAXNOTE;
    float y2 = mapFFTY(dB - 12);
    lines.push_back(HudLine{
        fvec2(x1, y1), //
        fvec2(x2, y2),
        fvec3(.3, .7, 1)}); // spectral plot

    x1 = x2;
    y1 = y2;
  }
  // freqbins[index] = complex_t(0,0);

  //////////////////////////////

  DrawBorder(context, OSC_X1, OSC_Y1, OSC_X2, OSC_Y2);
  // DrawBorder(context, ANA_X1, ANA_Y1, ANA_X2, ANA_Y2);

  MTXI->PopUIMatrix();

  for (int i = 36; i >= -96; i -= 12) {
    float db0 = i;
    float y   = mapFFTY(db0);

    drawtext(context, FormatString("%g dB", db0), 40, y + 10, fontscale, .6, 0, .8);
  }
  for (int n = 0; n < 108; n += 12) {
    float x = ANA_X1 - 20 + ANA_W * float(n) / 108.0;
    float f = midi_note_to_frequency(n + 36);
    drawtext(context, FormatString("  midi\n   %d\n(%d hz)", n + 36, int(f)), x, ANA_Y2 + 30, fontscale, .6, 0, .8);
  }

  //////////////////////////////
  // draw DSP blocks
  //////////////////////////////

  auto& algd = layd->_algData;
  auto alg   = HKF._alg;

  float xb    = DSPx;
  float yb    = 90;
  float dspw  = FUNw;
  float dsph  = 45;
  float dsphp = 165;
  float yinc  = dsph + 3;

  // glColor4f(.7, .7, .3, 1);

  yb      = 90;
  int ytb = 95;

  auto alghdr = FormatString("DSP Algorithm: %s", algd._name.c_str());
  drawtext(context, alghdr, xb + 80, yb - 10, fontscale, 1, 1, 1);

  //////////////////////

  auto PanPadOut = [context](const std::string& hdr, const DspBlockData* dbd, int xt, int yt) -> int {
    assert(dbd);
    // float v14DB = dbd->_v14Gain;
    float padDB = linear_amp_ratio_to_decibel(dbd->_inputPad);
    auto text   = FormatString("[%s] PAD<%g dB>", hdr.c_str(), padDB);
    drawtext(context, text, xt, yt, fontscale, 1, .2, .2);

    int h = 20;

    // if( dbd->_name == "F3" or dbd->_name == "F4" )
    {
      // text = FormatString("V15<0x%02x>",dbd._var15);
      // drawtext( text, xt, yt+=20, fontscale, 0.8,0.8,1 );

#if 0
        std::string panmode;
        switch( dbd->_panMode )
        {
            case 0:
              panmode = "Fixed";
              break;
            case 1:
              panmode = "MIDI+";
              break;
            case 2:
              panmode = "Auto";
              break;
            case 3:
              panmode = "Revrs";
              break;
        }

        text = FormatString("[%s] Pan<%d> Mode<%s>",hdr.c_str(), dbd->_pan, panmode.c_str() );
        drawtext( text, xt, yt+20, fontscale, 1,1,0.2 );
#endif
      h += 20;
    }
    return h;
  };

  //////////////////////

  int iblockID = 0;

  struct blockrect {
    int y1             = 0;
    int y2             = 0;
    DspBlock* dspblock = nullptr;
    bool enabled       = false;
  };

  std::vector<blockrect> _blockrects;

  for (int i = 0; i < kmaxdspblocksperlayer; i++) {
    auto dbd = layd->_dspBlocks[i];

    if (nullptr == dbd)
      continue;

    int xt        = xb + 10;
    int yt        = ytb + 30;
    int block_top = ytb;

    auto name = dbd->_dspBlock;
    if (name == "")
      continue;

    // auto schm = dbd->_paramScheme;
    auto text = FormatString("BLOCK: %s ", name.c_str());
    drawtext(context, text, xt, yt, fontscale, 1, 1, 1);
    yt += 20;
    // yt += PanPadOut( dbd, xt, yt );

    auto blk = alg->_block[i];

    int numparam = 1;

    if (blk) {
      int controlBlockID = i;
      bool block_ena     = _fblockEnable[controlBlockID];

      int fidx = blk->_baseIndex;

      int padDB = round(linear_amp_ratio_to_decibel(dbd->_inputPad));

      text = FormatString("INP<%d> OUT<%d> PAD<%d dB>", blk->numInputs(), blk->numOutputs(), padDB);
      drawtext(context, text, xt, yt, fontscale, 1, 1, 1);
      yt += 20;

      auto drawfhud = [&blk, //
                       &xt,
                       &yt,
                       &controlBlockID,
                       &layd,
                       context]( //
                          int idx,
                          float r,
                          float g,
                          float b) {
        const DspBlockData* dbd = &blk->_dbd;
        const DspParamData& dpd = dbd->_paramd[idx];

        float tot  = blk->_fval[idx];
        auto& CTRL = blk->_param[idx];

        const auto& SRC1   = dpd._mods._src1;
        float SRC1D        = dpd._mods._src1Depth;
        const auto& SRC2   = dpd._mods._src2;
        float SRC2minD     = dpd._mods._src2MinDepth;
        float SRC2maxD     = dpd._mods._src2MaxDepth;
        const auto& SRC2DC = dpd._mods._src2DepthCtrl;

        float coa   = CTRL._coarse;
        float fin   = CTRL._fine;
        float s1    = CTRL._C1();
        float s2    = CTRL._C2();
        float ko    = CTRL._keyOff;
        float kt    = CTRL._keyTrack;
        float kv    = CTRL._kval;
        float vo    = CTRL._unitVel;
        float vv    = CTRL._vval;
        int ks      = CTRL._kstartNote;
        char paramC = 'A' + idx;
        std::string text;

        if (fabs(tot) > 1)
          text = FormatString("P%c<%0.1f> _UV<%0.2f> vv<%0.2f>", paramC, tot, vo, vv);
        else
          text = FormatString("P%c<%g> _UV<%0.2f> vv<%0.2f>", paramC, tot, vo, vv);

        drawtext(context, text, xt, yt, fontscale, r, g, b);
        yt += 20;

        drawtext(context, FormatString("   coarse<%g> fine<%g>", coa, fin), xt, yt, fontscale, r, g, b);
        yt += 20;

        text = (fabs(s1) > 1) ? FormatString("   src1<%s> dep<%g> val<%0.1f>", SRC1.c_str(), SRC1D, s1)
                              : FormatString("   src1<%s> dep<%g> val<%0.2g>", SRC1.c_str(), SRC1D, s1);
        drawtext(context, text, xt, yt, fontscale, r, g, b);
        yt += 20;

        text = FormatString("   src2DC<%s> min<%g> max<%g> ", SRC2DC.c_str(), SRC2minD, SRC2maxD);
        drawtext(context, text, xt, yt, fontscale, r, g, b);
        yt += 20;

        text = (fabs(s2) > 1) ? FormatString("   src2<%s> val<%0.1f>", SRC2.c_str(), s2)
                              : FormatString("   src2<%s> val<%g>", SRC2.c_str(), s2);
        drawtext(context, text, xt, yt, fontscale, r, g, b);
        yt += 20;

        text = FormatString("   _ks<%d> _ko<%g> _kt<%g> _kv<%g>", ks, ko, kt, kv);
        drawtext(context, text, xt, yt, fontscale, r, g, b);
        yt += 20;
      };

      static const int kmaxcolors = 4;
      fvec3 colors[kmaxcolors]    = {
          fvec3(.7, .7, 1),
          fvec3(1, 1, 1),
          fvec3(1, .7, .7),
          fvec3(0.8, 0.7, 0.8),
      };

      assert(blk->_numParams <= 8);
      for (int p = 0; p < blk->_numParams; p++) {
        yt += 20;
        auto& C = colors[p % kmaxcolors];
        drawfhud(p, C.x, C.y, C.z);
      }

      numparam = blk->_numParams;

      int blockh = (dsph + dsphp * numparam) + 35;

      blockrect br;
      br.y1       = block_top;
      br.y2       = yb + blockh;
      br.dspblock = blk;
      br.enabled  = block_ena;

      _blockrects.push_back(br);
    }

    int blockh = (dsph + dsphp * numparam) + 35;

    yb += blockh;
    ytb += blockh;
  }

  yb = 90;
  MTXI->PushUIMatrix(width, height);

  /////////////////////////////////
  // draw dspblock borders
  /////////////////////////////////

  for (auto brect : _blockrects) {
    auto blk = brect.dspblock;

    const DspBlockData* dbd = &blk->_dbd;

    int color = 2;
    if (blk)
      color = brect.enabled ? 0 : 1;

    DrawBorder(context, xb, brect.y1, xb + dspw, brect.y2, color);

    yb = brect.y2;
  }

  MTXI->PopUIMatrix();

  //////////////////////

  // const auto& F3D = layd->_fBlock[3];
  // const auto& F4D = layd->_fBlock[4];

  float panx = DSPx + 16;
  // PanPadOut( "U", &F3D, panx,yb+40 );
  // PanPadOut( "L/S", &F4D, panx,yb+100 );

  if (HKF._miscText.length()) {

    drawtext(context, HKF._miscText, panx, yb + 160, fontscale, 1, 1, 1);
  }

  drawHudLines(context, lines);
}
} // namespace ork::audio::singularity
