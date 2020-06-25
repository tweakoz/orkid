////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/reflect/Description.h>
#include <ork/rtti/Class.h>
#include <ork/rtti/RTTIData.h>
#include <ork/rtti/downcast.h>

#include <ork/config/config.h>

namespace ork {
struct float_range {
  float _min = 0.0f;
  float _max = 0.0f;
};
} // namespace ork
namespace ork::object {
class ObjectClass;
}
namespace ork::rtti {

////////////////////////////////////////////////////////////////////////////////

template <typename ClassType> class DefaultPolicy {
public:
  static ICastable* Factory() {
    return new ClassType();
  }
  static castable_ptr_t sharedFactory() {
    return std::make_shared<ClassType>();
  }

  static void Initialize() {
    ClassType::GetClassStatic()->SetName(ClassType::DesignNameStatic());
    ClassType::GetClassStatic()->setRawFactory(Factory);
    ClassType::GetClassStatic()->setSharedFactory(sharedFactory);
    ClassType::RTTITyped::RTTICategory::template InitializeType<ClassType>();
  }
};

////////////////////////////////////////////////////////////////////////////////

template <typename ClassType> class AbstractPolicy {
public:
  static void Initialize() {
    ClassType::GetClassStatic()->SetName(ClassType::DesignNameStatic());
    ClassType::RTTITyped::RTTICategory::template InitializeType<ClassType>();
  }
};

////////////////////////////////////////////////////////////////////////////////

template <typename ClassType> class NamePolicy {
public:
  static void Initialize() {
    ClassType::GetClassStatic()->SetName(ClassType::DesignNameStatic());
  }
};

////////////////////////////////////////////////////////////////////////////////

template <typename ClassType> class CastablePolicy {
public:
  static void Initialize() {
  }
};

////////////////////////////////////////////////////////////////////////////////

template <
    typename ClassType,
    typename BaseType                = ICastable,
    template <typename> class Policy = DefaultPolicy,
    typename Category                = typename BaseType::RTTITyped::RTTICategory>

class RTTI : public BaseType {
public:
  typedef RTTI RTTITyped;
  typedef Category RTTICategory;
  typedef Policy<ClassType> policy_t;
  typedef BaseType basetype_t;

  static RTTIData ClassRTTI() {
    return RTTIData(BaseType::GetClassStatic(), &RTTI::ClassInitializer);
  }

  static RTTICategory* GetClassStatic() {
    return &sClass;
  }
  RTTICategory* GetClass() const override {
    return GetClassStatic();
  }

  static void Describe();                // overridden by users of RTTI.
  static ConstString DesignNameStatic(); // implemented (or overridden) by users of RTTI, as needed by policy.

protected:
  RTTI() {
  }

private:
  static void ClassInitializer() {
    OrkAssert(ClassType::GetClassStatic() != BaseType::GetClassStatic());

    Policy<ClassType>::Initialize();
  };

  static RTTICategory sClass;
};

////////////////////////////////////////////////////////////////////////////////

template <typename ClassType, typename BaseType = ICastable, typename Category = typename BaseType::RTTITyped::RTTICategory>
class Castable : public BaseType {
public:
  typedef Castable RTTITyped;
  typedef Category RTTICategory;

