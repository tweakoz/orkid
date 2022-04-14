////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/string/MutableString.h>

namespace ork {

MutableString::MutableString(char* buffer, size_type buffersize, size_type& length)
    : mpBuffer(buffer)
    , mLength(length)
    , mMaxSize(buffersize - 1) {
  buffer[mMaxSize] = '\0';
}

MutableString::iterator MutableString::begin() {
  return mpBuffer;
}

MutableString::iterator MutableString::end() {
  return mpBuffer + size();
}

MutableString::const_iterator MutableString::begin() const {
  return PieceString(*this).begin();
}

MutableString::const_iterator MutableString::end() const {
  return PieceString(*this).end();
}

const char* MutableString::data() const {
  return mpBuffer;
}

const char* MutableString::c_str() const {
  return mpBuffer;
}

MutableString::size_type MutableString::length() const {
  return mLength;
}

MutableString::size_type MutableString::size() const {
  return mLength;
}

MutableString::size_type MutableString::capacity() const {
  return mMaxSize;
}

MutableString::size_type MutableString::format(const char* format, ...) {
  va_list args;
  va_start(args, format);
  size_type result = vformat(format, args);
  va_end(args);

  return result;
}

MutableString::size_type MutableString::vformat(const char* format, va_list args) {
  int result = vsnprintf(mpBuffer, capacity(), format, args);

  if (result >= 0 && size_type(result) >= capacity()) {
    mpBuffer[capacity()] = '\0';
  } else if (result < 0) {
    mpBuffer[0] = '\0';
    result      = 0;
  }

  return mLength = size_type(result);
}

void MutableString::replace(size_type offset, size_type sz, PieceString value) {
  if (offset + sz > length())
    return;

  size_type tail_size = length() - (offset + sz);

  if (tail_size + offset + value.length() > capacity()) {
    if (offset + value.length() < capacity())
      tail_size = capacity() - (offset + value.length());
    else
      tail_size = 0;
  }

  if (offset + value.length() > capacity())
    value = value.substr(0, capacity() - offset);

  if (tail_size > 0)
    ::memmove(mpBuffer + offset + value.size(), mpBuffer + offset + sz, tail_size);

  ::memcpy(mpBuffer + offset, value.c_str(), value.length());

  mpBuffer[mLength = offset + value.length() + tail_size] = '\0';
}

///////////////////////////////////////////////////////////////////////////////////////

MutableString::size_type MutableString::find(char ch, MutableString::size_type pos) const {
  return PieceString(*this).find(ch, pos);
}

MutableString::size_type MutableString::find(const char* s, MutableString::size_type pos, MutableString::size_type len) const {
  return PieceString(*this).find(s, pos, len);
}

MutableString::size_type MutableString::find(const PieceString& s, MutableString::size_type pos) const {
  return PieceString(*this).find(s, pos);
}

MutableString::size_type MutableString::find_first_of(char ch, MutableString::size_type pos) const {
  return PieceString(*this).find_first_of(ch, pos);
}

MutableString::size_type MutableString::find_first_of(const char* s, MutableString::size_type pos) const {
  return PieceString(*this).find_first_of(s, pos);
}

MutableString::size_type MutableString::rfind(char ch, MutableString::size_type pos) const {
  return PieceString(*this).rfind(ch, pos);
}

MutableString::size_type MutableString::rfind(const char* s, MutableString::size_type pos, MutableString::size_type len) const {
  return PieceString(*this).rfind(s, pos, len);
}

MutableString::size_type MutableString::rfind(const PieceString& s, MutableString::size_type pos) const {
  return PieceString(*this).rfind(s, pos);
}

MutableString::size_type MutableString::find_last_of(char ch, MutableString::size_type pos) const {
  return PieceString(*this).find_last_of(ch, pos);
}

MutableString::size_type MutableString::find_last_of(const char* s, MutableString::size_type pos) const {
  return PieceString(*this).find_last_of(s, pos);
}

PieceString MutableString::substr(MutableString::size_type index, MutableString::size_type amount) const {
  return PieceString(*this).substr(index, amount);
}

bool MutableString::empty() const {
  return 0 == size();
}

MutableString::operator PieceString() const {
  return PieceString(data(), length());
}

MutableString::operator ConstString() const {
  return ConstString(c_str());
}

bool MutableString::operator==(const PieceString& other) const {
  return PieceString(*this) == other;
}

bool MutableString::operator<=(const PieceString& other) const {
  return PieceString(*this) <= other;
}

bool MutableString::operator>=(const PieceString& other) const {
  return PieceString(*this) >= other;
}

bool MutableString::operator<(const PieceString& other) const {
  return PieceString(*this) < other;
}

bool MutableString::operator>(const PieceString& other) const {
  return PieceString(*this) > other;
}

bool MutableString::operator!=(const PieceString& other) const {
  return PieceString(*this) != other;
}

const MutableString& MutableString::operator=(const MutableString& other) {
  return *this = PieceString(other);
}

const MutableString& MutableString::operator=(const PieceString& other) {
  PieceString copyable = other.substr(0, capacity());

  ::memcpy(mpBuffer, copyable.c_str(), copyable.length());
  mLength           = copyable.length();
  mpBuffer[mLength] = '\0';

  return *this;
}

const MutableString& MutableString::operator+=(const PieceString& other) {
  PieceString copyable = other.substr(0, capacity() - length());

  ::memcpy(mpBuffer + length(), copyable.c_str(), copyable.length());

  mLength += copyable.length();
  mpBuffer[mLength] = '\0';

  return *this;
}

const MutableString& MutableString::operator+=(char c) {
  PieceString s(&c, 1);

  return *this += s;
}

} // namespace ork
