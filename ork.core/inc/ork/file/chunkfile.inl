////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/file/chunkfile.h>

///
namespace ork {
PoolString AddPooledString(const PieceString& ps);
PoolString AddPooledLiteral(const ConstString& cs);
PoolString FindPooledString(const PieceString& ps);

namespace chunkfile {
///

template <typename T> void OutputStream::addItem(const T& data) {
  static_assert(not std::is_same<T, std::string>::value, "use addIndexedString(std::string) instead");
  T temp = data;
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
template <typename T> void InputStream::GetItem(T& item) {
  int isize = sizeof(T);
  OrkAssert((midx + isize) <= milength);
  const char* pchbase = (const char*)mpbase;
  T* pt               = (T*)&pchbase[midx];
  item                = *pt;
  midx += isize;
}
///////////////////////////////////////////////////////////////////////////////
template <typename T> void InputStream::RefItem(T*& item) {
  int isize = sizeof(T);
  int ileft = milength - midx;
  OrkAssert((midx + isize) <= milength);
  const char* pchbase = (const char*)mpbase;
  item                = (T*)&pchbase[midx];
  midx += isize;
}
///////////////////////////////////////////////////////////////////////////////
// template specialization readItem for std::string
template <> std::string InputStream::readItem<std::string>();
///////////////////////////////////////////////////////////////////////////////
template <typename T> T InputStream::readItem() {
  int isize = sizeof(T);
  int ileft = milength - midx;
  OrkAssert((midx + isize) <= milength);
  const char* pchbase = (const char*)mpbase;
  size_t out_index = midx;
  midx += isize;
  auto ptr_to_data = (T*)&pchbase[out_index];
  return *ptr_to_data;
}

///
} // namespace chunkfile
} // namespace ork
///
