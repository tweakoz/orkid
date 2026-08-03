////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
// RING scatter proxy gate (owner 07-22: "you cannot walk INSIDE the stone ring").
// The kiva collar collider must block LATERALLY while leaving its interior OPEN, so a
// walker drops to the terrain pit floor instead of standing on a solid box lid. This
// drives the SHARED ring builder (scatter_ring_shape.inl — the exact geometry
// BulletScatter.cpp ships) against a real btDiscreteDynamicsWorld and rayTests it:
//   (a) an outside-in horizontal ray hits the OUTER wall face  (~r_mid + t/2)
//   (b) a center-out horizontal ray hits the INNER wall face   (~r_mid - t/2)  [blocks BOTH ways]
//   (c) a vertical ray down THROUGH the center hits NOTHING     [interior open]
//   (d) center-out rays at every 5 degrees ALL hit             [no gap a capsule threads]
// plus a vertical ray down through the wall proves the collar has a top where the
// center has none.
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/cvector3.h>
#include <btBulletDynamicsCommon.h>
#include <utpp/UnitTest++.h>

#include "physics/scatter_ring_shape.inl"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace ork;
using namespace ork::ecs;

///////////////////////////////////////////////////////////////////////////////

namespace {

struct RingWorld {
  btDefaultCollisionConfiguration*     _cfg;
  btCollisionDispatcher*               _disp;
  btDbvtBroadphase*                    _bp;
  btSequentialImpulseConstraintSolver* _solver;
  btDiscreteDynamicsWorld*             _world;
  btRigidBody*                         _body;
  std::vector<btCollisionShape*>       _owned;

  RingWorld(const fvec3& dims, float scale) {
    _cfg    = new btDefaultCollisionConfiguration();
    _disp   = new btCollisionDispatcher(_cfg);
    _bp     = new btDbvtBroadphase();
    _solver = new btSequentialImpulseConstraintSolver();
    _world  = new btDiscreteDynamicsWorld(_disp, _bp, _solver, _cfg);

    auto compound = buildScatterRingCompound(dims, scale, _owned);
    btTransform xf;
    xf.setIdentity(); // ring centered at origin, no yaw — the synthetic fixture
    auto ms = new btDefaultMotionState(xf);
    btRigidBody::btRigidBodyConstructionInfo ci(0.0, ms, compound); // mass 0 -> static
    _body = new btRigidBody(ci);
    _world->addRigidBody(_body);
  }
  ~RingWorld() {
    _world->removeRigidBody(_body);
    delete _body->getMotionState();
    delete _body;
    for (auto s : _owned)
      delete s; // compound + every child box
    delete _world;
    delete _solver;
    delete _bp;
    delete _disp;
    delete _cfg;
  }
  int numSegments() const {
    return static_cast<btCompoundShape*>(_body->getCollisionShape())->getNumChildShapes();
  }
  // closest hit along [from,to]; returns hit point, sets `hit`.
  fvec3 ray(const fvec3& from, const fvec3& to, bool& hit) const {
    btVector3 f(from.x, from.y, from.z), t(to.x, to.y, to.z);
    btCollisionWorld::ClosestRayResultCallback cb(f, t);
    _world->rayTest(f, t, cb);
    hit = cb.hasHit();
    return hit ? fvec3(cb.m_hitPointWorld.x(), cb.m_hitPointWorld.y(), cb.m_hitPointWorld.z())
               : fvec3(0, 0, 0);
  }
};

} // namespace

///////////////////////////////////////////////////////////////////////////////

