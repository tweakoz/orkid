////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "DirectTypedMap.h"
#include "ITypedMap.hpp"
#include <ork/kernel/core_interface.h>
#include <ork/kernel/svariant_codec.inl>

namespace ork { namespace reflect {

template <typename MapType> bool DirectTypedMap<MapType>::isMultiMap(object_constptr_t obj) const {
  return IsMultiMapDeducer(GetMap(obj));
}

template <typename MapType>
DirectTypedMap<MapType>::DirectTypedMap(MapType Object::*prop)
    : mProperty(prop) {
}

template <typename MapType> MapType& DirectTypedMap<MapType>::GetMap(object_ptr_t object) const {
  return object.get()->*mProperty;
}

template <typename MapType> const MapType& DirectTypedMap<MapType>::GetMap(object_constptr_t object) const {
  return object.get()->*mProperty;
}

////////////////////////////////////////////////////////////////////////////////
template <typename MapType>
void DirectTypedMap<MapType>::setRawVariantElement(
    object_ptr_t obj, map_abstract_item_t key, const svar128_t& raw_val) const {
  SvarDecoder<key.ksize> decoder;
  auto typed_key_attempt = decoder.template decode<KeyType>(key);
  OrkAssert(typed_key_attempt);
  MapType& the_map = obj.get()->*mProperty;
  if constexpr (std::is_same_v<ValueType, svar128_t>) {
    auto it = the_map.find(typed_key_attempt.value());
    if (it != the_map.end())
      the_map.erase(it);
    the_map.insert(std::make_pair(typed_key_attempt.value(), raw_val));
  } else {
    OrkAssert(false); // ValueType is not svar128_t; use setElement() instead
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
void DirectTypedMap<MapType>::insertDefaultElement(object_ptr_t obj,
                                                    map_abstract_item_t key) const {
  SvarDecoder<key.ksize> decoder;
  auto typed_key_attempt = decoder.template decode<KeyType>(key);
  OrkAssert(typed_key_attempt);
  MapType& the_map = obj.get()->*mProperty;
  the_map.insert(std::make_pair(typed_key_attempt.value(), ValueType()));
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
void DirectTypedMap<MapType>::removeElement(object_ptr_t obj,
                                            map_abstract_item_t key) const {
  SvarDecoder<key.ksize> decoder;
  auto typed_key_attempt = decoder.template decode<KeyType>(key);
  OrkAssert(typed_key_attempt);
  MapType& the_map = obj.get()->*mProperty;
  auto it = the_map.find(typed_key_attempt.value());
  if (it != the_map.end()) {
    the_map.erase(it);
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
void DirectTypedMap<MapType>::setElement(object_ptr_t obj, //
                                         map_abstract_item_t key, //
                                         map_abstract_item_t val) const { //
  SvarDecoder<key.ksize> key_decoder;
  auto typed_key_attempt = key_decoder.template decode<KeyType>(key);
  OrkAssert(typed_key_attempt);
  MapType& the_map = obj.get()->*mProperty;
  if constexpr (sizeof(ValueType) <= val.ksize) {
    SvarDecoder<val.ksize> val_decoder;
    auto typed_val_attempt = val_decoder.template decode<ValueType>(val);
    OrkAssert(typed_val_attempt);
    auto it = the_map.find(typed_key_attempt.value());
    if (it != the_map.end())
      the_map.erase(it);
    the_map.insert(std::make_pair(typed_key_attempt.value(), typed_val_attempt.value()));
  } else {
    OrkAssert(false); // ValueType too large for abstract map interface
  }
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType>
bool DirectTypedMap<MapType>::EraseElement(
    object_ptr_t object, //
    const typename DirectTypedMap<MapType>::KeyType& key,
    int multi_index) const {
  MapType& map                  = object.get()->*mProperty;
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
bool DirectTypedMap<MapType>::ReadElement(
    object_constptr_t object,
    const typename DirectTypedMap<MapType>::KeyType& key,
    int multi_index,
    typename DirectTypedMap<MapType>::ValueType& value) const {
  const MapType& map                  = object.get()->*mProperty;
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
bool DirectTypedMap<MapType>::WriteElement(
    object_ptr_t object,
    const typename DirectTypedMap<MapType>::KeyType& key,
    int multi_index,
    const typename DirectTypedMap<MapType>::ValueType* value) const {
  MapType& map               = object.get()->*mProperty;
  const int orig_multi_index = multi_index;
  if (multi_index == IMap::kDeserializeInsertElement) {
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
      ev.mKey.set(key);
      ev.mOldValue.set(val2erase);
      object->notify(&ev);
      map.erase(it);
    }
  }
  return true;
}

template <typename MapType>
bool DirectTypedMap<MapType>::GetKey(
    object_constptr_t pser, //
    int idx,
    KeyType& kt) const {
  const MapType& map = pser.get()->*mProperty;
  OrkAssert(idx < int(map.size()));
  typename MapType::const_iterator it = map.begin();
  for (int i = 0; i < idx; i++)
    it++;
  kt = (*it).first;
  return true;
}
template <typename MapType>
bool DirectTypedMap<MapType>::GetVal(
    object_constptr_t pser, //
    const KeyType& k,
    ValueType& v) const {
  const MapType& map                  = pser.get()->*mProperty;
  typename MapType::const_iterator it = map.find(k);
  if (it == map.end())
    return false;
  v = (*it).second;
  return true;
}

}} // namespace ork::reflect
