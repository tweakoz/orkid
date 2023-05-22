////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "DirectEnum.h"
#include <ork/reflect/enum_serializer.inl>

////////////////////////////////////////////////

namespace ork::reflect {

template <typename T>
DirectEnum<T>::DirectEnum(T Object::*property)
    : _member(property) {
}

template <typename T> void DirectEnum<T>::get(T& value, object_constptr_t obj) const {
  value = obj.get()->*_member;
}

template <typename T> void DirectEnum<T>::set(const T& value, object_ptr_t obj) const {
  obj.get()->*_member = value;
}

template <typename T> void DirectEnum<T>::deserialize(serdes::node_ptr_t leaf_node) const {
  OrkAssert(false);
}
template <typename T> void DirectEnum<T>::serialize(serdes::node_ptr_t leaf_node) const {
  auto registrar = serdes::EnumRegistrar::instance();
  auto enumtype = registrar->findEnumClass<T>();
  OrkAssert(enumtype!=nullptr);
  auto instance   = leaf_node->_ser_instance;
  T e_val;
  get(e_val, instance);
  int int_val = static_cast<int>(e_val);
  auto it_s = enumtype->_int2strmap.find(int_val);
  serdes::enumvalue_ptr_t rewrite = std::make_shared<serdes::EnumValue>();
  rewrite->_name = it_s->second;
  rewrite->_value = it_s->first;
  auto serializer = leaf_node->_serializer;
  leaf_node->_value.template set<serdes::enumvalue_ptr_t>(rewrite);
  serializer->serializeLeaf(leaf_node);
}

template <typename T> enum_abs_array_t DirectEnum<T>::enumerateEnumerations(object_constptr_t obj) const{
  enum_abs_array_t rval;
  auto registrar = ::ork::reflect::serdes::EnumRegistrar::instance();
  auto enumtype  = registrar->findEnumClass<T>();
  OrkAssert(enumtype!=nullptr);
  for( auto item : enumtype->_str2intmap ) {
    auto valptr = std::make_shared<serdes::EnumValue>();
    valptr->_name = item.first;
    valptr->_value = item.second;
    rval.push_back( valptr );
  }
  return rval;
}

template <typename T> //
void DirectEnum<T>::setFromString( object_ptr_t obj, const std::string& str ) const {
  auto registrar = ::ork::reflect::serdes::EnumRegistrar::instance();
  auto enumtype  = registrar->findEnumClass<T>();
  OrkAssert(enumtype!=nullptr);
  auto item = enumtype->_str2intmap.find(str);
  int int_val = item->second;
  auto as_T = static_cast<T>(int_val);
  set( as_T, obj);
}
template <typename T> //
std::string DirectEnum<T>::toString( object_constptr_t obj ) const {
  auto registrar = ::ork::reflect::serdes::EnumRegistrar::instance();
  auto enumtype  = registrar->findEnumClass<T>();
  OrkAssert(enumtype!=nullptr);
  T e_val;
  get(e_val, obj);
  int int_val = static_cast<int>(e_val);
  auto it_s = enumtype->_int2strmap.find(int_val);
  return it_s->second;
}

} // namespace ork::reflect
