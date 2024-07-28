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
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace ork::dataflow;

namespace ork::lev2::particle {
fmtx4 createSphericalToEllipticalTransformationMatrix(const fvec3& center, const fvec3& semiMajorAxisDirection, fvec3 vscale) {
  // Start with an identity matrix
  fmtx4 transformation;

  // Scale matrix, assuming uniform scaling in all directions for simplicity
  fmtx4 S;
  S.setScale(vscale);

  // To align a vector A with vector B, rotate around their cross product by the angle between them
  fvec3 yAxis(0, 1, 0); // +Y is up
  fvec3 rotationAxis = yAxis.crossWith(semiMajorAxisDirection).normalized();
  float angle        = yAxis.angle(semiMajorAxisDirection);

  // Rotation matrix
  fquat Q(rotationAxis, angle);
  fmtx4 R = Q.toMatrix();

  // Translation matrix
  fmtx4 T;
  T.setTranslation(center);

  // Combine transformations: note the reverse order of operations
  transformation = T * S * R;

  return transformation;
}

struct EllipticalAttractorModuleInst : public ParticleModuleInst {

  EllipticalAttractorModuleInst(
      const EllipticalAttractorModuleData* gmd, //
      dataflow::GraphInst* ginst)
      : ParticleModuleInst(gmd, ginst) {
  }

  ////////////////////////////////////////////////////

  void onLink(GraphInst* inst) final {

    _onLink(inst);

    /////////////////
    // inputs
    /////////////////

    _input_inertia   = typedInputNamed<FloatXfPlugTraits>("Inertia");
    _input_dampening = typedInputNamed<FloatXfPlugTraits>("Dampening");
    _input_scalar    = typedInputNamed<FloatXfPlugTraits>("Scalar");
    _input_power    = typedInputNamed<FloatXfPlugTraits>("Power");
    _input_orient    = typedInputNamed<QuatXfPlugTraits>("Orientation");
    _input_p1        = typedInputNamed<Vec3XfPlugTraits>("P1");
    _input_p2        = typedInputNamed<Vec3XfPlugTraits>("P2");
  }

  ////////////////////////////////////////////////////

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    fvec3 fociA    = _input_p1->value();
    fvec3 fociB    = _input_p2->value();
    float finertia = _input_inertia->value();

    fvec3 center       = (fociA + fociB) * 0.5;
    float dampenFactor = _input_dampening->value();
    float dt           = updata->_dt;

    // Calculate the semi-major axis length and create a normalized direction vector for it
    fvec3 semiMajorAxisDirection = (fociB - fociA).normalized();
    float d                      = (fociB - fociA).magnitude(); // Distance between the foci
    float b                      = 1 + d;                       // Linear relationship for b based on the distance

    // Assuming a and c should be constants, or determined by another specific requirement
    float a = std::max(1.0f, log(b)); // Placeholder value, adjust based on your requirements
    float c = a;                      // Same as a, assuming symmetry about the Y-axis

    // Create transformation matrix to stretch coordinate system
    // Assuming fmtx4 is a type compatible with glm::mat4
    fmtx4 transform_sph_to_wld = createSphericalToEllipticalTransformationMatrix(center, semiMajorAxisDirection, fvec3(a, b, c));
    fmtx4 transform_wld_to_sph = transform_sph_to_wld.inverse();

    float power = _input_power->value();
    float scalar = _input_scalar->value();

    for (int i = 0; i < _pool->GetNumAlive(); i++) {
      BasicParticle* particle = _pool->GetActiveParticle(i);

      // Transform particle position into ellipse-aligned coordinate system
      fvec3 sph_position = (fvec4(particle->mPosition, 1.0).transform(transform_wld_to_sph)).xyz();

      // Now apply spherical logic in transformed space
      float sph_distanceToCenter = sph_position.magnitude();

      float radius = scalar;

      float sph_distanceToSurface = sph_distanceToCenter - radius;

      fvec3 sph_directionToSurface = (sph_distanceToCenter > radius)                         //
                                         ? sph_position.normalized() * sph_distanceToSurface //
                                         : -sph_position.normalized() * sph_distanceToSurface;

      fvec3 sph_surface_point = center + sph_directionToSurface;

      fvec3 wld_surface_point =
          fvec4(sph_surface_point, 1.0).transform(transform_sph_to_wld).xyz(); // Transform back to world space

      fvec3 wld_delta    = wld_surface_point - particle->mPosition;
      float wld_distance = wld_delta.magnitude();

      float forceMagnitude = 1.0 / (1.0 + wld_distance);
      forceMagnitude = powf(forceMagnitude,power);
      fvec3 accel          = wld_delta.normalized() * forceMagnitude / finertia;

      // Now ensure this direction is correctly applied to the particle's velocity in the original coordinate system.
      particle->mVelocity += accel * dt;
      particle->mVelocity *= dampenFactor;
    }
  }

  ////////////////////////////////////////////////////

  floatxf_inp_pluginst_ptr_t _input_dampening;
  floatxf_inp_pluginst_ptr_t _input_inertia;
  floatxf_inp_pluginst_ptr_t _input_scalar;
  floatxf_inp_pluginst_ptr_t _input_power;
  fvec3xf_inp_pluginst_ptr_t _input_p1;
  fvec3xf_inp_pluginst_ptr_t _input_p2;
  fquatxf_inp_pluginst_ptr_t _input_orient;
};

//////////////////////////////////////////////////////////////////////////

EllipticalAttractorModuleData::EllipticalAttractorModuleData() {
}

//////////////////////////////////////////////////////////////////////////

static void _reshapeEllipticalAttractorIOs(dataflow::moduledata_ptr_t data) {
  auto typed = std::dynamic_pointer_cast<EllipticalAttractorModuleData>(data);
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Inertia")->_range   = {-10.0f, 10.0f};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Dampening")->_range = {0, 1};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Scalar")->_range = {0, 100};
  ModuleData::createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Power")->_range = {0, 10};
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "P1")->_range         = {-1000.0f, 1000.0f};
  ModuleData::createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "P2")->_range         = {-1000.0f, 1000.0f};
  ModuleData::createInputPlug<QuatXfPlugTraits>(data, EPR_UNIFORM, "Orientation");
}
//////////////////////////////////////////////////////////////////////////

std::shared_ptr<EllipticalAttractorModuleData> EllipticalAttractorModuleData::createShared() {
  auto data = std::make_shared<EllipticalAttractorModuleData>();
  _initPoolIOs(data);
  _reshapeEllipticalAttractorIOs(data);
  return data;
}

rtti::castable_ptr_t EllipticalAttractorModuleData::sharedFactory() {
  return createShared();
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t EllipticalAttractorModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<EllipticalAttractorModuleInst>(this, ginst);
}

//////////////////////////////////////////////////////////////////////////

void EllipticalAttractorModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t { return EllipticalAttractorModuleData::createShared(); });
  clazz->annotateTyped<moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) { _reshapeEllipticalAttractorIOs(mdata); });
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::EllipticalAttractorModuleData, "psys::EllipticalAttractorModuleData");
