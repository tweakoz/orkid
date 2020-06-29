////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/rtti/RTTI.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/object/ObjectClass.h>
#include <ork/util/md5.h>
#include <boost/uuid/uuid.hpp>

namespace ork {

object_ptr_t LoadObjectFromFile(const char* filename);
object_ptr_t LoadObjectFromString(const char* jsondata);

namespace event {
class Event;
}

namespace object {
class Signal;
}

namespace reflect {
class BidirectionalSerializer;
} // namespace reflect

struct Object;

struct Object : public rtti::ICastable {

  static object_ptr_t clone(object_constptr_t source);
  static Md5Sum md5sum(object_constptr_t source);

  RttiDeclareAbstractWithCategory(Object, rtti::ICastable, object::ObjectClass);

public:
  Object();
  virtual ~Object();

  object::Signal* findSignal(ConstString name);

  virtual bool preSerialize(reflect::ISerializer&) const;
  virtual bool preDeserialize(reflect::IDeserializer&);
  virtual bool postSerialize(reflect::ISerializer&) const;
  virtual bool postDeserialize(reflect::IDeserializer&);

  void notify(const event::Event* pEV);

  boost::uuids::uuid _uuid;

private:
  virtual void doNotify(const event::Event* pEV) {
    return;
  }
};

} // namespace ork
