////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/fxgen.h>
#include <ork/lev2/aud/singularity/alg_filters.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendStereoChorus(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  /////////////////
  // stereo chorus
  /////////////////
  auto chorus                = stage->appendTypedBlock<StereoDynamicEcho>("echo");
  chorus->param(0)->_coarse  = 0.0f; // delay time (L)
  chorus->param(1)->_coarse  = 0.0f; // delay time (R)
  chorus->param(2)->_coarse  = 0.25; // feedback
  chorus->param(3)->_coarse  = 0.5;  // wet/dry mix
  auto delaytime_modL        = chorus->param(0)->_mods;
  auto delaytime_modR        = chorus->param(1)->_mods;
  auto DELAYTIMEMODL         = layer->appendController<CustomControllerData>("DELAYTIMEL");
  auto DELAYTIMEMODR         = layer->appendController<CustomControllerData>("DELAYTIMER");
  delaytime_modL->_src1      = DELAYTIMEMODL;
  delaytime_modL->_src1Depth = 1.0;
  DELAYTIMEMODL->_oncompute  = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( 0.010f + sinf(time * pi2 * .14) * 0.001f);
  };
  delaytime_modR->_src1      = DELAYTIMEMODR;
  delaytime_modR->_src1Depth = 1.0;
  DELAYTIMEMODR->_oncompute  = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( 0.005f + sinf(time * pi2 * 0.09) * 0.0077f);
  };
  /////////////////
  return chorus;
}
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendPitchShifter(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  auto shifter               = stage->appendTypedBlock<PitchShifter>("shifter");
  shifter->param(0)->_coarse = 0.5f; // wet/dry mix
  return shifter;
}
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendRecursivePitchShifter(lyrdata_ptr_t layer, dspstagedata_ptr_t stage,float feedback) {
  auto shifter               = stage->appendTypedBlock<RecursivePitchShifter>("shifter-recursive",feedback);
  shifter->param(0)->_coarse = 0.5f; // wet/dry mix
  return shifter;
}
///////////////////////////////////////////////////////////////////////////////
std::shared_ptr<Fdn4ReverbData> appendStereoReverb(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  auto fdn4               = stage->appendTypedBlock<Fdn4Reverb>("reverb");
  fdn4->param(0)->_coarse = 0.5f; // wet/dry mix
  fdn4->_input_gain = 1.0;
  fdn4->_output_gain = 1.0;
  fdn4->_time_base = 0.03;
  fdn4->_time_scale = 0.7;
  fdn4->_matrix_gain = 0.5;
  fdn4->_allpass_cutoff = 3500.0;
  fdn4->_hipass_cutoff = 200.0;
  return fdn4;
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoDistortion(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float adj) {
  auto l               = stage->appendTypedBlock<Distortion>("distorion-L");
  auto r               = stage->appendTypedBlock<Distortion>("distorion-R");
  l->param(0)->_coarse = adj;
  r->param(0)->_coarse = adj;
  l->addDspChannel(0);
  r->addDspChannel(1);
}
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendStereoStereoDynamicEcho(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float dtL,
    float dtR,
    float feedback,
    float wetness) {
  auto echo               = stage->appendTypedBlock<StereoDynamicEcho>("echo");
  echo->param(0)->_coarse = dtL;
  echo->param(1)->_coarse = dtR;
  echo->param(2)->_coarse = feedback;
  echo->param(3)->_coarse = wetness;
  echo->addDspChannel(0);
  echo->addDspChannel(1);
  return echo;
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoParaEQ(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc,
    float w,
    float gain) {
  auto eql               = stage->appendTypedBlock<ParametricEq>("peq-L");
  auto eqr               = stage->appendTypedBlock<ParametricEq>("peq-R");
  eql->param(0)->_coarse = fc;
  eql->param(1)->_coarse = w;
  eql->param(2)->_coarse = gain;
  eqr->param(0)->_coarse = fc;
  eqr->param(1)->_coarse = w;
  eqr->param(2)->_coarse = gain;
  eql->addDspChannel(0);
  eqr->addDspChannel(1);
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoHighPass(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc) {
  auto eql               = stage->appendTypedBlock<HighPass>("Highpass-L");
  auto eqr               = stage->appendTypedBlock<HighPass>("Highpass-R");
  eql->param(0)->_coarse = fc;
  eqr->param(0)->_coarse = fc;
  eql->addDspChannel(0);
  eqr->addDspChannel(1);
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoHighFreqStimulator(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc,
    float drive,
    float amp) {
  auto eql               = stage->appendTypedBlock<HighFreqStimulator>("Stim-L");
  auto eqr               = stage->appendTypedBlock<HighFreqStimulator>("Stim-R");
  eql->param(0)->_coarse = fc;
  eqr->param(0)->_coarse = fc;
  eql->param(1)->_coarse = drive;
  eqr->param(1)->_coarse = drive;
  eql->param(2)->_coarse = amp;
  eqr->param(2)->_coarse = amp;
  eql->addDspChannel(0);
  eqr->addDspChannel(1);
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
  auto fdn4 = stage->appendTypedBlock<Fdn4ReverbX>("reverb");

  math::FRANDOMGEN rg(10);
  fdn4->_speed            = rg.rangedf(minspeed, maxspeed);
  fdn4->param(0)->_coarse = 0.5f; // wet/dry mix
  fdn4->param(1)->_coarse = tscale * rg.rangedf(mint, maxt);
  fdn4->param(2)->_coarse = tscale * rg.rangedf(mint, maxt);
  fdn4->param(3)->_coarse = tscale * rg.rangedf(mint, maxt);
  fdn4->param(4)->_coarse = tscale * rg.rangedf(mint, maxt);
  fdn4->_input_gain       = 0.5;
  fdn4->_output_gain      = 0.75;
  fdn4->update();
  return fdn4;
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoEnhancer(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  auto stereoenh           = stage->appendTypedBlock<StereoDynamicEcho>("echo");
  auto width_mod           = stereoenh->param(0)->_mods;
  auto WIDTHCONTROL        = layer->appendController<CustomControllerData>("WIDTH");
  width_mod->_src1         = WIDTHCONTROL;
  width_mod->_src1Depth    = 1.0;
  WIDTHCONTROL->_oncompute = [](CustomControllerInst* cci) { //
    cci->setFloatValue( 0.7f);
  };
}
///////////////////////////////////////////////////////////////////////////////
void appendNiceVerb(
    lyrdata_ptr_t fxlayer, //
    dspstagedata_ptr_t fxstage,
    float wetness) {
  auto rv2               = appendStereoReverbX(fxlayer, fxstage, 10, 1.77, 0.01, 0.15, 0.00001, 0.001);
  auto rv1               = appendStereoReverbX(fxlayer, fxstage, 10, 0.47, 0.01, 0.15, 0.00001, 0.001);
  auto rv0               = appendStereoReverbX(fxlayer, fxstage, 10, 0.27, 0.01, 0.15, 0.00001, 0.001);
  rv0->param(0)->_coarse = wetness; // wet/dry mix
  rv1->param(0)->_coarse = wetness; // wet/dry mix
  rv2->param(0)->_coarse = wetness; // wet/dry mix
}
///////////////////////////////////////////////////////////////////////////////
void appendPitchChorus(
    lyrdata_ptr_t fxlayer, //
    dspstagedata_ptr_t fxstage,
    float wetness,
    float cents,
    float feedback) {
  /////////////////
  auto shifterL               = appendRecursivePitchShifter(fxlayer, fxstage,feedback);
  auto shifterR               = appendRecursivePitchShifter(fxlayer, fxstage,feedback);
  shifterL->param(0)->_coarse = wetness; // wet/dry mix
  shifterR->param(0)->_coarse = wetness; // wet/dry mix
  /////////////////
  shifterL->addDspChannel(0); // chorus voice 1 on left
  shifterR->addDspChannel(1); // chorus voice 2 on right
  /////////////////
  auto PITCHMODL        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT1");
  auto PITCHMODR        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT2");
  auto pmodL            = shifterL->param(1)->_mods;
  auto pmodR            = shifterR->param(1)->_mods;
  pmodL->_src1          = PITCHMODL;
  pmodL->_src1Depth     = 1.0;
  pmodR->_src1          = PITCHMODR;
  pmodR->_src1Depth     = 1.0;
  PITCHMODL->_oncompute = [cents](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( cents + sinf(time * pi2 * 0.03f) * 25 );
    return cci->getFloatValue();
  };
  PITCHMODR->_oncompute = [cents](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( cents + sinf(time * pi2 * 0.07f) * 30 );
    return cci->getFloatValue();
  };
}
void appendPitchRec(
    lyrdata_ptr_t fxlayer, //
    dspstagedata_ptr_t fxstage,
    float cents,
    float wetness,
    float feedback){
  /////////////////
  auto shifterL               = appendRecursivePitchShifter(fxlayer, fxstage, feedback);
  auto shifterR               = appendRecursivePitchShifter(fxlayer, fxstage, feedback);
  shifterL->param(0)->_coarse = wetness; // wet/dry mix
  shifterR->param(0)->_coarse = wetness; // wet/dry mix
  /////////////////
  shifterL->addDspChannel(0); // chorus voice 1 on left
  shifterR->addDspChannel(1); // chorus voice 2 on right
  /////////////////
  auto PITCHMODL        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT1");
  auto PITCHMODR        = fxlayer->appendController<CustomControllerData>("PITCHSHIFT2");
  auto pmodL            = shifterL->param(1)->_mods;
  auto pmodR            = shifterR->param(1)->_mods;
  pmodL->_src1          = PITCHMODL;
  pmodL->_src1Depth     = 1.0;
  pmodR->_src1          = PITCHMODR;
  pmodR->_src1Depth     = 1.0;
  PITCHMODL->_oncompute = [cents](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( cents + sinf(time * pi2 * 0.07f) * 15 );
    return cci->getFloatValue();
  };
  PITCHMODR->_oncompute = [cents](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->setFloatValue( cents + sinf(time * pi2 * 0.17f) * 20 );
    return cci->getFloatValue();
  };

  appendNiceVerb(fxlayer, fxstage, 0.1);
}
///////////////////////////////////////////////////////////////////////////////
void appendWackiVerb(lyrdata_ptr_t fxlayer, dspstagedata_ptr_t fxstage) {
  auto crverb = [fxlayer, fxstage](
                    int seed, //
                    float mint,
                    float maxt,
                    float mins,
                    float maxs) {
    math::FRANDOMGEN rg(seed);
    auto rv               = appendStereoReverbX(fxlayer, fxstage, seed, 0.0f, 0.00, 0.0, 0.00001, 0.001);
    rv->param(0)->_coarse = 0.25f; // wet/dry mix
    rv->param(1)->_coarse = 0.0f;
    rv->param(2)->_coarse = 0.0f;
    rv->param(3)->_coarse = 0.0f;
    rv->param(4)->_coarse = 0.0f;
    /////////////////
    auto basename = FormatString("crverb-%d-", seed);
    /////////////////
    float midt = (mint + maxt) * 0.5f;
    float rang = (maxt - mint) * 0.5f;
    /////////////////
    auto RV0DTMODA        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTA");
    auto& rvmoda          = rv->param(1)->_mods;
    rvmoda->_src1         = RV0DTMODA;
    rvmoda->_src1Depth    = 1.0;
    float speed           = rg.rangedf(mins, maxs);
    RV0DTMODA->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->setFloatValue( midt + sinf(time * pi2 * speed) * rang);
      return cci->getFloatValue();
    };
    /////////////////
    auto RV0DTMODB        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTB");
    auto rvmodb           = rv->param(2)->_mods;
    rvmodb->_src1         = RV0DTMODB;
    rvmodb->_src1Depth    = 1.0;
    speed                 = rg.rangedf(mins, maxs);
    RV0DTMODB->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->setFloatValue( midt + sinf(time * pi2 * speed) * rang);
      return cci->getFloatValue();
    };
    /////////////////
    auto RV0DTMODC        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTC");
    auto rvmodc           = rv->param(3)->_mods;
    rvmodc->_src1         = RV0DTMODC;
    rvmodc->_src1Depth    = 1.0;
    speed                 = rg.rangedf(mins, maxs);
    RV0DTMODC->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->setFloatValue( midt + sinf(time * pi2 * speed) * rang);
      return cci->getFloatValue();
    };
    /////////////////
    auto RV0DTDMOD        = fxlayer->appendController<CustomControllerData>(basename + "RV0DTD");
    auto rvmodd           = rv->param(4)->_mods;
    rvmodd->_src1         = RV0DTDMOD;
    rvmodd->_src1Depth    = 1.0;
    speed                 = rg.rangedf(mins, maxs);
    RV0DTDMOD->_oncompute = [speed, midt, rang](CustomControllerInst* cci) { //
      float time   = cci->_layer->_layerTime;
      cci->setFloatValue( midt + sinf(time * pi2 * speed) * rang);
      return cci->getFloatValue();
    };
  };
  crverb(11, 0.09, 0.13, 0.1, 0.17);
  crverb(112, 0.017, 0.037, 0.1, 0.5);
  crverb(113, 0.007, 0.009, 0.5, 1.5);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
