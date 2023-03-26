////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/string/MutableString.h>
#include <ork/kernel/string/PieceString.h>
#include <ork/kernel/string/ConstString.h>
///////////////////////////////////////////////////////////////////////////////

namespace ork {

/// A string holding a fixed-length array for its data.
///
/// All OrkStrings operate in a manner compatible with std::string
/// wherever feasable in the functions implemented.  ArrayStrings
/// have a maximum size, and can be passed generically to other
/// functions as MutableStrings, without loss of functionality.
///
/// @param template_size
///        the maximum length of the string, not including the NUL terminator.
///
/// @see PieceString
/// @see ConstString
/// @see MutableString
/// @see ResizableString

///////////////////////////////////////////////////////////////////////////////
template <size_t template_size> class ArrayString {
public:
  typedef PieceString::size_type size_type;
  static const size_type npos = size_type(PieceString::npos);

  typedef const char* const_iterator;
  typedef char* iterator;

  /// Constructs an array string from a PieceString.
  ArrayString(const PieceString& s);
  /// Constructs an array string from a NUL-terminated c-string.
  ArrayString(const char* s);
  /// Constructs an empty ArrayString.
  ArrayString();

  inline void clear() {
    mLength = 0;
  }

  /// @return a chararcter pointer to the data.
  const char* data() const;
  /// @return a chararcter pointer to the data.
  char* data();
  /// @return a chararcter pointer to NUL terminated data.
  const char* c_str() const;
  /// @return the current length of the string.
  size_type length() const;
  /// @return the current length of the string.
  size_type size() const;
  /// @return the maximum length of the string.
  size_type capacity() const;

  /// @return a const_iterator at the begining of the string.
  const_iterator begin() const;
  /// @return a const_iterator at the end of the string.
  const_iterator end() const;
  /// @return an iterator at the end of the string.
  iterator begin();
  /// @returns an iterator at the end of the string.
  iterator end();

  /// Casts this string to a PieceString.
  /// Do not operate on the ArrayString
  /// during the PieceString's lifetime.
  /// @return A PieceString interface to the ArrayString.
  operator PieceString() const;
  /// Casts this string to a ConstString.
  /// Do not operate on the ArrayString
  /// during the ConstString's lifetime.
  /// @return A ConstString interface to the ArrayString.
  operator ConstString() const;
  /// Casts this string to a MutableString.
  /// Note that this mutable string is a view on the ArrayString,
  /// not a copy, and can be used to edit the ArrayString.
  ///
  /// Multiple MutableStrings from the same ArrayString
  /// will remain correctly aliased to each-other.
  ///
  /// @return A MutableString interface to the ArrayString.
  operator MutableString();

  bool operator==(const PieceString& other) const;
  bool operator<=(const PieceString& other) const;
  bool operator>=(const PieceString& other) const;
  bool operator<(const PieceString& other) const;
  bool operator>(const PieceString& other) const;
  bool operator!=(const PieceString& other) const;

private:
  /// The size of the string.
  enum { TemplateSize = template_size };
  /// The array of character data
  char mArray[TemplateSize + 1];
  /// The length of the string.
  size_type mLength;
};

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline ArrayString<size>::ArrayString(const char* s) {
  strncpy(mArray, s, TemplateSize);
  mArray[TemplateSize] = '\0';
  mLength              = strlen(mArray); // Ideally this would be strnlen, but that's not supported on CW.
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size>
inline ArrayString<size>::ArrayString()
    : mLength(0) {
  mArray[0] = '\0';
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size>
inline ArrayString<size>::ArrayString(const PieceString& s)
    : mLength(0) {
  MutableString(*this) = s;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline const char* ArrayString<size>::data() const {
  return mArray;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline char* ArrayString<size>::data() {
  return mArray;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline const char* ArrayString<size>::c_str() const {
  return mArray;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline typename ArrayString<size>::size_type ArrayString<size>::length() const {
  return mLength;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline typename ArrayString<size>::size_type ArrayString<size>::size() const {
  return mLength;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline typename ArrayString<size>::size_type ArrayString<size>::capacity() const {
  return TemplateSize;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline typename ArrayString<size>::const_iterator ArrayString<size>::begin() const {
  return data();
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline typename ArrayString<size>::const_iterator ArrayString<size>::end() const {
  return data() + length();
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline typename ArrayString<size>::iterator ArrayString<size>::begin() {
  return data();
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline typename ArrayString<size>::iterator ArrayString<size>::end() {
  return data() + length();
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline ArrayString<size>::operator PieceString() const {
  return PieceString(data(), length());
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline ArrayString<size>::operator ConstString() const {
  return ConstString(c_str());
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline ArrayString<size>::operator MutableString() {
  return MutableString(mArray, capacity() + 1, mLength);
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline bool ArrayString<size>::operator==(const PieceString& other) const {
  return PieceString(*this) == other;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline bool ArrayString<size>::operator<=(const PieceString& other) const {
  return PieceString(*this) <= other;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline bool ArrayString<size>::operator>=(const PieceString& other) const {
  return PieceString(*this) >= other;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline bool ArrayString<size>::operator<(const PieceString& other) const {
  return PieceString(*this) < other;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline bool ArrayString<size>::operator>(const PieceString& other) const {
  return PieceString(*this) > other;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t size> inline bool ArrayString<size>::operator!=(const PieceString& other) const {
  return PieceString(*this) != other;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