  static RTTICategory* GetClassStatic() {
    return &sClass;
  }
  RTTICategory* GetClass() const override {
    return GetClassStatic();
  }
  static RTTIData ClassRTTI() {
    return RTTIData(BaseType::GetClassStatic(), NULL);
  }

private:
  static RTTICategory sClass;
};

////////////////////////////////////////////////////////////////////////////////
/// INTERNAL USE ONLY
////////////////////////////////////////////////////////////////////////////////

#define __INTERNAL_RTTI_DECLARE_TRANSPARENT__(ClassType, RTTIImplementation)                                                       \
public:                                                                                                                            \
  typedef typename RTTIImplementation RTTITyped;                                                                                    \
  typedef typename RTTITyped::basetype_t BaseType;                                                                                  \
  static ::ork::ConstString DesignNameStatic();                                                                                    \
  static void Describe();                                                                                                          \
  static RTTITyped::RTTICategory* GetClassStatic();                                                                                 \
  RTTITyped::RTTICategory* GetClass() const override;                                                                               \
                                                                                                                                   \
private:                                                                                                                           \
  static RTTITyped::RTTICategory sClass;

//

#define __INTERNAL_RTTI_DECLARE_TRANSPARENT_TEMPLATE__(ClassType, RTTIImplementation)                                              \
public:                                                                                                                            \
  typedef typename RTTIImplementation RTTITyped;                                                                                    \
  static ::ork::ConstString DesignNameStatic();                                                                                    \
  static void Describe();                                                                                                          \
  static typename RTTITyped::RTTICategory* GetClassStatic();                                                                        \
  virtual typename RTTITyped::RTTICategory* GetClass() const;                                                                       \
                                                                                                                                   \
private:                                                                                                                           \
  static typename RTTITyped::RTTICategory sClass;

//

#define __INTERNAL_RTTI_DECLARE_TRANSPARENT_CASTABLE__(ClassType, RTTIImplementation)                                              \
public:                                                                                                                            \
  typedef typename RTTIImplementation RTTITyped;                                                                                    \
  static RTTITyped::RTTICategory* GetClassStatic();                                                                                 \
  virtual RTTITyped::RTTICategory* GetClass() const;                                                                                \
                                                                                                                                   \
private:                                                                                                                           \
  static RTTITyped::RTTICategory sClass;

//

#define __INTERNAL_RTTI_DECLARE_TRANSPARENT_TEMPLATE_CASTABLE__(ClassType, RTTIImplementation)                                     \
public:                                                                                                                            \
  typedef typename RTTIImplementation RTTITyped;                                                                                    \
  static typename RTTITyped::RTTICategory* GetClassStatic();                                                                        \
  virtual typename RTTITyped::RTTICategory* GetClass() const;                                                                       \
                                                                                                                                   \
private:                                                                                                                           \
  static typename RTTITyped::RTTICategory sClass;

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

#define RTTI_1_ARG__(X) X
#define RTTI_2_ARG__(X, Y) X, Y
#define RTTI_3_ARG__(X, Y, Z) X, Y, Z
#define RTTI_4_ARG__(X, Y, Z, W) X, Y, Z, W

////////////////////////////////////////////////////////////////////////////////

#define DECLARE_TRANSPARENT_TEMPLATE_RTTI(ClassType, BaseType)                                                                     \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT_TEMPLATE__(ClassType, RTTI_2_ARG__(::ork::rtti::RTTI<ClassType, BaseType>))

////////////////////////////////////////////////////////////////////////////////

#define DECLARE_TRANSPARENT_TEMPLATE_ABSTRACT_RTTI(ClassType, BaseType)                                                            \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT_TEMPLATE__(                                                                                  \
      ClassType, RTTI_3_ARG__(::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::AbstractPolicy>))

////////////////////////////////////////////////////////////////////////////////

#define RttiDeclareAbstract(ClassType, BaseType)                                                                                   \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT__(                                                                                           \
      ClassType, RTTI_3_ARG__(::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::AbstractPolicy>))

////////////////////////////////////////////////////////////////////////////////

#define RttiDeclareNoFactory(ClassType, BaseType)                                                                                  \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT__(                                                                                           \
      ClassType, RTTI_3_ARG__(::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::AbstractPolicy>))

////////////////////////////////////////////////////////////////////////////////

#define RttiDeclareConcrete(ClassType, BaseType)                                                                                   \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT__(ClassType, RTTI_2_ARG__(::ork::rtti::RTTI<ClassType, BaseType>))

////////////////

#define RttiDeclareConcretePublic(ClassType, BaseType)                                                                             \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT__(ClassType, RTTI_2_ARG__(::ork::rtti::RTTI<ClassType, BaseType>))                           \
public:

////////////////////////////////////////////////////////////////////////////////

#define RttiDeclareAbstractWithCategory(ClassType, BaseType, CategoryType)                                                         \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT__(                                                                                           \
      ClassType, RTTI_4_ARG__(::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::AbstractPolicy, CategoryType>))

////////////////////////////////////////////////////////////////////////////////

#define RttiDeclareConcreteWithCategory(ClassType, BaseType, CategoryType)                                                         \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT__(                                                                                           \
      ClassType, RTTI_4_ARG__(::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::DefaultPolicy, CategoryType>))

////////////////////////////////////////////////////////////////////////////////

#define RttiDeclareExplicit(ClassType, BaseType, Policy, ClassImplementation)                                                      \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT__(                                                                                           \
      ClassType, RTTI_4_ARG__(::ork::rtti::RTTI<ClassType, BaseType, Policy, ClassImplementation>))

////////////////////////////////////////////////////////////////////////////////

