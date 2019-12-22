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
      return ( it != _themap.end() );
    }
    ///////////////////////////////////////////////////////////////////////////
    inline const val_t& valueForKey(const key_t& key) const {

        auto it = _themap.find(key);
        return (it==_themap.end()) ? nill() : it->second;

    }
    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    inline attempt_cast_const<T> typedValueForKey(const key_t& key) const {
      auto it = _themap.find(key);
      if( it != _themap.end() ){
        return it->second.TryAs<T>();
      }
      return attempt_cast_const<T>(nullptr);
    }
    ///////////////////////////////////////////////////////////////////////////
    template <typename T>
    inline attempt_cast<T> typedValueForKey(const key_t& key) {
      auto it = _themap.find(key);
      if( it != _themap.end() ){
        return it->second.TryAs<T>();
      }
      return attempt_cast<T>(nullptr);
    }
    ///////////////////////////////////////////////////////////////////////////
    template <typename T, typename... A>
    inline T& makeValueForKey(const key_t& key, A&&... args) {
      return _themap[key].Make<T>(std::forward(args)...);
    }
    ///////////////////////////////////////////////////////////////////////////
    inline const void setValueForKey(const key_t& key, const val_t& val) {
      _themap[key] = val;
    }
    ///////////////////////////////////////////////////////////////////////////
    inline val_t& operator [] (const key_t& key) {
      return _themap[key];
    }
    ///////////////////////////////////////////////////////////////////////////
    inline const val_t& operator [] (const key_t& key) const {
      return valueForKey(key);
    }
    ///////////////////////////////////////////////////////////////////////////

    std::map<key_t,val_t> _themap;
  };


}
