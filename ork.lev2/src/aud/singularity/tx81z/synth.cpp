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
// the YM3812 avoids multiplications by operating on log-transformed
// x*y = e^(ln(x)+ln(y))
// 256-word ROM stores the exponential function as a
// lookup table, used to convert the logarithmic scale
// signal back to linear scale when required
inline float proc_out(float inp) {
  constexpr float kclamp = 8.0f;
  constexpr float kscale = 0.25f;
  validateSample(inp);
  if (isfinite(inp) and not isnan(inp)) {
    return clip_float(inp, -kclamp, kclamp) * kscale;
  }
  return 0.0f;
}
////////////////////////////////////////////////////////////////////////////////
void configureTx81zAlgorithm(lyrdata_ptr_t layerdata, pm4prgdata_ptr_t prgdata) {
  auto algdout        = std::make_shared<AlgData>();
  layerdata->_algdata = algdout;
  algdout->_name      = ork::FormatString("tx81z<%d>", prgdata->_alg);
  //////////////////////////////////////////
  auto stage_ops    = algdout->appendStage("OPS");
  auto stage_opmix  = algdout->appendStage("OPMIX");
  auto stage_stereo = algdout->appendStage("STMIX"); // todo : quadraphonic, 3d?
  //////////////////////////////////////////
  stage_stereo->setNumIos(1, 2); // 1 in, 2 out
  /////////////////////////////////////////////////
  // instantiate ops in reverse order
  //  because of order of operations (3,2,1,0)
  /////////////////////////////////////////////////
  auto op3 = stage_ops->appendTypedBlock<PMX>();
  auto op2 = stage_ops->appendTypedBlock<PMX>();
  auto op1 = stage_ops->appendTypedBlock<PMX>();
  auto op0 = stage_ops->appendTypedBlock<PMX>();
  /////////////////////////////////////////////////
  int opchanbase      = 2;
  op0->_dspchannel[0] = opchanbase + 0;
  op1->_dspchannel[0] = opchanbase + 1;
  op2->_dspchannel[0] = opchanbase + 2;
  op3->_dspchannel[0] = opchanbase + 3;
  /////////////////////////////////////////////////
  auto opmix            = stage_opmix->appendTypedBlock<PMXMix>();
  opmix->_dspchannel[0] = 0;
  /////////////////////////////////////////////////
  switch (prgdata->_alg) {
    case 0:
      //   (3)->2->1->0
      stage_ops->setNumIos(1, 1);
      stage_opmix->setNumIos(1, 1);
      op1->_modulator            = true;
      op2->_modulator            = true;
      op3->_modulator            = true;
      op0->_pmInpChannels[0]     = opchanbase + 1;
      op1->_pmInpChannels[0]     = opchanbase + 2;
      op2->_pmInpChannels[0]     = opchanbase + 3;
      opmix->_pmixInpChannels[0] = opchanbase + 0;
      break;
    case 1:
      //   (3)
      // 2->1->0
      op1->_modIndex = 0.5f; // 2 inputs
      stage_ops->setNumIos(1, 1);
      stage_opmix->setNumIos(1, 1);
      op1->_modulator            = true;
      op2->_modulator            = true;
      op3->_modulator            = true;
      op0->_pmInpChannels[0]     = opchanbase + 1;
      op1->_pmInpChannels[0]     = opchanbase + 2;
      op1->_pmInpChannels[1]     = opchanbase + 3;
      opmix->_pmixInpChannels[0] = opchanbase + 0;
      break;
    case 2:
      //  2
      //  1 (3)
      //   0
      op0->_modIndex = 0.5f; // 2 inputs
      stage_ops->setNumIos(1, 1);
      stage_opmix->setNumIos(1, 1);
      op1->_modulator            = true;
      op2->_modulator            = true;
      op3->_modulator            = true;
      op0->_pmInpChannels[0]     = opchanbase + 1;
      op0->_pmInpChannels[1]     = opchanbase + 3;
      op1->_pmInpChannels[0]     = opchanbase + 2;
      opmix->_pmixInpChannels[0] = opchanbase + 0;
      break;
    case 3:
      // (3)
      //  1   2
      //    0
      op0->_modIndex         = 0.5f; // 2 inputs
      op0->_pmInpChannels[0] = opchanbase + 1;
      op0->_pmInpChannels[1] = opchanbase + 2;
      op1->_pmInpChannels[0] = opchanbase + 3;
      stage_ops->setNumIos(1, 1);
      stage_opmix->setNumIos(1, 1);
      op1->_modulator            = true;
      op2->_modulator            = true;
      op3->_modulator            = true;
      opmix->_pmixInpChannels[0] = opchanbase + 0;
      break;
    case 4:
      // 1 (3)
      // 0  2
      stage_ops->setNumIos(1, 2);
      stage_opmix->setNumIos(2, 1);
      op1->_modulator            = true;
      op3->_modulator            = true;
      op0->_pmInpChannels[0]     = opchanbase + 1;
      op2->_pmInpChannels[0]     = opchanbase + 3;
      opmix->_pmixInpChannels[0] = opchanbase + 0;
      opmix->_pmixInpChannels[1] = opchanbase + 2;
      break;
    case 5:
      //   (3)
      //   / \
      // 0  1  2
      stage_ops->setNumIos(1, 3);
      stage_opmix->setNumIos(3, 1);
      op3->_modulator            = true;
      op0->_pmInpChannels[0]     = opchanbase + 3;
      op1->_pmInpChannels[0]     = opchanbase + 3;
      op2->_pmInpChannels[0]     = opchanbase + 3;
      opmix->_pmixInpChannels[0] = opchanbase + 0;
      opmix->_pmixInpChannels[1] = opchanbase + 1;
      opmix->_pmixInpChannels[2] = opchanbase + 2;
      break;
    case 6:
      //      (3)
      // 0  1  2
      stage_ops->setNumIos(1, 3);
      stage_opmix->setNumIos(3, 1);
      op3->_modulator            = true;
      op2->_pmInpChannels[0]     = opchanbase + 3;
      opmix->_pmixInpChannels[0] = opchanbase + 0;
      opmix->_pmixInpChannels[1] = opchanbase + 1;
      opmix->_pmixInpChannels[2] = opchanbase + 2;
      break;
    case 7:
      //   0  1  2 (3)
      stage_ops->setNumIos(1, 4);
      stage_opmix->setNumIos(4, 1);
      opmix->_pmixInpChannels[0] = opchanbase + 0;
      opmix->_pmixInpChannels[1] = opchanbase + 1;
      opmix->_pmixInpChannels[2] = opchanbase + 2;
      opmix->_pmixInpChannels[3] = opchanbase + 3;
      break;
  }
  /////////////////////////////////////////////////
  // stereo mix out
  /////////////////////////////////////////////////
  auto stereoout        = stage_stereo->appendTypedBlock<MonoInStereoOut>();
  auto STEREOC          = layerdata->appendController<CustomControllerData>("STEREOMIX");
  auto& stereo_mod      = stereoout->_paramd[0]._mods;
  stereo_mod._src1      = STEREOC;
  stereo_mod._src1Depth = 1.0f;
  STEREOC->_onkeyon     = [](CustomControllerInst* cci, //
                         const KeyOnInfo& KOI) {    //
    cci->_curval = 1.0f;                            // amplitude to unity
  };
}
///////////////////////////////////////////////////////////////////////////////
PMXData::PMXData() {
  addParam().usePitchEvaluator();   // pitch
  addParam().useDefaultEvaluator(); // amp
  addParam().useDefaultEvaluator(); // feedback
}
///////////////////////////////////////////////////////////////////////////////
dspblk_ptr_t PMXData::createInstance() const {
  return std::make_shared<PMX>(this);
}
///////////////////////////////////////////////////////////////////////////////
PMX::PMX(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void PMX::compute(DspBuffer& dspbuf) { // final
  int inumframes   = _layer->_dspwritecount;
  float* output    = dspbuf.channel(_dspchannel[0]) + _layer->_dspwritebase;
  float pitch      = _param[0].eval(); // cents
  float amp        = _param[1].eval();
  float fbl        = _param[2].eval();
  float note       = pitch * 0.01;
  float frq        = midi_note_to_frequency(note);
  float clampedamp = std::clamp(amp, 0.0f, 1.0f);
  float clampefbl  = std::clamp(fbl, 0.0f, 1.0f);
  float amppow     = _modulator ? 1.0f : 1.0f;
  float ampsca     = _modulator ? 1.0f : 1.0f;
  ///////////////////////////////////////////////////////////////
  // printf("frq<%g> amp<%g> fbl<%g>\n", frq, amp, fbl);
  ///////////////////////////////////////////////////////////////
  const float* modinputs[PMXData::kmaxmodulators] = {
      nullptr,
      nullptr,
      nullptr,
      nullptr, //
      nullptr,
      nullptr,
      nullptr,
      nullptr};
  for (int m = 0; m < PMXData::kmaxmodulators; m++) {
    int inpchi = _pmxdata->_pmInpChannels[m];
    if (inpchi >= 0) {
      modinputs[m] = dspbuf.channel(inpchi) //
                     + _layer->_dspwritebase;
    }
  }
  ///////////////////////////////////////////////////////////////
  for (int i = 0; i < inumframes; i++) {
    float phase_offset = _pmosc._prevOutput * fbl * amp * ampsca;
    for (int m = 0; m < PMXData::kmaxmodulators; m++) {
      auto modinp = modinputs[m];
      if (modinp != nullptr) {
        phase_offset += modinp[i];
      }
    }
    float osc_out = _pmosc.compute(frq, phase_offset * _modIndex);
    output[i]     = osc_out * powf(ampsca * amp, amppow);
  }
  ///////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void PMX::doKeyOn(const KeyOnInfo& koi) { // final
  _pmxdata = (const PMXData*)_dbd;
  _pmosc.keyOn(_pmxdata->_pmoscdata);
  _modIndex  = _pmxdata->_modIndex;
  _modulator = _pmxdata->_modulator;
  if (_pmxdata->_txprogramdata) {
    auto name = _pmxdata->_txprogramdata->_name;
    int alg   = _pmxdata->_txprogramdata->_alg;
    printf("keyon prog<%s> alg<%d>\n", name.c_str(), alg);
  }
}
///////////////////////////////////////////////////////////////////////////////
void PMX::doKeyOff() { // final
  _pmosc.keyOff();
}
///////////////////////////////////////////////////////////////////////////////
PMXMixData::PMXMixData() {
  addParam().usePitchEvaluator();   // pitch
  addParam().useDefaultEvaluator(); // amp
  addParam().useDefaultEvaluator(); // feedback
}
///////////////////////////////////////////////////////////////////////////////
dspblk_ptr_t PMXMixData::createInstance() const {
  return std::make_shared<PMXMix>(this);
}
///////////////////////////////////////////////////////////////////////////////
PMXMix::PMXMix(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void PMXMix::compute(DspBuffer& dspbuf) { // final
  int inumframes = _layer->_dspwritecount;
  float* output  = dspbuf.channel(_dspchannel[0]) + _layer->_dspwritebase;
  ///////////////////////////////////////////////////////////////
  for (int m = 0; m < PMXMixData::kmaxinputs; m++) {
    int inpchi = _pmixdata->_pmixInpChannels[m];
    if (inpchi >= 0) {
      auto input = dspbuf.channel(inpchi) + _layer->_dspwritebase;
      for (int i = 0; i < inumframes; i++) {
        output[i] += input[i] * _finalamp;
      }
    }
  }
  for (int i = 0; i < inumframes; i++) {
    output[i] = proc_out(output[i]);
  }
}
///////////////////////////////////////////////////////////////////////////////
void PMXMix::doKeyOn(const KeyOnInfo& koi) { // final
  _pmixdata      = (const PMXMixData*)_dbd;
  int numoutputs = 0;
  for (int m = 0; m < PMXMixData::kmaxinputs; m++) {
    int inpchi = _pmixdata->_pmixInpChannels[m];
    if (inpchi >= 0) {
      numoutputs++;
    }
  }
  _finalamp = (numoutputs == 0) //
                  ? 1.0f
                  : 1.0f / float(numoutputs);
}
///////////////////////////////////////////////////////////////////////////////
void PMXMix::doKeyOff() { // final
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
