////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/string/StringPool.h>
#include <ork/rtti/Category.h>
#include <ork/rtti/Class.h>
#include <ork/rtti/RTTI.h>

#include <ork/kernel/orklut.hpp>

#include <ork/kernel/csystem.h>

#include <ork/application/application.h>

namespace ork {
template class orklut<PoolString, rtti::Class*>;
void TouchCoreClasses();
} // namespace ork

namespace ork { namespace rtti {

std::set<Class*> Class::_explicitLinkClasses;
static int counter = 0;

Class::Class(const RTTIData& rtti)
    : _sharedFactory(nullptr)
    , _rawFactory(nullptr)
    , mClassInitializer(rtti.ClassInitializer())
    , _parentClass(rtti.ParentClass())
    , mChildClass(NULL) 
    , mNextSiblingClass(this)
    , mPrevSiblingClass(this)
    , mNextClass(sLastClass) {

  assert(not(_parentClass == nullptr and counter == 2));

  sLastClass = this;
}

void Class::Initialize() {
  _initialized = true;
  if (_parentClass)
    _parentClass->AddChild(this);

  if (mClassInitializer != NULL) {
    (*mClassInitializer)();
  }
}

void Class::InitializeClasses() {
  TouchCoreClasses();
  counter++;
  std::set<Class*> _pendingclasses;
  for (Class* clazz = sLastClass; clazz != nullptr; clazz = clazz->mNextClass) {
    if (clazz)
      _pendingclasses.insert(clazz);
    // clazz->Initialize();
    // orkprintf("InitClass class<%p:%s> next<%p>\n", clazz, clazz->Name().c_str(), clazz->mNextClass);
  }
  sLastClass = NULL;
  for (auto clazz : _explicitLinkClasses) {
    if (clazz)
      _pendingclasses.insert(clazz);
  }

  for (auto itc : _pendingclasses) {
    auto clazz = itc;

    if (false == clazz->_initialized) {
      if (counter == 2) {
        //__asm__ volatile("int $0x03");
      }
      // orkprintf("InitClass class<%p:%s>\n", clazz, clazz->Name().c_str());
      clazz->Initialize();
    }
  }
}

void Class::SetName(ConstString name, bool badd2map) {
  if (name.length()) {
    mClassName = AddPooledString(name.c_str());

    if (badd2map) {
      ClassMapType::iterator it = mClassMap.find(mClassName);
      if (it != mClassMap.end()) {
        if (it->second != this) {
          Class* previous = it->second;

          orkprintf("ERROR: Duplicate class name %s! previous class %p\n", mClassName.c_str(), previous);
          OrkAssert(false);
        }
      } else {
        mClassMap.AddSorted(mClassName, this);
      }
    }
  }
}

void Class::setRawFactory(raw_factory_t factory) {
  _rawFactory = factory;
}
void Class::setSharedFactory(shared_factory_t factory) {
  _sharedFactory = factory;
}

Class* Class::Parent() {
  return _parentClass;
}

const Class* Class::Parent() const {
  return _parentClass;
}

Class* Class::FirstChild() {
  return mChildClass;
}

Class* Class::NextSibling() {
  return mNextSiblingClass;
}

Class* Class::PrevSibling() {
  return mPrevSiblingClass;
}

void Class::AddChild(Class* pClass) {
  if (mChildClass) {
    pClass->mNextSiblingClass = mChildClass->mNextSiblingClass;
    pClass->mPrevSiblingClass = mChildClass;
    pClass->FixSiblingLinks();
  }

  mChildClass = pClass;
}

void Class::FixSiblingLinks() {
  mNextSiblingClass->mPrevSiblingClass = this;
  mPrevSiblingClass->mNextSiblingClass = this;
}

void Class::RemoveFromHierarchy() {
  mNextSiblingClass->mPrevSiblingClass = mPrevSiblingClass;
  mPrevSiblingClass->mNextSiblingClass = mNextSiblingClass;

  if (_parentClass->mChildClass == this)
    _parentClass->mChildClass = mNextSiblingClass;

  if (_parentClass->mChildClass == this)
    _parentClass->mChildClass = NULL;

  mNextSiblingClass = mPrevSiblingClass = this;
}

const PoolString& Class::Name() const {
  return mClassName;
}

Class* Class::FindClass(const ConstString& name) {
  return OldStlSchoolFindValFromKey(mClassMap, FindPooledString(name.c_str()), NULL);
}

Class* Class::FindClassNoCase(const ConstString& name) {
  std::string nocasename = name.c_str();
  std::transform(nocasename.begin(), nocasename.end(), nocasename.begin(), ::tolower);
  for (const auto& it : mClassMap) {
    if (0 == strcasecmp(it.first.c_str(), nocasename.c_str()))
      return it.second;
  }
  return nullptr;
}

bool Class::IsSubclassOf(const Class* other) const {
  const Class* this_class = this;

  for (;;) {
    if (this_class == other)
      return true;
    if (this_class == NULL)
      return false;
    this_class = this_class->Parent();
  }
}

const ICastable* Class::Cast(const ICastable* other) const {
  if (NULL == other || other->GetClass()->IsSubclassOf(this))
    return other;
  return NULL;
}

ICastable* Class::Cast(ICastable* other) const {
  if (NULL == other || other->GetClass()->IsSubclassOf(this))
    return other;
  return NULL;
}

void Class::CreateClassAlias(ConstString name, Class* pclass) {
  PoolString ClassName = AddPooledString(name.c_str());

  ClassMapType::iterator it = mClassMap.find(ClassName);
  OrkAssert(it == mClassMap.end());
  mClassMap.AddSorted(ClassName, pclass);
}

Class* Class::sLastClass = NULL;
Class::ClassMapType Class::mClassMap;

Category* Class::category() {
  static Category s_category(RTTI<Class, ICastable, NamePolicy, Category>::ClassRTTI());
  return &s_category;
}

Class* Class::GetClass() const {
  return Class::GetClassStatic();
}
ConstString Class::DesignNameStatic() {
  return "Class";
}

}} // namespace ork::rtti
