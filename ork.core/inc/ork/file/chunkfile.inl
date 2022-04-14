////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

template <typename T> void OutputStream::AddItem(const T& data) {
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
///
} // namespace chunkfile
} // namespace ork
///
