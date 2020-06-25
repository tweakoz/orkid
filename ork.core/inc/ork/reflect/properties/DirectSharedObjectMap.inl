////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/properties/DirectSharedObjectMap.h>
#include <ork/kernel/core_interface.h>

namespace ork::reflect {
template <typename kt, typename vt> //
bool DSOM_IsMultiMapDeducer(const std::map<kt, vt>& map) {
  return false;
}

template <typename kt, typename vt> //
bool DSOM_IsMultiMapDeducer(const std::multimap<kt, vt>& map) {
  return true;
}

template <typename kt, typename vt> //
bool DSOM_IsMultiMapDeducer(const ork::orklut<kt, vt>& map) {
  return map.GetKeyPolicy() == ork::EKEYPOLICY_MULTILUT;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
DirectSharedObjectMap<MapType>::DirectSharedObjectMap(MapType Object::*prop) {
  mProperty = prop;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
MapType& DirectSharedObjectMap<MapType>::GetMap(Object* owner) const {
  return owner->*mProperty;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
const MapType& DirectSharedObjectMap<MapType>::GetMap(const Object* owner) const {
  return owner->*mProperty;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectSharedObjectMap<MapType>::IsMultiMap(const Object* owner) const {
  return DSOM_IsMultiMapDeducer(GetMap(owner));
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectSharedObjectMap<MapType>::ReadItem(
    const Object* owner, //
    const KeyType& key,
    int multi_index,
    object_ptr_t& value_out) const {
  const MapType& map                  = owner->*mProperty;
  typename MapType::const_iterator it = map.find(key);

  if (it == map.end())
    return false;

  while (multi_index > 0) {
    it++;
    multi_index--;
  }

  value_out = it->second;

  return true;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectSharedObjectMap<MapType>::WriteItem(
    Object* owner, //
    const KeyType& key,
    int multi_index,
    const object_ptr_t* value_inp) const {
  MapType& map               = owner->*mProperty;
  const int orig_multi_index = multi_index;
  if (multi_index == IMap::kDeserializeInsertItem) {
    OrkAssert(value_inp);
    map.insert(std::make_pair(key, *value_inp));
  } else {
    typename MapType::iterator it = map.find(key);
    while (multi_index > 0) {
      it++;
      multi_index--;
    }
    if (value_inp) {
      it->second = *value_inp;
    } else {
      typename DirectTypedMap<MapType>::ValueType val2erase = it->second;
      ItemRemovalEvent ev;
      ev.mProperty    = this;
      ev.miMultiIndex = orig_multi_index;
      ev.mKey.Set(key);
      ev.mOldValue.Set(val2erase);
      ork::Object* pevl = static_cast<ork::Object*>(owner);
      pevl->Notify(&ev);
      map.erase(it);
    }
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectSharedObjectMap<MapType>::EraseItem(
    Object* owner, //
    const KeyType& key,
    int multi_index) const {
  MapType& map                  = owner->*mProperty;
  typename MapType::iterator it = map.find(key);

  if (it != map.end()) {
    while (multi_index > 0) {
      it++;
      multi_index--;
    }

    OrkAssert(it != map.end());
    OrkAssert((*it).first == key);

    map.erase(it);
  }

  return true;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectSharedObjectMap<MapType>::MapSerialization(
    ItemSerializeFunction serialization_func, //
    BidirectionalSerializer& bidi,
    const Object* owner) const {

  if (bidi.Serializing()) {
    const MapType& map = owner->*mProperty;

    // const KeyType *last_key = NULL;

    typename MapType::const_iterator itprev;

    int imultiindex = 0;

    for (typename MapType::const_iterator it = map.begin(); it != map.end(); it++) {
      KeyType key = it->first;

      if (it != map.begin()) {
        const KeyType& ka = itprev->first;
        const KeyType& kb = it->first;

        if (ka == kb) {
          imultiindex++;
        } else {
          imultiindex = 0;
        }
      }

      itprev = it;

      ///////////////////////////////////////////////////
      // multi index hint
      ///////////////////////////////////////////////////

      bidi.Serializer()->Hint("MultiIndex", imultiindex);

      ///////////////////////////////////////////////////

      object_ptr_t value = it->second;

      (*serialization_func)(bidi, key, value);
    }
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectSharedObjectMap<MapType>::GetKey(
    const Object* owner, //
    int multi_index,
    KeyType& key_out) const {
  const MapType& map = owner->*mProperty;
  OrkAssert(multi_index < int(map.size()));
  typename MapType::const_iterator it = map.begin();
  for (int i = 0; i < multi_index; i++)
    it++;
  key_out = (*it).first;
  return true;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectSharedObjectMap<MapType>::GetVal(
    const Object* owner, //
    const KeyType& key,
    object_ptr_t& value_out) const {
  const MapType& map                  = owner->*mProperty;
  typename MapType::const_iterator it = map.find(key);
  if (it == map.end())
    return false;
  value_out = (*it).second;
  return true;
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
