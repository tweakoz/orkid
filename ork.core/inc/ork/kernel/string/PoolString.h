////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/orkconfig.h>
#include <ork/orkstd.h>

#include <ork/kernel/string/ConstString.h>
#include <ork/kernel/string/PieceString.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

///
/// PoolStrings are like ConstStrings except their string pointers are
/// guaranteed to be one-one with their data.  Comparisons between PoolStrings is
/// uses the pointer, rather than the data, and therefore may give
/// different orderings on different runs/platforms.
///
/// To conserve space, and because PoolStrings are used in maps,
/// there is no need to store the length of the string in the
/// structure.
///
/// Consider PoolString the same thing as a const char * for use in maps
/// with pooling keeping them unique.
///
/// PoolStrings may only be constructed from StringPools, which
/// guarantee the uniqueness of the strings.
///

class PoolString {
public:
  /// Constructs an empty pool string.
  PoolString();

  /// @return a chararcter pointer to the data.
  const char* data() const;
  /// @return a chararcter pointer to NUL terminated data.
  const char* c_str() const;

  /// Casts this string to a PieceString
  operator PieceString() const;
  /// Casts this string to a ConstString
  operator ConstString() const;

  bool operator==(const PoolString&) const;
  bool operator<=(const PoolString&) const;
  bool operator>=(const PoolString&) const;
  bool operator<(const PoolString&) const;
  bool operator>(const PoolString&) const;
  bool operator!=(const PoolString&) const;

  /// @returns false if the string is not set.
  operator bool() const;

  /// @returns true if the string is not set, or is the empty string.
  bool empty() const;

private:
  friend class StringPool;

  /// Constructs a pool string from a character pointer, used by StringPool.
  PoolString(const char* string);

  int compare(const PoolString&) const;

  /// The character pointer to the string data
  const char* _stringptr;
};
///////////////////////////////////////////////////////////////////////////////

inline const char* PoolString::data() const {
  return _stringptr;
}

///////////////////////////////////////////////////////////////////////////////

inline const char* PoolString::c_str() const {
  return _stringptr;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
