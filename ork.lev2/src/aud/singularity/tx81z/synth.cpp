#include <math.h>
#include <assert.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/fmosc.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
////////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
////////////////////////////////////////////////////////////////////////////////
typedef std::function<void(Layer* layer)> fm4alg_t;
////////////////////////////////////////////////////////////////////////////////
struct fm4vcpriv {
  int _dspchannel = 0;
  ///////////////////////////////////////////////////
  void callalg(Layer* layer) {
    _curalg(layer);
  }
  ///////////////////////////////////////////////////
  fm4vcpriv() {
    /////////////////////////////////////////////////
    _alg[0] = [this](Layer* layer) {
      //   (3)->2->1->0
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(_dspchannel) + layer->_dspwritebase;

      for (int i = 0; i < inumframes; i++) {
        updateControllers();
        computeOpParms();
        float l1        = _phasemodosc[1]._prevOutput;
        float l2        = _phasemodosc[2]._prevOutput;
        float l3        = _phasemodosc[3]._prevOutput;
        float phaseoff1 = l1 * _modindex[1] * _op_amplitude[1];
        float phaseoff2 = l2 * _modindex[2] * _op_amplitude[2];
        float phaseoff3 = l3 * _modindex[3] * _op_amplitude[3];

        // printf("phaseoff1<%g>\n", phaseoff1);

        float o3 = _phasemodosc[3].compute(_frequency[3], phaseoff3 * FBL());
        float o2 = _phasemodosc[2].compute(_frequency[2], phaseoff3);
        float o1 = _phasemodosc[1].compute(_frequency[0], phaseoff2);
        float o0 = _phasemodosc[0].compute(_frequency[0], phaseoff1);

        output[i] = o0 * _op_amplitude[0];
      }
    };
    /////////////////////////////////////////////////
    _alg[1] = [this](Layer* layer) {
      //     (3)
      //   2->1->0
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(_dspchannel) + layer->_dspwritebase;

      for (int i = 0; i < inumframes; i++) {
        updateControllers();
        computeOpParms();
        float l3        = _phasemodosc[3]._prevOutput;
        float l2        = _phasemodosc[2]._prevOutput;
        float l1        = _phasemodosc[1]._prevOutput;
        float phaseoff3 = l3 * _modindex[3] * _op_amplitude[3];
        float phaseoff2 = l2 * _modindex[2] * _op_amplitude[2];
        float phaseoff1 = l1 * _modindex[1] * _op_amplitude[1];

        float o3 = _phasemodosc[3].compute(_frequency[3], phaseoff3 * FBL());
        float o2 = _phasemodosc[2].compute(_frequency[2], phaseoff3);
        float o1 = _phasemodosc[1].compute(_frequency[1], phaseoff2);
        float o0 = _phasemodosc[0].compute(_frequency[0], phaseoff1);

        output[i] = o0 * _op_amplitude[0];
      }
    };
    /////////////////////////////////////////////////
    _alg[2] = [this](Layer* layer) {
      //   2
      //   1  (3)
      //     0
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(_dspchannel) + layer->_dspwritebase;

      for (int i = 0; i < inumframes; i++) {
        updateControllers();
        computeOpParms();
        float l3        = _phasemodosc[3]._prevOutput;
        float l2        = _phasemodosc[2]._prevOutput;
        float l1        = _phasemodosc[1]._prevOutput;
        float phaseoff3 = l3 * _modindex[3] * _op_amplitude[3];
        float phaseoff2 = l2 * _modindex[2] * _op_amplitude[2];
        float phaseoff1 = l1 * _modindex[1] * _op_amplitude[1];

        float o3 = _phasemodosc[3].compute(_frequency[3], phaseoff3 * FBL());
        float o2 = _phasemodosc[2].compute(_frequency[2], phaseoff3);
        float o1 = _phasemodosc[1].compute(_frequency[1], phaseoff2);
        float o0 = _phasemodosc[0].compute(_frequency[0], (phaseoff1 + phaseoff3));

        output[i] = o0 * _op_amplitude[0];
      }
    };
    /////////////////////////////////////////////////
    _alg[3] = [this](Layer* layer) {
      //      (3)
      //   1   2
      //     0
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(_dspchannel) + layer->_dspwritebase;

      for (int i = 0; i < inumframes; i++) {
        updateControllers();
        computeOpParms();
        float l3        = _phasemodosc[3]._prevOutput;
        float l2        = _phasemodosc[2]._prevOutput;
        float l1        = _phasemodosc[1]._prevOutput;
        float phaseoff3 = l3 * _modindex[3] * _op_amplitude[3];
        float phaseoff2 = l2 * _modindex[2] * _op_amplitude[2];
        float phaseoff1 = l1 * _modindex[1] * _op_amplitude[1];

        float o3 = _phasemodosc[3].compute(_frequency[3], phaseoff3 * FBL());
        float o2 = _phasemodosc[2].compute(_frequency[2], phaseoff3);
        float o1 = _phasemodosc[1].compute(_frequency[1], 0.0f);
        float o0 = _phasemodosc[0].compute(_frequency[0], (phaseoff1 + phaseoff2));

        output[i] = o0 * _op_amplitude[0];
      }
    };
    /////////////////////////////////////////////////
    _alg[4] = [this](Layer* layer) {
      // 1 (3)
      // 0  2
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(_dspchannel) + layer->_dspwritebase;

      for (int i = 0; i < inumframes; i++) {
        updateControllers();
        computeOpParms();
        float l3        = _phasemodosc[3]._prevOutput;
        float l1        = _phasemodosc[1]._prevOutput;
        float phaseoff3 = l3 * _modindex[3] * _op_amplitude[3];
        float phaseoff1 = l1 * _modindex[1] * _op_amplitude[1];

        float o3 = _phasemodosc[3].compute(_frequency[3], phaseoff3 * FBL());
        float o2 = _phasemodosc[2].compute(_frequency[2], phaseoff3);
        float o1 = _phasemodosc[1].compute(_frequency[1], 0.0f);
        float o0 = _phasemodosc[0].compute(_frequency[0], phaseoff1);

        // printf( "_modindex[1]<%f>\n", _modindex[1] );
        output[i] = o0 * _op_amplitude[0] * _olev[0] + //
                    o2 * _op_amplitude[2] * _olev[2];
      }
    };
    /////////////////////////////////////////////////
    _alg[5] = [this](Layer* layer) {
      //   (3)
      //   / \
      // 0  1  2
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(_dspchannel) + layer->_dspwritebase;

      for (int i = 0; i < inumframes; i++) {
        updateControllers();
        computeOpParms();
        float l3        = _phasemodosc[3]._prevOutput;
        float phaseoff3 = l3 * _modindex[3] * _op_amplitude[3];

        float o3 = _phasemodosc[3].compute(_frequency[3], phaseoff3 * FBL());
        float o2 = _phasemodosc[2].compute(_frequency[2], phaseoff3);
        float o1 = _phasemodosc[1].compute(_frequency[1], 0.0f);
        float o0 = _phasemodosc[0].compute(_frequency[0], phaseoff3);

        output[i] = o0 * _op_amplitude[0] + //
                    o1 * _op_amplitude[1] + //
                    o2 * _op_amplitude[2];
      }
    };
    /////////////////////////////////////////////////
    _alg[6] = [this](Layer* layer) {
      //      (3)
      // 0  1  2
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(_dspchannel) + layer->_dspwritebase;

      for (int i = 0; i < inumframes; i++) {
        updateControllers();
        computeOpParms();
        float l3        = _phasemodosc[3]._prevOutput;
        float phaseoff3 = l3 * _modindex[3] * _op_amplitude[3];

        float o3 = _phasemodosc[3].compute(_frequency[3], phaseoff3 * FBL());
        float o2 = _phasemodosc[2].compute(_frequency[2], phaseoff3);
        float o1 = _phasemodosc[1].compute(_frequency[1], 0.0f);
        float o0 = _phasemodosc[0].compute(_frequency[0], 0.0f);

        output[i] = o0 * _op_amplitude[0] + //
                    o1 * _op_amplitude[1] + //
                    o2 * _op_amplitude[2];
      }
    };
    /////////////////////////////////////////////////
    _alg[7] = [this](Layer* layer) {
      //   0  1  2 (3)
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(_dspchannel) + layer->_dspwritebase;

      for (int i = 0; i < inumframes; i++) {
        updateControllers();
        computeOpParms();
        float l3        = _phasemodosc[3]._prevOutput;
        float phaseoff3 = l3 * _modindex[3] * _op_amplitude[3];

        float o3 = _phasemodosc[3].compute(_frequency[3], phaseoff3 * FBL());
        float o2 = _phasemodosc[2].compute(_frequency[2], 0.0f);
        float o1 = _phasemodosc[1].compute(_frequency[1], 0.0f);
        float o0 = _phasemodosc[0].compute(_frequency[0], 0.0f);

        output[i] = o0 * _op_amplitude[0] + //
                    o1 * _op_amplitude[1] + //
                    o2 * _op_amplitude[2] + //
                    o3 * _op_amplitude[3];
      }
    };
    /////////////////////////////////////////////////
  }
  //////////////////////////////////////////////////////////////
  float FBL() {
    float fbl = 0.0f;
    if (_data._feedback > 0) {
      // 2.0 == 4PI (7)
      // 1.0 == 2PI (6)
      // 1/2 == PI (5)
      // 1/4 == PI/2 (4)
      // 1/8 == PI/4 (3)
      // 1/16 == PI/8 (2)
      // 1/32 == PI/16 (1)
      fbl = powf(2.0, _data._feedback - 6);
    }
    return fbl;
  }
  //////////////////////////////////////////////////////////////
  void updateControllers() {
    float crate = getControlRate();
  }
  //////////////////////////////////////////////////////////////
  float computeModIndex(int op) const {
    return _data._ops[op]._modIndex;
  }
  //////////////////////////////////////////////////////////////
  float computeOpFrq(int op) const {
    const auto& opd = _data._ops[op];
    if (opd._fixedFrqMode) {
      return opd._frqFixed;
    } else {
      float f = midi_note_to_frequency(_note) * opd._frqRatio;
      return f;
    }
  }
  //////////////////////////////////////////////////////////////
  void computeOpParms() {
    for (int i = 0; i < 4; i++) {
      _modindex[i]  = computeModIndex(i);
      _frequency[i] = computeOpFrq(i);
      // printf("op<%d> f<%g> mi<%g>\n", i, _frequency[i], _modindex[i]);
    }
  }
  //////////////////////////////////////////////////////////////
  // op<3> ol<99> tl<0> mi<25.132742>
  // op<2> ol<70> tl<29> mi<2.037071>
  // op<1> ol<88> tl<11> mi<9.689997>
  // op<0> ol<73> tl<26> mi<2.641754>
  //////////////////////////////////////////////////////////////
  void keyOn(Layer* l) {
    _curlayer = l;
    _newnote  = true;
    for (int i = 0; i < 4; i++) {
      const auto& opd  = _data._ops[i];
      _modindex[i]     = 0.0f;
      _op_amplitude[i] = 0.0f;
      _ratio[i]        = 0.0f;
      _fixed[i]        = 0.0f;
      _frequency[i]    = 0.0f;
      DspKeyOnInfo koi;
      _phasemodosc[i].keyOn(koi, opd);
      _olev[i] = float(opd._outLevel) / 99.0f;
    }

    auto& HUDTEXT = l->_HKF._miscText;

    computeOpParms();

    if (0)
      for (int i = 0; i < 4; i++) {
        const auto& opd = _data._ops[i];
        HUDTEXT += ork::FormatString("op<%d> olev<%d> wav<%d>\n", i, opd._outLevel, opd._waveform);
        HUDTEXT += ork::FormatString("       LS<%d> RS<%d>\n", i, opd._levScaling, opd._ratScaling);
      }
  }
  void keyOff() {
  }
  //////////////////////////////////////////////////////////////

