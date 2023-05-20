////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/ICastable.h>

#include <ork/kernel/string/ConstString.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/string/StringPool.h>
#include <ork/reflect/Description.h>

#include <ork/config/config.h>

namespace ork { namespace rtti {

struct RTTIData;

class Category;

using shared_factory_t = std::function<rtti::castable_ptr_t()>;
using raw_factory_t    = rtti::ICastable* (*)();

struct Class : public ICastable {
  public:
  using name_t = std::string;
  Class(const RTTIData&);

  static void InitializeClasses();

  Class* Parent();
  Class* FirstChild();
  Class* NextSibling();
  Class* PrevSibling();
  const Class* Parent() const;
  const name_t& Name() const;
  void SetName(name_t name, bool badd2map = true);

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

  static Class* FindClass(const name_t& name);
  static Class* FindClassNoCase(const name_t& name);

  static name_t DesignNameStatic();
  static Category* category();
  /*virtual*/ Class* GetClass() const;

  template <typename ClassType> static void InitializeType() {
  }

  void visitUpHierarchy(std::function<bool(const Class*)> visitor) const {
    bool finished = visitor(this);
    if((not finished) and _parentClass){
      _parentClass->visitUpHierarchy(visitor);
    }
  }

  bool IsSubclassOf(const Class* other) const;
  const ICastable* Cast(const ICastable* other) const;
  ICastable* Cast(ICastable* other) const;

  static void CreateClassAlias(name_t name, Class*);

  static inline void registerX(Class* clazz) {
    _explicitLinkClasses.insert(clazz);
  }

  bool _initialized = false;

  virtual void make_abstract() = 0;

  shared_factory_t sharedFactory() { return _sharedFactory; }
  
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

  name_t _classname;

  Class* mNextClass;

  static Class* sLastClass;

  using ClassMapType = orklut<name_t, Class*>;
  static std::set<Class*> _explicitLinkClasses;
};

}} // namespace ork::rtti
