////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// scatter_ring_shape — the RING scatter proxy builder (a walk-INTO annulus: the
// stone kiva collar). Shared by BulletScatter.cpp (the consumer) and the ring gate
// (bullet_scatter_ring.cpp) so the tested geometry IS the shipped geometry.
//
// A ring proxy blocks LATERALLY but leaves its interior OPEN — the walker enters
// over the low collar (or down the ladder) and stands on the terrain collider's pit
// floor, where a solid box would have trapped it. dims = (r_mid, half_height,
// thickness) in item-local meters; the segment count is FIXED at 12 (proxy_dims is a
// vec3, so it cannot also carry a count). Segments overlap slightly (chord * 1.15) so
// no walker capsule slips a gap between them.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/math/cvector3.h>
#include <BulletCollision/CollisionShapes/btCompoundShape.h>
#include <BulletCollision/CollisionShapes/btBoxShape.h>
#include <cmath>
#include <vector>

namespace ork::ecs {

// FIXED segment count (dims is a vec3 — no room to also declare it).
static constexpr int kScatterRingSegments = 12;

// Build ONE btCompoundShape for a single ring item, in the item's LOCAL frame (the
// body transform carries the item's yaw + world position, so the children need no
// yaw baked in). `dims` = (r_mid, half_height, thickness); `scale` = the item's
// uniform scale (a compound is non-convex, so it cannot ride a btUniformScalingShape
// — scale bakes into the child geometry here instead). The compound holds 12 boxes
// arranged around local +Y at radius r_mid, each rotated so its width axis is tangent
// and its depth axis is radial, spanning [-half_height, +half_height] vertically (the
// same centered convention the box proxy uses). The interior carries NO floor or lid.
// The compound AND every child box are appended to `owned` (btCompoundShape does NOT
// own its children — the caller frees the whole list).
inline btCompoundShape* buildScatterRingCompound(
    const ork::fvec3& dims, float scale, std::vector<btCollisionShape*>& owned) {
  constexpr double PI = 3.14159265358979323846;
  const int   N      = kScatterRingSegments;
  const float r_mid  = dims.x * scale;
  const float half_h = dims.y * scale;
  const float thick  = dims.z * scale;
  // chord sized so adjacent segments OVERLAP (no gap a walker capsule threads):
  // a bare tangent chord would just meet at the mid-radius; *1.15 gives margin.
  const float chord  = 2.0f * r_mid * float(std::tan(PI / double(N))) * 1.15f;

  auto compound = new btCompoundShape();
  for (int i = 0; i < N; i++) {
    const double theta = (2.0 * PI * double(i)) / double(N);
    const float  ct    = float(std::cos(theta));
    const float  st    = float(std::sin(theta));
    // box half-extents: local X = tangent (chord/2), Y = up (half_h), Z = radial (thick/2)
    auto box = new btBoxShape(btVector3(chord * 0.5f, half_h, thick * 0.5f));
    owned.push_back(box);
    btTransform xf;
    // basis columns: col0 = tangent(-st,0,ct), col1 = up(0,1,0), col2 = radial(ct,0,st)
    // (btMatrix3x3 ctor is row-major). A box is symmetric, so the tangent sign is moot.
    xf.setBasis(btMatrix3x3(
        -st,  0.0f, ct,
        0.0f, 1.0f, 0.0f,
        ct,   0.0f, st));
    xf.setOrigin(btVector3(r_mid * ct, 0.0f, r_mid * st));
    compound->addChildShape(xf, box);
  }
  owned.push_back(compound);
  return compound;
}

} // namespace ork::ecs
