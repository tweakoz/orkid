////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/file/chunkfile.h>
#include <cstring>

///
namespace ork {
PoolString AddPooledString(const PieceString& ps);
PoolString AddPooledLiteral(const ConstString& cs);
PoolString FindPooledString(const PieceString& ps);

namespace chunkfile {
///

template <typename T> void OutputStream::AddItem(const T& data) {
  T temp = data;
  Write((unsigned char*)&temp, sizeof(temp));
}
///////////////////////////////////////////////////////////////////////////////
template <typename T> void InputStream::GetItem(T& item) {
  size_t isize = sizeof(T);
  OrkAssert((midx + isize) <= milength);
  const char* pchbase = (const char*)mpbase;
  std::memcpy(&item, &pchbase[midx], sizeof(T));
  midx += isize;
}
///////////////////////////////////////////////////////////////////////////////
template <typename T> void InputStream::RefItem(T*& item) {
  size_t isize = sizeof(T);
  size_t ileft = milength - midx;
  OrkAssert((midx + isize) <= milength);
  const char* pchbase = (const char*)mpbase;
  item                = (T*)&pchbase[midx];
  midx += isize;
}
///////////////////////////////////////////////////////////////////////////////
template <typename T> T InputStream::ReadItem() {
  size_t isize = sizeof(T);
  size_t ileft = milength - midx;
  OrkAssert((midx + isize) <= milength);
  const char* pchbase = (const char*)mpbase;
  T result;
  std::memcpy(&result, &pchbase[midx], sizeof(T));
  midx += isize;
  return result;
}
///
} // namespace chunkfile
} // namespace ork
///
