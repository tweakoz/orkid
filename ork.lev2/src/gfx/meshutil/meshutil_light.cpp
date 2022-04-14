////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/math/collision_test.h>
#include <ork/math/sphere.h>

#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/registerX.inl>
#include <ork/math/cmatrix4.hpp>

///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::meshutil::Light, "MeshUtilLight");
ImplementReflectionX(ork::meshutil::LightContainer, "MeshUtilLightContainer");
ImplementReflectionX(ork::meshutil::AmbientLight, "MeshUtilAmbientLight");
ImplementReflectionX(ork::meshutil::DirLight, "MeshUtilDirLight");
ImplementReflectionX(ork::meshutil::PointLight, "MeshUtilPointLight");

template class ork::orklut<ork::PoolString, ork::meshutil::Light*>;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace meshutil {
///////////////////////////////////////////////////////////////////////////////

void LightContainer::describeX(class_t* clazz) {
  ork::reflect::RegisterMapProperty("Lights", &LightContainer::mLights);
}

///////////////////////////////////////////////////////////////////////////////

void Light::describeX(class_t* clazz) {
  clazz->directProperty("Color", &Light::mColor);
  clazz->directProperty("WorldMatrix", &Light::mWorldMatrix);
  clazz->directProperty("AffectsSpecular", &Light::mbSpecular);
  clazz->directProperty("ShadowCaster", &Light::mbIsShadowCaster);

  clazz->floatProperty("ShadowSamples", float_range{1, 256}, &Light::mShadowSamples);
  clazz->floatProperty("ShadowBlur", float_range{0, 1}, &Light::mShadowBlur);
  clazz->floatProperty("ShadowBias", float_range{0, 10}, &Light::mShadowBias);
}

///////////////////////////////////////////////////////////////////////////////

void DirLight::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

void AmbientLight::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

void PointLight::describeX(class_t* clazz) {
  clazz->directProperty("Falloff", &PointLight::mFalloff);
  clazz->directProperty("Radius", &PointLight::mRadius);
}

///////////////////////////////////////////////////////////////////////////////

bool PointLight::AffectsSphere(const fvec3& center, float radius) const {
  float dist          = (mWorldMatrix.translation() - center).magnitude();
  float combinedradii = (radius + mRadius);

  //	orkprintf( "PointLight::AffectsSphere point<%f %f %f> center<%f %f %f>\n",
  //				mWorldPosition.x, mWorldPosition.y, mWorldPosition.z,
  //				center.x, center.y, center.z );

  // float crsq = combinedradii; //(combinedradii*combinedradii);
  return (dist < combinedradii);
}

///////////////////////////////////////////////////////////////////////////////

bool PointLight::AffectsAABox(const AABox& aab) const {
  return CollisionTester::SphereAABoxTest(Sphere(mWorldMatrix.translation(), mRadius), aab);
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
