////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
// Regression: a same-path terrain REBAKE (height.exr + .terrain.json overwritten IN PLACE)
// must reload into the live Bullet heightfield collider — a path-only reload gate never
// fires, leaving physics stale while render shows the new terrain. This drives the real
// BulletShapeTerrainData -> BulletTerrainImpl load path against a real btDynamicsWorld and
// PROVES the physics-sampled surface (rayTest) tracks an in-place rebake, plus the
// loud-failure path (missing artifact -> named error, previous heights retained).
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/orkprotos.h> // ork::msleep
#include <ork/kernel/msgrouter.inl>
#include <ork/file/path.h>
#include <ork/lev2/gfx/image.h>
#include <ork/ecs/physics/bullet.h>
#include <utpp/UnitTest++.h>

#include "physics/bullet_impl.h" // BulletSystem, BulletShapeBaseInst, bullet headers

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>

using namespace ork;
using namespace ork::ecs;

///////////////////////////////////////////////////////////////////////////////

TEST(BulletTerrainRebakeReload) {
  namespace fs = std::filesystem;

  const int   D        = 33;
  const float EXTENT_M = 200.0f;
  const float HEIGHT_M = 100.0f;
  const std::string ASSET = "bullet_rebake_selftest";

  const std::string base = file::Path::expandPathString("<assetcache>/terrain/" + ASSET);
  fs::create_directories(base);
  const std::string exrpath      = base + "/height.exr";
  const std::string manifestpath = base + "/" + ASSET + ".terrain.json";

  // --- fixture writers (stand in for the GPU bake; heights are TRUE METERS, v2 manifest) ---
  auto write_exr = [&](const std::function<float(int, int)>& fn) {
    lev2::Image img;
    img.initWithFormat(size_t(D), size_t(D), lev2::EBufferFormat::R32F);
    for (int y = 0; y < D; y++)
      for (int x = 0; x < D; x++)
        img.pixel32f(x, y)[0] = fn(x, y);
    img.writeToFile(file::Path(exrpath.c_str()), true /*linear*/);
  };
  auto write_manifest = [&]() {
    FILE* f = fopen(manifestpath.c_str(), "w");
    OrkAssert(f);
    fprintf(f,
            "{\n  \"version\": 2,\n  \"scale\": {\n    \"extent_m\": %f,\n"
            "    \"dim\": %d,\n    \"origin_m\": [0.0, 0.0, 0.0],\n    \"up_axis\": \"y\",\n"
            "    \"height_anchor\": \"meters\"\n  },\n  \"format\": \"exr\",\n"
            "  \"channels\": {\n    \"height\": {\n      \"file\": \"height.exr\",\n"
            "      \"semantic\": \"height_meters\",\n      \"min\": 0.0,\n      \"max\": %f,\n"
            "      \"mean\": %f\n    }\n  },\n  \"provenance\": {\n    \"asset_name\": \"%s\"\n  }\n}\n",
            EXTENT_M, D, HEIGHT_M, HEIGHT_M * 0.5f, ASSET.c_str());
    fclose(f);
  };

  // two bakes that both span the SAME meter range (so the collider AABB is unchanged)
  // but differ everywhere off-center: rampA rises with the grid, rampB is its mirror.
  auto rampA = [&](int x, int) { return HEIGHT_M * float(x) / float(D - 1); };
  auto rampB = [&](int x, int) { return HEIGHT_M * float(D - 1 - x) / float(D - 1); };

  write_manifest();
  write_exr(rampA);

  // --- real physics world + terrain collider (no ECS FSM / GPU needed for a static collider) ---
  BulletSystemData bsysdata;
  BulletSystem     bsys(bsysdata, nullptr); // InitWorld() builds the btDynamicsWorld; Simulation unused by rayTest

  auto shapedata               = std::make_shared<BulletShapeTerrainData>();
  shapedata->_hf_asset         = ASSET;
  shapedata->_render_dimension = 0; // collide at full EXR res

  ShapeCreateData scd;
  scd.mEntity = nullptr; // terrain load never derefs the entity
  scd.mWorld  = &bsys;
  scd.mObject = nullptr;
  auto shapeinst = shapedata->CreateShape(scd); // -> BulletTerrainImpl ctor (initial load) + init_bullet_shape
  CHECK(shapeinst != nullptr);
  CHECK(shapeinst->_collisionShape != nullptr);

  btTransform xf;
  xf.setIdentity();
  auto ms   = new btDefaultMotionState(xf);
  btRigidBody::btRigidBodyConstructionInfo ci(0.0, ms, shapeinst->_collisionShape); // mass 0 -> static
  auto body = new btRigidBody(ci);
  bsys.GetDynamicsWorld()->addRigidBody(body);

  // sample the terrain SURFACE height at a world point via a physics raycast (straight down)
  auto sample = [&](float wx, float wz) -> float {
    btVector3 from(wx, HEIGHT_M * 2.0f, wz), to(wx, -HEIGHT_M * 2.0f, wz);
    btCollisionWorld::ClosestRayResultCallback cb(from, to);
    bsys.GetDynamicsWorld()->rayTest(from, to, cb);
    return cb.hasHit() ? float(cb.m_hitPointWorld.y()) : NAN;
  };
  // drain deferred terrain reloads exactly as BulletSystem::_onUpdate does (update thread)
  auto pump = [&]() {
    for (auto& item : bsys._terrainReloadPolls)
      item.second();
  };

  // off-center so rampA vs rampB differ maximally; inside the [-extent/2,+extent/2] field
  const float SX = 0.35f * EXTENT_M, SZ = 0.0f;

  float hitA = sample(SX, SZ);
  printf("REBAKE-TEST hitA=%g\n", hitA);
  CHECK(std::isfinite(hitA));

  // --- REBAKE in place (rampB) + the rebake-completion broadcast, then consume on 'update' ---
  ork::msleep(1100); // guarantee a distinct mtime on any FS (the stamp gate keys off mtime+size)
  write_manifest();
  write_exr(rampB);
  msgrouter::channel("bshdchanged")->post(nullptr); // == HeightFieldGenData::materialize's post
  pump();

  float hitB = sample(SX, SZ);
  printf("REBAKE-TEST hitB=%g delta=%g\n", hitB, std::fabs(hitA - hitB));
  CHECK(std::isfinite(hitB));
  // the physics surface tracked the in-place rebake (the whole point of the fix)
  CHECK(std::fabs(hitA - hitB) > 0.3f * HEIGHT_M);

  // --- loud failure: stamp points at a now-missing artifact -> keep OLD data (never zero) ---
  ork::msleep(1100);
  fs::remove(exrpath);
  msgrouter::channel("bshdchanged")->post(nullptr);
  pump(); // BulletShapeTerrain: RELOAD FAILED ... retaining previous heights

  float hitC = sample(SX, SZ);
  printf("REBAKE-TEST hitC=%g (expect ~hitB)\n", hitC);
  CHECK(std::isfinite(hitC));
  CHECK_CLOSE(hitB, hitC, 1e-3f); // reload failed -> previous heights retained, not zeroed

  // tidy: destroy the shape inst (unsubscribes + unregisters the reload poll) while bsys is alive
  bsys.GetDynamicsWorld()->removeRigidBody(body);
  delete shapeinst;
  delete body;
  delete ms;
  fs::remove(manifestpath);
}

///////////////////////////////////////////////////////////////////////////////
