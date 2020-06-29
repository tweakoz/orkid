////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "ITyped.h"
#include <ork/reflect/ISerializer.h>
#include <ork/reflect/BidirectionalSerializer.h>

#include <ork/stream/StringInputStream.h>
#include <ork/stream/StringOutputStream.h>
#include <ork/reflect/serialize/ShallowDeserializer.h>
#include <ork/reflect/serialize/ShallowSerializer.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include "codec.inl"

namespace ork { namespace reflect {

template <typename T> void ITyped<T>::deserialize(IDeserializer::node_ptr_t desernode) const {
  auto instance   = desernode->_instance;
  const auto& var = desernode->_value;
  T value;
  decode_value<T>(var, value);
  set(value, instance);
}

template <> //
inline void ITyped<int>::deserialize(IDeserializer::node_ptr_t desernode) const {
  auto instance   = desernode->_instance;
  const auto& var = desernode->_value;
  set(int(var.Get<double>()), instance);
}
template <> //
inline void ITyped<uint32_t>::deserialize(IDeserializer::node_ptr_t desernode) const {
  auto instance   = desernode->_instance;
  const auto& var = desernode->_value;
  set(uint32_t(var.Get<double>()), instance);
}
template <> //
inline void ITyped<uint64_t>::deserialize(IDeserializer::node_ptr_t desernode) const {
  auto instance   = desernode->_instance;
  const auto& var = desernode->_value;
  set(uint64_t(var.Get<double>()), instance);
}
template <> //
inline void ITyped<size_t>::deserialize(IDeserializer::node_ptr_t desernode) const {
  auto instance   = desernode->_instance;
  const auto& var = desernode->_value;
  set(size_t(var.Get<double>()), instance);
}

template <typename T> void ITyped<T>::serialize(ISerializer::node_ptr_t leafnode) const {
  auto serializer = leafnode->_serializer;
  auto instance   = leafnode->_instance;
  T value;
  get(value, instance);
  leafnode->_value.template Set<T>(value);
  serializer->serializeLeaf(leafnode);
}

}} // namespace ork::reflect
