////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_eq.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_nonlin.h>
#include <ork/lev2/aud/singularity/alg_filters.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/dsp_ringmod.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/reflect/properties/registerX.inl>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

DspBuffer::DspBuffer()
    : _maxframes(16384)
    , _numframes(0) {
  for (int i = 0; i < kmaxdspblocksperstage; i++) {
    _channels[i].resize(_maxframes);
  }
}

///////////////////////////////////////////////////////////////////////////////

void DspBuffer::resize(int inumframes) {
  if (inumframes > _maxframes) {
    for (int i = 0; i < kmaxdspblocksperstage; i++) {
      _channels[i].resize(inumframes);
    }
    _maxframes = inumframes;
  }
  _numframes = inumframes;
}

///////////////////////////////////////////////////////////////////////////////

float* DspBuffer::channel(int ich) {
  ich = ich % kmaxdspblocksperstage;
  return _channels[ich].data();
}

} //namespace ork::audio::singularity {
