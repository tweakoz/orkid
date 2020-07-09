#pragma once

#include <ork/reflect/properties/registerX.inl>
#include <ork/object/ObjectClass.h>
#include <ork/math/multicurve.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/kernel/orklut.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>
#include <ork/asset/Asset.h>
#include <array>

using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::rtti;

struct SimpleTest;
struct SharedTest;

using simpletest_ptr_t = std::shared_ptr<SimpleTest>;
using sharedtest_ptr_t = std::shared_ptr<SharedTest>;

////////////////////////////////////////////////////////////////////////////////

struct SimpleTest final : public Object {
  DeclareConcreteX(SimpleTest, Object);

public:
  SimpleTest(std::string str = "");
  std::string _strvalue;
};

////////////////////////////////////////////////////////////////////////////////

struct SharedTest final : public Object {
  DeclareConcreteX(SharedTest, Object);

public:
  SharedTest();

  object_ptr_t _accessorChild   = nullptr;
  simpletest_ptr_t _directChild = nullptr;
  int _directInt                = -1;
  uint32_t _directUint32        = 0;
  size_t _directSizeT           = 0;
  bool _directBool              = false;
  float _directFloat            = 0.0f;
  double _directDouble          = 0.0;
  std::string _directString     = "";

  void getChild(object_ptr_t& outptr) const {
    outptr = _accessorChild;
  }
  void setChild(object_ptr_t const& v) {
    _accessorChild = v;
  }
};

////////////////////////////////////////////////////////////////////////////////

struct MapTest final : public Object {
  DeclareConcreteX(MapTest, Object);

public:
  MapTest();

  std::map<int, std::string> _directintstrmap;
  std::map<std::string, int> _directstrintmap;
  std::unordered_map<int, std::string> _directintstrumap;
  std::unordered_map<std::string, int> _directstrintumap;
  orklut<std::string, int> _directstrintlut;
  std::map<std::string, simpletest_ptr_t> _directstrobjmap;
};

////////////////////////////////////////////////////////////////////////////////

struct VectorTest final : public Object {
  DeclareConcreteX(VectorTest, Object);

public:
  VectorTest();

  std::vector<int> _directintvect;
  std::vector<std::string> _directstrvect;
  std::vector<simpletest_ptr_t> _directobjvect;
};

////////////////////////////////////////////////////////////////////////////////

struct ArrayTest final : public Object {
  DeclareConcreteX(ArrayTest, Object);

public:
  ArrayTest();

  std::array<int, 4> _directstdaryvect;
  int _directcaryvect[4];
  simpletest_ptr_t _directcaryobjvect[4];
};

////////////////////////////////////////////////////////////////////////////////

struct EnumTest final : public Object {
  DeclareConcreteX(EnumTest, Object);

public:
  EnumTest();

  MultiCurveSegmentType _mcst = MultiCurveSegmentType::LINEAR;
};

////////////////////////////////////////////////////////////////////////////////

struct MathTest final : public Object {
  DeclareConcreteX(MathTest, Object);

public:
  MathTest();

  fvec2 _fvec2;
  fvec3 _fvec3;
  fvec4 _fvec4;

  fquat _fquat;

  fmtx3 _fmtx3;
  fmtx4 _fmtx4;
};

////////////////////////////////////////////////////////////////////////////////

struct AssetTest final : public Object {
  DeclareConcreteX(AssetTest, Object);

public:
  AssetTest();
  asset::asset_ptr_t _assetptr;
};

////////////////////////////////////////////////////////////////////////////////
