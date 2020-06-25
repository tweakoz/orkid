#pragma once

#include <ork/reflect/properties/registerX.inl>
#include <ork/object/ObjectClass.h>
#include <ork/rtti/RTTIX.inl>

using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::rtti;

class SharedTest final : public Object {
  DeclareConcreteX(SharedTest, Object);

public:
  object_ptr_t _childObject = nullptr;
  void getChild(object_ptr_t& outptr) const {
    outptr = _childObject;
  }
  void setChild(object_ptr_t const& v) {
    _childObject = v;
  }
};
