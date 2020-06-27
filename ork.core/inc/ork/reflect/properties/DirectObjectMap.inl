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
#include "DirectObjectMap.h"
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
DirectObjectMap<MapType>::DirectObjectMap(MapType Object::*prop) {
  _member = prop;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
MapType& DirectObjectMap<MapType>::GetMap(object_ptr_t instance) const {
  return instance->*_member;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
const MapType& DirectObjectMap<MapType>::GetMap(object_constptr_t instance) const {
  auto non_const = const_cast<Object*>(instance.get());
  return non_const->*_member;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectObjectMap<MapType>::isMultiMap(object_constptr_t instance) const {
  return DSOM_IsMultiMapDeducer(GetMap(instance));
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectObjectMap<MapType>::ReadItem(
    object_constptr_t instance, //
    const KeyType& key,
    int multi_index,
    object_ptr_t& value_out) const {
  const MapType& map                  = instance.get()->*_member;
  typename MapType::const_iterator it = map.find(key);

  // printf("dsom read key<%s>\n", key.c_str());

  if (it == map.end()) {
    return false;
  }

  while (multi_index > 0) {
    it++;
    multi_index--;
  }

  value_out = it->second;

  return true;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectObjectMap<MapType>::WriteItem(
    object_ptr_t instance, //
    const KeyType& key,
    int multi_index,
    const object_ptr_t* value_inp) const {
  MapType& map               = instance.get()->*_member;
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
      ork::object_ptr_t pevl = static_cast<ork::object_ptr_t>(instance);
      pevl->Notify(&ev);
      map.erase(it);
    }
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectObjectMap<MapType>::EraseItem(
    object_ptr_t instance, //
    const KeyType& key,
    int multi_index) const {
  MapType& map                  = instance.get()->*_member;
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
bool DirectObjectMap<MapType>::MapSerialization(
    ItemSerializeFunction serialization_func, //
    BidirectionalSerializer& bidi,
    object_constptr_t instance) const {

  if (bidi.Serializing()) {
    const MapType& map = instance.get()->*_member;

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

      // printf("ser key<%s> val<%p>\n", key.c_str(), value.get());
      (*serialization_func)(bidi, key, value);
    }
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectObjectMap<MapType>::GetKey(
    object_constptr_t instance, //
    int multi_index,
    KeyType& key_out) const {
  auto non_const     = const_cast<Object*>(instance.get());
  const MapType& map = non_const->*_member;
  OrkAssert(multi_index < int(map.size()));
  typename MapType::const_iterator it = map.begin();
  for (int i = 0; i < multi_index; i++)
    it++;
  key_out = (*it).first;
  return true;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectObjectMap<MapType>::GetVal(
    object_constptr_t instance, //
    const KeyType& key,
    object_ptr_t& value_out) const {
  auto non_const                      = const_cast<Object*>(instance.get());
  const MapType& map                  = non_const->*_member;
  typename MapType::const_iterator it = map.find(key);
  if (it == map.end())
    return false;
  value_out = (*it).second;
  return true;
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
