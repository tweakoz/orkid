////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/string/ResizableString.h>

namespace ork {

ResizableString::size_type ResizableString::format(const char* format, ...) {
  va_list args;

  va_start(args, format);
  size_type result = vformat(format, args);
  va_end(args);

  return result;
}

ResizableString::size_type ResizableString::vformat(const char* format, va_list args) {
  size_type formatsize = MutableString(*this).vformat(format, args);

  if (formatsize > capacity()) {
    resize(formatsize, 0, formatsize);
    MutableString(*this).vformat(format, args);
  }

  return formatsize;
}

ResizableString::ResizableString()
    : mpBuffer(NULL)
    , mMaxSize(0)
    , mLength(0) {
}

ResizableString::ResizableString(const char* s)
    : mpBuffer(NULL)
    , mMaxSize(0)
    , mLength(0) {
  copy(s, mLength = ::strlen(s));
}

ResizableString::ResizableString(const PieceString& s)
    : mpBuffer(NULL)
    , mMaxSize(0)
    , mLength(0) {
  copy(s.c_str(), mLength = s.length());
}

void ResizableString::reserve(size_type sz) {
  resize(sz, size(), sz);
}

ResizableString::operator MutableString() {
  resize(1, capacity(), 64);
  return MutableString(mpBuffer, capacity() + 1, mLength);
}

ResizableString::iterator ResizableString::begin() {
  return mpBuffer;
}

ResizableString::iterator ResizableString::end() {
  return mpBuffer + size();
}

ResizableString::const_iterator ResizableString::begin() const {
  return PieceString(*this).begin();
}

ResizableString::const_iterator ResizableString::end() const {
  return PieceString(*this).end();
}

///////////////////////////////////////////////////////////////////////////////

ResizableString::size_type ResizableString::find(char ch, ResizableString::size_type pos) const {
  return PieceString(*this).find(ch, pos);
}

ResizableString::size_type
ResizableString::find(const char* s, ResizableString::size_type pos, ResizableString::size_type len) const {
  return PieceString(*this).find(s, pos, len);
}

ResizableString::size_type ResizableString::find(const PieceString& s, ResizableString::size_type pos) const {
  return PieceString(*this).find(s, pos);
}

ResizableString::size_type ResizableString::find_first_of(char ch, ResizableString::size_type pos) const {
  return PieceString(*this).find_first_of(ch, pos);
}

ResizableString::size_type ResizableString::find_first_of(const char* s, ResizableString::size_type pos) const {
  return PieceString(*this).find_first_of(s, pos);
}

ResizableString::size_type ResizableString::rfind(char ch, ResizableString::size_type pos) const {
  return PieceString(*this).rfind(ch, pos);
}

ResizableString::size_type
ResizableString::rfind(const char* s, ResizableString::size_type pos, ResizableString::size_type len) const {
  return PieceString(*this).rfind(s, pos, len);
}

ResizableString::size_type ResizableString::rfind(const PieceString& s, ResizableString::size_type pos) const {
  return PieceString(*this).rfind(s, pos);
}

ResizableString::size_type ResizableString::find_last_of(char ch, ResizableString::size_type pos) const {
  return PieceString(*this).find_last_of(ch, pos);
}

ResizableString::size_type ResizableString::find_last_of(const char* s, ResizableString::size_type pos) const {
  return PieceString(*this).find_last_of(s, pos);
}

PieceString ResizableString::substr(ResizableString::size_type index, ResizableString::size_type amount) const {
  return PieceString(*this).substr(index, amount);
}

bool ResizableString::empty() const {
  return 0 == size();
}

ResizableString::operator PieceString() const {
  return PieceString(data(), length());
}

ResizableString::operator ConstString() const {
  return ConstString(c_str());
}

bool ResizableString::operator==(const PieceString& other) const {
  return PieceString(*this) == other;
}

bool ResizableString::operator<=(const PieceString& other) const {
  return PieceString(*this) <= other;
}

bool ResizableString::operator>=(const PieceString& other) const {
  return PieceString(*this) >= other;
}

bool ResizableString::operator<(const PieceString& other) const {
  return PieceString(*this) < other;
}

bool ResizableString::operator>(const PieceString& other) const {
  return PieceString(*this) > other;
}

bool ResizableString::operator!=(const PieceString& other) const {
  return PieceString(*this) != other;
}

void ResizableString::replace(size_type offset, size_type sz, PieceString value) {
  if (offset + sz > length())
    return;

  size_type required_size = length() - sz + value.length();

  resize(required_size, length(), required_size * 2);

  MutableString(*this).replace(offset, sz, value);
}

const ResizableString& ResizableString::operator=(const ResizableString& other) {
  return operator=(PieceString(other));
}

const ResizableString& ResizableString::operator=(const char* other) {
  return operator=(PieceString(other));
}

const ResizableString& ResizableString::operator=(const PieceString& other) {
  resize(other.length(), 0, other.length());

  MutableString(*this) = other;

  return *this;
}

const ResizableString& ResizableString::operator+=(const PieceString& other) {
  size_type len  = length();
  size_type olen = other.length();
  resize(len + olen, len, len * 2 + olen);

  MutableString(*this) += other;

  return *this;
}

const ResizableString& ResizableString::operator+=(const char* other) {
  return operator+=(PieceString(other));
}

const ResizableString& ResizableString::operator+=(char c) {
  return operator+=(PieceString(&c, 1));
}

void ResizableString::resize(size_type required, size_type keep, size_type newsize) {
  if (mMaxSize < required) {
    char* newbuffer = new char[newsize + 1];

    if (mpBuffer) {
      if (keep != 0)
        ::memcpy(newbuffer, mpBuffer, keep);

      delete[] mpBuffer;
    }

    mpBuffer = newbuffer;

    mMaxSize = newsize;

    mpBuffer[length()] = '\0';
  }
}

void ResizableString::copy(const char* s, size_type len) {
  resize(len, 0, len);
  if (0 != mMaxSize) {
    ::strncpy(mpBuffer, s, len);
    mpBuffer[len] = '\0';
  }
}

ResizableString::~ResizableString() {
  if (mpBuffer) {
    delete[] mpBuffer;
    mpBuffer = NULL;
  }
}

const char* ResizableString::data() const {
  return mpBuffer;
}

const char* ResizableString::c_str() const {
  return mpBuffer ? mpBuffer : "";
}

ResizableString::size_type ResizableString::length() const {
  return mLength;
}

ResizableString::size_type ResizableString::size() const {
  return mLength;
}

ResizableString::size_type ResizableString::capacity() const {
  return mMaxSize;
}

} // namespace ork
