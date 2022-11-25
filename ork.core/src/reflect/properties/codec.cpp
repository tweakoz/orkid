////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/reflect/properties/codec.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/file/path.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/object/Object.h>
#include <ork/util/logger.h>
namespace ork {
  PoolString AddPooledString(const PieceString& ps);
}
////////////////////////////////////////////////////////////////////////////////
namespace ork::reflect::serdes {
static logchannel_ptr_t logchan_rcodec = logger()->createChannel("reflection.codec",fvec3(0.9,1,0.9));
////////////////////////////////////////////////////////////////////////////////
using ulong_t = unsigned long int;
using uint_t  = unsigned int;

template <> //
void decode_key(std::string keystr, int& key_out) {
  key_out = atoi(keystr.c_str());
}
template <> //
void decode_key(std::string keystr, float& key_out) {
  key_out = atof(keystr.c_str());
}
template <> //
void decode_key(std::string keystr, double& key_out) {
  key_out = atof(keystr.c_str());
}
template <> //
void decode_key(std::string keystr, std::string& key_out) {
  key_out = keystr;
}
template <> //
void decode_key(std::string keystr, file::Path& key_out) {
  key_out = keystr;
}
template <> //
void decode_key(std::string keystr, PoolString& key_out) {
  key_out = AddPooledString(keystr.c_str());;
}
template <> //
void decode_key(std::string keystr, object::ObjectClass*& key_out) {
  auto clazz = rtti::Class::FindClass(keystr.c_str());
  key_out = dynamic_cast<object::ObjectClass*>(clazz);
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template <> //
void decode_value(var_t val_inp, int& val_out) {
  val_out = int(val_inp.get<double>());
}
template <> //
void decode_value(var_t val_inp, uint_t& val_out) {
  val_out = uint_t(val_inp.get<double>());
}
template <> //
void decode_value(var_t val_inp, ulong_t& val_out) {
  val_out = ulong_t(val_inp.get<double>());
}
template <> //
void decode_value(var_t val_inp, float& val_out) {
  val_out = float(val_inp.get<double>());
}
template <> //
void decode_value(var_t val_inp, file::Path& val_out) {
  val_out = val_inp.get<std::string>();
}
template <> //
void decode_value(var_t val_inp, PoolString& val_out) {
  val_out = AddPooledString(val_inp.get<std::string>().c_str());
}
template <> //
void decode_value(var_t val_inp, rtti::Class* &val_out) {
  const auto& classname = val_inp.get<std::string>();
  auto clazz = rtti::Class::FindClass(classname.c_str());
  val_out = clazz;
}
////////////////////////////////////////////////////////////////////////////////
// decoding into another svar is only supported for std::string,double,and bool
//  (the sane choice for deserializing JSON)
////////////////////////////////////////////////////////////////////////////////
template <> //
void decode_value(var_t val_inp, svar64_t& val_out) {
  if( auto as_str = val_inp.tryAs<std::string>() ){
    val_out.set<std::string>(as_str.value());
  }
  else if( auto as_dbl = val_inp.tryAs<double>() ){
    val_out.set<double>(as_dbl.value());
  }
  else if( auto as_bool = val_inp.tryAs<bool>() ){
    val_out.set<bool>(as_bool.value());
  }
  else{
    OrkAssert(false);
  }
}
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
template <> //
void encode_key(std::string& keystr_out, const int& key_inp) {
  keystr_out = FormatString("%d", key_inp);
}
template <> //
void encode_key(std::string& keystr_out, const float& key_inp) {
  keystr_out = FormatString("%g", key_inp);
}
template <> //
void encode_key(std::string& keystr_out, const double& key_inp) {
  keystr_out = FormatString("%g", key_inp);
}
template <> //
void encode_key(std::string& keystr_out, const PoolString& key_inp) {
  keystr_out = key_inp.c_str();
  logchan_rcodec->log("encode key<%s>", keystr_out.c_str());
}
template <> //
void encode_key(std::string& keystr_out, const std::string& key_inp) {
  keystr_out = key_inp;
  logchan_rcodec->log("encode key<%s>", keystr_out.c_str());
}
template <> //
void encode_key(std::string& keystr_out, rtti::Class* const & key_inp) {
  keystr_out = key_inp->Name();
  logchan_rcodec->log("encode key<%s>", keystr_out.c_str());
}
template <> // TODO - use rtti::CLass for all subclasses as the impl will be the same
void encode_key(std::string& keystr_out, object::ObjectClass* const & key_inp) {
  auto clazzname = key_inp->Name();
  keystr_out = clazzname.c_str();
  logchan_rcodec->log("encode class<%p> key<%s>", (void*) key_inp, keystr_out.c_str());
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serdes
