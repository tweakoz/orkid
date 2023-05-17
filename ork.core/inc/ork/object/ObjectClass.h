////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/ObjectCategory.h>
#include <ork/rtti/RTTI.h>
#include <ork/config/config.h>
#include <boost/uuid/uuid.hpp>

namespace ork::reflect {
struct ObjectProperty;
}
namespace ork { namespace object {

struct PropertyModifier;

struct PropertyModifier {
  template <typename T> inline PropertyModifier* annotate(const ConstString& key, T value);
  inline PropertyModifier* Annotate(const ConstString& key, ConstString value);
  inline PropertyModifier* operator->();
  reflect::ObjectProperty* _property = nullptr;
};

struct ObjectClass : public rtti::Class {
  RttiDeclareExplicit(ObjectClass, rtti::Class, rtti::NamePolicy, ObjectCategory) public : ObjectClass(const rtti::RTTIData&);

public:
  static boost::uuids::uuid genUUID();

  typedef ork::reflect::Description::anno_t anno_t;

  object_ptr_t createShared() const;

  void annotate(const ConstString& key, const anno_t& val);
  const anno_t& annotation(const ConstString& key);

  template <typename T> void annotateTyped(const ConstString& key, const T& val) {
    _description.annotateClassTyped(key, val);
  }
  template <typename T> attempt_cast_const<T> annotationTyped(const ConstString& key) const {
    return _description.classAnnotationTyped<T>(key);
  }

  reflect::Description& Description();
  const reflect::Description& Description() const;

  template <typename ClassType> static void InitializeType() {
    ClassType::Describe();
  }

  template <typename ClassType, typename MemberType>
  inline PropertyModifier directProperty(const char* name, MemberType ClassType::*member);

  template <typename ClassType, typename MemberMapType>
  inline PropertyModifier directMapProperty(const char* name, MemberMapType ClassType::*member);

  template <typename ClassType, typename MemberArrayType>
  inline PropertyModifier directArrayProperty(const char* name, MemberArrayType ClassType::*member);

  template <typename ClassType, typename MemberVectorType>
  inline PropertyModifier directVectorProperty(const char* name, MemberVectorType ClassType::*member);

  template <typename ClassType, typename MemberType>
  inline PropertyModifier directObjectMapProperty(
      const char* name, //
      MemberType ClassType::*member);

  template <typename ClassType, typename MemberType>
  inline PropertyModifier directObjectVectorProperty(
      const char* name, //
      MemberType ClassType::*member);

  template <typename ClassType, typename MemberType>
  inline PropertyModifier directTypedObjectMapProperty(
      const char* name, //
      MemberType ClassType::*member);

  template <typename ClassType, typename MemberType>
  inline PropertyModifier lambdaProperty(
      const char* name, //
      std::function<void(const ClassType*, MemberType&)>,
      std::function<void(ClassType*, const MemberType&)>);

  template <typename ClassType, typename MemberType>
  inline PropertyModifier accessorProperty(
      const char* name, //
      void (ClassType::*getter)(MemberType&) const,
      void (ClassType::*setter)(const MemberType&));

  template <typename ClassType>
  inline object::PropertyModifier accessorVariant(
      const char* name,
      bool (ClassType::*serialize)(reflect::serdes::ISerializer&) const,
      bool (ClassType::*deserialize)(reflect::serdes::IDeserializer&));

  template <typename ClassType, typename MemberType>
  inline PropertyModifier directObjectProperty(
      const char* name, //
      MemberType ClassType::*member);

  template <typename ClassType, typename MemberType>
  inline PropertyModifier directAssetProperty(
      const char* name, //
      MemberType ClassType::*member);

  template <typename ClassType>
  inline PropertyModifier floatProperty(const char* name, float_range range, float ClassType::*member);

  template <typename ClassType> inline PropertyModifier intProperty(const char* name, int_range range, int ClassType::*member);

private:
  inline void make_abstract() override {
  }

  reflect::Description _description;
  void Initialize() override;
};

}} // namespace ork::object