  FmOsc _phasemodosc[4];

  fm4alg_t _curalg;
  Fm4ProgData _data;
  fm4alg_t _alg[8];
  float _modindex[4];
  float _frequency[4];
  float _xegval[4];
  float _op_amplitude[4];
  float _ratio[4];
  float _fixed[4];
  float _olev[4];
  bool _newnote = false;
  int _note;
  Layer* _curlayer;
};
///////////////////////////////////////////////////////////////////////////////
fm4syn::fm4syn() {
  auto priv = new fm4vcpriv;
  _pimpl.Set<fm4vcpriv*>(priv);
}
///////////////////////////////////////////////////////////////////////////////
void fm4syn::compute(Layer* layer) {
  auto priv = _pimpl.Get<fm4vcpriv*>();
  for (int i = 0; i < 4; i++) {
    priv->_op_amplitude[i] = _opAmp[i];
    // printf( "got amp<%d:%f>\n", i, _opAmp[i] );
  }
  priv->callalg(layer);
}
///////////////////////////////////////////////////////////////////////////////
void fm4syn::keyOn(const DspKeyOnInfo& koi) {
  _opAmp[0]  = 0.0f;
  _opAmp[1]  = 0.0f;
  _opAmp[2]  = 0.0f;
  _opAmp[3]  = 0.0f;
  auto dspb  = koi._prv;
  auto dbd   = dspb->_dbd;
  auto progd = dbd->_vars.typedValueForKey<fm4prgdata_ptr_t>("FM4").value();
  auto l     = koi._layer;

  // int curpitch = l->_curnote;
  printf("_curnote<%d>\n", l->_curnote);
  _data             = *progd;
  auto priv         = _pimpl.Get<fm4vcpriv*>();
  priv->_dspchannel = dbd->_dspchannel[0];

  priv->_note   = l->_curnote + (_data._middleC - 24);
  priv->_curalg = priv->_alg[_data._alg];
  priv->_data   = *progd;

  l->_HKF._miscText = ork::FormatString(
      "FM4 alg<%d> fbl<%d> MIDC<%d>\n", //
      _data._alg,
      _data._feedback,
      _data._middleC);
  l->_HKF._useFm4 = true;

  priv->keyOn(l);
}

