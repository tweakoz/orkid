#include <stdio.h>
#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include <ork/file/file.h>
#include <ork/kernel/string/string.h>
#include <stdint.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/cz101.h>
#include <ork/lev2/aud/singularity/fmosc.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/kernel/string/string.h>

using namespace ork;

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
struct czpriv {
  /////////////////////////////
  czpriv()
      : _newnote(false) {
  }
  /////////////////////////////
  void keyOn(layer* l, const CzProgData& prgdat) {
    printf("cz keyon prog<%s>\n", prgdat._name.c_str());
    _data     = prgdat;
    _curlayer = l;
    _newnote  = true;
    _note     = l->_curnote;
    DspKeyOnInfo koi;
    /////////////////////////////
    // testprg
    /////////////////////////////
    if (0) {
      _data._lineSel              = 0;
      _data._lineMod              = 0;
      _data._oscData[0]._dcoWaveA = 5;
      _data._oscData[0]._dcoWaveB = 5;
    }
    /////////////////////////////

    switch (_data._lineSel) {
      case 0: // 1
        _osc[0].keyOn(koi, _data._oscData[0]);
        break;
      case 1: // 2
        _osc[0].keyOn(koi, _data._oscData[1]);
        break;
      case 2: // 1+1'
        _osc[0].keyOn(koi, _data._oscData[0]);
        _osc[1].keyOn(koi, _data._oscData[0]);
        break;
      case 3: // 1+2'
        _osc[0].keyOn(koi, _data._oscData[0]);
        _osc[1].keyOn(koi, _data._oscData[1]);
        break;
      default:
        assert(false);
    }
    _ph = 0.0f;
  }
  /////////////////////////////
  void keyOff() {
  }
  /////////////////////////////
  void compute(DspBuffer& dspbuf) {
    int inumframes = dspbuf._numframes;
    float* U       = dspbuf.channel(0);
    float f1       = midi_note_to_frequency(float(_note));
    float mi       = 0.5f + sinf(_ph) * 0.5f;
    if (_data._lineSel < 2) {
      for (int i = 0; i < inumframes; i++) {
        float s0 = _osc[0].compute(f1, mi);
        U[i]     = s0;
      }
    } else {
      float f2 = midi_note_to_frequency(float(_note) + float(_data._detuneCents * 0.01f));
      switch (_data._lineMod) {
        case 0:
          for (int i = 0; i < inumframes; i++) {
            float s0 = _osc[0].compute(f1, mi);
            float s1 = _osc[1].compute(f2, mi);
            U[i]     = s0 + s1;
          }
          break;
        case 4:
          for (int i = 0; i < inumframes; i++) {
            float s0 = _osc[0].compute(f1, mi);
            float s1 = _osc[1].compute(f2, mi);
            U[i]     = s0 * s1;
          }
          break;
        case 3:
          for (int i = 0; i < inumframes; i++) {
            float r  = (rand() & 0xffff) / 65536.0f;
            float s0 = _osc[0].compute(f1, mi);
            float s1 = _osc[1].compute(f2, mi);
            U[i]     = lerp(r, s0, s1);
          }
          { break; }
        default:
          assert(false);
      }
    }

    _ph += 0.01f;
  }
  /////////////////////////////
  CzOsc _osc[2];
  float _ph;
  float _basefreq;
  int _note;
  layer* _curlayer;
  CzProgData _data;
  bool _newnote;
};
///////////////////////////////////////////////////////////////////////////////
czsyn::czsyn() {
  _pimpl.Set<czpriv*>(new czpriv);
}
///////////////////////////////////////////////////////////////////////////////
void czsyn::compute(DspBuffer& dspbuf) {
  _pimpl.Get<czpriv*>()->compute(dspbuf);
}
///////////////////////////////////////////////////////////////////////////////
void czsyn::keyOn(const DspKeyOnInfo& koi) {
  auto dspb  = koi._prv;
  auto& dbd  = dspb->_dbd;
  auto progd = dbd.getExtData("PDX").Get<czprogdata_ptr_t>();
  auto l     = koi._layer;

  _data = *progd;

  l->_HKF._miscText = FormatString("CZ\n");
  l->_HKF._useFm4   = false;

  _pimpl.Get<czpriv*>()->keyOn(l, *progd);
}
///////////////////////////////////////////////////////////////////////////////
void czsyn::keyOff() {
  _pimpl.Get<czpriv*>()->keyOff();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
