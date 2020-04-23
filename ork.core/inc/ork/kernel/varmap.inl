#pragma once
#include <ork/kernel/svariant.h>
#include <string>
#include <map>

namespace ork::varmap {

typedef svar64_t val_t;
typedef std::string key_t;

struct VarMap {
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
      return it->second.TryAs<T>();
    }
    return attempt_cast_const<T>(nullptr);
  }
  ///////////////////////////////////////////////////////////////////////////
  template <typename T> inline attempt_cast<T> typedValueForKey(const key_t& key) {
    auto it = _themap.find(key);
    if (it != _themap.end()) {
      return it->second.TryAs<T>();
    }
    return attempt_cast<T>(nullptr);
  }
  ///////////////////////////////////////////////////////////////////////////
  template <typename T, typename... A> inline T& makeValueForKey(const key_t& key, A&&... args) {
    return _themap[key].Make<T>(std::forward(args)...);
  }
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

  std::map<key_t, val_t> _themap;
};

} // namespace ork::varmap
