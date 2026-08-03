////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
static logchannel_ptr_t logchan_rcodec = logger()->configureChannel("reflection.codec",fvec3(0.9,1,0.9), false);
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
// varmap object codec table (see codec.h)
////////////////////////////////////////////////////////////////////////////////
namespace {
using varobj_encoder_map_t = std::unordered_map<TypeId::hashtype_t, varobj_encoder_t>;
using varobj_decoder_map_t = std::map<const rtti::Class*, varobj_decoder_t>;

varobj_encoder_map_t& _varobj_encoders() {
  static varobj_encoder_map_t the_map;
  return the_map;
}
varobj_decoder_map_t& _varobj_decoders() {
  static varobj_decoder_map_t the_map;
  return the_map;
}
} // namespace
////////////////////////////////////////////////////////////////////////////////
void registerVarObjectCodec(
    const TypeId& vartype,        //
    const rtti::Class* clazz,     //
    varobj_encoder_t encoder,     //
    varobj_decoder_t decoder) {
  _varobj_encoders()[vartype._hashed] = encoder;
  _varobj_decoders()[clazz]           = decoder;
  logchan_rcodec->log("registerVarObjectCodec class<%p> vartype<%s>", (const void*)clazz, vartype._typename.c_str());
}
////////////////////////////////////////////////////////////////////////////////
object_ptr_t varObjectEncode(const varmap::var_t& val_inp) {
  auto typeid_of_val = val_inp.getOrkTypeId();
  auto it            = _varobj_encoders().find(typeid_of_val._hashed);
  if (it == _varobj_encoders().end())
    return nullptr;
  return it->second(val_inp);
}
////////////////////////////////////////////////////////////////////////////////
bool varObjectDecode(object_ptr_t obj_inp, varmap::var_t& val_out) {
  if (nullptr == obj_inp)
    return false;
  // walk to the nearest registered ancestor — a scene may hand over a subclass
  // of the type whose codec was registered.
  const rtti::Class* clazz = obj_inp->GetClass();
  while (clazz) {
    auto it = _varobj_decoders().find(clazz);
    if (it != _varobj_decoders().end()) {
      it->second(obj_inp, val_out);
      return true;
    }
    clazz = clazz->Parent();
  }
  return false;
}
////////////////////////////////////////////////////////////////////////////////
void varObjectDecodeFailure(object_ptr_t obj_inp) {
  throw std::runtime_error(FormatString(
      "varmap object decode: no codec registered for class <%s> — the codec that WROTE this "
      "value is missing (register it with reflect::serdes::registerVarObjectCodec<ptr_t>()).",
      obj_inp ? obj_inp->GetClass()->Name().c_str() : "nil"));
}
////////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serdes
