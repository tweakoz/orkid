////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>
#include <ork/lev2/aud/singularity/alg_pan.inl>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

Fdn4ReverbData::Fdn4ReverbData() {
  _blocktype = "Fdn4Reverb";
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t Fdn4ReverbData::createInstance() const { // override
  return std::make_shared<Fdn4Reverb>(this);
}

///////////////////////////////////////////////////////////////////////////////

Fdn4Reverb::Fdn4Reverb(const Fdn4ReverbData* dbd)
    : DspBlock(dbd) {
  _delaybufferA.resize(dbd->_maxdelaylen);
  _delaybufferB.resize(dbd->_maxdelaylen);
  _delaybufferC.resize(dbd->_maxdelaylen);
  _delaybufferD.resize(dbd->_maxdelaylen);
  _indexA = 0;
  _indexB = 0;
  _indexC = 0;
  _indexD = 0;
}
static constexpr float kinv64k = 1.0f / 65536.0f;

///////////////////////////////////////////////////////////////////////////////

void Fdn4Reverb::compute(DspBuffer& dspbuf) // final
{
}

///////////////////////////////////////////////////////////////////////////////

void Fdn4Reverb::doKeyOn(const KeyOnInfo& koi) // final
{
}
} // namespace ork::audio::singularity
