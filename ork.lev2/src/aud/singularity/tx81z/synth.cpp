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
struct fm4impl;
using fm4alg_t = std::function<void(fm4impl* impl)>;
////////////////////////////////////////////////////////////////////////////////
struct fmoperator {
  FmOsc _pmosc;
  float _amp      = 0.0f;
  float _frq      = 0.0f;
  float _modindex = 0.0f;
  inline float phaseOffset() const {
    return _pmosc._prevOutput * _modindex * _amp;
  }
};
////////////////////////////////////////////////////////////////////////////////
struct fm4impl {
  //////////////////////////////////////////////////////////////
  fm4impl(FM4* fm4);
  //////////////////////////////////////////////////////////////
  void compute();
  void keyOn();
  void keyOff();
  void callalg();
  //////////////////////////////////////////////////////////////
  fmoperator _ops[4];
  Fm4ProgData _data;
  fm4alg_t _curalg;
  int _dspchannel  = 0;
  float _feedback  = 0.0f;
  FM4* _fm4        = nullptr;
  Layer* _curlayer = nullptr;
};
////////////////////////////////////////////////////////////////////////////////
fm4alg_t tx4op_algs[8] = {
    //// ALG 0 ////
    [](fm4impl* impl) {
      //   (3)->2->1->0
      auto layer     = impl->_curlayer;
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(impl->_dspchannel) //
                      + layer->_dspwritebase;
      auto& ops      = impl->_ops;
      auto& op0      = ops[0];
      auto& op1      = ops[1];
      auto& op2      = ops[2];
      auto& op3      = ops[3];
      float feedback = impl->_feedback;
      for (int i = 0; i < inumframes; i++) {
        float phaseoff1 = op1.phaseOffset();
        float phaseoff2 = op2.phaseOffset();
        float phaseoff3 = op3.phaseOffset();
        float o3        = op3._pmosc.compute(op3._frq, phaseoff3 * feedback);
        float o2        = op2._pmosc.compute(op2._frq, phaseoff3);
        float o1        = op1._pmosc.compute(op0._frq, phaseoff2);
        float o0        = op0._pmosc.compute(op0._frq, phaseoff1);
        output[i]       = o0 * op0._amp;
      }
    },
    /////////////////////////////////////////////////
    //// ALG 1 ////
    [](fm4impl* impl) {
      //     (3)
      //   2->1->0
      auto layer     = impl->_curlayer;
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(impl->_dspchannel) + layer->_dspwritebase;
      auto& ops      = impl->_ops;
      auto& op0      = ops[0];
      auto& op1      = ops[1];
      auto& op2      = ops[2];
      auto& op3      = ops[3];
      float feedback = impl->_feedback;
      for (int i = 0; i < inumframes; i++) {
        float phaseoff3 = op3.phaseOffset();
        float phaseoff2 = op2.phaseOffset();
        float phaseoff1 = op1.phaseOffset();
        float o3        = op3._pmosc.compute(op3._frq, phaseoff3 * feedback);
        float o2        = op2._pmosc.compute(op2._frq, phaseoff3);
        float o1        = op1._pmosc.compute(op1._frq, phaseoff2);
        float o0        = op0._pmosc.compute(op0._frq, phaseoff1);
        output[i]       = o0 * op0._amp;
      }
    },
    /////////////////////////////////////////////////
    //// ALG 2 ////
    [](fm4impl* impl) {
      //   2
      //   1  (3)
      //     0
      auto layer     = impl->_curlayer;
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(impl->_dspchannel) + layer->_dspwritebase;
      auto& ops      = impl->_ops;
      auto& op0      = ops[0];
      auto& op1      = ops[1];
      auto& op2      = ops[2];
      auto& op3      = ops[3];
      float feedback = impl->_feedback;
      for (int i = 0; i < inumframes; i++) {
        float phaseoff3 = op3.phaseOffset();
        float phaseoff2 = op2.phaseOffset();
        float phaseoff1 = op1.phaseOffset();
        float o3        = op3._pmosc.compute(op3._frq, phaseoff3 * feedback);
        float o2        = op2._pmosc.compute(op2._frq, phaseoff3);
        float o1        = op1._pmosc.compute(op1._frq, phaseoff2);
        float o0        = op0._pmosc.compute(op0._frq, (phaseoff1 + phaseoff3));
        output[i]       = o0 * op0._amp;
      }
    },
    /////////////////////////////////////////////////
    //// ALG 3 ////
    [](fm4impl* impl) {
      //      (3)
      //   1   2
      //     0
      auto layer     = impl->_curlayer;
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(impl->_dspchannel) + layer->_dspwritebase;
      auto& ops      = impl->_ops;
      auto& op0      = ops[0];
      auto& op1      = ops[1];
      auto& op2      = ops[2];
      auto& op3      = ops[3];
      float feedback = impl->_feedback;
      for (int i = 0; i < inumframes; i++) {
        float phaseoff3 = op3.phaseOffset();
        float phaseoff2 = op2.phaseOffset();
        float phaseoff1 = op1.phaseOffset();
        float o3        = op3._pmosc.compute(op3._frq, phaseoff3 * feedback);
        float o2        = op2._pmosc.compute(op2._frq, phaseoff3);
        float o1        = op1._pmosc.compute(op1._frq, 0.0f);
        float o0        = op0._pmosc.compute(op0._frq, (phaseoff1 + phaseoff2));
        output[i]       = o0 * op0._amp;
      }
    },
    /////////////////////////////////////////////////
    //// ALG 4 ////
    [](fm4impl* impl) {
      // 1 (3)
      // 0  2
      auto layer     = impl->_curlayer;
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(impl->_dspchannel) + layer->_dspwritebase;
      auto& ops      = impl->_ops;
      auto& op0      = ops[0];
      auto& op1      = ops[1];
      auto& op2      = ops[2];
      auto& op3      = ops[3];
      float feedback = impl->_feedback;
      for (int i = 0; i < inumframes; i++) {
        float phaseoff3 = op3.phaseOffset();
        float phaseoff1 = op1.phaseOffset();
        float o3        = op3._pmosc.compute(op3._frq, phaseoff3 * feedback);
        float o2        = op2._pmosc.compute(op2._frq, phaseoff3);
        float o1        = op1._pmosc.compute(op1._frq, 0.0f);
        float o0        = op0._pmosc.compute(op0._frq, phaseoff1);
        // printf( "_modindex[1]<%f>\n", _modindex[1] );
        output[i] = o0 * op0._amp + //
                    o2 * op2._amp;
      }
    },
    /////////////////////////////////////////////////
    //// ALG 5 ////
    [](fm4impl* impl) {
      //   (3)
      //   / \
      // 0  1  2
      auto layer     = impl->_curlayer;
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(impl->_dspchannel) + layer->_dspwritebase;
      auto& ops      = impl->_ops;
      auto& op0      = ops[0];
      auto& op1      = ops[1];
      auto& op2      = ops[2];
      auto& op3      = ops[3];
      float feedback = impl->_feedback;
      for (int i = 0; i < inumframes; i++) {
        float phaseoff3 = op3.phaseOffset();
        float o3        = op3._pmosc.compute(op3._frq, phaseoff3 * feedback);
        float o2        = op2._pmosc.compute(op2._frq, phaseoff3);
        float o1        = op1._pmosc.compute(op1._frq, 0.0f);
        float o0        = op0._pmosc.compute(op0._frq, phaseoff3);
        output[i]       = o0 * op0._amp + //
                    o1 * op1._amp +       //
                    o2 * op2._amp;
      }
    },
    /////////////////////////////////////////////////
    //// ALG 6 ////
    [](fm4impl* impl) {
      //      (3)
      // 0  1  2
      auto layer     = impl->_curlayer;
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(impl->_dspchannel) + layer->_dspwritebase;
      auto& ops      = impl->_ops;
      auto& op0      = ops[0];
      auto& op1      = ops[1];
      auto& op2      = ops[2];
      auto& op3      = ops[3];
      float feedback = impl->_feedback;
      for (int i = 0; i < inumframes; i++) {
        float phaseoff3 = op3.phaseOffset();
        float o3        = op3._pmosc.compute(op3._frq, phaseoff3 * feedback);
        float o2        = op2._pmosc.compute(op2._frq, phaseoff3);
        float o1        = op1._pmosc.compute(op1._frq, 0.0f);
        float o0        = op0._pmosc.compute(op0._frq, 0.0f);
        output[i]       = o0 * op0._amp + //
                    o1 * op1._amp +       //
                    o2 * op2._amp;
      }
    },
    /////////////////////////////////////////////////
    //// ALG 7 ////
    [](fm4impl* impl) {
      //   0  1  2 (3)
      auto layer     = impl->_curlayer;
      auto& dspbuf   = *layer->_dspbuffer;
      int inumframes = layer->_dspwritecount;
      float* output  = dspbuf.channel(impl->_dspchannel) + layer->_dspwritebase;
      auto& ops      = impl->_ops;
      auto& op0      = ops[0];
      auto& op1      = ops[1];
      auto& op2      = ops[2];
      auto& op3      = ops[3];
      float feedback = impl->_feedback;
      for (int i = 0; i < inumframes; i++) {
        float phaseoff3 = op3.phaseOffset();
        float o3        = op3._pmosc.compute(op3._frq, phaseoff3 * feedback);
        float o2        = op2._pmosc.compute(op2._frq, 0.0f);
        float o1        = op1._pmosc.compute(op1._frq, 0.0f);
        float o0        = op0._pmosc.compute(op0._frq, 0.0f);
        output[i]       = o0 * op0._amp + //
                    o1 * op1._amp +       //
                    o2 * op2._amp +       //
                    o3 * op3._amp;
      }
    },
};
///////////////////////////////////////////////////
fm4impl::fm4impl(FM4* fm4)
    : _fm4(fm4) {
  /////////////////////////////////////////////////

  /////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void fm4impl::compute() {
  //////////////////////////////////////////////
  // update modulation
  //////////////////////////////////////////////
  for (int i = 0; i < 4; i++) {
    float pitch       = _fm4->_param[0 + i].eval(); // cents
    float frq         = midi_note_to_frequency(pitch * 0.01);
    float amp         = _fm4->_param[4 + i].eval();
    auto& dest_op     = _ops[i];
    dest_op._frq      = frq;
    dest_op._amp      = amp;
    dest_op._modindex = _data._ops[i]._modIndex;
  }
  _feedback = _fm4->_param[8].eval();
  //////////////////////////////////////////////
  // audio computation
  //////////////////////////////////////////////
  _curalg(this);
}
///////////////////////////////////////////////////////////////////////////////
void fm4impl::keyOn() {
  auto layer  = _fm4->_layer;
  auto dbd    = _fm4->_dbd;
  auto progd  = dbd->_vars.typedValueForKey<fm4prgdata_ptr_t>("FM4").value();
  _data       = *progd;
  _dspchannel = dbd->_dspchannel[0];
  _curalg     = tx4op_algs[_data._alg];
  _data       = *progd;
  _curlayer   = layer;
  for (int i = 0; i < 4; i++) {
    auto& dest_op     = _ops[i];
    dest_op._frq      = 0.0f;
    dest_op._amp      = 0.0f;
    dest_op._modindex = 0.0f;
    const auto& opd   = _data._ops[i];
    _ops[i]._pmosc.keyOn(opd);
  }
}
///////////////////////////////////////////////////////////////////////////////
void fm4impl::keyOff() {
}
////////////////////////////////////////////////////////////////////////////////
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
FM4Data::FM4Data(fm4prgdata_ptr_t fmdata)
    : _fmdata(fmdata) {
  addParam().usePitchEvaluator();   // pitch0
  addParam().usePitchEvaluator();   // pitch1
  addParam().usePitchEvaluator();   // pitch2
  addParam().usePitchEvaluator();   // pitch3
  addParam().useDefaultEvaluator(); // amp0
  addParam().useDefaultEvaluator(); // amp1
  addParam().useDefaultEvaluator(); // amp2
  addParam().useDefaultEvaluator(); // amp3
  addParam().useDefaultEvaluator(); // feedback
  _vars.makeValueForKey<fm4prgdata_ptr_t>("FM4") = _fmdata;
}
///////////////////////////////////////////////////////////////////////////////
dspblk_ptr_t FM4Data::createInstance() const {
  return std::make_shared<FM4>(this);
}
///////////////////////////////////////////////////////////////////////////////
FM4::FM4(const DspBlockData* dbd)
    : DspBlock(dbd) {
  _pimpl.makeShared<fm4impl>(this);
}
///////////////////////////////////////////////////////////////////////////////
using implptr_t = std::shared_ptr<fm4impl>;
///////////////////////////////////////////////////////////////////////////////
void FM4::compute(DspBuffer& dspbuf) { // final
  _pimpl.Get<implptr_t>()->compute();
}
///////////////////////////////////////////////////////////////////////////////
void FM4::doKeyOn(const KeyOnInfo& koi) { // final
  _pimpl.Get<implptr_t>()->keyOn();
}
///////////////////////////////////////////////////////////////////////////////
void FM4::doKeyOff() { // final
  _pimpl.Get<implptr_t>()->keyOff();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
