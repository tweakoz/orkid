////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#include <ork/kernel/opq.h>
#if 0

///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/editorsupport/objectmodel.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/IMap.h>
#include <ork/reflect/properties/IArray.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/properties/DirectTyped.h>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>

#include <ork/util/crc.h>
#include <queue>

INSTANTIATE_TRANSPARENT_RTTI(ork::reflect::editor::PersistantMap, "CorePersistantMap");
INSTANTIATE_TRANSPARENT_RTTI(ork::reflect::editor::PersistMapContainer, "CorePersistMapContainer");

template class ork::orklut<int, ork::reflect::editor::PersistantMap*>;

namespace ork::reflect::editor {

///////////////////////////////////////////////////////////////////////////////

void PersistMapContainer::Describe() {
  // ork::reflect::RegisterMapProperty("Maps", &PersistMapContainer::mPropPersistMap);
}

///////////////////////////////////////////////////////////////////////////////

PersistMapContainer::PersistMapContainer() {
}
PersistMapContainer::~PersistMapContainer() {
}

///////////////////////////////////////////////////////////////////////////////

/*void PersistMapContainer::CloneFrom(const PersistMapContainer& oth) {
  for (auto item : oth.mPropPersistMap) {
    ork::Object* pclone       = item.second->Clone();
    PersistantMap* pclone_map = rtti::autocast(pclone);
    mPropPersistMap.AddSorted(item.first, pclone_map);
  }
}*/

///////////////////////////////////////////////////////////////////////////////

void PersistantMap::Describe() {
  // ork::reflect::RegisterMapProperty("CollapeState", &PersistantMap::mProperties);
}

///////////////////////////////////////////////////////////////////////////////

PersistantMap::PersistantMap() {
}
PersistantMap::~PersistantMap() {
}

///////////////////////////////////////////////////////////////////////////////

PersistHashContext::PersistHashContext()
    : mObject(0)
    , mProperty(0)
    , mString(0) {
}

///////////////////////////////////////////////////////////////////////////////

int PersistHashContext::GenerateHash() const {
  uint32_t phash        = 0;
  uint32_t ohash        = 0;
  const char* classname = 0;
  if (mProperty) {
    phash = Crc32::HashMemory(
        mProperty->_name.c_str(), //
        mProperty->_name.length());
  }
  if (mObject) {
    auto pclass                 = mObject->GetClass();
    const ork::PoolString& name = pclass->Name();
    ohash                       = Crc32::HashMemory(name.c_str(), int(strlen(name.c_str())));
  }
  uint32_t key = phash ^ ohash;
  int ikey     = *reinterpret_cast<int*>(&key);
  return ikey;
}

///////////////////////////////////////////////////////////////////////////////

PersistantMap* ObjectModel::GetPersistMap(const PersistHashContext& Ctx) {
  PersistantMap* prval                           = 0;
  int key                                        = Ctx.GenerateHash();
  orklut<int, PersistantMap*>::const_iterator it = mPersistMapContainer.GetMap().find(key);
  if (it == mPersistMapContainer.GetMap().end()) {
    prval = new PersistantMap;
    mPersistMapContainer.GetMap().AddSorted(key, prval);
  } else
    prval = it->second;
  return prval;
}

///////////////////////////////////////////////////////////////////////////////

const std::string& PersistantMap::GetValue(const std::string& key) {
  orklut<std::string, std::string>::const_iterator it = mProperties.find(key);
  if (it == mProperties.end()) {
    mProperties.AddSorted(key, "");
    it = mProperties.find(key);
  }
  return it->second;
}

///////////////////////////////////////////////////////////////////////////////

void PersistantMap::SetValue(const std::string& key, const std::string& val) {
  orklut<std::string, std::string>::iterator it = mProperties.find(key);
  if (it == mProperties.end())
    it = mProperties.AddSorted(key, val);
  else
    it->second = val;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::editor
#endif