////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/ICastable.h>

#include <ork/kernel/string/ConstString.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/string/StringPool.h>
#include <ork/reflect/Description.h>

#include <ork/config/config.h>

namespace ork { namespace rtti {

class RTTIData;

class Category;

using shared_factory_t = std::function<rtti::castable_ptr_t()>;
using raw_factory_t    = rtti::ICastable* (*)();

class Class : public ICastable {
public:
  Class(const RTTIData&);

  static void InitializeClasses();

  Class* Parent();
  Class* FirstChild();
  Class* NextSibling();
  Class* PrevSibling();
  const Class* Parent() const;
  const PoolString& Name() const;
  void SetName(ConstString name, bool badd2map = true);

  void setRawFactory(raw_factory_t factory);
  void setSharedFactory(shared_factory_t factory);

  bool hasFactory() const {
    return hasRawFactory() or hasSharedFactory();
  }
  bool hasRawFactory() const {
    return (_rawFactory != nullptr);
  }
  bool hasSharedFactory() const {
    return (_sharedFactory != nullptr);
  }

  virtual void Initialize();

  static Class* FindClass(const ConstString& name);
  static Class* FindClassNoCase(const ConstString& name);

  static ConstString DesignNameStatic();
  static Category* category();
  /*virtual*/ Class* GetClass() const;

  template <typename ClassType> static void InitializeType() {
  }

  bool IsSubclassOf(const Class* other) const;
  const ICastable* Cast(const ICastable* other) const;
  ICastable* Cast(ICastable* other) const;

  static void CreateClassAlias(ConstString name, Class*);

  static inline void registerX(Class* clazz) {
    _explicitLinkClasses.insert(clazz);
  }

  bool _initialized = false;

protected:
  shared_factory_t _sharedFactory;
  raw_factory_t _rawFactory;

private:
  void AddChild(Class* pClass);
  void FixSiblingLinks();
  void RemoveFromHierarchy();

  void (*mClassInitializer)();

  Class* _parentClass;
  Class* mChildClass;
  Class* mNextSiblingClass;
  Class* mPrevSiblingClass;

  PoolString mClassName;

  Class* mNextClass;

  static Class* sLastClass;

  typedef orklut<PoolString, Class*> ClassMapType;
  static std::set<Class*> _explicitLinkClasses;
  static ClassMapType mClassMap;
};

}} // namespace ork::rtti
