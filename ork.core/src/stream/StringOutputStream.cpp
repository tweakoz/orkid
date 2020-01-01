////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