#define DECLARE_TRANSPARENT_NAME_RTTI(ClassType, BaseType)                                                                         \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT__(ClassType, RTTI_3_ARG__(::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::NamePolicy>))

////////////////////////////////////////////////////////////////////////////////

#define DECLARE_TRANSPARENT_CUSTOM_POLICY_RTTI(ClassType, BaseType, Policy)                                                        \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT__(ClassType, RTTI_3_ARG__(::ork::rtti::RTTI<ClassType, BaseType, Policy>))

////////////////////////////////////////////////////////////////////////////////

#define DECLARE_TRANSPARENT_CASTABLE(ClassType, BaseType)                                                                          \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT_CASTABLE__(ClassType, RTTI_2_ARG__(::ork::rtti::Castable<ClassType, BaseType>))

////////////////////////////////////////////////////////////////////////////////

#define DECLARE_TRANSPARENT_TEMPLATE_CASTABLE(ClassType, BaseType)                                                                 \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT_TEMPLATE_CASTABLE__(ClassType, RTTI_2_ARG__(::ork::rtti::Castable<ClassType, BaseType>))

////////////////////////////////////////////////////////////////////////////////

#define DECLARE_TRANSPARENT_CASTABLE_EX(ClassType, BaseType, ClassImplementation)                                                  \
  __INTERNAL_RTTI_DECLARE_TRANSPARENT_CASTABLE__(                                                                                  \
      ClassType, RTTI_3_ARG__(::ork::rtti::Castable<ClassType, BaseType, ClassImplementation>))

////////////////////////////////////////////////////////////////////////////////

#define INSTANTIATE_CASTABLE_SERIALIZE(ObjectType)                                                                                 \
  namespace ork { namespace reflect {                                                                                              \
  template <>                                                                                                                      \
  void Serialize<ObjectType*>(ObjectType* const* in, ObjectType** out, ::ork::reflect::BidirectionalSerializer& bidi) {            \
    if (bidi.Serializing()) {                                                                                                      \
      bidi | static_cast<const rtti::ICastable*>(*in);                                                                             \
    } else {                                                                                                                       \
      rtti::ICastable* result;                                                                                                     \
      bidi | result;                                                                                                               \
      *out = rtti::safe_downcast<ObjectType*>(result);                                                                             \
    }                                                                                                                              \
  }                                                                                                                                \
  }                                                                                                                                \
  }

////////////////////////////////////////////////////////////////////////////////
// force link of an predeclared class type
// protos
template <typename ClassType> Class* Link();
Class* ForceLink(Class*);

////////////////////////////////////////////////////////////////////////////////

#define INSTANTIATE_LINK_FUNCTION(ClassName)                                                                                       \
  namespace ork { namespace rtti {                                                                                                 \
  template <> Class* Link<ClassName>() {                                                                                           \
    return ForceLink(::ClassName::GetClassStatic());                                                                               \
  }                                                                                                                                \
  }                                                                                                                                \
  }

////////////////////////////////////////////////////////////////////////////////

#define INSTANTIATE_RTTI(ClassName, TheDesignName)                                                                                 \
  namespace ork {                                                                                                                  \
  ConstString ClassName::RTTITyped::DesignNameStatic() {                                                                            \
    return TheDesignName;                                                                                                          \
  }                                                                                                                                \
  ClassName::RTTITyped::RTTICategory ClassName::RTTITyped::sClass(ClassName::ClassRTTI());                                           \
  }                                                                                                                                \
  INSTANTIATE_CASTABLE_SERIALIZE(ClassName)                                                                                        \
  INSTANTIATE_CASTABLE_SERIALIZE(const ClassName)                                                                                  \
  INSTANTIATE_LINK_FUNCTION(ClassName)

////////////////////////////////////////////////////////////////////////////////

#define INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ClassName, TheDesignName)                                                            \
  template <> ork::ConstString ClassName::DesignNameStatic() {                                                                     \
    return TheDesignName;                                                                                                          \
  }                                                                                                                                \
  template <> ClassName::RTTITyped::RTTICategory ClassName::sClass(ClassName::RTTITyped::ClassRTTI());                               \
  template <> ClassName::RTTITyped::RTTICategory* ClassName::GetClassStatic() {                                                     \
    return &sClass;                                                                                                                \
  }                                                                                                                                \
  template <> ClassName::RTTITyped::RTTICategory* ClassName::GetClass() const {                                                     \
    return GetClassStatic();                                                                                                       \
  }                                                                                                                                \
  INSTANTIATE_CASTABLE_SERIALIZE(ClassName)                                                                                        \
  INSTANTIATE_CASTABLE_SERIALIZE(const ClassName)                                                                                  \
  INSTANTIATE_LINK_FUNCTION(ClassName)

