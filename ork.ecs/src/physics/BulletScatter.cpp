///////////////////////////////////////////////////////////////////////////////
//
// BulletScatter — see bullet.h BulletShapeScatterData. Per-ITEM static rigid
// bodies for a placed ScatterSet (bullet's many-instances idiom: one shared base
// collision shape per type, N bodies referencing it; small per-body AABBs let
// broadphase cull). Item kind+dims come from the set's own proxy_kind/proxy_dims
// channels (declared at the scatter() sink, baked by the placer) — nothing
// hardcoded here. Items with kind -1 contribute no body. Per-item uniform scale
// rides a btUniformScalingShape wrapper around the shared base (a shared shape
// cannot carry per-body scaling). The earlier scene-spanning single-btCompoundShape
// form is retired: its AABB pairs with everything, and compound midphase costs showed
// up proportional to child count.
//
// The RING proxy is the ONE exception to the shared-convex-base rule: it is a
// per-item 12-box btCompoundShape (a walk-INTO annulus — the kiva collar). That is a
// PER-ITEM compound with a tight local AABB, not the retired scene-spanning one, so
// broadphase still culls; a compound cannot ride btUniformScalingShape (non-convex),
// so its scale bakes into the child geometry (see scatter_ring_shape.inl).
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/meshutil/geometry.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include "bullet_impl.h"
#include "scatter_ring_shape.inl"
#include <BulletCollision/CollisionShapes/btUniformScalingShape.h>
#include <BulletCollision/CollisionShapes/btEmptyShape.h>
#include <filesystem>
#include <map>
#include <array>

ImplementReflectionX(ork::ecs::BulletShapeScatterData, "BulletShapeScatterData");

namespace ork::ecs {

// proxy_kind vocabulary (baked per point by the placer): -1 none, 0 sphere(d0),
// 1 capsule(d0,d1), 2 box(d0,d1,d2), 3 cone(d0,d1). RING is kind 4 — kind 3 is a
// live cone path (btConeShape + the DSL "cone" collider vocab), so ring takes the
// next free slot rather than silently repurposing it. dims = (r_mid, half_height,
// thickness); see scatter_ring_shape.inl.
static constexpr int kProxyKindRing = 4;

void BulletShapeScatterData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("scatter_asset", &BulletShapeScatterData::_scatter_asset);
  clazz->directProperty("sink", &BulletShapeScatterData::_sink);
  clazz->directProperty("ogeo_path", &BulletShapeScatterData::_ogeo_path);
}

