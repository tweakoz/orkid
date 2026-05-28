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
#include <ork/lev2/gfx/particle/collider_common.inl>
#include <ork/lev2/gfx/particle/vdbcollider_holder.inl>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/math/cmatrix4.h>

#include <openvdb/tools/Interpolation.h>

using namespace ork::dataflow;

namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////

struct VdbColliderModuleInst : public ParticleModuleInst {

  using sampler_t = openvdb::tools::GridSampler<
      vdb_floatgrid_t,
      openvdb::tools::BoxSampler>;
  using sampler_ptr_t = std::shared_ptr<sampler_t>;

  VdbColliderModuleInst(const VdbColliderModuleData* d, dataflow::GraphInst* ginst)
      : ParticleModuleInst(d, ginst)
      , _vrd(d) {
  }

  void onLink(GraphInst* inst) final {
    _onLink(inst);
    _input_restitution = typedInputNamed<FloatXfPlugTraits>("Restitution");
    _input_friction    = typedInputNamed<FloatXfPlugTraits>("Friction");
  }

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    // The grid is set from Python after the module is created, so it can
    // be null on early frames. Bail cleanly.
    if (not _vrd->_sdfGrid) return;
    auto grid = _vrd->_sdfGrid->grid;
    if (not grid) return;

    // Lazy-construct (or rebuild if the user swapped grids). The sampler
    // caches the grid's accessor → way faster than re-fetching per query.
    if (_sampler_grid.get() != grid.get()) {
      _sampler      = std::make_shared<sampler_t>(*grid);
      _sampler_grid = grid;
      // Gradient step: ~1 voxel in world units. Trilinear sampling
      // inside a voxel is C0-continuous; central differences at one
      // voxel step give a smooth-enough normal for collision response.
      auto v        = grid->voxelSize();
      _grad_step    = float(v[0]);
      if (_grad_step <= 0.0f) _grad_step = 0.05f;
    }

    float restitution = _input_restitution->value();
    float friction    = _input_friction->value();

    const int n  = _pool->GetNumAlive();
    const float h = _grad_step;
    const float inv2h = 1.0f / (2.0f * h);

    // Local-space mode: if the data side names a published entity and
    // the host has a live resolver wired, fetch the entity's live
    // decompxf and compose to a matrix once per frame here. We bring
    // particle positions into the entity's local frame before SDF
    // sampling and rotate the gradient normal back by host_xf for the
    // collision response. Standalone graphs (no resolver bound) or
    // unpublished names fall through to world-space — matches legacy
    // behavior and keeps Tier 1/2 standalone hosts working without ECS.
    bool local_space = false;
    fmtx4 host_xf  = fmtx4::Identity();
    fmtx4 host_inv = fmtx4::Identity();
    if (not _vrd->_follow_entity.empty() && inst->_resolveEntityXf) {
      if (auto xf = inst->_resolveEntityXf(_vrd->_follow_entity)) {
        host_xf     = xf->composed();
        host_inv    = host_xf.inverse();
        local_space = true;
      }
    }

    for (int i = 0; i < n; i++) {
      BasicParticle* ptc = _pool->GetActiveParticle(i);
      fvec3 p = ptc->mPosition;
      if (local_space) {
        // transform(matrix) returns a fvec4 with the affine xy w=1
        // result; orkid's translation is in the last column, so this
        // is a true world→local point transform.
        auto p4 = p.transform(host_inv);
        p = fvec3(p4.x, p4.y, p4.z);
      }

      // SDF sample at the (possibly local-space) particle position.
      float sdf = _sampler->wsSample(openvdb::Vec3R(p.x, p.y, p.z));
      if (sdf >= 0.0f) continue;  // outside the solid → no contact

      // Central-difference gradient for the outward normal.
      // For a proper SDF this gradient should be unit-length; small
      // deviations are normalized below.
      float dx = _sampler->wsSample(openvdb::Vec3R(p.x + h, p.y, p.z))
               - _sampler->wsSample(openvdb::Vec3R(p.x - h, p.y, p.z));
      float dy = _sampler->wsSample(openvdb::Vec3R(p.x, p.y + h, p.z))
               - _sampler->wsSample(openvdb::Vec3R(p.x, p.y - h, p.z));
      float dz = _sampler->wsSample(openvdb::Vec3R(p.x, p.y, p.z + h))
               - _sampler->wsSample(openvdb::Vec3R(p.x, p.y, p.z - h));

      fvec3 grad(dx * inv2h, dy * inv2h, dz * inv2h);
      float glen2 = grad.dotWith(grad);
      if (glen2 < 1e-8f) continue;  // degenerate gradient (e.g. far inside)
      fvec3 normal = grad * (1.0f / std::sqrt(glen2));

      if (local_space) {
        // Rotate the local-space normal back to world. Transform as a
        // direction (3x3 part of the matrix, no translation).
        normal = normal.transform3x3(host_xf);
      }

      resolve_collision(*ptc, normal, -sdf, restitution, friction,
                        COLLIDER_BIT_VDB);
    }
  }

  const VdbColliderModuleData* _vrd;
  floatxf_inp_pluginst_ptr_t _input_restitution;
  floatxf_inp_pluginst_ptr_t _input_friction;

  // Cached sampler — rebuilt only when the source grid pointer changes.
  sampler_ptr_t       _sampler;
  vdb_floatgrid_ptr_t _sampler_grid;
  float               _grad_step = 0.05f;
};

///////////////////////////////////////////////////////////////////////////////

VdbColliderModuleData::VdbColliderModuleData() {
}

static void _reshapeVdbColliderIOs(dataflow::moduledata_ptr_t mdata) {
  auto typed = std::dynamic_pointer_cast<VdbColliderModuleData>(mdata);
  ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "Restitution")->_range = {0.0f, 2.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "Friction")->_range    = {0.0f, 1.0f};
}

std::shared_ptr<VdbColliderModuleData> VdbColliderModuleData::createShared() {
  auto data = std::make_shared<VdbColliderModuleData>();
  _initPoolIOs(data);
  _reshapeVdbColliderIOs(data);
  auto in_rest = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Restitution"));
  auto in_fric = std::dynamic_pointer_cast<inplugdata<FloatXfPlugTraits>>(data->inputNamed("Friction"));
  if (in_rest) in_rest->setValue(0.5f);
  if (in_fric) in_fric->setValue(0.0f);
  return data;
}

void VdbColliderModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t {
    return VdbColliderModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs", [](dataflow::moduledata_ptr_t mdata) {
    _reshapeVdbColliderIOs(mdata);
  });
  clazz->directProperty("follow_entity", &VdbColliderModuleData::_follow_entity);
}

dgmoduleinst_ptr_t VdbColliderModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<VdbColliderModuleInst>(this, ginst);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::VdbColliderModuleData, "psys::VdbColliderModuleData");