TEST(BulletScatterRingProxy) {
  const float R_MID   = 3.0f;
  const float HALF_H  = 0.75f;
  const float THICK   = 0.3f;
  const fvec3 dims(R_MID, HALF_H, THICK);
  const float outer   = R_MID + THICK * 0.5f; // 3.15
  const float inner   = R_MID - THICK * 0.5f; // 2.85
  const float TOL     = 0.06f;                 // box-face precision (< half a course)

  RingWorld rw(dims, 1.0f);
  printf("RING-TEST segments=%d (expect 12)\n", rw.numSegments());
  CHECK_EQUAL(kScatterRingSegments, rw.numSegments());

  bool hit = false;
  const float y0 = 0.0f; // mid-wall (wall spans y in [-HALF_H,+HALF_H])

  // (a) outside-in horizontal ray hits the OUTER wall face
  {
    fvec3 h = rw.ray(fvec3(outer + 2.0f, y0, 0), fvec3(0, y0, 0), hit);
    printf("RING-TEST (a) outer-in hitX=%g (expect ~%g) hit=%d\n", h.x, outer, int(hit));
    CHECK(hit);
    CHECK_CLOSE(outer, h.x, TOL);
  }

  // (b) center-out horizontal ray hits the INNER wall face (blocks BOTH ways)
  {
    fvec3 h = rw.ray(fvec3(0, y0, 0), fvec3(outer + 2.0f, y0, 0), hit);
    printf("RING-TEST (b) center-out hitX=%g (expect ~%g) hit=%d\n", h.x, inner, int(hit));
    CHECK(hit);
    CHECK_CLOSE(inner, h.x, TOL);
  }

  // (c) vertical ray down THROUGH the center hits NOTHING (interior open)
  {
    rw.ray(fvec3(0, HALF_H + 5.0f, 0), fvec3(0, -HALF_H - 5.0f, 0), hit);
    printf("RING-TEST (c) center-down hit=%d (expect 0 — open interior)\n", int(hit));
    CHECK(not hit);
  }

  // corroboration: vertical ray down through the WALL hits the collar top (~+HALF_H)
  {
    fvec3 h = rw.ray(fvec3(R_MID, HALF_H + 5.0f, 0), fvec3(R_MID, -HALF_H - 5.0f, 0), hit);
    printf("RING-TEST wall-down hitY=%g (expect ~%g) hit=%d\n", h.y, HALF_H, int(hit));
    CHECK(hit);
    CHECK_CLOSE(HALF_H, h.y, TOL);
  }

  // (d) no-gap: center-out rays at every 5 degrees ALL hit, near the wall radius
  {
    const float RCAST = outer + 1.0f;
    int nrays = 0, nhit = 0;
    float min_r = 1e9f, max_r = -1e9f;
    for (int deg = 0; deg < 360; deg += 5) {
      const double a = double(deg) * 3.14159265358979323846 / 180.0;
      fvec3 to(RCAST * float(std::cos(a)), y0, RCAST * float(std::sin(a)));
      fvec3 h = rw.ray(fvec3(0, y0, 0), to, hit);
      nrays++;
      if (hit) {
        nhit++;
        const float r = std::sqrt(h.x * h.x + h.z * h.z);
        min_r = std::min(min_r, r);
        max_r = std::max(max_r, r);
      }
    }
    printf("RING-TEST (d) no-gap hit %d/%d rays, hit-radius [%g..%g] (expect all, ~[%g..%g])\n",
           nhit, nrays, min_r, max_r, inner, outer);
    CHECK_EQUAL(nrays, nhit);            // every angle blocked — no gap
    CHECK(min_r >= inner - TOL);          // no hit inside the wall
    CHECK(max_r <= outer + TOL);          // no hit outside the wall
  }
}

///////////////////////////////////////////////////////////////////////////////
// SCALE: the item's uniform scale bakes into the compound (a compound cannot ride
// btUniformScalingShape). A 2x item must place its wall at 2x radius / thickness.
///////////////////////////////////////////////////////////////////////////////

TEST(BulletScatterRingProxyScaled) {
  const float R_MID  = 3.0f, HALF_H = 0.75f, THICK = 0.3f, S = 2.0f;
  const fvec3 dims(R_MID, HALF_H, THICK);
  const float outer  = (R_MID + THICK * 0.5f) * S; // 6.3
  const float TOL    = 0.12f;                       // scaled face precision

  RingWorld rw(dims, S);
  bool hit = false;
  fvec3 h  = rw.ray(fvec3(outer + 2.0f, 0, 0), fvec3(0, 0, 0), hit);
  printf("RING-TEST scale=2 outer-in hitX=%g (expect ~%g) hit=%d\n", h.x, outer, int(hit));
  CHECK(hit);
  CHECK_CLOSE(outer, h.x, TOL);

  // interior still open at 2x
  rw.ray(fvec3(0, HALF_H * S + 5.0f, 0), fvec3(0, -HALF_H * S - 5.0f, 0), hit);
  printf("RING-TEST scale=2 center-down hit=%d (expect 0)\n", int(hit));
  CHECK(not hit);
}

///////////////////////////////////////////////////////////////////////////////
