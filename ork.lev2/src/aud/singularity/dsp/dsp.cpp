////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

//#include <audiofile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synth.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/*
As the SHAPER receives input signals, it evaluates the signal’s level
according to its own internal scale. When the SHAPER’s Adjust value is
at .25, an input signal moving from negative full scale to positive
full scale (a sawtooth) will map to an output curve with a single-
cycle sine wave shape. At an adjust value of .5, the same input signal
would map to a 2-cycle sine wave output signal. Adjust values of .75
and 1.0 for the SHAPER would map to 3-and 4-cycle sine wave output
signals, respectively. Beyond values of 1.0, some portions of the
output will pin at zero-scale.
*/

float shaper(float inp, float adj) {
  float index = clip_float(inp * 4.0 * adj, -2.0, 2.0);

  // adj = 0.85f;
  // adj *= 5.0f;

  // float absinp = fabs(inp);
  // float inpsqu = inp*inp;
  // float N = inp * (absinp + adj);
  // float D = (inpsqu+(adj-1.0f)*absinp+1.0f);
  // float rval = N/D;
  // printf( "adj<%g> N<%g> D<%g> rv<%g>\n", adj, N, D, rval );
  return sinf(index * pi2); /// adj;
}

///////////////////////////////////////////////////////////////////////////////

float wrap(float inp, float adj) {
  float wrapamp = 30.0f + adj;
  // printf( "wrap adj<%f> amp<%f>\n", adj, wrapamp );
  float amped = decibel_to_linear_amp_ratio(wrapamp);
  return fmod(inp * amped, 1.0f);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
