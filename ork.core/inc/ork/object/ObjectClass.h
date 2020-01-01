////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/ObjectCategory.h>
#include <ork/rtti/RTTI.h>

#include <ork/config/config.h>

namespace ork::reflect {
class IObjectProperty;
}
namespace ork { namespace object {

class ObjectCategory;

struct PropertyModifier;

struct PropertyModifier {
  template <typename T> inline PropertyModifier* annotate(const ConstString& key, T value);
  inline PropertyModifier* Annotate(const ConstString& key, ConstString value);
  inline PropertyModifier* operator->();
  reflect::IObjectProperty* _property = nullptr;
};

class ObjectClass : public rtti::Class {
  RttiDeclareExplicit(ObjectClass, rtti::Class, rtti::NamePolicy, ObjectCategory) public : ObjectClass(const rtti::RTTIData&);

public:
  typedef ork::reflect::Description::anno_t anno_t;

  void annotate( const ConstString &key, const anno_t& val );
  const anno_t& annotation( const ConstString &key );

  reflect::Description& Description();
  const reflect::Description& Description() const;

  template <typename ClassType> static void InitializeType() { ClassType::Describe(); }

  template <typename ClassType, typename MemberType>
  inline PropertyModifier memberProperty(const char* name, MemberType ClassType::*member);

  template <typename ClassType, typename MemberType>
  inline PropertyModifier
  accessorProperty(const char* name, void (ClassType::*getter)(MemberType&) const, void (ClassType::*setter)(const MemberType&));

  template <typename ClassType>
  inline PropertyModifier floatProperty(const char* name, float_range range, float ClassType::*member);

private:
  reflect::Description _description;
  void Initialize() override;
};

}} // namespace ork::object
