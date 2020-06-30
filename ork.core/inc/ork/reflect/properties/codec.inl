#pragma once
#include <ork/kernel/string/string.h>
////////////////////////////////////////////////////////////////////////////////
namespace ork::reflect {
////////////////////////////////////////////////////////////////////////////////
template <typename T> //
inline void decode_key(std::string keystr, T& key_out) {
  OrkAssert(false);
}
template <> //
inline void decode_key(std::string keystr, int& key_out) {
  OrkAssert(false);
}
template <> //
inline void decode_key(std::string keystr, std::string& key_out) {
  key_out = keystr;
}
template <typename T> //
inline void decode_value(IDeserializer::var_t val_inp, T& val_out) {
  val_out = val_inp.Get<T>();
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
} // namespace ork::reflect
