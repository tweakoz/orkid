////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// Reflection definition for RigidPrimitiveDrawableData. The struct
// itself lives in the header-only rigid_primitive.inl (it's templated
// adjacency-wise and inlined everywhere); reflection definitions
// have to live in exactly one TU, so they're collected here.

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>

ImplementReflectionX(ork::meshutil::RigidPrimitiveDrawableData,
                     "RigidPrimitiveDrawableData");

namespace ork::meshutil {

// Empty: none of the runtime fields (_primitive, _pipeline, _material)
// round-trip. See the comment on the struct declaration.
void RigidPrimitiveDrawableData::describeX(object::ObjectClass* /*clazz*/) {
}

} // namespace ork::meshutil