BulletShapeScatterData::BulletShapeScatterData() {
  _shapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst* {
    auto rval = new BulletShapeBaseInst(this);

    std::string path = _ogeo_path;
    if (path.empty()) {
      OrkAssert(not _scatter_asset.empty() and not _sink.empty());
      path = file::Path::expandPathString(
          "<assetcache>/terrain/" + _scatter_asset + "/" + _sink + ".ogeo");
    }
    if (not std::filesystem::exists(path)) {
      printf(
          "BulletShapeScatter: ScatterSet MISSING <%s> — the HeightField asset must "
          "materialize (and place) BEFORE this shape (declaration order = dependency "
          "order)\n",
          path.c_str());
      OrkAssert(false);
    }
    auto geo = meshutil::Geometry::readChunkfile(file::Path(path.c_str()));
    OrkAssert(geo);
    auto chX = geo->_point.channelAs<fmtx4>("xform");
    auto chK = geo->_point.channelAs<int>("proxy_kind");
    auto chD = geo->_point.channelAs<fvec3>("proxy_dims");
    OrkAssert(chX);
    if (not chK or not chD) {
      printf(
          "BulletShapeScatter: <%s> has NO proxy channels — declare colliders={...} on "
          "the scatter() sink (the collider shape rides the SET items)\n",
          path.c_str());
      OrkAssert(false);
    }

    auto batch = std::make_shared<ShapeBatchBodies>();
    batch->_bodies.reserve(chX->_data.size());
    // ONE base convex per distinct (kind, declared dims) — every item of a type
    // shares it; the per-item wrapper carries only that item's uniform scale.
    std::map<std::array<float, 4>, btConvexShape*> base_shapes;
    const auto& CDATA = data.mObject->data();
    int built = 0;
    for (size_t i = 0; i < chX->_data.size(); i++) {
      const int kind = chK->_data[i];
      if (kind < 0)
        continue; // this item's type declared no collider
      const auto& m = chX->_data[i];
      // the baked xform = T * R * S (uniform scale): scale goes to the wrapper,
      // bullet gets a RIGID body transform (normalized basis + translation).
      fvec4 c0 = m.column(0), c1 = m.column(1), c2 = m.column(2), c3 = m.column(3);
      const float s = fvec3(c0.x, c0.y, c0.z).magnitude();
      const float inv_s = (s > 1e-6f) ? (1.0f / s) : 1.0f;
      const fvec3 d = chD->_data[i];

      if (kind == kProxyKindRing) {
        // RING (walk-INTO annulus): a per-item 12-box btCompoundShape. Non-convex,
        // so it does NOT share a base / ride btUniformScalingShape — scale bakes in.
        // The RIGID body transform (normalized basis + translation) carries the
        // item's yaw and world position; the compound's children sit in local space.
        auto compound = buildScatterRingCompound(d, s, batch->_ownedShapes);
        btTransform xf;
        xf.setBasis(btMatrix3x3(
            c0.x * inv_s, c1.x * inv_s, c2.x * inv_s,
            c0.y * inv_s, c1.y * inv_s, c2.y * inv_s,
            c0.z * inv_s, c1.z * inv_s, c2.z * inv_s));
        xf.setOrigin(btVector3(c3.x, c3.y, c3.z));
        auto body = data.mWorld->AddLocalRigidBody(
            data.mEntity, 0.0f /*static*/, xf, compound, CDATA._groupAssign, CDATA._groupCollidesWith);
        body->forceActivationState(ISLAND_SLEEPING);
        body->setRestitution(CDATA._restitution);
        body->setFriction(CDATA._friction);
        batch->_bodies.push_back(body);
        built++;
        continue;
      }

      std::array<float, 4> key = {float(kind), d.x, d.y, d.z};
      btConvexShape*& base = base_shapes[key];
      if (not base) {
        switch (kind) {
          case 0: base = new btSphereShape(d.x); break;
          case 1: base = new btCapsuleShape(d.x, d.y); break; // radius, cylinder height
          case 2: base = new btBoxShape(btVector3(d.x, d.y, d.z)); break;
          case 3: base = new btConeShape(d.x, d.y); break;    // radius, height (Y-axis cone)
          default: break;
        }
        if (base)
          batch->_ownedShapes.push_back(base);
      }
      if (not base)
        continue;
      auto scaled = new btUniformScalingShape(base, s);
      batch->_ownedShapes.push_back(scaled);
      btTransform xf;
      xf.setBasis(btMatrix3x3( // columns normalized (scale removed); bullet basis is row-major ctor
          c0.x * inv_s, c1.x * inv_s, c2.x * inv_s,
          c0.y * inv_s, c1.y * inv_s, c2.y * inv_s,
          c0.z * inv_s, c1.z * inv_s, c2.z * inv_s));
      btVector3 origin(c3.x, c3.y, c3.z);
      if (kind == 3) {
        // CONE CONVENTION: dims=(radius,height) with the BASE AT THE ITEM
        // ORIGIN, apex up local +Y — matching the hypermesh cone primitive
        // (base ring at y=0, apex at y=H). btConeShape is CENTERED, so lift
        // the body by +height/2 (item-scaled) along the item's local up.
        origin += btVector3(c1.x * inv_s, c1.y * inv_s, c1.z * inv_s) * (d.y * 0.5f * s);
      }
      xf.setOrigin(origin);
      auto body = data.mWorld->AddLocalRigidBody(
          data.mEntity, 0.0f /*static*/, xf, scaled, CDATA._groupAssign, CDATA._groupCollidesWith);
      // mass 0 => CF_STATIC_OBJECT (never simulated/integrated); make the sleep
      // state explicit too so nothing ever ticks these.
      body->forceActivationState(ISLAND_SLEEPING);
      // the component's declared material — _onActivate only dresses the single
      // component body (our empty anchor), so batch bodies take it here.
      body->setRestitution(CDATA._restitution);
      body->setFriction(CDATA._friction);
      batch->_bodies.push_back(body);
      built++;
    }
    printf("BulletShapeScatter: %d static proxy bodies / %zu shared bases (of %zu items) from <%s>\n",
           built, base_shapes.size(), chX->_data.size(), path.c_str());
    rval->_impl.set<shapebatch_ptr_t>(batch);
    // the component's OWN body is just an anchor — empty shape, no contacts.
    rval->_collisionShape = new btEmptyShape();
    return rval;
  };
}

} // namespace ork::ecs
