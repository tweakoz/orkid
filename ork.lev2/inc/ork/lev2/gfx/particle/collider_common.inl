////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/lev2/gfx/particle/particle.h>
#include <ork/math/cvector3.h>

namespace ork::lev2::particle {

// ----------------------------------------------------------------------------
// Shared collision-response helper for all *Collider modules.
//
// Given a particle that has been detected as penetrating a surface, this:
//   1. Snaps the particle along contact_n by `penetration` units so it
//      sits on the surface (not below it).
//   2. If the velocity has a component INTO the surface (vn < 0), splits
//      velocity into normal/tangential parts, reflects the normal part
//      scaled by restitution (bounce), and damps the tangential part by
//      friction (slide drag).
//   3. ORs `state_bit` into mColliderStates as a "just touched" marker.
//      Each collider type owns a bit so multi-collider scenes can tell
//      them apart downstream:
//          bit 0 = PlaneCollider
//          bit 1 = SphereCollider
//          bit 2 = BoxCollider
//          bit 3 = (reserved, future VdbCollider)
//
// Coalescing: a single particle hit by multiple colliders in one frame
// gets each pass applied sequentially. Order is graph topological order
// — author responsibility to chain them sensibly (cheap-test first).
// ----------------------------------------------------------------------------
static inline void resolve_collision(
    BasicParticle& p,           //
    const fvec3& contact_n,     // outward unit normal at contact
    float penetration,          // depth (positive)
    float restitution,          // [0..2] — 1 elastic, 0 inelastic, >1 jumpy
    float friction,             // [0..1] — 1 = full tangential stop
    uint32_t state_bit) {       // marker bit for downstream consumers
  p.mPosition = p.mPosition + contact_n * penetration;
  float vn = p.mVelocity.dotWith(contact_n);
  if (vn < 0.0f) {
    fvec3 v_n   = contact_n * vn;
    fvec3 v_t   = p.mVelocity - v_n;
    p.mVelocity = v_t * (1.0f - friction) - v_n * restitution;
    p.mColliderStates |= state_bit;
  }
}

// Collider state bits — keep in sync with module implementations.
static constexpr uint32_t COLLIDER_BIT_PLANE  = 1u << 0;
static constexpr uint32_t COLLIDER_BIT_SPHERE = 1u << 1;
static constexpr uint32_t COLLIDER_BIT_BOX    = 1u << 2;
static constexpr uint32_t COLLIDER_BIT_VDB    = 1u << 3;

} // namespace ork::lev2::particle
