////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/kernel/orklut.hpp>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

void PersistantMap::describeX(object::ObjectClass* clazz) {
  // ork::reflect::RegisterMapProperty( "CollapeState", & PersistantMap::_properties );
}
void PersistMapContainer::describeX(object::ObjectClass* clazz) {
  // ork::reflect::RegisterMapProperty( "Maps", & PersistMapContainer::_prop_persist_map );
}

///////////////////////////////////////////////////////////////////////////////

PersistantMap::PersistantMap() {
}
PersistantMap::~PersistantMap() {
}

///////////////////////////////////////////////////////////////////////////////

PersistHashContext::PersistHashContext() {
}

///////////////////////////////////////////////////////////////////////////////

uint32_t PersistHashContext::hash() const {
  uint32_t phash        = 0;
  uint32_t ohash        = 0;
  const char* classname = 0;
  if (_property) {
    auto clazz        = _object->objectClass();
    const auto& name  = clazz->Name();
    const char* pname = name.c_str();
    phash             = Crc32::HashMemory(pname, int(strlen(pname)));
  }
  if (_object) {
    auto clazz       = _object->GetClass();
    const auto& name = clazz->Name();
    ohash            = Crc32::HashMemory(name.c_str(), int(strlen(name.c_str())));
  }
  uint32_t key = phash ^ ohash;
  return key;
}

///////////////////////////////////////////////////////////////////////////////

const std::string& PersistantMap::value(const std::string& key) {
  auto it = _properties.find(key);
  if (it == _properties.end()) {
    _properties.AddSorted(key, "");
    it = _properties.find(key);
  }
  return it->second;
}

///////////////////////////////////////////////////////////////////////////////

void PersistantMap::setValue(const std::string& key, const std::string& val) {
  auto it = _properties.find(key);
  if (it == _properties.end())
    it = _properties.AddSorted(key, val);
  else
    it->second = val;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

PersistMapContainer::PersistMapContainer(){}
PersistMapContainer::~PersistMapContainer(){}
  ///////////////////////////////////////////////////////////////////////////////

  void PersistMapContainer::cloneFrom( const PersistMapContainer& oth ){
  	for( auto item : oth._prop_persist_map ) {
  		auto pclone = Object::clone(item.second);
  		auto typed_clone = std::dynamic_pointer_cast<PersistantMap>(pclone);
  		_prop_persist_map[item.first] = typed_clone;
  	}
  }

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged

ImplementReflectionX(ork::lev2::ged::PersistantMap, "GedPersistantMap");
ImplementReflectionX(ork::lev2::ged::PersistMapContainer, "GedPersistMapContainer");

////////////////////////////////////////////////////////////////

template class ork::orklut<int, ork::lev2::ged::persistantmap_ptr_t>;
