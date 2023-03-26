////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

#include <ork/util/triple_buffer.h>

using namespace ork::dataflow;

/////////////////////////////////////////
namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

ParticlePoolRenderBuffer::ParticlePoolRenderBuffer(int index)
  : _index(index) {
}
ParticlePoolRenderBuffer::~ParticlePoolRenderBuffer() {
  if (_particles) {
    delete[] _particles;
  }
}

int _nexthigherPO2(int inp) {
  int outp = inp;
  outp--;
  outp |= outp >> 1;
  outp |= outp >> 2;
  outp |= outp >> 4;
  outp |= outp >> 8;
  outp |= outp >> 16;
  outp++;
  return outp;
}

void ParticlePoolRenderBuffer::setCapacity(int icap) {
  if (icap > _maxParticles) {
    if (_particles)
      delete[] _particles;
    _maxParticles = _nexthigherPO2(icap);
    _particles    = new BasicParticle[_maxParticles];
  }
}
void ParticlePoolRenderBuffer::update(const pool_t& the_pool) {
  int icnt = the_pool.GetNumAlive();
  setCapacity(icnt);
  _numParticles = icnt;

  for (int i = 0; i < icnt; i++) {
    auto ptcl = the_pool.GetActiveParticle(i);
    _particles[i] = *ptcl;
  }
}


///////////////////////////////////////////////////////////////////////////////
} //namespace lev2::particle {
/////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

