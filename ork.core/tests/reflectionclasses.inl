#pragma once

#include <ork/reflect/properties/registerX.inl>
#include <ork/object/ObjectClass.h>
#include <ork/rtti/RTTIX.inl>

using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::rtti;

////////////////////////////////////////////////////////////////////////////////

struct SharedTest final : public Object {
  DeclareConcreteX(SharedTest, Object);

public:
  SharedTest();

  object_ptr_t _accessorChild = nullptr;
  object_ptr_t _directChild   = nullptr;
  int _directInt              = -1;
  uint32_t _directUint32      = 0;
  size_t _directSizeT         = 0;
  bool _directBool            = false;
  float _directFloat          = 0.0f;
  double _directDouble        = 0.0;
  std::string _directString   = "";

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
};

////////////////////////////////////////////////////////////////////////////////
