////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#pragma once
#include <ork/kernel/string/string.h>
#include <ork/kernel/varmap.inl>
#include <ork/rtti/Class.h>
namespace ork {
  PoolString AddPooledString(const PieceString& ps);
}
////////////////////////////////////////////////////////////////////////////////
namespace ork::reflect::serdes {
////////////////////////////////////////////////////////////////////////////////
using ulong_t = unsigned long int;
using uint_t  = unsigned int;
using var_array_t = std::vector<svar64_t>;

template <typename T> //
void decode_key(std::string keystr, T& key_out);
template <typename T> //
void decode_value(var_t val_inp, T& val_out);
template <typename T> //
void encode_key(std::string& keystr_out, const T& key_inp);

////////////////////////////////////////////////////////////////////////////////
// varmap object codec table.
//
// A varmap::var_t holding a shared_ptr to a reflected object is invisible to
// the serializer on its own: svar type identity is EXACT, so a
// skyatmospheredata_ptr_t is not an object_ptr_t and NODEENC<varmap::var_t>
// would write it as "null:". Downstream libs register the concrete ptr type
// here (mirrors varmap::VarMap::registerStringEncoder in ork/kernel/varmap.inl);
// once registered the value serializes through standard reflection as a nested
// object node, and comes back typed.
//
// Registration must happen before the first serialize/deserialize touching that
// type — the class-registration TU (lev2_init.cpp et al) is the place.
////////////////////////////////////////////////////////////////////////////////

using varobj_encoder_t = std::function<object_ptr_t(const varmap::var_t&)>;
using varobj_decoder_t = std::function<void(object_ptr_t, varmap::var_t&)>;

// keyed on the CLASS SINGLETON, not its name: registration runs from the
// class-toucher pass, where rtti names are not assigned yet.
void registerVarObjectCodec(
    const TypeId& vartype,    //
    const rtti::Class* clazz, //
    varobj_encoder_t encoder, //
    varobj_decoder_t decoder);

// null when the var does not hold a registered object ptr type
object_ptr_t varObjectEncode(const varmap::var_t& val_inp);
// false when the instance's class (or any ancestor) has no registered codec
bool varObjectDecode(object_ptr_t obj_inp, varmap::var_t& val_out);
// a stream carrying an object no codec can type is unrecoverable - name it and throw
[[noreturn]] void varObjectDecodeFailure(object_ptr_t obj_inp);

////////////////////////////////////////////////////////////////////////////////
// T is the concrete shared_ptr alias (e.g. pbr::skyatmospheredata_ptr_t).
////////////////////////////////////////////////////////////////////////////////
template <typename T> //
inline void registerVarObjectCodec() {
  using obj_t = typename T::element_type;
  registerVarObjectCodec(
      TypeId::of<T>(),
      obj_t::GetClassStatic(),
      [](const varmap::var_t& val_inp) -> object_ptr_t { //
        return std::dynamic_pointer_cast<Object>(val_inp.template get<T>());
      },
      [](object_ptr_t obj_inp, varmap::var_t& val_out) { //
        val_out.template set<T>(std::dynamic_pointer_cast<obj_t>(obj_inp));
      });
}

////////////////////////////////////////////////////////////////////////////////
} //namespace ork::reflect::serdes {
