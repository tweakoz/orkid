////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/rtti/RTTI.h>
#include <ork/reflect/Serializable.h>
#include <ork/object/ObjectClass.h>
#include <ork/util/md5.h>
#include <boost/uuid/uuid.hpp>

namespace ork {

namespace event {
class Event;
}

namespace object {
class Signal;
}

namespace reflect {
class ISerializer;
class IDeserializer;
class BidirectionalSerializer;
} // namespace reflect

struct Object;

struct Object : public rtti::ICastable {
private:
  RttiDeclareAbstractWithCategory(Object, rtti::ICastable, object::ObjectClass);

public:
  virtual ~Object() {
  }

  static void xxxSerializeShared(object_constptr_t obj, reflect::ISerializer&);
  static void xxxDeserializeShared(object_ptr_t obj, reflect::IDeserializer&);

  object::Signal* FindSignal(ConstString name);

  virtual bool PreSerialize(reflect::ISerializer&) const;
  virtual bool PreDeserialize(reflect::IDeserializer&);
  virtual bool PostSerialize(reflect::ISerializer&) const;
  virtual bool PostDeserialize(reflect::IDeserializer&);

  virtual object_ptr_t cloneShared() const;
  void _cloneInto(object_ptr_t& into) const;

  Md5Sum CalcMd5() const;

  bool Notify(const event::Event* pEV) {
    return DoNotify(pEV);
  }
  bool Query(event::Event* pEV) const {
    return DoQuery(pEV);
  }

  boost::uuids::uuid _uuid;

private:
  virtual bool DoNotify(const event::Event* pEV) {
    return false;
  }
  virtual bool DoQuery(event::Event* pEV) const {
    return false;
  }
};

reflect::BidirectionalSerializer& operator||(reflect::BidirectionalSerializer&, Object&);
reflect::BidirectionalSerializer& operator||(reflect::BidirectionalSerializer&, const Object&);

/*template <typename T>
inline void DeserializeUnknownObject(
    ork::reflect::IDeserializer& deser, //
    std::shared_ptr<T>& out_value) {
  object_ptr_t obj = nullptr;
  auto objclz      = object::ObjectClass::GetClassStatic();
  auto objcat      = objclz->GetClass();
  objcat->deserializeObject(deser, obj);
  out_value = std::dynamic_pointer_cast<T>(obj);
}*/

// Object* DeserializeObject(PieceString file);

} // namespace ork
