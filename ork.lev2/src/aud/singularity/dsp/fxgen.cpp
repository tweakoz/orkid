#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
void appendStereoChorus(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
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
  auto DELAYTIMEMODL        = layer->appendController<CustomControllerData>("DELAYTIME");
  auto DELAYTIMEMODR        = layer->appendController<CustomControllerData>("DELAYTIME");
  delaytime_modL._src1      = DELAYTIMEMODL;
  delaytime_modL._src1Depth = 1.0;
  DELAYTIMEMODL->_oncompute = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->_curval = 0.030f + sinf(time * pi2 * 1.1) * 0.001f;
  };
  delaytime_modR._src1      = DELAYTIMEMODR;
  delaytime_modR._src1Depth = 1.0;
  DELAYTIMEMODR->_oncompute = [](CustomControllerInst* cci) { //
    float time   = cci->_layer->_layerTime;
    cci->_curval = 0.030f + sinf(time * pi2 * 0.9) * 0.001f;
  };
  /////////////////
}
///////////////////////////////////////////////////////////////////////////////
void appendStereoReverb(lyrdata_ptr_t layer, dspstagedata_ptr_t stage) {
  auto fdn4              = stage->appendTypedBlock<Fdn4Reverb>();
  fdn4->param(0)._coarse = 0.5f; // wet/dry mix
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

lyrdata_ptr_t fxpreset_stereochorus() {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  appendStereoEnhancer(fxlayer, fxstage);
  appendStereoChorus(fxlayer, fxstage);
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_fdn4reverb() {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  appendStereoEnhancer(fxlayer, fxstage);
  appendStereoReverb(fxlayer, fxstage);
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_multitest() {
  auto fxprog       = std::make_shared<ProgramData>();
  auto fxlayer      = fxprog->newLayer();
  auto fxalg        = std::make_shared<AlgData>();
  fxlayer->_algdata = fxalg;
  fxalg->_name      = ork::FormatString("FxAlg");
  /////////////////
  // output effect
  /////////////////
  auto fxstage = fxalg->appendStage("FX");
  fxstage->setNumIos(2, 2); // stereo in, stereo out
  /////////////////
  appendStereoEnhancer(fxlayer, fxstage);
  appendStereoChorus(fxlayer, fxstage);
  appendStereoReverb(fxlayer, fxstage);
  /////////////////
  return fxlayer;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
