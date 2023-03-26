////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
