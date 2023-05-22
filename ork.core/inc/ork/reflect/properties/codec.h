////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#pragma once
#include <ork/kernel/string/string.h>
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
} //namespace ork::reflect::serdes {
