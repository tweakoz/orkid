////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

template <typename T> //
void decode_key(std::string keystr, T& key_out);
template <typename T> //
void decode_value(var_t val_inp, T& val_out);
template <typename T> //
void encode_key(std::string& keystr_out, const T& key_inp);

////////////////////////////////////////////////////////////////////////////////
} //namespace ork::reflect::serdes {
