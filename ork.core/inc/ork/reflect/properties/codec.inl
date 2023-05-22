////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#pragma once

#include <ork/reflect/properties/codec.h>
#include <ork/math/cvector4.h>

namespace ork{
  class PoolString;
}
namespace ork::file{
  class Path;
}
namespace ork::rtti{
  struct Class;
}
namespace ork::object{
  struct ObjectClass;
}

namespace ork::reflect::serdes {
////////////////////////////////////////////////////////////////////////////////
// forward declare specializations
////////////////////////////////////////////////////////////////////////////////
template <> void decode_key(std::string keystr, int& key_out);
template <> void decode_key(std::string keystr, float& key_out);
template <> void decode_key(std::string keystr, double& key_out);
template <> void decode_key(std::string keystr, std::string& key_out);
template <> void decode_key(std::string keystr, file::Path& key_out);
template <> void decode_key(std::string keystr, PoolString& key_out);
template <> void decode_key(std::string keystr, object::ObjectClass*& key_out);
//
template <> void decode_value(var_t val_inp, int& val_out);
template <> void decode_value(var_t val_inp, uint_t& val_out);
template <> void decode_value(var_t val_inp, ulong_t& val_out);
template <> void decode_value(var_t val_inp, float& val_out);
template <> void decode_value(var_t val_inp, file::Path& val_out);
template <> void decode_value(var_t val_inp, PoolString& val_out);
template <> void decode_value(var_t val_inp, rtti::Class*& val_out);
template <> void decode_value(var_t val_inp, svar64_t& val_out);
//
template <>void encode_key(std::string& keystr_out, const int& key_inp);
template <>void encode_key(std::string& keystr_out, const float& key_inp);
template <>void encode_key(std::string& keystr_out, const double& key_inp);
template <>void encode_key(std::string& keystr_out, const PoolString& key_inp);
template <>void encode_key(std::string& keystr_out, const std::string& key_inp);
template <>void encode_key(std::string& keystr_out, rtti::Class* const& key_inp);
template <>void encode_key(std::string& keystr_out, object::ObjectClass* const& key_inp);

////////////////////////////////////////////////////////////////////////////////

template <> //
inline void decode_value(var_t val_inp, fvec4& val_out) {
  if( auto as_fvec4 = val_inp.tryAs<fvec4>() ){
    val_out = as_fvec4.value();
  }
  else if( auto as_var_array_t = val_inp.tryAs<var_array_t>() ){
    auto& var_array = as_var_array_t.value();
    OrkAssert(var_array.size() == 4);
    val_out.x = var_array[0].get<double>();
    val_out.y = var_array[1].get<double>();
    val_out.z = var_array[2].get<double>();
    val_out.w = var_array[3].get<double>();
  }
  else{
    OrkAssert(false);
  }
}

////////////////////////////////////////////////////////////////////////////////

template <> //
inline void decode_value(var_t val_inp, object_ptr_t& val_out) {
  printf( "val_inp<%s>\n", val_inp.typestr().c_str() );
  if(auto as_obj = val_inp.tryAs<object_ptr_t>()){
    val_out = as_obj.value();
  }
  else{
    OrkAssert(false);
  }
}

////////////////////////////////////////////////////////////////////////////////
// fallthroughs
////////////////////////////////////////////////////////////////////////////////
template <typename T> //
inline void decode_key(std::string keystr, T& key_out) {
  printf("keystr_inp<%s> typeid(T)<%s>\n", keystr.c_str(), typeid(T).name());
  OrkAssert(false);
}
template <typename T> //
inline void decode_value(var_t val_inp, T& val_out) {
  val_out = val_inp.get<T>();
}
template <typename T> //
void encode_key(std::string& keystr_out, const T& key_inp) {
  printf("keystr_out<%s> typeid(T)<%s>\n", keystr_out.c_str(), typeid(T).name());
  OrkAssert(false);
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serdes {
