////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/kernel/svariant.h>
#include <ork/kernel/string/string.h>
#include <string>
#include <map>

namespace ork::varmap {

typedef std::string key_t;

struct VarParamFloatCast{
  bool _was_set = false;
  float _value = 0.0f;
  operator bool () const { return _was_set; }
  float value() const { return _value; }
};

template <typename val_t> struct TVarMap {
  using value_type = val_t;
  using str_transformer_t = std::function<std::string(const val_t&)>;
  using str_transformer_map_t = std::unordered_map<ork::TypeId::hashtype_t, str_transformer_t>;
  using map_t = std::map<key_t, value_type>;
  ///////////////////////////////////////////////////////////////////////////
  static const val_t& nill() {
    static const val_t noval = nullptr;
    return noval;
  }
  ///////////////////////////////////////////////////////////////////////////
  inline bool hasKey(const key_t& key) const {
    auto it = _themap.find(key);
    return (it != _themap.end());
  }
  ///////////////////////////////////////////////////////////////////////////
  inline const val_t& valueForKey(const key_t& key) const {

    auto it = _themap.find(key);
    return (it == _themap.end()) ? nill() : it->second;
  }
  ///////////////////////////////////////////////////////////////////////////
  template <typename T> inline attempt_cast_const<T> typedValueForKey(const key_t& key) const {
    auto it = _themap.find(key);
    if (it != _themap.end()) {
      return it->second.template tryAs<T>();
    }
    return attempt_cast_const<T>(nullptr);
  }
  ///////////////////////////////////////////////////////////////////////////
  template <typename T> inline T& mergedValueForKey(const key_t& key) {
    auto& val_untyped = _themap[key];
    auto it = _themap.find(key);
    if (val_untyped.template isA<T>()) {
      return val_untyped.template get<T>();
    }
    return val_untyped.template make<T>();
  }
  ///////////////////////////////////////////////////////////////////////////
  template <typename T> inline attempt_cast<T> typedValueForKey(const key_t& key) {
    auto it = _themap.find(key);
    if (it != _themap.end()) {
      return it->second.template tryAs<T>();
    }
    return attempt_cast<T>(nullptr);
  }
  ///////////////////////////////////////////////////////////////////////////
  template <typename T, typename... A> inline T& makeValueForKey(const key_t& key, A&&... args) {
    return _themap[key].template make<T>(std::forward<A>(args)...);
  }
  ///////////////////////////////////////////////////////////////////////////
  template <typename T, typename... A> inline std::shared_ptr<T> makeSharedForKey(const key_t& key, A&&... args) {
    return _themap[key].template makeShared<T>(std::forward<A>(args)...);
  }
  ///////////////////////////////////////////////////////////////////////////
  template <typename T, typename... A> inline std::shared_ptr<T> getSharedForKey(const key_t& key) {
    return _themap[key].template getShared<T>();
  }
  ///////////////////////////////////////////////////////////////////////////
  template <typename T> inline void set(const key_t& key, const T& val) {
    _themap[key].template set<T>(val);
  }
  ///////////////////////////////////////////////////////////////////////////
  VarParamFloatCast tryKeyAsNumber(const std::string& key) {
        VarParamFloatCast rval;
        if( auto as_float = typedValueForKey<float>(key)){
          rval._value = as_float.value();
          rval._was_set = true;
        }
        else if( auto as_double = typedValueForKey<double>(key)){
          rval._value = as_double.value();
          rval._was_set = true;
        }
        else if( auto as_int = typedValueForKey<int>(key)){
          rval._value = float(as_int.value());
          rval._was_set = true;
        }
        return rval;
      };
  ///////////////////////////////////////////////////////////////////////////
  VarParamFloatCast tryKeysAsNumber(const std::string& key1, const std::string& key2) {
    VarParamFloatCast rval = tryKeyAsNumber(key1);
    if( rval._was_set == false ){
      rval = tryKeyAsNumber(key2);
    }
    return rval;
  };
  ///////////////////////////////////////////////////////////////////////////
  inline void removeItemForKey(const key_t& key) {
    auto it = _themap.find(key);
    OrkAssert(it != _themap.end());
    _themap.erase(it);
  }
  ///////////////////////////////////////////////////////////////////////////
  inline void clearKey(const key_t& key) {
    auto it = _themap.find(key);
    if (it != _themap.end())
      _themap.erase(it);
  }
  ///////////////////////////////////////////////////////////////////////////
  inline void setValueForKey(const key_t& key, const val_t& val) {
    _themap[key] = val;
  }
  ///////////////////////////////////////////////////////////////////////////
  inline val_t& operator[](const key_t& key) {
    return _themap[key];
  }
  ///////////////////////////////////////////////////////////////////////////
  inline const val_t& operator[](const key_t& key) const {
    return valueForKey(key);
  }
  ///////////////////////////////////////////////////////////////////////////
  uint64_t hash() const {
    boost::Crc64 crcgen;
    crcgen.init();
    for (const auto& item : _themap) {
      const auto& key = item.first;
      uint64_t item_h = item.second.hash();
      crcgen.accumulate((const void*)key.c_str(), key.length());
      crcgen.accumulate((const void*)item_h, sizeof(uint64_t));
    }
    crcgen.finish();
    return crcgen.result();
  }
  ///////////////////////////////////////////////////////////////////////////
  std::vector<std::string> dumpkeys() {
    std::vector<std::string> rval;
    for (const auto& item : _themap) {
      const auto& key = item.first;
      rval.push_back(key);
    }
    return rval;
  }
  ///////////////////////////////////////////////////////////////////////////
  void mergeVars(const TVarMap& oth_vars) {
    for (const auto& item : oth_vars._themap) {
      const auto& key = item.first;
      _themap[key] = item.second;
    }  using str_transformer_t = std::function<std::string(const val_t&)>;

  }
  ///////////////////////////////////////////////////////////////////////////
  static str_transformer_map_t& str_transformer_map() {
    static str_transformer_map_t the_map;
    return the_map;
  }
  ///////////////////////////////////////////////////////////////////////////
  std::string encodeAsString(const key_t& key) const {
    auto it = _themap.find(key);
    if (it == _themap.end()) {
      return "nil";
    }
    auto& val = it->second;
    // try primitives
    if (auto as_bool = val.template tryAs<bool>()) {
      return FormatString("bool<%d>", int(as_bool.value()));
    } else if (auto as_float = val.template tryAs<float>()) {
      return FormatString("float<%g>", as_float.value());
    } else if (auto as_double = val.template tryAs<double>()) {
      return FormatString("double<%g>", as_double.value());
    } else if (auto as_int = val.template tryAs<int>()) {
      return FormatString("int<%d>", as_int.value());
    } else if (auto as_uint64_t = val.template tryAs<uint64_t>()) {
      return FormatString("uint64_t<0x%zx>", as_uint64_t.value());
    } else if (auto as_str = val.template tryAs<std::string>()) {
      return FormatString("str<%s>", as_str.value().c_str());
    } else {
      auto orktypeid = val.getOrkTypeId();
      auto it = str_transformer_map().find(orktypeid._hashed);
      if (it != str_transformer_map().end()) {
        auto encoder = it->second;
        return encoder(val);
      }
      else{
        return FormatString("UNKNOWNTYPE<%s>", val.typeName() );
      }
    }
    return "";
  }
  ///////////////////////////////////////////////////////////////////////////
  static void registerStringEncoder(const ork::TypeId& orktypeid, str_transformer_t encoder) {
    str_transformer_map()[orktypeid._hashed] = encoder;
  }
  ///////////////////////////////////////////////////////////////////////////

  std::map<key_t, val_t> _themap;

};
using var_t             = svar128_t;
using VarMap            = TVarMap<var_t>;
using varmap_ptr_t      = std::shared_ptr<VarMap>;
using varmap_constptr_t = std::shared_ptr<const VarMap>;

} // namespace ork::varmap
