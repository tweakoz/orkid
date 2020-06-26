////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "DirectTypedMap.h"
#include "ITypedMap.hpp"
#include <ork/kernel/core_interface.h>

namespace ork { namespace reflect {

template <typename kt, typename vt> bool IsMultiMapDeducer(const std::map<kt, vt>& map) {
  return false;
}

template <typename kt, typename vt> bool IsMultiMapDeducer(const std::multimap<kt, vt>& map) {
  return true;
}

template <typename kt, typename vt> bool IsMultiMapDeducer(const ork::orklut<kt, vt>& map) {
  return map.GetKeyPolicy() == ork::EKEYPOLICY_MULTILUT;
}

template <typename MapType> bool DirectTypedMap<MapType>::isMultiMap(const Object* obj) const {
  return IsMultiMapDeducer(GetMap(obj));
}

template <typename MapType>
DirectTypedMap<MapType>::DirectTypedMap(MapType Object::*prop)
    : mProperty(prop) {
}

template <typename MapType> MapType& DirectTypedMap<MapType>::GetMap(Object* object) const {
  return object->*mProperty;
}

template <typename MapType> const MapType& DirectTypedMap<MapType>::GetMap(const Object* object) const {
  return object->*mProperty;
}

template <typename MapType>
bool DirectTypedMap<MapType>::EraseItem(Object* object, const typename DirectTypedMap<MapType>::KeyType& key, int multi_index)
    const {
  MapType& map                  = object->*mProperty;
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

template <typename MapType>
bool DirectTypedMap<MapType>::ReadItem(
    const Object* object,
    const typename DirectTypedMap<MapType>::KeyType& key,
    int multi_index,
    typename DirectTypedMap<MapType>::ValueType& value) const {
  const MapType& map                  = object->*mProperty;
  typename MapType::const_iterator it = map.find(key);

  if (it == map.end())
    return false;

  while (multi_index > 0) {
    it++;
    multi_index--;
  }

  value = it->second;

  return true;
}

template <typename MapType>
bool DirectTypedMap<MapType>::WriteItem(
    Object* object,
    const typename DirectTypedMap<MapType>::KeyType& key,
    int multi_index,
    const typename DirectTypedMap<MapType>::ValueType* value) const {
  MapType& map               = object->*mProperty;
  const int orig_multi_index = multi_index;
  if (multi_index == IMap::kDeserializeInsertItem) {
    OrkAssert(value);
    map.insert(std::make_pair(key, *value));
  } else {
    typename MapType::iterator it = map.find(key);
    while (multi_index > 0) {
      it++;
      multi_index--;
    }
    if (value) {
      it->second = *value;
    } else {
      typename DirectTypedMap<MapType>::ValueType val2erase = it->second;
      ItemRemovalEvent ev;
      ev.mProperty    = this;
      ev.miMultiIndex = orig_multi_index;
      ev.mKey.Set(key);
      ev.mOldValue.Set(val2erase);
      ork::Object* pevl = static_cast<ork::Object*>(object);
      pevl->Notify(&ev);
      map.erase(it);
    }
  }
  return true;
}

template <typename MapType>
bool DirectTypedMap<MapType>::MapSerialization(
    typename DirectTypedMap<MapType>::ItemSerializeFunction serialization_func,
    BidirectionalSerializer& bidi,
    const Object* serialize_object) const {

  if (bidi.Serializing()) {
    const MapType& map = serialize_object->*mProperty;

    size_t count = map.size();
    bidi.Serializer()->Hint("Count", count);

    // const KeyType *last_key = NULL;

    typename MapType::const_iterator itprev;

    int item_index      = 0;
    int item_multiindex = 0;

    for (auto it = map.begin(); //
         it != map.end();
         it++) {

      KeyType key = it->first;

      //////////////////////////////////////////
      // keep track of multimap
      //  consecutive items with same key
      //////////////////////////////////////////

      if (it != map.begin()) {

        const KeyType& ka = itprev->first;
        const KeyType& kb = it->first;

        if (ka == kb) {
          item_multiindex++;
        } else {
          item_multiindex = 0;
        }
      }

      itprev = it;

      ///////////////////////////////////////////////////
      // index hints
      ///////////////////////////////////////////////////

      bidi.Serializer()->Hint("Index", item_index);
      bidi.Serializer()->Hint("MultiIndex", item_multiindex);

      ///////////////////////////////////////////////////
      // serialize the item
      ///////////////////////////////////////////////////

      ValueType value = it->second;

      (*serialization_func)(bidi, key, value);

      item_index++;
    }
  }

  return true;
}

template <typename MapType> bool DirectTypedMap<MapType>::GetKey(const Object* pser, int idx, KeyType& kt) const {
  const MapType& map = pser->*mProperty;
  OrkAssert(idx < int(map.size()));
  typename MapType::const_iterator it = map.begin();
  for (int i = 0; i < idx; i++)
    it++;
  kt = (*it).first;
  return true;
}
template <typename MapType> bool DirectTypedMap<MapType>::GetVal(const Object* pser, const KeyType& k, ValueType& v) const {
  const MapType& map                  = pser->*mProperty;
  typename MapType::const_iterator it = map.find(k);
  if (it == map.end())
    return false;
  v = (*it).second;
  return true;
}

}} // namespace ork::reflect