///////////////////////////////////////////////////////////////////////////////
void fm4syn::keyOff() {
  auto priv = _pimpl.Get<fm4vcpriv*>();
  priv->keyOff();
}

///////////////////////////////////////////////////////////////////////////////

FM4::FM4(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

///////////////////////////////////////////////////////////////////////////////
void FM4::compute(DspBuffer& dspbuf) { // final
  for (int i = 0; i < 4; i++) {
    _fval[i]       = _param[i].eval();
    _fm4._opAmp[i] = _fval[i];
  }
  _fm4.compute(_layer);
}

///////////////////////////////////////////////////////////////////////////////

void FM4::doKeyOn(const DspKeyOnInfo& koi) { // final
  _fm4.keyOn(koi);
}

///////////////////////////////////////////////////////////////////////////////

void FM4::doKeyOff() { // final
  _fm4.keyOff();
}

///////////////////////////////////////////////////////////////////////////////

FM4Data::FM4Data(fm4prgdata_ptr_t fmdata)
    : _fmdata(fmdata) {
  addParam().useDefaultEvaluator(); // amp0
  addParam().useDefaultEvaluator(); // amp1
  addParam().useDefaultEvaluator(); // amp2
  addParam().useDefaultEvaluator(); // amp3
  _vars.makeValueForKey<fm4prgdata_ptr_t>("FM4") = _fmdata;
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t FM4Data::createInstance() const {
  return std::make_shared<FM4>(this);
}

///////////////////////////////////////////////////////////////////////////////

void configureTx81zAlgorithm(lyrdata_ptr_t layerdata, fm4prgdata_ptr_t prgdata) {
  auto algdout        = std::make_shared<AlgData>();
  layerdata->_algdata = algdout;
  algdout->_name      = ork::FormatString("tx81z<%d>", prgdata->_alg);
  //////////////////////////////////////////
  auto stage_ops       = algdout->appendStage("OPS");
  auto stage_amp       = algdout->appendStage("AMP");
  auto stage_modindexx = algdout->appendStage("MIX"); // todo : quadraphonic, 3d?
  //////////////////////////////////////////
  stage_ops->setNumIos(1, 1);
  stage_amp->setNumIos(1, 1);
  stage_modindexx->setNumIos(1, 2); // 1 in, 2 out
  /////////////////////////////////////////////////
  auto ops = stage_ops->appendTypedBlock<FM4>(prgdata);
  /////////////////////////////////////////////////
  // stereo mix out
  /////////////////////////////////////////////////
  auto stereoout        = stage_modindexx->appendTypedBlock<MonoInStereoOut>();
  auto STEREOC          = layerdata->appendController<CustomControllerData>("DCO1DETUNE");
  auto& stereo_mod      = stereoout->_paramd[0]._mods;
  stereo_mod._src1      = STEREOC;
  stereo_mod._src1Depth = 1.0f;
  STEREOC->_onkeyon     = [](CustomControllerInst* cci, //
                         const KeyOnInfo& KOI) {    //
    cci->_curval = 1.0f;                            // amplitude to unity
  };
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
