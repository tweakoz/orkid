////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "RTTI.h"
#include <ork/object/ObjectClass.h>

namespace ork::object {
class ObjectClass;
}
namespace ork::rtti {
// clang-format off
////////////////////////////////////////////////////////////////////////////////
// NewStyle RTTI macros
////////////////////////////////////////////////////////////////////////////////

#define DeclareTemplateConcreteX(ClassType, BaseType)           \
public:                                                         \
  using RTTITyped =                                             \
      typename ::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::DefaultPolicy, BaseType::RTTITyped::RTTICategory>; \
  using class_t = typename RTTITyped::RTTICategory;             \
  static void describeX(class_t* clazz);                        \
  static void Describe();                                       \
  static ::ork::ConstString DesignNameStatic();                 \
  static class_t* GetClassStatic();                             \
  static object::class_ptr_t objectClassStatic();               \
  class_t* GetClass() const override;                           \
                                                                \
private:

#define DeclareConcreteX(ClassType, BaseType)                   \
public:                                                         \
  typedef ::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::DefaultPolicy, BaseType::RTTITyped::RTTICategory> RTTITyped;\
  typedef RTTITyped::RTTICategory class_t;                      \
  static void describeX(class_t* clazz);                        \
  static void Describe();                                       \
  static ::ork::ConstString DesignNameStatic();                 \
  static class_t* GetClassStatic();                             \
  static object::class_ptr_t objectClassStatic();               \
  class_t* GetClass() const override;                           \
                                                                \

////////////////

#define DeclareAbstractX(ClassType, BaseType)                   \
public:                                                         \
  typedef ::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::AbstractPolicy, BaseType::RTTITyped::RTTICategory> RTTITyped; \
  typedef RTTITyped::RTTICategory class_t;                      \
  static void describeX(class_t* clazz);                        \
  static void Describe();                                       \
  static ::ork::ConstString DesignNameStatic();                 \
  static class_t* GetClassStatic();                             \
  static object::class_ptr_t objectClassStatic();               \
  class_t* GetClass() const override;                           \
                                                                \

////////////////

#define DeclareTemplateAbstractX(ClassType, BaseType)           \
public:                                                         \
  using RTTITyped =                                             \
      typename ::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::AbstractPolicy, BaseType::RTTITyped::RTTICategory>;  \
  using class_t = typename RTTITyped::RTTICategory;             \
  static void describeX(class_t* clazz);                        \
  static void Describe();                                       \
  static ::ork::ConstString DesignNameStatic();                 \
  static class_t* GetClassStatic();                             \
  static object::class_ptr_t objectClassStatic();               \
  class_t* GetClass() const override;                           \
                                                                \
private:

////////////////

#define DeclareExplicitX(ClassType, BaseType, Policy, ClassImplementation)      \
public:                                                         \
  typedef ::ork::rtti::RTTI<ClassType, BaseType, Policy, ClassImplementation> RTTITyped;\
  typedef RTTITyped::RTTICategory class_t;                      \
  static void describeX(class_t* clazz);                        \
  static void Describe();                                       \
  static ::ork::ConstString DesignNameStatic();                 \
  static class_t* GetClassStatic();                             \
  static object::class_ptr_t objectClassStatic();               \
  class_t* GetClass() const override;                           \
                                                                \
private:

////////////////

#define ImplementReflectionX(ClassName, TheDesignName)          \
  ::ork::ConstString ClassName::DesignNameStatic() {            \
    return TheDesignName;                                       \
  }                                                             \
  void ClassName::Describe() {                                  \
    describeX(GetClassStatic());                                \
  }                                                             \
  ClassName::class_t* ClassName::GetClassStatic() {             \
    static ClassName::class_t _clazz(ClassName::RTTITyped::ClassRTTI()); \
    return &_clazz;                                             \
  }                                                             \
  ClassName::class_t* ClassName::GetClass() const {             \
    return GetClassStatic();                                    \
  }                                                             \
  ::ork::object::class_ptr_t ClassName::objectClassStatic() {    \
    return dynamic_cast<::ork::object::class_ptr_t>(GetClassStatic()); \
  }                                                             \
  INSTANTIATE_CASTABLE_SERIALIZE(ClassName)                     \
  INSTANTIATE_CASTABLE_SERIALIZE(const ClassName)               \
  INSTANTIATE_LINK_FUNCTION(ClassName)

////////////////

#define ImplementTemplateReflectionX(ClassName, TheDesignName) \
  template <> \
  ClassName::class_t* ClassName::GetClassStatic() { \
    static ClassName::class_t _clazz(ClassName::RTTITyped::ClassRTTI());\
    return &_clazz;                                             \
  }                                                              \
  template <> \
  ::ork::object::class_ptr_t ClassName::objectClassStatic() {    \
    return dynamic_cast<::ork::object::class_ptr_t>(GetClassStatic()); \
  }                                                             \
  template <> \
  ::ork::ConstString ClassName::DesignNameStatic() { \
    return TheDesignName;                                       \
  }                                                             \
  template <> \
  void ClassName::Describe() {                      \
    describeX(GetClassStatic());                                \
  }                                                             \
  template <> \
  ClassName::class_t* ClassName::GetClass() const { \
    return GetClassStatic();                                    \
  }                                                             \
  INSTANTIATE_CASTABLE_SERIALIZE(ClassName)                     \
  INSTANTIATE_CASTABLE_SERIALIZE(const ClassName)               \
  INSTANTIATE_LINK_FUNCTION(ClassName)

////////////////

#define RegisterClassX(ClassName) ::ork::rtti::Class::registerX(ClassName::GetClassStatic());
// clang-format on

} // namespace ork::rtti
