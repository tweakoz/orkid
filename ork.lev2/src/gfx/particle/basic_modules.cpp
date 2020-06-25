////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/reflect/enum_serializer.inl>
#include <ork/math/collision_test.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/particle/modular_particles.h>
#include <ork/lev2/lev2_asset.h>
#include <signal.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::Global, "psys::Global");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::ParticlePool, "psys::Pool");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::RingEmitter, "psys::RingEmitterModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::NozzleEmitter, "psys::NozzleEmitterModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::ReEmitter, "psys::ReEmitterModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::WindModule, "psys::WindModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::GravityModule, "psys::GravityModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::PlanarColliderModule, "psys::PlanarColliderModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::SphericalColliderModule, "psys::SphericalColliderModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::DecayModule, "psys::DecayModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::TurbulenceModule, "psys::TurbulenceModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::VortexModule, "psys::VortexModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::ExtConnector, "psys::ExtConnector");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::Constants, "psys::Constants");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::FloatOp2Module, "psys::FloatOp2Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::Vec3Op2Module, "psys::Vec3Op2Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::Vec3SplitModule, "psys::Vec3SplitModule");

BEGIN_ENUM_SERIALIZER(ork::lev2::particle, EPSYS_FLOATOP)
DECLARE_ENUM(EPSYS_FLOATOP_ADD)
DECLARE_ENUM(EPSYS_FLOATOP_SUB)
DECLARE_ENUM(EPSYS_FLOATOP_MUL)
END_ENUM_SERIALIZER()

