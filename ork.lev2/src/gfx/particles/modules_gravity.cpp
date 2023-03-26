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

using namespace ork::dataflow;

namespace ork::lev2::particle {

struct GravityModuleInst : public ParticleModuleInst {

  GravityModuleInst(const GravityModuleData* gmd)
      : ParticleModuleInst(gmd) {
  }

  ////////////////////////////////////////////////////

  void onLink(GraphInst* inst) final {

  _onLink(inst);

    /////////////////
    // inputs
    /////////////////

    _input_g           = typedInputNamed<FloatXfPlugTraits>("G");
    _input_mass        = typedInputNamed<FloatXfPlugTraits>("Mass");
    _input_othermass   = typedInputNamed<FloatXfPlugTraits>("OthMass");
    _input_mindistance = typedInputNamed<FloatXfPlugTraits>("MinDistance");

    _input_center = typedInputNamed<Vec3XfPlugTraits>("Center");

  }

  ////////////////////////////////////////////////////

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    float fmass    = powf(10.0f, _input_mass->value());
    float fothmass = powf(10.0f, _input_othermass->value());
    float fG       = powf(10.0f, _input_g->value());
    float finvmass = (fothmass == 0.0f) ? 0.0f : (1.0f / fothmass);
    float numer    = (fmass * fothmass * fG);
    // printf( "fmass<%f>\n", fmass );
    // printf( "fothmass<%f>\n", fothmass );
    // printf( "fG<%f>\n", fG );
    // printf( "finvmass<%f>\n", finvmass );
    // printf( "numer<%f>\n", numer );
    float mindist = _input_mindistance->value();
    fvec3 center  = _input_center->value();
    float dt      = updata->_dt;

    for (int i = 0; i < _pool->GetNumAlive(); i++) {
      BasicParticle* particle = _pool->GetActiveParticle(i);
      const fvec3& old_pos    = particle->mPosition;
      fvec3 direction         = (center - old_pos);
      float magnitude         = direction.magnitude();
      // printf( "old_pos<%f %f %f>\n", old_pos.x, old_pos.y, old_pos.z );
      // printf( "direction<%f %f %f>\n", direction.x, direction.y, direction.z );
      // printf( "magnitude<%f>\n", magnitude );
      if (magnitude < mindist)
        magnitude = mindist;
      direction *= (1.0f / magnitude);
      float denom    = magnitude * magnitude;
      fvec3 forceVec = direction * (numer / denom);
      fvec3 accel    = forceVec * finvmass;
      // printf( "denim<%f> accel<%f %f %f>\n", denom, accel.x, accel.y, accel.z );
      particle->mVelocity += accel * dt;
    }
  }

  ////////////////////////////////////////////////////

  floatxf_inp_pluginst_ptr_t _input_g;
  floatxf_inp_pluginst_ptr_t _input_mass;
  floatxf_inp_pluginst_ptr_t _input_othermass;
  floatxf_inp_pluginst_ptr_t _input_mindistance;
  fvec3xf_inp_pluginst_ptr_t _input_center;

};

//////////////////////////////////////////////////////////////////////////

void GravityModuleData::describeX(class_t* clazz) {
}

//////////////////////////////////////////////////////////////////////////

GravityModuleData::GravityModuleData() {
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<GravityModuleData> GravityModuleData::createShared() {
  auto data = std::make_shared<GravityModuleData>();

  _initShared(data);

  //RegisterFloatXfPlug(GravityModule, G, -10.0f, 10.0f, ged::OutPlugChoiceDelegate);
  //RegisterFloatXfPlug(GravityModule, Mass, -10.0f, 10.0f, ged::OutPlugChoiceDelegate);
  //RegisterFloatXfPlug(GravityModule, OthMass, -10.0f, 10.0f, ged::OutPlugChoiceDelegate);
  //RegisterFloatXfPlug(GravityModule, MinDistance, 0.0f, 100.0f, ged::OutPlugChoiceDelegate);
  //RegisterVect3XfPlug(GravityModule, Center, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);

  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "G");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Mass");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "OthMass");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "MinDistance");
  createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Center");

  return data;
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t GravityModuleData::createInstance() const {
  return std::make_shared<GravityModuleInst>(this);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::GravityModuleData, "psys::GravityModuleData");
