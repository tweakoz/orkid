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

CZX::CZX(dspblkdata_constptr_t dbd)
    : DspBlock(dbd) {
}
void CZX::compute(DspBuffer& dspbuf) // final
{
  float centoff  = _param[0].eval();
  _fval[0]       = centoff;
  int inumframes = dspbuf._numframes;
  float* U       = dspbuf.channel(0);
  //_layer->_curPitchOffsetInCents = centoff;
  // todo: dco(pitch) env mod
  // todo: mi from dcw env
  static float _ph = 0.0;
  float modindex   = 0.5f + sinf(_ph) * 0.5f;
  _ph += 0.01f;
  //////////////////////////////////////////
  float lyrcents = _layer->_curCentsOSC;
  float cin      = (lyrcents + centoff) * 0.01;
  float frq      = midi_note_to_frequency(cin);
  //////////////////////////////////////////
  for (int i = 0; i < inumframes; i++) {
    U[i] = _czosc.compute(frq, modindex);
  }
}

void CZX::doKeyOn(const DspKeyOnInfo& koi) // final
{
  auto dspb         = koi._prv;
  auto dbd          = dspb->_dbd;
  auto oscdata      = dbd->getExtData("CZX").Get<czxdata_constptr_t>();
  auto l            = koi._layer;
  l->_HKF._miscText = FormatString("CZ\n");
  l->_HKF._useFm4   = false;
  _czosc.keyOn(koi, oscdata);
}
void CZX::doKeyOff() // final
{
  _czosc.keyOff();
}
void CZX::initBlock(dspblkdata_ptr_t blockdata, czxdata_constptr_t czdata) {
  blockdata->_dspBlock = "CZX";
  blockdata->_paramd[0].usePitchEvaluator();
  blockdata->_extdata["CZX"].Set<czxdata_constptr_t>(czdata);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
