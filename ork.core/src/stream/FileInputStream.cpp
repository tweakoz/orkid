////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/stream/FileInputStream.h>

namespace ork { namespace stream {
/////////////////////////////////////////

FileInputStream::FileInputStream(const char *filename)
	: mFile(filename, EFM_READ)
{
}

FileInputStream::~FileInputStream() {}

size_t FileInputStream::Read(unsigned char *buffer, size_type bufmax)
{
	OrkAssert( mFile.IsOpen() );

	size_t read_limit = size_t(bufmax);

	// FIXME: rewrite file so we don't have to access member variables directly.

	size_t ilen = 0;
	mFile.GetLength(ilen);

	if(read_limit + mFile.GetUserPos() > ilen )
	{
		read_limit = ilen - mFile.GetUserPos();
	}
	if( 0 == read_limit )
	{
		return kEOF;
	}

	if(buffer == NULL)
	{
		mFile.SeekFromCurrent(read_limit);
		return read_limit;
	}


	return mFile.Read(buffer, read_limit) == EFEC_FILE_OK ? read_limit : kEOF;
}

} }
