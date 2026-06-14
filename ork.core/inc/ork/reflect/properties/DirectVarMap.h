////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedMap.hpp"
#include <ork/kernel/varmap.inl>
#include <ork/kernel/svariant_codec.inl>

// DirectVarMap — reflected-property wrapper for a `varmap::VarMap`
// instance member. ITypedMap<std::string, varmap::var_t> already has
// the generic serialize/deserialize logic plus the variant-as-
// tagged-string codec (NODEENC<var_t> in ITypedMap.hpp, decode_value
// <svar128_t> in codec.inl), so this class only has to wire the
// abstract per-element accessors through VarMap's `_themap` field.

namespace ork::reflect {

class DirectVarMap : public ITypedMap<std::string, varmap::var_t> {
public:
  using KeyType   = std::string;
  using ValueType = varmap::var_t;

  explicit DirectVarMap(varmap::varmap_ptr_t Object::*prop) : mProperty(prop) {}

  // Read-side accessor: returns the underlying shared_ptr by value —
  // may be null if the holder never initialized it. Read paths check
  // for null and treat null as an empty map.
  varmap::varmap_ptr_t GetVarMap(object_constptr_t obj) const {
    return obj.get()->*mProperty;
  }
  // Write-side accessor: lazy-allocates the shared_ptr on first call
  // so deserialization (which writes into the map before the holder
  // had a chance to init it) always has a live target. Returns the
  // (now-non-null) shared_ptr by value.
  varmap::varmap_ptr_t GetOrMakeVarMap(object_ptr_t obj) const {
    auto& slot = obj.get()->*mProperty;
    if (not slot) slot = std::make_shared<varmap::VarMap>();
    return slot;
  }

  bool isMultiMap(object_constptr_t /*obj*/) const final {
    return false;
  }
  bool isRawVariantMap() const final {
    return true;  // value is svar128_t (var_t)
  }

  size_t elementCount(object_constptr_t obj) const final {
    auto vmap = GetVarMap(obj);
    return vmap ? vmap->_themap.size() : 0;
  }

protected:
  bool GetKey(object_constptr_t obj, int idx, KeyType& out) const final {
    auto vmap = GetVarMap(obj);
    if (not vmap) return false;
    auto const& m = vmap->_themap;
    OrkAssert(idx >= 0 && size_t(idx) < m.size());
    auto it = m.begin();
    std::advance(it, idx);
    out = it->first;
    return true;
  }
  bool GetVal(object_constptr_t obj, const KeyType& k, ValueType& v) const final {
    auto vmap = GetVarMap(obj);
    if (not vmap) return false;
    auto const& m = vmap->_themap;
    auto it = m.find(k);
    if (it == m.end()) return false;
    v = it->second;
    return true;
  }
  bool ReadElement(
      object_constptr_t obj,
      const KeyType&    key,
      int               multi_index,
      ValueType&        value) const final {
    auto vmap = GetVarMap(obj);
    if (not vmap) return false;
    auto const& m = vmap->_themap;
    auto it = m.find(key);
    if (it == m.end()) return false;
    // VarMap is single-valued per key; multi_index is ignored.
    (void)multi_index;
    value = it->second;
    return true;
  }
  bool WriteElement(
      object_ptr_t       obj,
      const KeyType&     key,
      int                multi_index,
      const ValueType*   value) const final {
    auto vmap = GetOrMakeVarMap(obj);
    auto& m = vmap->_themap;
    if (multi_index == IMap::kDeserializeInsertElement) {
      OrkAssert(value);
      m.insert(std::make_pair(key, *value));
    } else {
      auto it = m.find(key);
      if (it == m.end()) return false;
      if (value) {
        it->second = *value;
      } else {
        m.erase(it);
      }
    }
    return true;
  }
  bool EraseElement(object_ptr_t obj, const KeyType& key, int /*multi_index*/) const final {
    auto vmap = GetVarMap(obj);
    if (not vmap) return false;
    auto& m = vmap->_themap;
    auto it = m.find(key);
    if (it == m.end()) return false;
    m.erase(it);
    return true;
  }

  // IMap "abstract" interface — for editor / generic-tools paths.
  void insertDefaultElement(object_ptr_t obj, map_abstract_item_t key) const final {
    SvarDecoder<key.ksize> decoder;
    auto k = decoder.template decode<KeyType>(key);
    OrkAssert(k);
    auto vmap = GetOrMakeVarMap(obj);
    vmap->_themap.insert(std::make_pair(k.value(), ValueType()));
  }
  void removeElement(object_ptr_t obj, map_abstract_item_t key) const final {
    SvarDecoder<key.ksize> decoder;
    auto k = decoder.template decode<KeyType>(key);
    OrkAssert(k);
    auto vmap = GetVarMap(obj);
    if (not vmap) return;
    auto& m = vmap->_themap;
    auto it = m.find(k.value());
    if (it != m.end()) m.erase(it);
  }
  void setElement(object_ptr_t /*obj*/,
                  map_abstract_item_t /*key*/,
                  map_abstract_item_t /*val*/) const final {
    // ValueType is svar128_t; can't embed inside the abstract item's
    // svar buffer. Use setRawVariantElement instead.
    OrkAssert(false);
  }
  void setRawVariantElement(object_ptr_t obj,
                            map_abstract_item_t key,
                            const svar128_t& raw_val) const final {
    SvarDecoder<key.ksize> decoder;
    auto k = decoder.template decode<KeyType>(key);
    OrkAssert(k);
    auto vmap = GetOrMakeVarMap(obj);
    auto& m = vmap->_themap;
    auto it = m.find(k.value());
    if (it != m.end()) m.erase(it);
    m.insert(std::make_pair(k.value(), raw_val));
  }

private:
  varmap::varmap_ptr_t Object::*mProperty;
};

} // namespace ork::reflect
