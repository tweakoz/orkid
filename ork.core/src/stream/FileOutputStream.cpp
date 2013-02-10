////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/orkconfig.h>
#include <ork/orkstd.h>
#include <ork/orktypes.h>

#include <ork/stream/FileOutputStream.h>

namespace ork { namespace stream {

FileOutputStream::FileOutputStream(const char *filename)
	: mFile(filename, EFM_WRITE)
{
}

FileOutputStream::~FileOutputStream() {}

bool FileOutputStream::Write(const unsigned char *buffer, size_type bufsize)
{
   return mFile.Write(buffer, bufsize) == EFEC_FILE_OK;
}

} }