////////////////////////////////////////////////////////////////////////////////

#define INSTANTIATE_TRANSPARENT_RTTI(ClassName, TheDesignName)                                                                     \
  ::ork::ConstString ClassName::DesignNameStatic() {                                                                               \
    return TheDesignName;                                                                                                          \
  }                                                                                                                                \
  ClassName::RTTITyped::RTTICategory ClassName::sClass(ClassName::RTTITyped::ClassRTTI());                                           \
  ClassName::RTTITyped::RTTICategory* ClassName::GetClassStatic() {                                                                 \
    return &sClass;                                                                                                                \
  }                                                                                                                                \
  ClassName::RTTITyped::RTTICategory* ClassName::GetClass() const {                                                                 \
    return GetClassStatic();                                                                                                       \
  }                                                                                                                                \
  INSTANTIATE_CASTABLE_SERIALIZE(ClassName)                                                                                        \
  INSTANTIATE_CASTABLE_SERIALIZE(const ClassName)                                                                                  \
  INSTANTIATE_LINK_FUNCTION(ClassName)

////////////////////////////////////////////////////////////////////////////////

#define INSTANTIATE_CASTABLE(ClassName)                                                                                            \
  template <> ClassName::RTTITyped::RTTICategory ClassName::RTTITyped::sClass(ClassName::ClassRTTI());                               \
  INSTANTIATE_CASTABLE_SERIALIZE(ClassName)                                                                                        \
  INSTANTIATE_CASTABLE_SERIALIZE(const ClassName)                                                                                  \
  INSTANTIATE_LINK_FUNCTION(ClassName)

////////////////////////////////////////////////////////////////////////////////

#define INSTANTIATE_TRANSPARENT_CASTABLE(ClassName)                                                                                \
  ClassName::RTTITyped::RTTICategory ClassName::sClass(ClassName::RTTITyped::ClassRTTI());                                           \
  ClassName::RTTITyped::RTTICategory* ClassName::GetClassStatic() {                                                                 \
    return &sClass;                                                                                                                \
  }                                                                                                                                \
  ClassName::RTTITyped::RTTICategory* ClassName::GetClass() const {                                                                 \
    return GetClassStatic();                                                                                                       \
  }                                                                                                                                \
  INSTANTIATE_CASTABLE_SERIALIZE(ClassName)                                                                                        \
  INSTANTIATE_CASTABLE_SERIALIZE(const ClassName)                                                                                  \
  INSTANTIATE_LINK_FUNCTION(ClassName)

////////////////////////////////////////////////////////////////////////////////

#define INSTANTIATE_TRANSPARENT_TEMPLATE_CASTABLE(ClassName)                                                                       \
  ClassName::RTTITyped::RTTICategory ClassName::sClass(ClassName::RTTITyped::ClassRTTI());                                           \
  ClassName::RTTITyped::RTTICategory* ClassName::GetClassStatic() {                                                                 \
    return &sClass;                                                                                                                \
  }                                                                                                                                \
  ClassName::RTTITyped::RTTICategory* ClassName::GetClass() const {                                                                 \
    return GetClassStatic();                                                                                                       \
  }                                                                                                                                \
  INSTANTIATE_CASTABLE_SERIALIZE(ClassName)                                                                                        \
  INSTANTIATE_CASTABLE_SERIALIZE(const ClassName)                                                                                  \
  INSTANTIATE_LINK_FUNCTION(ClassName)

} // namespace ork::rtti

// For TransparentRTTI
//
// usage:
// --- MyClass.h ---
// class MyClass : public BaseClass
// {
// public:
//   static ConstString DesignNameStatic();
//   static void Describe();
//
// /// Here's the transparent RTTI Boilerplate
//   static rtti::Class *GetClassStatic() { return &sClass; }
//   /*virtual*/ rtti::Class *GetClass() const { return GetClassStatic(); }
// private:
//   static Class sClass;
// };
// --- MyClass.cpp ---
// IMPLEMENT_TRANSPARENT_RTTI(MyClass, BaseClass, "MyClass")
// void MyClass::Describe()
// {
//    reflect::RegisterProperty(...);
// }
