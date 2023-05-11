////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedMap.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename MapType> //
class DirectObjectMap       //
    : public ITypedMap<
          typename MapType::key_type, //
          object_ptr_t> {
public:
  using key_type        = typename MapType::key_type;
  using value_type      = typename MapType::value_type;
  using mapped_type     = typename MapType::mapped_type;
  using element_type    = typename mapped_type::element_type; 
  using mutable_element_type = typename std::remove_const<element_type>::type;
  using const_element_type = const mutable_element_type;

  DirectObjectMap(MapType Object::*);

  MapType& GetMap(object_ptr_t obj) const;
  const MapType& GetMap(object_constptr_t obj) const;

  bool isMultiMap(object_constptr_t obj) const override;

  MapType Object::*_member;

protected:
  bool ReadElement(object_constptr_t, const key_type&, int, object_ptr_t&) const override;
  bool WriteElement(object_ptr_t, const key_type&, int, const object_ptr_t*) const override;
  bool EraseElement(object_ptr_t, const key_type&, int) const override;

  size_t elementCount(object_constptr_t obj) const override {
    return GetMap(obj).size();
  }
  bool GetKey(object_constptr_t, int idx, key_type&) const override;
  bool GetVal(object_constptr_t, const key_type& k, object_ptr_t& v) const override;
  void insertDefaultElement(object_ptr_t obj,map_abstract_item_t key) const override;

private:
  //void deserialize(serdes::node_ptr_t) const override;
  //void serialize(serdes::node_ptr_t sernode) const override;
};

}} // namespace ork::reflect
