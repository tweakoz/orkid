////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/orkstd.h>

#include <ork/stream/StringOutputStream.h>

namespace ork { namespace stream {

StringOutputStream::StringOutputStream(MutableString string)
    : mString(string)
{}

bool StringOutputStream::Write(const unsigned char *buffer, size_type bufsize)
{
	if(mString.capacity() - mString.length() < bufsize)
		return false;

	mString += PieceString(reinterpret_cast<const char*>(buffer), PieceString::size_type(bufsize));

    return true;
}

} }
