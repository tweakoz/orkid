////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// VdbColliderGridHolder — concrete definition of the opaque holder
// declared in modular_forces.h. Wraps a vdb_floatgrid_ptr_t in a
// shared_ptr-friendly struct so the header doesn't need to include
// <openvdb>. Included by:
//   - modules_force_vdb_collider.cpp (the consumer)
//   - pyext_gfx_particles.cpp        (the .sdf_grid setter)
//
#pragma once

#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/openvdb.h>

namespace ork::lev2::particle {

struct VdbColliderGridHolder {
  vdb_floatgrid_ptr_t grid;
};

} // namespace ork::lev2::particle
