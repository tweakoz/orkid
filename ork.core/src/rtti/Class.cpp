////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

using class_map_t = std::unordered_map<std::string, Class*>;
using class_map_ptr_t = std::shared_ptr<class_map_t>;
static class_map_ptr_t getClassMap(){
  static class_map_ptr_t _gclassMap = std::make_shared<class_map_t>();
  return _gclassMap;
}

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
    //orkprintf("InitClass class<%p:%s> next<%p>\n", clazz, clazz->Name().c_str(), clazz->mNextClass);
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
      clazz->Initialize();
      //orkprintf("InitClass@2 class<%p:%s>\n", clazz, clazz->Name().c_str());
    }
  }
  //OrkAssert(false);
}

void Class::SetName(std::string name, bool badd2map) {
  OrkAssert(name.length());
  if (name.length()) {
    _classname = name;
    //printf( "Class<%p>::SetName<%s> add2map<%d> classmap<%p>\n", (void*) this, _classname.c_str(), int(badd2map), (void*) getClassMap().get() );

    if (badd2map) {
      auto it = getClassMap()->find(_classname);
      if (it != getClassMap()->end()) {
        if (it->second != this) {
          Class* previous = it->second;

          orkprintf("ERROR: Duplicate class name %s! previous class %p\n", _classname.c_str(), previous);
          OrkAssert(false);
        }
      } else {
        (*getClassMap())[_classname]=this;
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

void Class::AddChild(Class* clazz) {
  if (mChildClass) {
    clazz->mNextSiblingClass = mChildClass->mNextSiblingClass;
    clazz->mPrevSiblingClass = mChildClass;
    clazz->FixSiblingLinks();
  }

  mChildClass = clazz;
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

const std::string& Class::Name() const {
  return _classname;
}

Class* Class::FindClass(const std::string& name) {
  auto it = getClassMap()->find(name);
  Class* rval = nullptr;
  if (it != getClassMap()->end()) {
    rval = it->second;
  }
  //printf( "Class::FindClass<%s> clazz<%p> classmap<%p>\n", name.c_str(), rval, (void*) getClassMap().get() );
  return rval;

}

Class* Class::FindClassNoCase(const std::string& name) {
  std::string nocasename = name.c_str();
  std::transform(nocasename.begin(), nocasename.end(), nocasename.begin(), ::tolower);
  for (const auto& it : (*getClassMap())) {
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

void Class::CreateClassAlias(std::string name, Class* clazz) {

  auto it = getClassMap()->find(name);
  OrkAssert(it == getClassMap()->end());
  (*getClassMap())[name] = clazz;
}

Class* Class::sLastClass = NULL;

Category* Class::category() {
  static Category s_category(RTTI<Class, ICastable, NamePolicy, Category>::ClassRTTI());
  return &s_category;
}

Class* Class::GetClass() const {
  return Class::GetClassStatic();
}
std::string Class::DesignNameStatic() {
  return "Class";
}

}} // namespace ork::rtti
