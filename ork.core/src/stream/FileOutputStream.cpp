////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
