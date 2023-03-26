////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/math/misc_math.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

namespace audiomath {

float linear_time_to_timecent(float time);
float timecent_to_linear_time(float timecent);
float decibel_to_linear_amp_ratio(float decibel);
float linear_amp_ratio_to_decibel(float linear);
float centibel_to_linear_amp_ratio(float centibel);
float linear_amp_ratio_to_centibel(float linear);
float linear_freq_ratio_to_cents(float freq_ratio);
float cents_to_linear_freq_ratio(float cents);

S32 round_to_nearest(float in);

float midi_note_to_frequency(float midinote);
float frequency_to_midi_note(float frequency);

float clip_float(float in, float minn, float maxx);

float softsat(float x, float a);
float smoothstep(float edge0, float edge1, float x);

float slopeDBPerSample(float dbpsec, float samplerate);

float lerp(float from, float to, float index);

} // namespace audiomath

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
