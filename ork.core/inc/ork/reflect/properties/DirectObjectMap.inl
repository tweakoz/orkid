////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once 

#include <ork/pch.h>

#include <ork/object/Object.h>
#include <ork/reflect/Command.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/IDeserializer.h>
#include "DirectObjectMap.h"
#include <ork/kernel/core_interface.h>
#include <ork/kernel/svariant_codec.inl>
#include "ITypedMap.hpp"

namespace ork::reflect {
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
  return IsMultiMapDeducer(GetMap(instance));
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
void DirectObjectMap<MapType>::insertDefaultElement(object_ptr_t obj,
                                                    map_abstract_item_t key) const {

  SvarDecoder<key.ksize> decoder;

  auto typed_key_attempt = decoder.decode<key_type>(key);
  OrkAssert(typed_key_attempt);
  MapType& the_map                  = obj.get()->*_member;
  the_map.insert(std::make_pair(typed_key_attempt.value(), nullptr));
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
void DirectObjectMap<MapType>::removeElement(object_ptr_t obj,
                                             map_abstract_item_t key) const {

  SvarDecoder<key.ksize> decoder;

  auto typed_key_attempt = decoder.decode<key_type>(key);
  OrkAssert(typed_key_attempt);
  MapType& the_map                  = obj.get()->*_member;
  auto it = the_map.find(typed_key_attempt.value());
  if(it!=the_map.end()){
    the_map.erase(it);
  }
}////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
void DirectObjectMap<MapType>::setElement(object_ptr_t obj, //
                                          map_abstract_item_t key, //
                                          map_abstract_item_t val) const { //

  SvarDecoder<key.ksize> decoder;

  auto typed_key_attempt = decoder.decode<key_type>(key);
  OrkAssert(typed_key_attempt);
  const auto& typed_key = typed_key_attempt.value();
  MapType& the_map                  = obj.get()->*_member;
  auto V = val.get<object_ptr_t>();
  auto TV = std::dynamic_pointer_cast<element_type>(V);
  auto P = std::make_pair(typed_key, TV);
  if( auto it = the_map.find(typed_key); it != the_map.end() )
    the_map.erase(it); // erase old value (if any
  the_map.insert(P);
  printf( "setElement val<%p>\n", (void*) V.get() );
  //ork::svar64_t key64;
  //OrkAssert( key64.canConvertFrom(key) );

}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectObjectMap<MapType>::ReadElement(
    object_constptr_t instance, //
    const key_type& key,
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

  auto nonconst_ptr_value = std::const_pointer_cast<mutable_element_type>(it->second);

  value_out = std::dynamic_pointer_cast<Object>(nonconst_ptr_value);

  return true;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectObjectMap<MapType>::WriteElement(
    object_ptr_t instance, //
    const key_type& key,
    int multi_index,
    const object_ptr_t* value_inp) const {
  MapType& map               = instance.get()->*_member;
  const int orig_multi_index = multi_index;
  if (multi_index == IMap::kDeserializeInsertElement) {
    OrkAssert(value_inp);
    auto typed_ptr_value = std::dynamic_pointer_cast<mutable_element_type>(*value_inp);
    map.insert(std::make_pair(key, typed_ptr_value));
  } else {
    auto it = map.find(key);
    while (multi_index > 0) {
      it++;
      multi_index--;
    }
    if (value_inp) {
      auto typed_ptr_value = std::dynamic_pointer_cast<mutable_element_type>(*value_inp);
      it->second           = typed_ptr_value;
    } else {
      auto val2erase = it->second;
      ItemRemovalEvent ev;
      ev.mProperty    = this;
      ev.miMultiIndex = orig_multi_index;
      ev.mKey.set(key);
      ev.mOldValue.set(val2erase);
      ork::object_ptr_t pevl = static_cast<ork::object_ptr_t>(instance);
      pevl->notify(&ev);
      map.erase(it);
    }
  }
  return true;
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
bool DirectObjectMap<MapType>::EraseElement(
    object_ptr_t instance, //
    const key_type& key,
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
bool DirectObjectMap<MapType>::GetKey(
    object_constptr_t instance, //
    int multi_index,
    key_type& key_out) const {
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
    const key_type& key,
    object_ptr_t& value_out) const {
  auto non_const                      = const_cast<Object*>(instance.get());
  const MapType& map                  = non_const->*_member;
  typename MapType::const_iterator it = map.find(key);
  if (it == map.end())
    return false;
  auto nonconst_ptr_value = std::const_pointer_cast<mutable_element_type>((*it).second);
  value_out = std::dynamic_pointer_cast<Object>(nonconst_ptr_value);
  return true;
}
////////////////////////////////////////////////////////////////////////////////
/*template <typename MapType> //
void DirectObjectMap<MapType>::serialize(serdes::node_ptr_t sernode) const {
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto mapnode           = serializer->pushNode(_name, serdes::NodeType::MAP);
  mapnode->_parent       = sernode;
  mapnode->_ser_instance = instance;
  int numelements        = elementCount(instance);
  for (size_t i = 0; i < numelements; i++) {
    //////////////////////////////
    key_type K;
    value_type V;
    GetKey(instance, i, K);
    GetVal(instance, K, V);
    //////////////////////////////
    std::string keystr;
    serdes::encode_key(keystr, K);
    //////////////////////////////
    auto elemnode = serializer->pushNode(keystr, serdes::NodeType::MAP_ELEMENT_LEAF);
    //////////////////////////////
    elemnode->_key = keystr;
    elemnode->_value.template set<value_type>(V);
    elemnode->_index        = i;
    elemnode->_parent       = mapnode;
    elemnode->_ser_instance = instance;
    elemnode->_serializer   = serializer;
    auto childnode          = serializer->serializeContainerElement(elemnode);
    //////////////////////////////
    serializer->popNode(); // pop elemnode
    //////////////////////////////
  }
  serializer->popNode(); // pop mapnode
}
////////////////////////////////////////////////////////////////////////////////
template <typename MapType> //
void DirectObjectMap<MapType>::deserialize(serdes::node_ptr_t dsernode) const {
  key_type key;
  value_type value;
  auto deserializer  = dsernode->_deserializer;
  size_t numelements = dsernode->_numchildren;
  auto instance      = dsernode->_deser_instance;
  for (size_t i = 0; i < numelements; i++) {
    dsernode->_index  = i;
    auto elemnode     = deserializer->pushNode("", serdes::NodeType::MAP_ELEMENT_LEAF);
    elemnode->_parent = dsernode;
    auto childnode    = deserializer->deserializeElement(elemnode);
    serdes::decode_key<key_type>(childnode->_key, key);
    childnode->_name = childnode->_key;
    serdes::decode_value<value_type>(childnode->_value, value);
    this->WriteElement(
        instance, //
        key,
        -1,
        &value);
    deserializer->popNode();
  }
}*/
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
