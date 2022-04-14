////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/alg_eq.h>
#include <ork/lev2/aud/singularity/alg_nonlin.h>

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
// fx modules
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendStereoChorus(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage);
///////////////////////////////////////////////////////////////////////////////
void appendStereoParaEQ(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc,
    float w,
    float gain);
///////////////////////////////////////////////////////////////////////////////
void appendStereoStereoDynamicEcho(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float dtL,
    float dtR,
    float feedback,
    float wetness);
///////////////////////////////////////////////////////////////////////////////
void appendStereoDistortion(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float adj);
///////////////////////////////////////////////////////////////////////////////
void appendStereoHighPass(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc);
///////////////////////////////////////////////////////////////////////////////
void appendStereoHighFreqStimulator(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float fc,
    float drive,
    float amp);
///////////////////////////////////////////////////////////////////////////////
void appendNiceVerb(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float wetness);
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendStereoReverbX(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    int seed,
    float tscale,
    float mint,
    float maxt,
    float minspeed,
    float maxspeed);
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendPitchShifter(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage);
///////////////////////////////////////////////////////////////////////////////
dspblkdata_ptr_t appendStereoReverb(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage,
    float tscale);
///////////////////////////////////////////////////////////////////////////////
void appendStereoEnhancer(
    lyrdata_ptr_t layer, //
    dspstagedata_ptr_t stage);
///////////////////////////////////////////////////////////////////////////////
void appendPitchChorus(
    lyrdata_ptr_t fxlayer, //
    dspstagedata_ptr_t fxstage,
    float wetness,
    float cents);
///////////////////////////////////////////////////////////////////////////////
void appendWackiVerb(
    lyrdata_ptr_t fxlayer, //
    dspstagedata_ptr_t fxstage);
///////////////////////////////////////////////////////////////////////////////
// fx presets
///////////////////////////////////////////////////////////////////////////////
lyrdata_ptr_t fxpreset_stereochorus();
lyrdata_ptr_t fxpreset_fdn4reverb();
lyrdata_ptr_t fxpreset_multitest();
lyrdata_ptr_t fxpreset_niceverb();
lyrdata_ptr_t fxpreset_echoverb();
lyrdata_ptr_t fxpreset_wackiverb();
lyrdata_ptr_t fxpreset_pitchoctup();
lyrdata_ptr_t fxpreset_pitchwave();
lyrdata_ptr_t fxpreset_pitchchorus();
void loadAllFxPresets(synth* s);
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
