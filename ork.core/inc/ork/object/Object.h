////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

using ObjectMap = std::map<PoolString,Object*>;

object_ptr_t loadObjectFromFile(const char* filename);
object_ptr_t loadObjectFromString(const char* jsondata);

namespace event {
class Event;
}

namespace object {
class Signal;
struct ObjectClass;
} // namespace object

struct Object;

struct Object : public rtti::ICastable {

  static object_ptr_t clone(object_constptr_t source);
  static Md5Sum md5sum(object_constptr_t source);

  RttiDeclareAbstractWithCategory(Object, rtti::ICastable, object::ObjectClass);

public:
  Object();
  virtual ~Object();

  object::ObjectClass* objectClass() const;
  object::Signal* findSignal(ConstString name);

  virtual bool preSerialize(reflect::serdes::ISerializer&) const;
  virtual bool preDeserialize(reflect::serdes::IDeserializer&);
  virtual bool postSerialize(reflect::serdes::ISerializer&) const;
  virtual bool postDeserialize(reflect::serdes::IDeserializer&);

  void notify(const event::Event* pEV);

  boost::uuids::uuid _uuid;

private:
  virtual void doNotify(const event::Event* pEV) {
    return;
  }
};

template <typename T>
inline std::shared_ptr<T> //
objcast(object_ptr_t obj) {
  return std::dynamic_pointer_cast<T>(obj);
}

} // namespace ork