BEGIN_ENUM_SERIALIZER(ork::lev2::particle, EPSYS_VEC3OP)
DECLARE_ENUM(EPSYS_VEC3OP_ADD)
DECLARE_ENUM(EPSYS_VEC3OP_SUB)
DECLARE_ENUM(EPSYS_VEC3OP_MUL)
DECLARE_ENUM(EPSYS_VEC3OP_DOT)
DECLARE_ENUM(EPSYS_VEC3OP_CROSS)
END_ENUM_SERIALIZER()

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { namespace particle {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Global::Describe() {
  RegisterFloatXfPlug(Global, TimeScale, -100, 100.0f, ged::OutPlugChoiceDelegate);
}
///////////////////////////////////////////////////////////////////////////////
Global::Global()
    : ConstructOutPlug(Time, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(RelTime, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(TimeDiv10, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(TimeDiv100, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(Random, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(RandomNormal, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(SlowNoise, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(Noise, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(FastNoise, dataflow::EPR_UNIFORM)
    , mPlugInpTimeScale(this, dataflow::EPR_UNIFORM, mfTimeScale, "ts")
    , mfTimeScale(1.0f)
    , mOutDataRandom(0.0f)
    , mOutDataRandomNormal(0.0f, 0.0f, 0.0f)
    , mOutDataNoise(0.0f)
    , mfNoiseTim(0.0f)
    , mfSlowNoiseTim(0.0f)
    , mfFastNoiseTim(0.0f)
    , mfNoisePrv(0.0f)
    , mfSlowNoisePrv(0.0f)
    , mfFastNoisePrv(0.0f)
    , mfNoiseBas(0.0f)
    , mfSlowNoiseBas(0.0f)
    , mfFastNoiseBas(0.0f)
    , mfTimeBase(0.0f) {
}
void Global::OnStart() {
  mfTimeBase = ork::OldSchool::GetRef().GetLoResTime() * mPlugInpTimeScale.GetValue();
}
///////////////////////////////////////////////////////////////////////////////
void Global::Compute(float fdt) {
  float ftime        = mpParticleContext->CurrentTime() * mPlugInpTimeScale.GetValue();
  mOutDataTime       = ftime;
  mOutDataTimeDiv10  = ftime * 0.1f;
  mOutDataTimeDiv100 = ftime * 0.01f;
  mOutDataRandom     = float(std::rand() % 32767) / 32768.0f;
  mOutDataRelTime    = (ftime - mfTimeBase);
  /////////////////////////////////////////

  mOutDataRandomNormal.SetX(float(std::rand() % 32767) / 32768.0f);
  mOutDataRandomNormal.SetY(float(std::rand() % 32767) / 32768.0f);
  mOutDataRandomNormal.SetZ(float(std::rand() % 32767) / 32768.0f);

  mOutDataRandomNormal.Normalize();

  /////////////////////////////////////////
  // compute band limited noise
  /////////////////////////////////////////
  if (mfNoiseTim >= mfNoisePrv) {
    mfNoiseBas = mOutDataNoise;
    mfNoiseTim = float(std::rand() % 32767) / 32768.0f;
    mfNoiseNew = float(std::rand() % 32767) / 32768.0f;
    mfNoiseRat = (mfNoiseNew - mfNoiseBas) / mfNoiseTim;
    mfNoiseTim = 0.0f;
  }
  mOutDataNoise = mfNoiseBas + (mfNoiseRat * mfNoiseTim);
  if (mfNoiseNew > mfNoiseBas) {
    if (mOutDataNoise > mfNoiseNew)
      mOutDataNoise = mfNoiseNew;
  }
  if (mfNoiseNew < mfNoiseBas) {
    if (mOutDataNoise < mfNoiseNew)
      mOutDataNoise = mfNoiseNew;
  }
  mfNoiseTim += fdt;
  /////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
dataflow::outplugbase* Global::GetOutput(int idx) {
  dataflow::outplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &OutPlugName(RelTime);
      break;
    case 1:
      rval = &OutPlugName(Time);
      break;
    case 2:
      rval = &OutPlugName(TimeDiv10);
      break;
    case 3:
      rval = &OutPlugName(TimeDiv100);
      break;
    case 4:
      rval = &OutPlugName(Random);
      break;
    case 5:
      rval = &OutPlugName(RandomNormal);
      break;
    case 6:
      rval = &OutPlugName(Noise);
      break;
    case 7:
      rval = &OutPlugName(FastNoise);
      break;
    case 8:
      rval = &OutPlugName(SlowNoise);
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
Constants::Constants()
    : mbPlugsDirty(true) {
}
void Constants::Describe() {
  ork::reflect::RegisterMapProperty("Floats", &Constants::mFloatConsts);
  ork::reflect::RegisterMapProperty("Vect3s", &Constants::mVect3Consts);
  // ork::reflect::annotatePropertyForEditor< Constants >("FloatPlugs", "editor.factorylistbase", "dflow/outplug<float>" );
  // ork::reflect::annotatePropertyForEditor< Constants >("Vect3Plugs", "editor.factorylistbase", "dflow/outplug<vect3>" );
}

int Constants::GetNumOutputs() const {
  int iret      = 0;
  size_t inumfp = mFloatPlugs.size();
  size_t inumvp = mVect3Plugs.size();
  ////////////////////////////////////////////
  for (size_t i = 0; i < inumfp; i++) {
    const std::pair<PoolString, ork::dataflow::outplug<float>*>& pr = mFloatPlugs.GetItemAtIndex(i);
    ork::dataflow::outplug<float>* __restrict pplug                 = pr.second;
    if (pplug) {
      pplug->SetModule(const_cast<Constants*>(this));
      pplug->SetName(pr.first);
      pplug->SetRate(ork::dataflow::EPR_UNIFORM);
      iret++;
    }
  }
  ////////////////////////////////////////////
  for (size_t i = 0; i < inumvp; i++) {
    const std::pair<PoolString, ork::dataflow::outplug<fvec3>*>& pr = mVect3Plugs.GetItemAtIndex(i);
    ork::dataflow::outplug<fvec3>* __restrict pplug                 = pr.second;
    if (pplug) {
      pplug->SetModule(const_cast<Constants*>(this));
      pplug->SetName(pr.first);
      pplug->SetRate(ork::dataflow::EPR_UNIFORM);
      iret++;
    }
  }
  ////////////////////////////////////////////
  return iret;
}
dataflow::outplugbase* Constants::GetOutput(int idx) {
  int ipcounter = 0;
  size_t inumfp = mFloatPlugs.size();
  size_t inumvp = mVect3Plugs.size();
  ////////////////////////////////////////////
  for (size_t i = 0; i < inumfp; i++) {
    ork::dataflow::outplug<float>* __restrict pplug = mFloatPlugs.GetItemAtIndex(i).second;
    if (pplug) {
      if (ipcounter == idx) {
        return pplug;
      }
      ipcounter++;
    }
  }
  ////////////////////////////////////////////
  for (size_t i = 0; i < inumvp; i++) {
    ork::dataflow::outplug<fvec3>* __restrict pplug = mVect3Plugs.GetItemAtIndex(i).second;
    if (pplug) {
      if (ipcounter == idx) {
        return pplug;
      }
      ipcounter++;
    }
  }
  ////////////////////////////////////////////
  return 0;
}
void Constants::OnTopologyUpdate(void) {
  size_t inumfp = mFloatConsts.size();
  size_t inumvp = mVect3Consts.size();
  ////////////////////////////////////////////
  for (size_t i = 0; i < inumfp; i++) {
    const std::pair<PoolString, float>& pr = mFloatConsts.GetItemAtIndex(i);
    /////////////////////////////////////////////
    // get data
    PoolString Name  = pr.first;
    const float& val = pr.second;
    /////////////////////////////////////////////
    // find existing plug
    ork::orklut<ork::PoolString, ork::dataflow::outplug<float>*>::iterator it = mFloatPlugs.find(Name);
    /////////////////////////////////////////////
    // merge if does not exist
    if (it == mFloatPlugs.end()) {
      ork::dataflow::outplug<float>* pplug =
          new ork::dataflow::outplug<float>(this, ork::dataflow::EPR_UNIFORM, &val, pr.first.c_str());
      mFloatPlugs.insert(std::make_pair(pr.first, pplug));
      it = mFloatPlugs.find(pr.first);
    }
    it->second->ConnectData(&val);
  }
  ////////////////////////////////////////////
  for (size_t i = 0; i < inumvp; i++) {
    const std::pair<PoolString, fvec3>& pr = mVect3Consts.GetItemAtIndex(i);
    /////////////////////////////////////////////
    // get data
    PoolString Name  = pr.first;
    const fvec3& val = pr.second;
    /////////////////////////////////////////////
    // find existing plug
    ork::orklut<ork::PoolString, ork::dataflow::outplug<fvec3>*>::iterator it = mVect3Plugs.find(Name);
    /////////////////////////////////////////////
    // merge if does not exist
    if (it == mVect3Plugs.end()) {
      ork::dataflow::outplug<fvec3>* pplug =
          new ork::dataflow::outplug<fvec3>(this, ork::dataflow::EPR_UNIFORM, &val, pr.first.c_str());
      mVect3Plugs.insert(std::make_pair(pr.first, pplug));
      it = mVect3Plugs.find(pr.first);
    }
    it->second->ConnectData(&val);
  }
  ////////////////////////////////////////////
  // delete unused float plugs
  ////////////////////////////////////////////
  std::set<PoolString> FloatNames;
  for (ork::orklut<ork::PoolString, ork::dataflow::outplug<float>*>::const_iterator it = mFloatPlugs.begin();
       it != mFloatPlugs.end();
       it++) {
    FloatNames.insert(it->first);
  }
  while (FloatNames.size()) {
    std::set<PoolString>::iterator it                       = FloatNames.begin();
    ork::orklut<ork::PoolString, float>::const_iterator itf = mFloatConsts.find(*it);
    if (itf == mFloatConsts.end()) // erase it
    {
      ork::orklut<ork::PoolString, ork::dataflow::outplug<float>*>::iterator itE = mFloatPlugs.find(*it);
      ork::dataflow::outplug<float>* pplug                                       = itE->second;
      delete pplug;
      mFloatPlugs.erase(itE);
    }
    FloatNames.erase(FloatNames.begin());
  }
  ////////////////////////////////////////////
  // delete unused vec3 plugs
  ////////////////////////////////////////////
  std::set<PoolString> Vect3Names;
  for (ork::orklut<ork::PoolString, ork::dataflow::outplug<fvec3>*>::iterator it = mVect3Plugs.begin(); it != mVect3Plugs.end();
       it++) {
    Vect3Names.insert(it->first);
  }
  while (Vect3Names.size()) {
    std::set<PoolString>::iterator it                       = Vect3Names.begin();
    ork::orklut<ork::PoolString, fvec3>::const_iterator itf = mVect3Consts.find(*it);
    if (itf == mVect3Consts.end()) // erase it
    {
      ork::orklut<ork::PoolString, ork::dataflow::outplug<fvec3>*>::iterator itE = mVect3Plugs.find(*it);
      ork::dataflow::outplug<fvec3>* pplug                                       = itE->second;
      delete pplug;
      mVect3Plugs.erase(itE);
    }
    Vect3Names.erase(Vect3Names.begin());
  }
  ////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

bool Constants::DoNotify(const ork::event::Event* event) {
  // invalidate topology when editor modified map
  if (const ItemRemovalEvent* pev = rtti::autocast(event)) {
    const ork::reflect::ObjectProperty* prop = pev->mProperty;
    OnTopologyUpdate();
  } else if (const MapItemCreationEvent* pev = rtti::autocast(event)) {
    const ork::reflect::ObjectProperty* prop = pev->mProperty;
    OnTopologyUpdate();
  }
  return true;
}
bool Constants::PostDeserialize(reflect::IDeserializer&) {
  OnTopologyUpdate();
  return (true);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ExtConnector::ExtConnector()
//: mpExternalConnector( 0 )
{
}
void ExtConnector::Describe() {
  ork::reflect::RegisterMapProperty("FloatPlugs", &ExtConnector::mFloatPlugs);
  ork::reflect::RegisterMapProperty("Vect3Plugs", &ExtConnector::mVect3Plugs);
  ork::reflect::annotatePropertyForEditor<ExtConnector>("FloatPlugs", "editor.factorylistbase", "dflow/outplug<float>");
  ork::reflect::annotatePropertyForEditor<ExtConnector>("Vect3Plugs", "editor.factorylistbase", "dflow/outplug<vect3>");
}
int ExtConnector::GetNumOutputs() const {
  int iret      = 0;
  size_t inumfp = mFloatPlugs.size();
  size_t inumvp = mVect3Plugs.size();
  ////////////////////////////////////////////
  for (size_t i = 0; i < inumfp; i++) {
    const std::pair<PoolString, ork::dataflow::outplug<float>*>& pr = mFloatPlugs.GetItemAtIndex(i);
    ork::dataflow::outplug<float>* pplug                            = pr.second;
    if (pplug) {
      pplug->SetModule(const_cast<ExtConnector*>(this));
      pplug->SetName(pr.first);
      pplug->SetRate(ork::dataflow::EPR_UNIFORM);
      iret++;
    }
  }
  ////////////////////////////////////////////
  for (size_t i = 0; i < inumvp; i++) {
    const std::pair<PoolString, ork::dataflow::outplug<fvec3>*>& pr = mVect3Plugs.GetItemAtIndex(i);
    ork::dataflow::outplug<fvec3>* pplug                            = pr.second;
    if (pplug) {
      pplug->SetModule(const_cast<ExtConnector*>(this));
      pplug->SetName(pr.first);
      pplug->SetRate(ork::dataflow::EPR_UNIFORM);
      iret++;
    }
  }
  ////////////////////////////////////////////
  return iret;
}
dataflow::outplugbase* ExtConnector::GetOutput(int idx) {
  int ipcounter = 0;
  size_t inumfp = mFloatPlugs.size();
  size_t inumvp = mVect3Plugs.size();
  ////////////////////////////////////////////
  for (size_t i = 0; i < inumfp; i++) {
    ork::dataflow::outplug<float>* pplug = mFloatPlugs.GetItemAtIndex(i).second;
    if (pplug) {
      if (ipcounter == idx) {
        return pplug;
      }
      ipcounter++;
    }
  }
  ////////////////////////////////////////////
  for (size_t i = 0; i < inumvp; i++) {
    ork::dataflow::outplug<fvec3>* pplug = mVect3Plugs.GetItemAtIndex(i).second;
    if (pplug) {
      if (ipcounter == idx) {
        return pplug;
      }
      ipcounter++;
    }
  }
  ////////////////////////////////////////////
  return 0;
}
void ExtConnector::BindConnector(dataflow::dyn_external* pconnector) {
  if (pconnector) {
    size_t inumfp = mFloatPlugs.size();
    size_t inumvp = mVect3Plugs.size();
    for (size_t i = 0; i < inumfp; i++) {
      ork::dataflow::outplug<float>* pplug = mFloatPlugs.GetItemAtIndex(i).second;
      if (pplug) {
        const float* pfloat                                                            = 0;
        const orklut<PoolString, dataflow::dyn_external::FloatBinding>& float_bindings = pconnector->GetFloatBindings();
        orklut<PoolString, dataflow::dyn_external::FloatBinding>::const_iterator it    = float_bindings.find(pplug->GetName());
        if (it != float_bindings.end()) {
          pfloat = it->second.mpSource;
        }
        pplug->ConnectData(pfloat);
      }
    }
    for (size_t i = 0; i < inumvp; i++) {
      ork::dataflow::outplug<fvec3>* pplug = mVect3Plugs.GetItemAtIndex(i).second;
      if (pplug) {
        const fvec3* pvect3                                                            = 0;
        const orklut<PoolString, dataflow::dyn_external::Vect3Binding>& vect3_bindings = pconnector->GetVect3Bindings();
        orklut<PoolString, dataflow::dyn_external::Vect3Binding>::const_iterator it    = vect3_bindings.find(pplug->GetName());
        if (it != vect3_bindings.end()) {
          pvect3 = it->second.mpSource;
        }
        pplug->ConnectData(pvect3);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void FloatOp2Module::Describe() {
  RegisterFloatXfPlug(FloatOp2Module, InputA, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(FloatOp2Module, InputB, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  // static const char* EdGrpStr =
  //	        "grp://yp Input Gravity";
  // reflect::annotateClassForEditor<FloatOp2Module>( "editor.prop.groups", EdGrpStr );
}
///////////////////////////////////////////////////////////////////////////////
FloatOp2Module::FloatOp2Module()
    : ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(InputA, dataflow::EPR_UNIFORM, mfInputA)
    , ConstructInpPlug(InputB, dataflow::EPR_UNIFORM, mfInputB)
    , mfInputA(0.0f)
    , mfInputB(0.0f)
    , mOutDataOutput(0.0f)
    , meOp(EPSYS_FLOATOP_ADD) {
}
///////////////////////////////////////////////////////////////////////////////
void FloatOp2Module::Compute(float dt) {
  switch (meOp) {
    case EPSYS_FLOATOP_ADD:
      mOutDataOutput = mPlugInpInputA.GetValue() + mPlugInpInputB.GetValue();
      break;
    case EPSYS_FLOATOP_SUB:
      mOutDataOutput = mPlugInpInputA.GetValue() - mPlugInpInputB.GetValue();
      break;
    case EPSYS_FLOATOP_MUL:
      mOutDataOutput = mPlugInpInputA.GetValue() * mPlugInpInputB.GetValue();
      break;
  }
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* FloatOp2Module::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInputA;
      break;
    case 1:
      rval = &mPlugInpInputB;
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Vec3Op2Module::Describe() {
  RegisterVect3XfPlug(Vec3Op2Module, InputA, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterVect3XfPlug(Vec3Op2Module, InputB, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  // static const char* EdGrpStr =
  //	        "grp://yp Input Gravity";
  // reflect::annotateClassForEditor<FloatOp2Module>( "editor.prop.groups", EdGrpStr );
}
///////////////////////////////////////////////////////////////////////////////
Vec3Op2Module::Vec3Op2Module()
    : ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(InputA, dataflow::EPR_UNIFORM, mvInputA)
    , ConstructInpPlug(InputB, dataflow::EPR_UNIFORM, mvInputB)
    , mvInputA(0.0f, 0.0f, 0.0f)
    , mvInputB(0.0f, 0.0f, 0.0f)
    , mOutDataOutput(0.0f, 0.0f, 0.0f)
    , meOp(EPSYS_VEC3OP_ADD) {
}
///////////////////////////////////////////////////////////////////////////////
void Vec3Op2Module::Compute(float dt) {
  switch (meOp) {
    case EPSYS_VEC3OP_ADD:
      mOutDataOutput = mPlugInpInputA.GetValue() + mPlugInpInputB.GetValue();
      break;
    case EPSYS_VEC3OP_SUB:
      mOutDataOutput = mPlugInpInputA.GetValue() - mPlugInpInputB.GetValue();
      break;
    case EPSYS_VEC3OP_MUL:
      mOutDataOutput = mPlugInpInputA.GetValue() * mPlugInpInputB.GetValue();
      break;
    case EPSYS_VEC3OP_DOT: { // fvec3 a = mPlugInpInputA.GetValue();
                             // fvec3 b = mPlugInpInputA.GetValue();
                             // mOutDataOutput = .Dot(mPlugInpInputB.GetValue());
    } break;
    case EPSYS_VEC3OP_CROSS:
      mOutDataOutput = mPlugInpInputA.GetValue().Cross(mPlugInpInputB.GetValue());
      break;
  }
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* Vec3Op2Module::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInputA;
      break;
    case 1:
      rval = &mPlugInpInputB;
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void Vec3SplitModule::Describe() {
  RegisterVect3XfPlug(Vec3SplitModule, Input, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
}
///////////////////////////////////////////////////////////////////////////////
Vec3SplitModule::Vec3SplitModule()
    : ConstructOutPlug(OutputX, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(OutputY, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(OutputZ, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(Input, dataflow::EPR_UNIFORM, mvInput)
    , mvInput(0.0f, 0.0f, 0.0f)
    , mOutDataOutputX(0.0f)
    , mOutDataOutputY(0.0f)
    , mOutDataOutputZ(0.0f) {
}
///////////////////////////////////////////////////////////////////////////////
void Vec3SplitModule::Compute(float dt) {
  fvec3 inp       = mPlugInpInput.GetValue();
  mOutDataOutputX = inp.GetX();
  mOutDataOutputY = inp.GetY();
  mOutDataOutputZ = inp.GetZ();
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* Vec3SplitModule::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ParticlePool::Describe() {
  RegisterObjOutPlug(ParticlePool, Output);
  RegisterFloatXfPlug(ParticlePool, PathInterval, 0.0f, 10.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(ParticlePool, PathProbability, 0.0f, 1.0f, ged::OutPlugChoiceDelegate);
  ork::reflect::RegisterProperty("MaxParticles", &ParticlePool::miPoolSize);
  ork::reflect::annotatePropertyForEditor<ParticlePool>("MaxParticles", "editor.range.min", "1");
  ork::reflect::annotatePropertyForEditor<ParticlePool>("MaxParticles", "editor.range.max", "20000");
  ork::reflect::RegisterProperty("PathStochasticQID", &ParticlePool::mPathStochasticQueueID);
  ork::reflect::RegisterProperty("PathIntervalQID", &ParticlePool::mPathIntervalQueueID);
}
///////////////////////////////////////////////////////////////////////////////
ParticlePool::ParticlePool()
    : ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(UnitAge, dataflow::EPR_VARYING1)
    , ConstructInpPlug(PathInterval, dataflow::EPR_UNIFORM, mfPathInterval)
    , ConstructInpPlug(PathProbability, dataflow::EPR_VARYING1, mfPathProbability)
    , miPoolSize(40)
    , mPathIntervalEventQueue(0)
    , mPathStochasticEventQueue(0)
    , mfPathInterval(0.0f)
    , mfPathProbability(0.0f)
    , mOutDataUnitAge(0.0f) {
  mOutDataOutput.mPool = &mPoolOutput;
}
///////////////////////////////////////////////////////////////////////////////
ork::dataflow::outplugbase* ParticlePool::GetOutput(int idx) {
  ork::dataflow::outplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &OutPlugName(Output);
      break;
    case 1:
      rval = &OutPlugName(UnitAge);
      break;
  }
  return rval;
}
dataflow::inplugbase* ParticlePool::GetInput(int idx) {
  ork::dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &InpPlugName(PathInterval);
      break;
    case 1:
      rval = &InpPlugName(PathProbability);
      break;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
void ParticlePool::Compute(float fdt) {
  if (mPoolOutput.GetMax() != miPoolSize) {
    mPoolOutput.Init(miPoolSize);
  }
  int inumalive = mPoolOutput.GetNumAlive();
  for (int i = 0; i < inumalive; i++) {
    BasicParticle* ptc = mPoolOutput.mActiveParticles[i];
    /////////////////////////////////
    float fage      = ptc->mfAge;
    float unit_age  = (fage / ptc->mfLifeSpan);
    mOutDataUnitAge = std::clamp(unit_age, 0.001f, 0.999f);

    // printf( "ptcl<%d> age<%f> ls<%f> IsDead<%d> unitage<%f>\n", i, fage, ptc->mfLifeSpan, int(ptc->IsDead()), mOutDataUnitAge );
    /////////////////////////////////
    int ia1 = int(ptc->mfAge / mfPathInterval);
    int ia2 = int((ptc->mfAge + fdt) / mfPathInterval);
    if ((mPathIntervalEventQueue != 0) && (ia2 > ia1)) {
      Event PathEv;
      PathEv.mEventType    = Char4("PATH");
      PathEv.mPosition     = ptc->mPosition;
      PathEv.mLastPosition = ptc->mLastPosition;
      PathEv.mVelocity     = ptc->mVelocity;
      mPathIntervalEventQueue->QueueEvent(PathEv);
    }
    /////////////////////////////////
    if (mPathStochasticEventQueue != 0) {
      int irand   = rand() % 1000;
      float frand = float(irand) * 0.001f;
      float fprob = mPlugInpPathProbability.GetValue();
      if (frand < fprob) {
        Event PathEv;
        PathEv.mEventType    = Char4("PATH");
        PathEv.mPosition     = ptc->mPosition;
        PathEv.mLastPosition = ptc->mLastPosition;
        PathEv.mVelocity     = ptc->mVelocity;
        mPathStochasticEventQueue->QueueEvent(PathEv);
      }
    }
    /////////////////////////////////
    ptc->mfAge += fdt;
    ptc->mLastPosition = ptc->mPosition;
    ptc->mPosition += ptc->mVelocity * fdt;
  }
}
void ParticlePool::Reset() {
  mPoolOutput.Reset();
}
void ParticlePool::DoLink() {
  mPathIntervalQueueID4 = particle::PoolString2Char4(mPathIntervalQueueID);
  if (mPathIntervalQueueID4.GetU32())
    mPathIntervalEventQueue = mpParticleContext->MergeQueue(mPathIntervalQueueID4);
  mPathStochasticQueueID4 = particle::PoolString2Char4(mPathStochasticQueueID);
  if (mPathStochasticQueueID4.GetU32())
    mPathStochasticEventQueue = mpParticleContext->MergeQueue(mPathStochasticQueueID4);
  //////	printf( "RingEmitter<%p>::DoLink() KillQ<%p>\n", this, mDeathEventQueue );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RingEmitter::Describe() {
  RegisterObjInpPlug(RingEmitter, Input);
  RegisterObjOutPlug(RingEmitter, Output);
  RegisterFloatXfPlug(RingEmitter, Lifespan, 0.0f, 20.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(RingEmitter, EmissionRadius, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(RingEmitter, EmissionRate, 0.0f, 800.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(RingEmitter, EmissionVelocity, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(RingEmitter, EmitterSpinRate, -10.0f, 10.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(RingEmitter, DispersionAngle, 0.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(RingEmitter, OffsetX, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(RingEmitter, OffsetY, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(RingEmitter, OffsetZ, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterVect3XfPlug(RingEmitter, Direction, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  ork::reflect::RegisterProperty("DeathQID", &RingEmitter::mDeathQueueID);

  ork::reflect::RegisterProperty("DirectionType", &RingEmitter::meDirection);
  ork::reflect::annotatePropertyForEditor<RingEmitter>("DirectionType", "editor.class", "ged.factory.enum");
  static const char* EdGrpStr = "sort://Input "
                                "Direction DirectionType DispersionAngle Lifespan EmitterSpinRate "
                                "EmissionRadius EmissionRate EmissionVelocity "
                                "grp://Offset OffsetX OffsetY OffsetZ "
                                "grp://Event DeathQID PathIntervalQID PathStochasticQID";
  reflect::annotateClassForEditor<RingEmitter>("editor.prop.groups", EdGrpStr);
}
///////////////////////////////////////////////////////////////////////////////
RingEmitter::RingEmitter()
    : mOutDataOutput()
    , ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(Lifespan, dataflow::EPR_UNIFORM, mfLifespan)
    , ConstructInpPlug(EmissionRadius, dataflow::EPR_UNIFORM, mfEmissionRadius)
    , ConstructInpPlug(EmissionRate, dataflow::EPR_UNIFORM, mfEmissionRate)
    , ConstructInpPlug(EmissionVelocity, dataflow::EPR_UNIFORM, mfEmissionVelocity)
    , ConstructInpPlug(EmitterSpinRate, dataflow::EPR_UNIFORM, mfEmitterSpinRate)
    , ConstructInpPlug(DispersionAngle, dataflow::EPR_UNIFORM, mfDispersionAngle)
    , ConstructInpPlug(OffsetX, dataflow::EPR_UNIFORM, mfOffsetX)
    , ConstructInpPlug(OffsetY, dataflow::EPR_UNIFORM, mfOffsetY)
    , ConstructInpPlug(OffsetZ, dataflow::EPR_UNIFORM, mfOffsetZ)
    , ConstructInpPlug(Direction, dataflow::EPR_UNIFORM, mvDirection)
    , mfPhase(0.0f)
    , mfPhase2(0.0f)
    , mfDispersionAngle(0.0f)
    , mfEmissionRadius(0.0f)
    , mfEmissionRate(0.0f)
    , mfEmitterSpinRate(0.0f)
    , mfEmissionVelocity(0.0f)
    , mfAccumTime(0.0f)
    , meDirection(EMITDIR_VEL)
    , mDirectedEmitter(*this)
    , mfOffsetX(0.0f)
    , mfOffsetY(0.0f)
    , mfOffsetZ(0.0f)
    , mvDirection(0.0f, 0.0f, 0.0f)
    , mDeathEventQueue(0) {
}
void RingEmitter::Reset() {
  mfPhase  = 0.0f;
  mfPhase2 = 0.0f;
}
void RingEmitter::DoLink() {
  mDeathQueueID4 = particle::PoolString2Char4(mDeathQueueID);
  if (mDeathQueueID4.GetU32())
    mDeathEventQueue = mpParticleContext->MergeQueue(mDeathQueueID4);
  //////	printf( "RingEmitter<%p>::DoLink() KillQ<%p>\n", this, mDeathEventQueue );
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* RingEmitter::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &InpPlugName(Input);
      break;
    case 1:
      rval = &InpPlugName(Lifespan);
      break;
    case 2:
      rval = &InpPlugName(EmissionRadius);
      break;
    case 3:
      rval = &InpPlugName(EmissionRate);
      break;
    case 4:
      rval = &InpPlugName(EmissionVelocity);
      break;
    case 5:
      rval = &InpPlugName(EmitterSpinRate);
      break;
    case 6:
      rval = &InpPlugName(DispersionAngle);
      break;
    case 7:
      rval = &InpPlugName(OffsetX);
      break;
    case 8:
      rval = &InpPlugName(OffsetY);
      break;
    case 9:
      rval = &InpPlugName(OffsetZ);
      break;
    case 10:
      rval = &InpPlugName(Direction);
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
dataflow::outplugbase* RingEmitter::GetOutput(int idx) {
  dataflow::outplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &OutPlugName(Output);
      break;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
void RingDirectedEmitter::ComputePosDir(float fi, fvec3& pos, fvec3& dir) {
  float scaler = (fi * mEmitterModule.mfThisRadius) + ((1.0f - fi) * mEmitterModule.mfLastRadius);
  float phase  = (fi * mEmitterModule.mfPhase2) + ((1.0f - fi) * mEmitterModule.mfPhase);
  float fpx    = cosf(phase);
  float fpz    = sinf(phase);
  float fdx    = cosf(phase + PI_DIV_2);
  float fdz    = sinf(phase + PI_DIV_2);
  pos          = fvec3((fpx * scaler), 0.0f, (fpz * scaler));
  if (meDirection == EMITDIR_USER) {
    dir = mUserDir;
  } else {
    dir = fvec3(fdx, 0.0f, fdz);
  }
}
///////////////////////////////////////////////////////////////////////////////
void RingEmitter::Emit(float fdt) {
  const psys_ptclbuf& pb    = InpPlugName(Input).GetValue();
  Pool<BasicParticle>& pool = *pb.mPool;
  float scaler              = mPlugInpEmissionRadius.GetValue();
  float femitvel            = mPlugInpEmissionVelocity.GetValue();
  float lifespan            = std::clamp(mPlugInpLifespan.GetValue(), 0.01f, 10.0f);
  float emissionrate        = mPlugInpEmissionRate.GetValue();
  float fspr                = mPlugInpEmitterSpinRate.GetValue() * PI2;
  float offx                = mPlugInpOffsetX.GetValue();
  float offy                = mPlugInpOffsetY.GetValue();
  float offz                = mPlugInpOffsetZ.GetValue();
  float fadaptive           = ((mfPhase2 - mfPhase) / fdt);
  if (fadaptive == 0.0f)
    fadaptive = 1.0f;
  fadaptive                      = std::clamp(fadaptive, 0.1f, 1.0f);
  mEmitterCtx.mPool              = pb.mPool;
  mEmitterCtx.mfEmissionRate     = emissionrate * fadaptive;
  mEmitterCtx.mKey               = (void*)this;
  mEmitterCtx.mfLifespan         = lifespan;
  mEmitterCtx.mfDeltaTime        = fdt;
  mEmitterCtx.mfEmissionVelocity = femitvel;
  mEmitterCtx.mDispersion        = mPlugInpDispersionAngle.GetValue();
  mDirectedEmitter.meDirection   = meDirection;
  mDirectedEmitter.mUserDir      = mPlugInpDirection.GetValue();
  mEmitterCtx.mPosition          = fvec3(offx, offy, offz);
  mDirectedEmitter.Emit(mEmitterCtx);
  float fphaseINC = fspr * fdt;
  mfPhase         = fmodf(mfPhase + fphaseINC, PI2 * 1000.0f);
  mfPhase2        = fmodf(mfPhase + fphaseINC, PI2 * 1000.0f);
  mfLastRadius    = mfThisRadius;
  mfThisRadius    = mPlugInpEmissionRadius.GetValue();
}
///////////////////////////////////////////////////////////////////////////////
void RingEmitter::Reap(float fdt) {
  const psys_ptclbuf& pb  = InpPlugName(Input).GetValue();
  mEmitterCtx.mPool       = pb.mPool;
  mEmitterCtx.mfDeltaTime = fdt;
  mEmitterCtx.mKey        = (void*)this;
  ///////////////////////////////////////
  mEmitterCtx.mDeathQueue = mDeathEventQueue;
  { mDirectedEmitter.Reap(mEmitterCtx); }
  mEmitterCtx.mDeathQueue = 0;
  ///////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void RingEmitter::Compute(float fdt) {
  const psys_graph* pgraf = rtti::autocast(this->GetParent());
  bool bemit              = pgraf->GetEmitEnable();
  const psys_ptclbuf& pb  = InpPlugName(Input).GetValue();
  mfAccumTime += fdt;
  const float fstep = fdt; // 1.0f / 30.0f;
  // printf( "re::compute fdt<%f> acc<%f>\n", fdt, mfAccumTime );
  if (mfAccumTime >= fstep) // limit to 30hz
  {
    float fdelta = fstep;
    mfAccumTime -= fstep;
    if (pb.mPool) {
      Reap(fdelta);
      if (bemit) {
        Emit(fdelta);
      }
    }
  }
  if (mfAccumTime < 0.01f) {
    mfAccumTime = 0.0f;
  }
  mOutDataOutput.mPool = pb.mPool;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void NozzleEmitter::Describe() {
  RegisterObjInpPlug(NozzleEmitter, Input);
  RegisterObjOutPlug(NozzleEmitter, Output);
  RegisterObjOutPlug(NozzleEmitter, TheDead);
  RegisterFloatXfPlug(NozzleEmitter, Lifespan, 0.0f, 20.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(NozzleEmitter, EmissionRate, 0.0f, 400.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(NozzleEmitter, EmissionVelocity, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(NozzleEmitter, DispersionAngle, 0.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterVect3XfPlug(NozzleEmitter, Offset, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterVect3XfPlug(NozzleEmitter, Direction, -1.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterVect3XfPlug(NozzleEmitter, OffsetVelocity, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  static const char* EdGrpStr = "sort://Input "
                                "DispersionAngle Lifespan "
                                "EmissionRate EmissionVelocity "
                                "Offset Direction  OffsetVelocity";
  reflect::annotateClassForEditor<NozzleEmitter>("editor.prop.groups", EdGrpStr);
}
///////////////////////////////////////////////////////////////////////////////
NozzleEmitter::NozzleEmitter()
    : mOutDataOutput()
    , mOutDataTheDead(&mDeadPool)
    , ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructOutPlug(TheDead, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(Lifespan, dataflow::EPR_UNIFORM, mfLifespan)
    , ConstructInpPlug(EmissionRate, dataflow::EPR_UNIFORM, mfEmissionRate)
    , ConstructInpPlug(EmissionVelocity, dataflow::EPR_UNIFORM, mfEmissionVelocity)
    , ConstructInpPlug(DispersionAngle, dataflow::EPR_UNIFORM, mfDispersionAngle)
    , ConstructInpPlug(Offset, dataflow::EPR_UNIFORM, mvOffset)
    , ConstructInpPlug(Direction, dataflow::EPR_UNIFORM, mvDirection)
    , ConstructInpPlug(OffsetVelocity, dataflow::EPR_UNIFORM, mvOffsetVelocity)
    , mfDispersionAngle(0.0f)
    , mfEmissionRate(0.0f)
    , mfEmissionVelocity(0.0f)
    , mfAccumTime(0.0f)
    , mDirectedEmitter(*this)
    , mvOffset(0.0f, 0.0f, 0.0f)
    , mvDirection(0.0f, 0.0f, 0.0f) {
}
void NozzleEmitter::Reset() {
  mLastPosition  = fvec3(0.0f, 0.0f, 0.0f);
  mLastDirection = fvec3(0.0f, 1.0f, 0.0f);
  mfAccumTime    = 0.0f;
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* NozzleEmitter::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &InpPlugName(Input);
      break;
    case 1:
      rval = &InpPlugName(Lifespan);
      break;
    case 2:
      rval = &InpPlugName(EmissionRate);
      break;
    case 3:
      rval = &InpPlugName(EmissionVelocity);
      break;
    case 4:
      rval = &InpPlugName(DispersionAngle);
      break;
    case 5:
      rval = &InpPlugName(Offset);
      break;
    case 6:
      rval = &InpPlugName(Direction);
      break;
    case 7:
      rval = &InpPlugName(OffsetVelocity);
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
dataflow::outplugbase* NozzleEmitter::GetOutput(int idx) {
  dataflow::outplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &OutPlugName(Output);
      break;
    case 1:
      rval = &OutPlugName(TheDead);
      break;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
void NozzleDirectedEmitter::ComputePosDir(float fi, fvec3& pos, fvec3& dir) {
  // pos = mEmitterModule.mvOffset;
  pos.Lerp(mEmitterModule.mLastPosition, mEmitterModule.mOffsetSample, fi);
  dir.Lerp(mEmitterModule.mLastDirection, mEmitterModule.mDirectionSample, fi);
}
///////////////////////////////////////////////////////////////////////////////
void NozzleEmitter::Emit(float fdt) {
  const psys_ptclbuf& pb         = InpPlugName(Input).GetValue();
  Pool<BasicParticle>& pool      = *pb.mPool;
  float femitvel                 = mPlugInpEmissionVelocity.GetValue();
  float lifespan                 = std::clamp(mPlugInpLifespan.GetValue(), 0.01f, 10.0f);
  float emissionrate             = mPlugInpEmissionRate.GetValue();
  mEmitterCtx.mPool              = pb.mPool;
  mEmitterCtx.mfEmissionRate     = emissionrate;
  mEmitterCtx.mKey               = (void*)this;
  mEmitterCtx.mfLifespan         = lifespan;
  mEmitterCtx.mfDeltaTime        = fdt;
  mEmitterCtx.mfEmissionVelocity = femitvel;
  mEmitterCtx.mDispersion        = mPlugInpDispersionAngle.GetValue();
  mDirectedEmitter.meDirection   = EMITDIR_CONSTANT;
  fvec3 dir                      = mPlugInpDirection.GetValue();
  mEmitterCtx.mPosition          = fvec3(0.0f, 0.0f, 0.0f); // mPlugInpOffset.GetValue();
  fvec3 offsetVel                = mPlugInpOffsetVelocity.GetValue();
  mEmitterCtx.mOffsetVelocity    = offsetVel;
  mDirectedEmitter.Emit(mEmitterCtx);
}
///////////////////////////////////////////////////////////////////////////////
void NozzleEmitter::Reap(float fdt) {
  const psys_ptclbuf& pb  = InpPlugName(Input).GetValue();
  mEmitterCtx.mPool       = pb.mPool;
  mEmitterCtx.mfDeltaTime = fdt;
  mEmitterCtx.mKey        = (void*)this;
  mDirectedEmitter.Reap(mEmitterCtx);
}
///////////////////////////////////////////////////////////////////////////////
void NozzleEmitter::Compute(float fdt) {
  const psys_graph* pgraf = rtti::autocast(this->GetParent());
  bool bemit              = pgraf->GetEmitEnable();
  const psys_ptclbuf& pb  = InpPlugName(Input).GetValue();
  mfAccumTime += fdt;

  mLastPosition  = mOffsetSample;
  mLastDirection = mDirectionSample;

  mDirectionSample = mPlugInpDirection.GetValue();
  mOffsetSample    = mPlugInpOffset.GetValue();

  if (mDirectionSample.MagSquared() == 0.0f) {
    mDirectionSample = fvec3::Green();
  }

  if (mfAccumTime >= 0.03333f) // limit to 30hz
  {
    float fdelta = 0.03333f;
    mfAccumTime -= 0.03333f;
    if (pb.mPool) {
      Reap(fdelta);
      if (bemit) {
        Emit(fdelta);
      }
    }
  }

  mOutDataOutput.mPool = pb.mPool;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ReDirectedEmitter::ComputePosDir(float fi, fvec3& pos, fvec3& dir) {
  // pos.Lerp( mEmitterModule.mEmitterCtx.mPosition, mEmitterModule.mEmitterCtx.mLastPosition, fi );
  // dir = (mEmitterModule.mEmitterCtx.mPosition-mEmitterModule.mEmitterCtx.mLastPosition).Normal();
}
///////////////////////////////////////////////////////////////////////////////
void ReEmitter::Describe() {
  RegisterObjInpPlug(ReEmitter, Input);

  RegisterObjOutPlug(ReEmitter, Output);

  RegisterFloatXfPlug(ReEmitter, SpawnProbability, 0.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(ReEmitter, SpawnMultiplier, 1.0f, 30.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(ReEmitter, Lifespan, 0.0f, 20.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(ReEmitter, EmissionRate, 0.0f, 400.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(ReEmitter, EmissionVelocity, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(ReEmitter, DispersionAngle, 0.0f, 1.0f, ged::OutPlugChoiceDelegate);

  ork::reflect::RegisterProperty("DirectionType", &ReEmitter::meDirection);
  ork::reflect::RegisterProperty("SpawnQID", &ReEmitter::mSpawnQueueID);
  ork::reflect::RegisterProperty("DeathQID", &ReEmitter::mDeathQueueID);
  ork::reflect::annotatePropertyForEditor<ReEmitter>("DirectionType", "editor.class", "ged.factory.enum");

  static const char* EdGrpStr = "grp://Input "
                                "Input DirectionType DispersionAngle Lifespan "
                                "EmissionRate EmissionVelocity "
                                "grp://Event SpawnProbability SpawnMultiplier SpawnQID DeathQID ";
  reflect::annotateClassForEditor<ReEmitter>("editor.prop.groups", EdGrpStr);
}
///////////////////////////////////////////////////////////////////////////////
ReEmitter::ReEmitter()
    : mOutDataOutput()
    , ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(SpawnProbability, dataflow::EPR_UNIFORM, mfSpawnProbability)
    , ConstructInpPlug(SpawnMultiplier, dataflow::EPR_UNIFORM, mfSpawnMultiplier)
    , ConstructInpPlug(Lifespan, dataflow::EPR_UNIFORM, mfLifespan)
    , ConstructInpPlug(EmissionRate, dataflow::EPR_UNIFORM, mfEmissionRate)
    , ConstructInpPlug(EmissionVelocity, dataflow::EPR_UNIFORM, mfEmissionVelocity)
    , ConstructInpPlug(DispersionAngle, dataflow::EPR_UNIFORM, mfDispersionAngle)
    , mfLifespan(0.0f)
    , mfEmissionRate(0.0f)
    , mfEmissionVelocity(0.0f)
    , mfDispersionAngle(0.0f)
    , meDirection(EMITDIR_VEL)
    , mDirectedEmitter(*this)
    , mSpawnEventQueue(0)
    , mDeathEventQueue(0)
    , mfSpawnProbability(1.0f)
    , mfSpawnMultiplier(1.0f) {
}
///////////////////////////////////////////////////////////////////////////////
void ReEmitter::DoLink() {
  mSpawnQueueID4 = particle::PoolString2Char4(mSpawnQueueID);
  if (mSpawnQueueID4.GetU32())
    mSpawnEventQueue = mpParticleContext->MergeQueue(mSpawnQueueID4);
  mDeathQueueID4 = particle::PoolString2Char4(mDeathQueueID);
  if (mDeathQueueID4.GetU32())
    mDeathEventQueue = mpParticleContext->MergeQueue(mDeathQueueID4);

  // printf( "ReEmitter<%p>::DoLink() SpawnQ<%s:%p>\n", this, mSpawnQueueID4.c_str(), mSpawnEventQueue );
  // printf( "ReEmitter<%p>::DoLink() DeathQ<%s:%p>\n", this, mDeathQueueID4.c_str(), mDeathEventQueue );
}
///////////////////////////////////////////////////////////////////////////////
void ReEmitter::Compute(float dt) {
  // printf( "ReEmitter<%p>::Compute() SpawnQ<%p>\n", this, mSpawnEventQueue );

  const psys_ptclbuf& pb = mPlugInpInput.GetValue();
  if (pb.mPool) {
    Reap(dt);
    Emit(dt);
  }
  mOutDataOutput.mPool = pb.mPool;
}
///////////////////////////////////////////////////////////////////////////////
void ReEmitter::Emit(float fdt) {
  const psys_ptclbuf& pbp = mPlugInpInput.GetValue();
  // const psys_ptclbuf& pbr = mPlugInpInputRef.GetValue();
  if (pbp.mPool) // && pbr.mPool )
  {
    float emissionrate             = mPlugInpEmissionRate.GetValue();
    mEmitterCtx.mPool              = pbp.mPool;
    mEmitterCtx.mfDeltaTime        = fdt;
    mEmitterCtx.mfLifespan         = mPlugInpLifespan.GetValue();
    mEmitterCtx.mKey               = (void*)this;
    mEmitterCtx.mfEmissionRate     = emissionrate;
    mEmitterCtx.mDispersion        = mPlugInpDispersionAngle.GetValue();
    mEmitterCtx.mfEmissionVelocity = mPlugInpEmissionVelocity.GetValue();
    mEmitterCtx.mfSpawnProbability = mPlugInpSpawnProbability.GetValue();
    mEmitterCtx.mfSpawnMultiplier  = mPlugInpSpawnMultiplier.GetValue();
    mEmitterCtx.mSpawnQueue        = mSpawnEventQueue;
    {
      // printf( "mEmitterCtx.mSpawnQueue<%p>\n", mEmitterCtx.mSpawnQueue );
      mDirectedEmitter.meDirection = meDirection;
      mDirectedEmitter.Emit(mEmitterCtx);
    }
    mEmitterCtx.mSpawnQueue = 0;
    // mEmitterCtx.mSpawnQueue = 0;
  }
}
///////////////////////////////////////////////////////////////////////////////
void ReEmitter::Reap(float fdt) {
  const psys_ptclbuf& pb  = mPlugInpInput.GetValue();
  mEmitterCtx.mPool       = pb.mPool;
  mEmitterCtx.mfDeltaTime = fdt;
  mEmitterCtx.mKey        = (void*)this;
  mEmitterCtx.mDeathQueue = mDeathEventQueue;
  { mDirectedEmitter.Reap(mEmitterCtx); }
  mEmitterCtx.mDeathQueue = 0;
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* ReEmitter::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
    case 1:
      rval = &mPlugInpLifespan;
      break;
    case 2:
      rval = &mPlugInpEmissionRate;
      break;
    case 3:
      rval = &mPlugInpEmissionVelocity;
      break;
    case 4:
      rval = &mPlugInpDispersionAngle;
      break;
    case 5:
      rval = &mPlugInpSpawnProbability;
      break;
    case 6:
      rval = &mPlugInpSpawnMultiplier;
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
dataflow::outplugbase* ReEmitter::GetOutput(int idx) {
  dataflow::outplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &OutPlugName(Output);
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WindModule::Describe() {
  RegisterObjInpPlug(WindModule, Input);
  RegisterObjOutPlug(WindModule, Output);
  RegisterFloatXfPlug(WindModule, Force, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  static const char* EdGrpStr = "grp://yp Input Force";
  reflect::annotateClassForEditor<WindModule>("editor.prop.groups", EdGrpStr);
}
///////////////////////////////////////////////////////////////////////////////
WindModule::WindModule()
    : ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(Force, dataflow::EPR_UNIFORM, mfForce)
    , mfForce(0.0f) {
}
///////////////////////////////////////////////////////////////////////////////
void WindModule::Compute(float dt) {
  const psys_ptclbuf& pb = mPlugInpInput.GetValue();
  if (pb.mPool) {
    ork::fvec4 accel(0.0f, mPlugInpForce.GetValue(), 0.0f);
    for (int i = 0; i < pb.mPool->GetNumAlive(); i++) {
      BasicParticle* particle = pb.mPool->GetActiveParticle(i);
      particle->mVelocity += accel * dt;
    }
  }
  mOutDataOutput.mPool = pb.mPool;
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* WindModule::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
    case 1:
      rval = &mPlugInpForce;
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GravityModule::Describe() {
  RegisterObjInpPlug(GravityModule, Input);
  RegisterObjOutPlug(GravityModule, Output);
  RegisterFloatXfPlug(GravityModule, G, -10.0f, 10.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(GravityModule, Mass, -10.0f, 10.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(GravityModule, OthMass, -10.0f, 10.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(GravityModule, MinDistance, 0.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterVect3XfPlug(GravityModule, Center, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);

  static const char* EdGrpStr = "grp://yp Input G Mass OthMass MinDistance Center";
  reflect::annotateClassForEditor<GravityModule>("editor.prop.groups", EdGrpStr);
}
///////////////////////////////////////////////////////////////////////////////
GravityModule::GravityModule()
    : ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(G, dataflow::EPR_UNIFORM, mfG)
    , ConstructInpPlug(Mass, dataflow::EPR_UNIFORM, mfMass)
    , ConstructInpPlug(OthMass, dataflow::EPR_UNIFORM, mfOthMass)
    , ConstructInpPlug(MinDistance, dataflow::EPR_UNIFORM, mfMinDistance)
    , ConstructInpPlug(Center, dataflow::EPR_UNIFORM, mvCenter)
    , mvCenter(0.0f, 0.0f, 0.0f)
    , mfMass(0.0f)
    , mfMinDistance(0.0f)
    , mfOthMass(0.0f)
    , mfG(0.0f) {

  printf("&G<%p>\n", &mfG);
  // raise(SIGINT);
}
///////////////////////////////////////////////////////////////////////////////
void GravityModule::Compute(float dt) {
  const psys_ptclbuf& pb = mPlugInpInput.GetValue();
  if (pb.mPool) {
    float fmass    = powf(10.0f, mPlugInpMass.GetValue());
    float fothmass = powf(10.0f, mPlugInpOthMass.GetValue());
    float fG       = powf(10.0f, mPlugInpG.GetValue());
    float finvmass = (fothmass == 0.0f) ? 0.0f : (1.0f / fothmass);
    float numer    = (fmass * fothmass * fG);
    float mindist  = mPlugInpMinDistance.GetValue();
    for (int i = 0; i < pb.mPool->GetNumAlive(); i++) {
      BasicParticle* particle = pb.mPool->GetActiveParticle(i);
      const fvec3& OldPos     = particle->mPosition;
      fvec3 Dir               = (mvCenter - OldPos);
      float Mag               = Dir.Mag();
      if (Mag < mindist)
        Mag = mindist;
      Dir            = Dir * (1.0f / Mag);
      float denom    = Mag * Mag;
      fvec3 forceVec = Dir * (numer / denom);
      fvec3 accel    = forceVec * finvmass;
      // printf( "numer<%f> denim<%f> accel<%f %f %f>\n", numer, denom, accel.GetX(), accel.GetY(), accel.GetZ() );
      particle->mVelocity += accel * dt;
    }
  }
  mOutDataOutput.mPool = pb.mPool;
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* GravityModule::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
    case 1:
      rval = &mPlugInpG;
      break;
    case 2:
      rval = &mPlugInpMass;
      break;
    case 3:
      rval = &mPlugInpOthMass;
      break;
    case 4:
      rval = &mPlugInpCenter;
      break;
    case 5:
      rval = &mPlugInpMinDistance;
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void PlanarColliderModule::Describe() {
  RegisterObjInpPlug(PlanarColliderModule, Input);
  RegisterObjOutPlug(PlanarColliderModule, Output);
  RegisterFloatXfPlug(PlanarColliderModule, NormalX, -1.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(PlanarColliderModule, NormalY, -1.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(PlanarColliderModule, NormalZ, -1.0f, 1.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(PlanarColliderModule, OriginX, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(PlanarColliderModule, OriginY, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(PlanarColliderModule, OriginZ, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(PlanarColliderModule, Absorbtion, 0.0f, 1.0f, ged::OutPlugChoiceDelegate);

  static const char* EdGrpStr = "grp://Main Input DiodeDirection Absorbtion "
                                "grp://Normal NormalX NormalY NormalZ "
                                "grp://Origin OriginX OriginY OriginZ ";

  reflect::annotateClassForEditor<PlanarColliderModule>("editor.prop.groups", EdGrpStr);
  ork::reflect::RegisterProperty("DiodeDirection", &PlanarColliderModule::miDiodeDirection);
  ork::reflect::annotatePropertyForEditor<PlanarColliderModule>("DiodeDirection", "editor.range.min", "-1");
  ork::reflect::annotatePropertyForEditor<PlanarColliderModule>("DiodeDirection", "editor.range.max", "1");
}
///////////////////////////////////////////////////////////////////////////////
PlanarColliderModule::PlanarColliderModule()
    : ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(NormalX, dataflow::EPR_UNIFORM, mfNormalX)
    , ConstructInpPlug(NormalY, dataflow::EPR_UNIFORM, mfNormalY)
    , ConstructInpPlug(NormalZ, dataflow::EPR_UNIFORM, mfNormalZ)
    , ConstructInpPlug(OriginX, dataflow::EPR_UNIFORM, mfOriginX)
    , ConstructInpPlug(OriginY, dataflow::EPR_UNIFORM, mfOriginY)
    , ConstructInpPlug(OriginZ, dataflow::EPR_UNIFORM, mfOriginZ)
    , ConstructInpPlug(Absorbtion, dataflow::EPR_UNIFORM, mfAbsorbtion)
    , mfOriginX(0.0f)
    , mfOriginY(0.0f)
    , mfOriginZ(0.0f)
    , mfNormalX(0.0f)
    , mfNormalY(0.0f)
    , mfNormalZ(0.0f)
    , mfAbsorbtion(0.0f) {
}
///////////////////////////////////////////////////////////////////////////////
void PlanarColliderModule::Compute(float dt) {
  const psys_ptclbuf& pb = mPlugInpInput.GetValue();
  if (pb.mPool) {
    //////////////////////////////////////////////////////////
    ork::fvec3 PlaneN;
    PlaneN.SetX(mPlugInpNormalX.GetValue());
    PlaneN.SetY(mPlugInpNormalY.GetValue());
    PlaneN.SetZ(mPlugInpNormalZ.GetValue());
    PlaneN.Normalize();
    if (PlaneN.Mag() == 0.0f)
      PlaneN = fvec3(0.0f, 1.0f, 0.0f);
    ork::fvec3 PlaneO;
    PlaneO.SetX(mPlugInpOriginX.GetValue());
    PlaneO.SetY(mPlugInpOriginY.GetValue());
    PlaneO.SetZ(mPlugInpOriginZ.GetValue());
    ork::fplane3 CollisionPlane;
    CollisionPlane.CalcFromNormalAndOrigin(PlaneN, PlaneO);
    float retention = 1.0f - mPlugInpAbsorbtion.GetValue();
    //////////////////////////////////////////////////////////

    for (int i = 0; i < pb.mPool->GetNumAlive(); i++) {
      auto particle        = pb.mPool->GetActiveParticle(i);
      const fvec3& cur_pos = particle->mPosition;
      const fvec3& cur_vel = particle->mVelocity;

      auto nxt_pos = cur_pos + cur_vel * dt;

      float pntdist = CollisionPlane.pointDistance(nxt_pos);

      bool cur_inside = particle->mColliderStates & 1;
      bool nxt_inside = pntdist < 0;

      if ((false == cur_inside) && nxt_inside) {
        particle->mVelocity = cur_vel.Reflect(PlaneN) * retention;
      }
      if (nxt_inside)
        particle->mColliderStates |= 1;
      else
        particle->mColliderStates &= ~1;
    }
  }
  mOutDataOutput.mPool = pb.mPool;
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* PlanarColliderModule::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
    case 1:
      rval = &mPlugInpNormalX;
      break;
    case 2:
      rval = &mPlugInpNormalY;
      break;
    case 3:
      rval = &mPlugInpNormalZ;
      break;
    case 4:
      rval = &mPlugInpOriginX;
      break;
    case 5:
      rval = &mPlugInpOriginY;
      break;
    case 6:
      rval = &mPlugInpOriginZ;
      break;
    case 7:
      rval = &mPlugInpAbsorbtion;
      break;
    default:
      OrkAssert(false);
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SphericalColliderModule::Describe() {
  RegisterObjInpPlug(SphericalColliderModule, Input);
  RegisterObjOutPlug(SphericalColliderModule, Output);
  RegisterFloatXfPlug(SphericalColliderModule, CenterX, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(SphericalColliderModule, CenterY, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(SphericalColliderModule, CenterZ, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(SphericalColliderModule, Radius, -1000.0f, 1000.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(SphericalColliderModule, Absorbtion, 0.0f, 1.0f, ged::OutPlugChoiceDelegate);

  static const char* EdGrpStr = "grp://Main Input Absorbtion "
                                "grp://Sphere CenterX CenterY CenterZ Radius";

  reflect::annotateClassForEditor<SphericalColliderModule>("editor.prop.groups", EdGrpStr);
}
///////////////////////////////////////////////////////////////////////////////
SphericalColliderModule::SphericalColliderModule()
    : ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(CenterX, dataflow::EPR_UNIFORM, mfCenterX)
    , ConstructInpPlug(CenterY, dataflow::EPR_UNIFORM, mfCenterY)
    , ConstructInpPlug(CenterZ, dataflow::EPR_UNIFORM, mfCenterZ)
    , ConstructInpPlug(Radius, dataflow::EPR_UNIFORM, mfRadius)
    , ConstructInpPlug(Absorbtion, dataflow::EPR_UNIFORM, mfAbsorbtion)
    , mfRadius(1.0f)
    , mfCenterX(0.0f)
    , mfCenterY(0.0f)
    , mfCenterZ(0.0f)
    , mfAbsorbtion(0.0f) {
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* SphericalColliderModule::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
    case 1:
      rval = &mPlugInpCenterX;
      break;
    case 2:
      rval = &mPlugInpCenterY;
      break;
    case 3:
      rval = &mPlugInpCenterZ;
      break;
    case 4:
      rval = &mPlugInpRadius;
      break;
    case 5:
      rval = &mPlugInpAbsorbtion;
      break;
    default:
      OrkAssert(false);
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void SphericalColliderModule::Compute(float dt) {
  const psys_ptclbuf& pb = mPlugInpInput.GetValue();
  if (pb.mPool) {
    //////////////////////////////////////////////////////////
    Sphere the_sphere(fvec3(), 1.0f);

    the_sphere.mCenter.SetX(mPlugInpCenterX.GetValue());
    the_sphere.mCenter.SetY(mPlugInpCenterY.GetValue());
    the_sphere.mCenter.SetZ(mPlugInpCenterZ.GetValue());
    the_sphere.mRadius = mPlugInpRadius.GetValue();
    float retention    = 1.0f - mPlugInpAbsorbtion.GetValue();
    //////////////////////////////////////////////////////////

    auto m1 = 1000.0f;
    auto m2 = 1.0f;

    float sphereradsquared = the_sphere.mRadius * the_sphere.mRadius;
    auto& sphereCenter     = the_sphere.mCenter;

    for (int i = 0; i < pb.mPool->GetNumAlive(); i++) {
      auto particle        = pb.mPool->GetActiveParticle(i);
      const fvec3& cur_pos = particle->mPosition;
      const fvec3& cur_vel = particle->mVelocity;
      auto cur_dir         = cur_vel.Normal();

      auto nxt_pos     = cur_pos + cur_vel * dt;
      float ndcsquared = (nxt_pos - the_sphere.mCenter).MagSquared();

      bool cur_inside = particle->mColliderStates & 1;
      bool nxt_inside = ndcsquared <= sphereradsquared;

      if ((false == cur_inside) && nxt_inside) {
        auto N              = (cur_pos - sphereCenter).Normal();
        particle->mVelocity = cur_vel.Reflect(N) * retention;
      }
      if (nxt_inside)
        particle->mColliderStates |= 1;
      else
        particle->mColliderStates &= ~1;
    }
  }
  mOutDataOutput.mPool = pb.mPool;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void DecayModule::Describe() {
  RegisterObjInpPlug(DecayModule, Input);
  RegisterObjOutPlug(DecayModule, Output);
  RegisterFloatXfPlug(DecayModule, Decay, 0.0f, 1.0f, ged::OutPlugChoiceDelegate);
  static const char* EdGrpStr = "grp://yp Input Decay";
  reflect::annotateClassForEditor<DecayModule>("editor.prop.groups", EdGrpStr);
}
///////////////////////////////////////////////////////////////////////////////
DecayModule::DecayModule()
    : ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(Decay, dataflow::EPR_UNIFORM, mfDecay)
    , mfDecay(0.99f) {
}
///////////////////////////////////////////////////////////////////////////////
void DecayModule::Compute(float dt) {
  const psys_ptclbuf& pb = mPlugInpInput.GetValue();
  if (pb.mPool) {
    float decay = powf(mPlugInpDecay.GetValue(), dt);
    for (int i = 0; i < pb.mPool->GetNumAlive(); i++) {
      BasicParticle* particle = pb.mPool->GetActiveParticle(i);
      particle->mVelocity *= decay;
    }
  }
  mOutDataOutput.mPool = pb.mPool;
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* DecayModule::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
    case 1:
      rval = &mPlugInpDecay;
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void TurbulenceModule::Describe() {
  RegisterObjInpPlug(TurbulenceModule, Input);
  RegisterObjOutPlug(TurbulenceModule, Output);
  RegisterFloatXfPlug(TurbulenceModule, AmountX, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(TurbulenceModule, AmountY, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(TurbulenceModule, AmountZ, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  static const char* EdGrpStr = "grp://yp Input AmountX AmountY AmountZ ";
  reflect::annotateClassForEditor<TurbulenceModule>("editor.prop.groups", EdGrpStr);
}
///////////////////////////////////////////////////////////////////////////////
TurbulenceModule::TurbulenceModule()
    : ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(AmountX, dataflow::EPR_UNIFORM, mfAmountX)
    , ConstructInpPlug(AmountY, dataflow::EPR_UNIFORM, mfAmountY)
    , ConstructInpPlug(AmountZ, dataflow::EPR_UNIFORM, mfAmountZ)
    , mfAmountX(0.0f)
    , mfAmountY(0.0f)
    , mfAmountZ(0.0f) {
}
///////////////////////////////////////////////////////////////////////////////
void TurbulenceModule::Compute(float dt) {
  const psys_ptclbuf& pb = mPlugInpInput.GetValue();
  if (pb.mPool) {
    for (int i = 0; i < pb.mPool->GetNumAlive(); i++) {
      BasicParticle* particle = pb.mPool->GetActiveParticle(i);
      float furx              = ((std::rand() % 256) / 256.0f) - 0.5f;
      float fury              = ((std::rand() % 256) / 256.0f) - 0.5f;
      float furz              = ((std::rand() % 256) / 256.0f) - 0.5f;
      /////////////////////////////////////////
      F32 randX = mPlugInpAmountX.GetValue() * furx;
      F32 randY = mPlugInpAmountY.GetValue() * fury;
      F32 randZ = mPlugInpAmountZ.GetValue() * furz;
      ork::fvec4 accel(randX, randY, randZ);
      particle->mVelocity += accel * dt;
    }
  }
  mOutDataOutput.mPool = pb.mPool;
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* TurbulenceModule::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
    case 1:
      rval = &mPlugInpAmountX;
      break;
    case 2:
      rval = &mPlugInpAmountY;
      break;
    case 3:
      rval = &mPlugInpAmountZ;
      break;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void VortexModule::Describe() {
  RegisterObjInpPlug(VortexModule, Input);
  RegisterObjOutPlug(VortexModule, Output);
  RegisterFloatXfPlug(VortexModule, Falloff, 0.0f, 10.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(VortexModule, VortexStrength, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(VortexModule, OutwardStrength, -100.0f, 100.0f, ged::OutPlugChoiceDelegate);
  static const char* EdGrpStr = "grp://yp Input Falloff VortexStrength OutwardStrength";
  reflect::annotateClassForEditor<VortexModule>("editor.prop.groups", EdGrpStr);
}
///////////////////////////////////////////////////////////////////////////////
VortexModule::VortexModule()
    : ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
    , ConstructInpPlug(Input, dataflow::EPR_UNIFORM, gNoCon)
    , ConstructInpPlug(Falloff, dataflow::EPR_UNIFORM, mfFalloff)
    , ConstructInpPlug(VortexStrength, dataflow::EPR_UNIFORM, mfVortexStrength)
    , ConstructInpPlug(OutwardStrength, dataflow::EPR_UNIFORM, mfOutwardStrength)
    , mfFalloff(0.0f)
    , mfVortexStrength(0.0f)
    , mfOutwardStrength(0.0f) {
}
///////////////////////////////////////////////////////////////////////////////
void VortexModule::Compute(float dt) {
  const psys_ptclbuf& pb = mPlugInpInput.GetValue();
  if (pb.mPool) {
    for (int i = 0; i < pb.mPool->GetNumAlive(); i++) {
      BasicParticle* particle = pb.mPool->GetActiveParticle(i);
      /////////////////////////////////////////
      F32 falloff         = mPlugInpFalloff.GetValue();
      F32 vortexstrength  = mPlugInpVortexStrength.GetValue();
      F32 outwardstrength = mPlugInpOutwardStrength.GetValue();

      fvec3 Pos2D = particle->mPosition;
      Pos2D.SetY(0.0f);
      fvec3 N     = particle->mPosition.Normal();
      fvec3 Dir   = N.Cross(fvec3::UnitY());
      float fstr  = 1.0f / (1.0f + falloff / Pos2D.Mag());
      fvec3 Force = Dir * (vortexstrength * fstr);
      Force += N * (outwardstrength * fstr);

      particle->mVelocity += Force * dt;
    }
  }
  mOutDataOutput.mPool = pb.mPool;
}
///////////////////////////////////////////////////////////////////////////////
dataflow::inplugbase* VortexModule::GetInput(int idx) {
  dataflow::inplugbase* rval = 0;
  switch (idx) {
    case 0:
      rval = &mPlugInpInput;
      break;
    case 1:
      rval = &mPlugInpFalloff;
      break;
    case 2:
      rval = &mPlugInpVortexStrength;
      break;
    case 3:
      rval = &mPlugInpOutwardStrength;
      break;
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::lev2::particle
