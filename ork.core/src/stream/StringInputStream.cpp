////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/stream/StringInputStream.h>

#include <string>

namespace ork { namespace stream {

StringInputStream::StringInputStream(const PieceString& string)
    : mString(string) {
}

size_t StringInputStream::Read(unsigned char* buffer, size_type bufmax) {
  size_type size(bufmax);

  if (size > mString.size())
    size = mString.size();

  if (size == 0)
    return kEOF;

  if (buffer != NULL)
    ::memcpy(buffer, reinterpret_cast<const unsigned char*>(mString.c_str()), size);

  mString = mString.substr(size);

  return int(size);
}

void StringInputStream::SetStreamData(const PieceString& string) {
  mString = string;
}

}} // namespace ork::stream
