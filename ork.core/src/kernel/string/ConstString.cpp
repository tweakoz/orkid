////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/string/ConstString.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/reflect/Serialize.h>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork {

ConstString::ConstString(const char* s)
    : mpString(s)
    , mLength(::strlen(s)) {
}

ConstString::ConstString()
    : mpString("")
    , mLength(0) {
}

const char* ConstString::c_str() const {
  return mpString;
}
const char* ConstString::data() const {
  return mpString;
}
ConstString::size_type ConstString::length() const {
  return mLength;
}
ConstString::size_type ConstString::size() const {
  return mLength;
}

///////////////////////////////////////////

/*namespace reflect {
template <> void Serialize(const ConstString* in, ConstString* out, reflect::BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi.Serializer()->serializeElement(PieceString(*in));
  } else {
    PoolString pool_string;
    bidi | pool_string;
    *out = pool_string.c_str();
  }
}*/

///////////////////////////////////////////

ConstString::size_type ConstString::find(char ch, ConstString::size_type pos) const {
  return PieceString(*this).find(ch, pos);
}

ConstString::size_type ConstString::find(const char* s, ConstString::size_type pos, ConstString::size_type len) const {
  return PieceString(*this).find(s, pos, len);
}

ConstString::size_type ConstString::find(const PieceString& s, ConstString::size_type pos) const {
  return PieceString(*this).find(s, pos);
}

ConstString::size_type ConstString::find_first_of(char ch, ConstString::size_type pos) const {
  return PieceString(*this).find_first_of(ch, pos);
}

ConstString::size_type ConstString::find_first_of(const char* s, ConstString::size_type pos) const {
  return PieceString(*this).find_first_of(s, pos);
}

ConstString::size_type ConstString::rfind(char ch, ConstString::size_type pos) const {
  return PieceString(*this).rfind(ch, pos);
}

ConstString::size_type ConstString::rfind(const char* s, ConstString::size_type pos, ConstString::size_type len) const {
  return PieceString(*this).rfind(s, pos, len);
}

ConstString::size_type ConstString::rfind(const PieceString& s, ConstString::size_type pos) const {
  return PieceString(*this).rfind(s, pos);
}

ConstString::size_type ConstString::find_last_of(char ch, ConstString::size_type pos) const {
  return PieceString(*this).find_last_of(ch, pos);
}

ConstString::size_type ConstString::find_last_of(const char* s, ConstString::size_type pos) const {
  return PieceString(*this).find_last_of(s, pos);
}

PieceString ConstString::substr(ConstString::size_type index, ConstString::size_type amount) const {
  return PieceString(*this).substr(index, amount);
}

bool ConstString::empty() const {
  return 0 == size();
}

ConstString::operator PieceString() const {
  return PieceString(data(), length());
}

bool ConstString::operator==(const PieceString& other) const {
  return PieceString(*this) == other;
}

bool ConstString::operator<=(const PieceString& other) const {
  return PieceString(*this) <= other;
}

bool ConstString::operator>=(const PieceString& other) const {
  return PieceString(*this) >= other;
}

bool ConstString::operator<(const PieceString& other) const {
  return PieceString(*this) < other;
}

bool ConstString::operator>(const PieceString& other) const {
  return PieceString(*this) > other;
}

bool ConstString::operator!=(const PieceString& other) const {
  return PieceString(*this) != other;
}

} // namespace ork
