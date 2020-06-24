#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>
#include <string.h>

#include <ork/reflect/RegisterProperty.h>
#include <ork/object/ObjectClass.h>
#include <ork/rtti/RTTI.h>

using namespace ork;
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

ImplementReflectionX(SharedTest, "SharedTest");

void SharedTest::describeX(ork::object::ObjectClass* clazz) {
  clazz->accessorProperty(
      "sharedtest", //
      &SharedTest::getChild,
      &SharedTest::setChild);
}

TEST(ReflectSharedProperty) {

  CHECK(true);
}
