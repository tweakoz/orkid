////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "RTTI.h"

namespace ork::rtti {
////////////////////////////////////////////////////////////////////////////////
// NewStyle RTTI macros
////////////////////////////////////////////////////////////////////////////////

#define DeclareConcreteX(ClassType, BaseType)                                                                                      \
public:                                                                                                                            \
  typedef ::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::DefaultPolicy, BaseType::RTTIType::RTTICategory> RTTIType;           \
  typedef RTTIType::RTTICategory class_t;                                                                                          \
  static void describeX(class_t* clazz);                                                                                           \
  static void Describe();                                                                                                          \
  static ::ork::ConstString DesignNameStatic();                                                                                    \
  static class_t* GetClassStatic();                                                                                                \
  class_t* GetClass() const override;                                                                                              \
                                                                                                                                   \
private:

////////////////

#define DeclareAbstractX(ClassType, BaseType)                                                                                      \
public:                                                                                                                            \
  typedef ::ork::rtti::RTTI<ClassType, BaseType, ::ork::rtti::AbstractPolicy, BaseType::RTTIType::RTTICategory> RTTIType;          \
  typedef RTTIType::RTTICategory class_t;                                                                                          \
  static void describeX(class_t* clazz);                                                                                           \
  static void Describe();                                                                                                          \
  static ::ork::ConstString DesignNameStatic();                                                                                    \
  static class_t* GetClassStatic();                                                                                                \
  class_t* GetClass() const override;                                                                                              \
                                                                                                                                   \
private:

////////////////

#define DeclareExplicitX(ClassType, BaseType, Policy, ClassImplementation)                                                         \
public:                                                                                                                            \
  typedef ::ork::rtti::RTTI<ClassType, BaseType, Policy, ClassImplementation> RTTIType;                                            \
  typedef RTTIType::RTTICategory class_t;                                                                                          \
  static void describeX(class_t* clazz);                                                                                           \
  static void Describe();                                                                                                          \
  static ::ork::ConstString DesignNameStatic();                                                                                    \
  static class_t* GetClassStatic();                                                                                                \
  class_t* GetClass() const override;                                                                                              \
                                                                                                                                   \
private:

////////////////

#define ImplementReflectionX(ClassName, TheDesignName)                                                                             \
  ::ork::ConstString ClassName::DesignNameStatic() {                                                                               \
    return TheDesignName;                                                                                                          \
  }                                                                                                                                \
  void ClassName::Describe() {                                                                                                     \
    describeX(GetClassStatic());                                                                                                   \
  }                                                                                                                                \
  ClassName::class_t* ClassName::GetClassStatic() {                                                                                \
    static ClassName::class_t _clazz(ClassName::RTTIType::ClassRTTI());                                                            \
    return &_clazz;                                                                                                                \
  }                                                                                                                                \
  ClassName::class_t* ClassName::GetClass() const {                                                                                \
    return GetClassStatic();                                                                                                       \
  }                                                                                                                                \
  INSTANTIATE_CASTABLE_SERIALIZE(ClassName)                                                                                        \
  INSTANTIATE_CASTABLE_SERIALIZE(const ClassName)                                                                                  \
  INSTANTIATE_LINK_FUNCTION(ClassName)

////////////////

#define RegisterClassX(ClassName) ::ork::rtti::Class::registerX(ClassName::GetClassStatic());

} // namespace ork::rtti
