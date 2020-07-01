#pragma once
#include <ork/kernel/string/string.h>
////////////////////////////////////////////////////////////////////////////////
namespace ork::reflect::serdes {
////////////////////////////////////////////////////////////////////////////////
using ulong_t = unsigned long int;
using uint_t  = unsigned int;

template <typename T> //
inline void decode_key(std::string keystr, T& key_out) {
  OrkAssert(false);
}
template <> //
inline void decode_key(std::string keystr, int& key_out) {
  key_out = atoi(keystr.c_str());
}
template <> //
inline void decode_key(std::string keystr, std::string& key_out) {
  key_out = keystr;
}
template <typename T> //
inline void decode_value(var_t val_inp, T& val_out) {
  val_out = val_inp.Get<T>();
}
template <> //
inline void decode_value(var_t val_inp, int& val_out) {
  val_out = int(val_inp.Get<double>());
}
template <> //
inline void decode_value(var_t val_inp, uint_t& val_out) {
  val_out = uint_t(val_inp.Get<double>());
}
template <> //
inline void decode_value(var_t val_inp, ulong_t& val_out) {
  val_out = ulong_t(val_inp.Get<double>());
}
template <> //
inline void decode_value(var_t val_inp, float& val_out) {
  val_out = float(val_inp.Get<double>());
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template <typename T> //
inline void encode_key(std::string& keystr_out, const T& key_inp) {
  OrkAssert(false);
}
template <> //
inline void encode_key(std::string& keystr_out, const int& key_inp) {
  keystr_out = FormatString("%d", key_inp);
}
template <> //
inline void encode_key(std::string& keystr_out, const std::string& key_inp) {
  keystr_out = key_inp;
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serdes
