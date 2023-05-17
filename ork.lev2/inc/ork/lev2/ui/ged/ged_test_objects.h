#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/object/Object.h>
#include <ork/math/multicurve.h>
#include <ork/math/gradient.h>
#include <ork/dataflow/dataflow.h>

namespace ork::lev2::ged {
///////////////////////////////////////////////////////////////////////////

struct TestObject : public ork::Object {
  DeclareConcreteX(TestObject, ork::Object);
public:
  int int_prop = 0;
  bool bool_prop = false;
  float float_prop= 0.0f;
  asset::asset_ptr_t _genericAsset;
  orklut<std::string, multicurve1d_ptr_t> _curves;
  orklut<std::string, gradient_fvec4_ptr_t> _gradients;
  orklut<std::string, dataflow::graphdata_ptr_t> _particlesystems;
};

using testobject_ptr_t      = std::shared_ptr<TestObject>;
using testobject_constptr_t = std::shared_ptr<const TestObject>;

///////////////////////////////////////////////////////////////////////////

struct TestObjectConfiguration : public ork::Object {
  DeclareConcreteX(TestObjectConfiguration, ork::Object);
public:
  orklut<std::string, testobject_ptr_t> _testobjects;
};

using testobjectconfiguration_ptr_t      = std::shared_ptr<TestObjectConfiguration>;

} //namespace ork::lev2::ged {
