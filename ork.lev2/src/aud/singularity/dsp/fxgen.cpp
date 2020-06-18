#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/lev2/aud/singularity/alg_filters.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendStereoChorus(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  /////////////////
  // stereo chorus
  /////////////////
  auto chorus               = stage->appendTypedBlock<StereoDynamicEcho>();
  chorus->param(0)._coarse  = 0.0f; // delay time (L)
  chorus->param(1)._coarse  = 0.0f; // delay time (R)
  chorus->param(2)._coarse  = 0.15; // feedback
  chorus->param(3)._coarse  = 0.4;  // wet/dry mix
  auto& delaytime_modL      = chorus->param(0)._mods;
  auto& delaytime_modR      = chorus->param(1)._mods;
  auto DELAYTIMEMODL        = layer->appendController<CustomControllerData>("DELAYTIMEL");
  auto DELAYTIMEMODR        = layer->appendController<CustomControllerData>("DELAYTIMER");
  delaytime_modL._src1      = DELAYTIMEMODL;
  delaytime_modL._src1Depth = 1.0;
  DELAYTIMEMODL->_oncompute = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->_curval = 0.010f + sinf(time * pi2 * .1) * 0.001f;
  };
  delaytime_modR._src1      = DELAYTIMEMODR;
  delaytime_modR._src1Depth = 1.0;
  DELAYTIMEMODR->_oncompute = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->_curval = 0.005f + sinf(time * pi2 * 0.09) * 0.0047f;
  };
  /////////////////
  return chorus;
}
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendPitchShifter(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  auto shifter              = stage->appendTypedBlock<PitchShifter>();
  shifter->param(0)._coarse = 0.5f; // wet/dry mix
  return shifter;
}
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendStereoReverb(lyrdata_ptr_t layer, dspstagedata_ptr_t stage, float tscale) {
  auto fdn4              = stage->appendTypedBlock<Fdn4Reverb>(tscale);
  fdn4->param(0)._coarse = 0.5f; // wet/dry mix
  return fdn4;
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoDistortion(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float adj) {
  auto l              = stage->appendTypedBlock<Distortion>();
  auto r              = stage->appendTypedBlock<Distortion>();
  l->param(0)._coarse = adj;
  r->param(0)._coarse = adj;
  l->_dspchannel[0]   = 0;
  r->_dspchannel[0]   = 1;
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoParaEQ(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc,
    float w,
    float gain) {
  auto eql              = stage->appendTypedBlock<ParametricEq>();
  auto eqr              = stage->appendTypedBlock<ParametricEq>();
  eql->param(0)._coarse = fc;
  eql->param(1)._coarse = w;
  eql->param(2)._coarse = gain;
  eqr->param(0)._coarse = fc;
  eqr->param(1)._coarse = w;
  eqr->param(2)._coarse = gain;
  eql->_dspchannel[0]   = 0;
  eqr->_dspchannel[0]   = 1;
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoHighPass(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc) {
  auto eql              = stage->appendTypedBlock<HighPass>();
  auto eqr              = stage->appendTypedBlock<HighPass>();
  eql->param(0)._coarse = fc;
  eqr->param(0)._coarse = fc;
  eql->_dspchannel[0]   = 0;
  eqr->_dspchannel[0]   = 1;
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoHighFreqStimulator(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc,
    float drive,
    float amp) {
  auto eql              = stage->appendTypedBlock<HighFreqStimulator>();
  auto eqr              = stage->appendTypedBlock<HighFreqStimulator>();
  eql->param(0)._coarse = fc;
  eqr->param(0)._coarse = fc;
  eql->param(1)._coarse = drive;
  eqr->param(1)._coarse = drive;
  eql->param(2)._coarse = amp;
  eqr->param(2)._coarse = amp;
  eql->_dspchannel[0]   = 0;
  eqr->_dspchannel[0]   = 1;
}
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendStereoReverbX(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    int seed,
    float tscale,
    float mint,
    float maxt,
    float minspeed,
    float maxspeed) {
  auto fdn4 = stage->appendTypedBlock<Fdn4ReverbX>(tscale);
  math::FRANDOMGEN rg(seed);
  fdn4->_axis.x = rg.rangedf(-1, 1);
  fdn4->_axis.y = rg.rangedf(-1, 1);
  fdn4->_axis.z = rg.rangedf(-1, 1);
  fdn4->_axis.Normalize();
  fdn4->_speed           = rg.rangedf(minspeed, maxspeed);
  fdn4->param(1)._coarse = tscale * rg.rangedf(mint, maxt);
  fdn4->param(2)._coarse = tscale * rg.rangedf(mint, maxt);
  fdn4->param(3)._coarse = tscale * rg.rangedf(mint, maxt);
  fdn4->param(4)._coarse = tscale * rg.rangedf(mint, maxt);
  float input_g          = 0.75f;
  float output_g         = 0.75f;
  fdn4->_inputGainsL     = fvec4(input_g, input_g, input_g, input_g);
  fdn4->_inputGainsR     = fvec4(input_g, input_g, input_g, input_g);
  fdn4->_outputGainsL    = fvec4(output_g, output_g, 0, 0);
  fdn4->_outputGainsR    = fvec4(0, 0, output_g, output_g);
  fdn4->param(0)._coarse = 0.5f; // wet/dry mix
  return fdn4;
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoEnhancer(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  auto stereoenh           = stage->appendTypedBlock<StereoDynamicEcho>();
  auto& width_mod          = stereoenh->param(0)._mods;
  auto WIDTHCONTROL        = layer->appendController<CustomControllerData>("WIDTH");
  width_mod._src1          = WIDTHCONTROL;
  width_mod._src1Depth     = 1.0;
  WIDTHCONTROL->_oncompute = [](CustomControllerInst* cci) { //
    cci->_curval = 0.7f;
  };
}
///////////////////////////////////////////////////////////////////////////////
void appendNiceVerb(
    lyrdata_ptr_t fxlayer, //
    dspstagedata_ptr_t fxstage,
    float wetness) {
  auto rv2              = appendStereoReverbX(fxlayer, fxstage, 10, 1.77, 0.01, 0.15, 0.00001, 0.001);
  auto rv1              = appendStereoReverbX(fxlayer, fxstage, 10, 0.47, 0.01, 0.15, 0.00001, 0.001);
  auto rv0              = appendStereoReverbX(fxlayer, fxstage, 10, 0.27, 0.01, 0.15, 0.00001, 0.001);
  rv0->param(0)._coarse = wetness; // wet/dry mix
  rv1->param(0)._coarse = wetness; // wet/dry mix
  rv2->param(0)._coarse = wetness; // wet/dry mix
}
///////////////////////////////////////////////////////////////////////////////
void appendPitchChorus(
    lyrdata_ptr_t fxlayer, //
    dspstagedata_ptr_t fxstage,
    float wetness,
    float cents) {
  /////////////////
  auto shifterL              = appendPitchShifter(fxlayer, fxstage);
  auto shifterR              = appendPitchShifter(fxlayer, fxstage);
  shifterL->param(0)._coarse = wetness; // wet/dry mix
  shifterR->param(0)._coarse = wetness; // wet/dry mix
  /////////////////
  shifterL->_dspchannel[0] = 0; // chorus voice 1 on left
  shifterR->_dspchannel[0] = 1; // chorus voice 2 on right
  /////////////////
  auto PITCHMODL        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT1");
  auto PITCHMODR        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT2");
  auto& pmodL           = shifterL->param(1)._mods;
  auto& pmodR           = shifterR->param(1)._mods;
  pmodL._src1           = PITCHMODL;
  pmodL._src1Depth      = 1.0;
  pmodR._src1           = PITCHMODR;
  pmodR._src1Depth      = 1.0;
  PITCHMODL->_oncompute = [cents](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->_curval = sinf(time * pi2 * 0.03f) * cents;
    return cci->_curval;
  };
  PITCHMODR->_oncompute = [cents](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->_curval = sinf(time * pi2 * 0.07f) * cents;
    return cci->_curval;
  };
}
///////////////////////////////////////////////////////////////////////////////
void appendWackiVerb(lyrdata_ptr_t fxlayer, dspstagedata_ptr_t fxstage) {
  auto crverb = [fxlayer, fxstage](int seed, float mint, float maxt, float mins, float maxs) {
    math::FRANDOMGEN rg(seed);
    auto rv              = appendStereoReverbX(fxlayer, fxstage, seed, 0.0f, 0.00, 0.0, 0.00001, 0.001);
    rv->param(0)._coarse = 0.5f; // wet/dry mix
    rv->param(1)._coarse = 0.0f;
    rv->param(2)._coarse = 0.0f;
    rv->param(3)._coarse = 0.0f;
    rv->param(4)._coarse = 0.0f;
    /////////////////
    auto basename = FormatString("crverb-%d-", seed);
    /////////////////
    float midt = (mint + maxt) * 0.5f;
    float rang = (maxt - mint) * 0.5f;
    /////////////////
    auto RV0DTMODA        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTA");
    auto& rvmoda          = rv->param(1)._mods;
    rvmoda._src1          = RV0DTMODA;
    rvmoda._src1Depth     = 1.0;
    float speed           = rg.rangedf(mins, maxs);
    RV0DTMODA->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->_curval = midt + sinf(time * pi2 * speed) * rang;
      return cci->_curval;
    };
    /////////////////
    auto RV0DTMODB        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTB");
    auto& rvmodb          = rv->param(2)._mods;
    rvmodb._src1          = RV0DTMODB;
    rvmodb._src1Depth     = 1.0;
    speed                 = rg.rangedf(mins, maxs);
    RV0DTMODB->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->_curval = midt + sinf(time * pi2 * speed) * rang;
      return cci->_curval;
    };
    /////////////////
    auto RV0DTMODC        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTC");
    auto& rvmodc          = rv->param(3)._mods;
    rvmodc._src1          = RV0DTMODC;
    rvmodc._src1Depth     = 1.0;
    speed                 = rg.rangedf(mins, maxs);
    RV0DTMODC->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->_curval = midt + sinf(time * pi2 * speed) * rang;
      return cci->_curval;
    };
    /////////////////
    auto RV0DTDMOD        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTD");
    auto& rvmodd          = rv->param(4)._mods;
    rvmodd._src1          = RV0DTDMOD;
    rvmodd._src1Depth     = 1.0;
    speed                 = rg.rangedf(mins, maxs);
    RV0DTDMOD->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->_curval = midt + sinf(time * pi2 * speed) * rang;
      return cci->_curval;
    };
  };
  crverb(11, 0.09, 0.13, 0.1, 0.17);
  crverb(112, 0.017, 0.037, 0.1, 0.5);
  crverb(113, 0.007, 0.009, 0.5, 1.5);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
