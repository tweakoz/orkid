#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_nonlin.h>

namespace ork::audio::singularity {

float shaper(float inp, float adj);
float wrap(float inp, float adj);

///////////////////////////////////////////////////////////////////////////////

SHAPER::SHAPER(const DspBlockData& dbd)
    : DspBlock(dbd) {
}

void SHAPER::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd._inputPad;
  int inumframes = dspbuf._numframes;

  float amt = _param[0].eval(); //,0.01f,100.0f);
  _fval[0]  = amt;

  float* inpbuf = getInpBuf1(dspbuf);

  // float la = decibel_to_linear_amp_ratio(amt);
  if (1)
    for (int i = 0; i < inumframes; i++) {
      float s1 = shaper(inpbuf[i] * pad, amt);
      output1(dspbuf, i, s1);
    }
}

///////////////////////////////////////////////////////////////////////////////

SHAPE2::SHAPE2(const DspBlockData& dbd)
    : DspBlock(dbd) {
}

void SHAPE2::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd._inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = dspbuf.channel(0);
  float amt      = _param[0].eval();
  _fval[0]       = amt;
  if (1)
    for (int i = 0; i < inumframes; i++) {
      float s1 = shaper(ubuf[i] * pad, amt);
      float s2 = shaper(s1, amt * 0.75);
      ubuf[i]  = s2;
    }
}

///////////////////////////////////////////////////////////////////////////////

TWOPARAM_SHAPER::TWOPARAM_SHAPER(const DspBlockData& dbd)
    : DspBlock(dbd) {
}

/*float shaper(float inp, float adj)
{
    float index = pi*4.0f*inp*adj;
    return sinf(index); ///adj;
}*/

void TWOPARAM_SHAPER::doKeyOn(const DspKeyOnInfo& koi) {
  ph1 = 0.0f;
  ph2 = 0.0f;
}

void TWOPARAM_SHAPER::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd._inputPad;
  int inumframes = dspbuf._numframes;
  float* ubuf    = dspbuf.channel(0);
  float evn      = _param[0].eval();
  float odd      = _param[1].eval();

  // evn = -22;//(0.5f+sinf(ph1+pi)*0.5f)*-60.0f;
  // odd = (sinf(ph1)*30.f)-30.0f;
  ph1 += 0.0003f;

  _fval[0] = evn;
  _fval[1] = odd;
  // printf( "_dbd._inputPad<%f>\n", _dbd._inputPad );
  if (1)
    for (int i = 0; i < inumframes; i++) {
      float u   = ubuf[i] * pad;
      float usq = u * u;
      float le  = usq * decibel_to_linear_amp_ratio(evn);
      float lo  = u * decibel_to_linear_amp_ratio(odd);

      // float e = (2.0f*powf(le,2.0f))-1.0f;
      // float e = ((2.0f*powf(le,2.0f))-1.0f)*1000.0f;//decibel_to_linear_amp_ratio(-evn);
      float index = clip_float(le * 6, -12, 12); // clip_float(powf(le,4),-12.0f,12.0f);
      // index *= index;
      float e = sinf(index * pi2); /// adj;

      index   = clip_float(lo * 6, -12.0f, 12.0f);
      float o = sinf(index * pi2); /// adj;

      float r = (e + o) * 0.5f;
      // r = wrap(r,-30);
      ubuf[i] = r;
    }
}

///////////////////////////////////////////////////////////////////////////////

WRAP::WRAP(const DspBlockData& dbd)
    : DspBlock(dbd) {
}

void WRAP::compute(DspBuffer& dspbuf) // final
{
  int inumframes = dspbuf._numframes;
  float* inpbuf  = getInpBuf1(dspbuf);
  float rpoint   = _param[0].eval(); //,-100,100.0f);
  _fval[0]       = rpoint;
  if (1)
    for (int i = 0; i < inumframes; i++) {
      output1(dspbuf, i, wrap(inpbuf[i], rpoint));
    }
}

///////////////////////////////////////////////////////////////////////////////

DIST::DIST(const DspBlockData& dbd)
    : DspBlock(dbd) {
}

void DIST::compute(DspBuffer& dspbuf) // final
{
  float pad      = _dbd._inputPad;
  int inumframes = dspbuf._numframes;
  float* inpbuf  = getInpBuf1(dspbuf);
  float adj      = _param[0].eval();
  _fval[0]       = adj;
  float ratio    = decibel_to_linear_amp_ratio(adj + 30.0) * pad;

  if (1)
    for (int i = 0; i < inumframes; i++) {
      float v = inpbuf[i] * ratio;
      v       = softsat(v, 1);
      output1(dspbuf, i, v);
    }
}

} // namespace ork::audio::singularity
