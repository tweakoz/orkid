////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedMap.h"
#include <ork/reflect/Command.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include "codec.inl"

namespace ork::reflect {
template <typename kt, typename vt> bool IsMultiMapDeducer(const std::map<kt, vt>& map) {
  return false;
}
template <typename kt, typename vt> bool IsMultiMapDeducer(const std::unordered_map<kt, vt>& map) {
  return false;
}

template <typename kt, typename vt> bool IsMultiMapDeducer(const std::multimap<kt, vt>& map) {
  return true;
}

template <typename kt, typename vt> bool IsMultiMapDeducer(const ork::orklut<kt, vt>& map) {
  return map.GetKeyPolicy() == ork::EKEYPOLICY_MULTILUT;
}
////////////////////////////////////////////////////////////////////////////////
// Helper: pack a typed value into a serialization node.
// For varmap::var_t (svar128_t), the value cannot be embedded inside
// another svar (sizeof(svar128_t) > ksize), so we encode it as a
// type-tagged string: "<type>:<data>".  All other types use set<T> directly.
////////////////////////////////////////////////////////////////////////////////
template<typename vt>
struct NODEENC {
  static void enc(serdes::node_ptr_t elemnode, const vt& value) {
    elemnode->_value.template set<vt>(value);
  }
};
template<>
struct NODEENC<varmap::var_t> {
  static void enc(serdes::node_ptr_t elemnode, const varmap::var_t& value) {
    std::string encoded;
    if      (value.isA<float>())       encoded = "float:"  + std::to_string(value.get<float>());
    else if (value.isA<int>())         encoded = "int:"    + std::to_string(value.get<int>());
    else if (value.isA<bool>())        encoded = std::string("bool:") + (value.get<bool>() ? "1" : "0");
    else if (value.isA<fvec3>()) {
      auto v = value.get<fvec3>();
      encoded = "fvec3:" + std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z);
    } else if (value.isA<fvec4>()) {
      auto v = value.get<fvec4>();
      encoded = "fvec4:" + std::to_string(v.x) + "," + std::to_string(v.y) + "," + std::to_string(v.z) + "," + std::to_string(v.w);
    } else if (value.isA<std::string>()) {
      encoded = "string:" + value.get<std::string>();
    } else {
      encoded = "null:";
    }
    elemnode->_value.set<std::string>(encoded);
  }
};
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType> //
void ITypedMap<KeyType, ValueType>::serialize(serdes::node_ptr_t sernode) const {
  auto serializer        = sernode->_serializer;
  auto instance          = sernode->_ser_instance;
  auto mapnode           = serializer->pushNode(_name, serdes::NodeType::MAP);
  mapnode->_parent       = sernode;
  mapnode->_ser_instance = instance;
  int numelements        = elementCount(instance);
  //printf( "map<%s> ser numelem<%d>\n", _name.c_str(), numelements );
  for (size_t i = 0; i < numelements; i++) {
    //////////////////////////////
    KeyType K;
    ValueType V;
    GetKey(instance, i, K);
    GetVal(instance, K, V);
    //////////////////////////////
    std::string keystr;
    serdes::encode_key(keystr, K);
    //////////////////////////////
    auto elemnode = serializer->pushNode(keystr, serdes::NodeType::MAP_ELEMENT_LEAF);
    //////////////////////////////
    elemnode->_key = keystr;
    NODEENC<ValueType>::enc(elemnode, V);
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
template <typename KeyType, typename ValueType> //
void ITypedMap<KeyType, ValueType>::deserialize(serdes::node_ptr_t dsernode) const {
  KeyType key;
  ValueType value;


  auto deserializer  = dsernode->_deserializer;
  size_t numelements = dsernode->_numchildren;
  auto instance      = dsernode->_deser_instance;
  for (size_t i = 0; i < numelements; i++) {
    dsernode->_index  = i;
    auto elemnode     = deserializer->pushNode("", serdes::NodeType::MAP_ELEMENT_LEAF);
    elemnode->_parent = dsernode;
    auto childnode    = deserializer->deserializeElement(elemnode);
    serdes::decode_key<KeyType>(childnode->_key, key);

    if constexpr (std::is_same_v<std::remove_cvref_t<KeyType>, std::string>) {
      if(0)printf( "  KEY<%s>\n", key.c_str() );
    }


    childnode->_name = childnode->_key;
    serdes::decode_value<ValueType>(childnode->_value, value);
    this->WriteElement(
        instance, //
        key,
        -1,
        &value);
    deserializer->popNode();
  }
}
////////////////////////////////////////////////////////////////////////////////
template  <typename KeyType, typename ValueType> struct PAIRSET {
  static void set(map_pair_t& the_pair, const KeyType& key, const ValueType& value) {
    the_pair.first.set<KeyType>(key);
    the_pair.second.set<ValueType>(value);
  }
};
// custom setter for var_t's so we can use them as values in maps
//   (since they can not be embedded in another var_t due to size contraints)
template<typename KeyType>
struct PAIRSET<KeyType, varmap::var_t> {
    static void set(map_pair_t& the_pair, const KeyType& key, const varmap::var_t& value) {
      the_pair.first.set<KeyType>(key);
      the_pair.second = value;
    }
};
////////////////////////////////////////////////////////////////////////////////
template <typename KeyType, typename ValueType> //
map_kvarray_t ITypedMap<KeyType, ValueType>::enumerateElements(object_constptr_t instance) const {
  map_kvarray_t rval;
  int numelements        = elementCount(instance);
  //printf( "map<%s> ser numelem<%d>\n", _name.c_str(), numelements );
  for (size_t i = 0; i < numelements; i++) {
    //////////////////////////////
    KeyType K;
    ValueType V;
    GetKey(instance, i, K);
    GetVal(instance, K, V);
    //////////////////////////////
    map_pair_t P;
    PAIRSET<KeyType, ValueType>::set(P, K, V);
    rval.push_back(P);
  }
  return rval;
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect
