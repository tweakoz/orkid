////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/file/file.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class FileDevStd : public ork::FileDev
{

public:

	FileDevStd( );

private:
	virtual ork::EFileErrCode _doOpenFile( ork::File &rFile );
	virtual ork::EFileErrCode _doCloseFile( ork::File &rFile );
	virtual ork::EFileErrCode _doRead( ork::File &rFile, void *pTo, size_t iSize, size_t& actualread );
	virtual ork::EFileErrCode _doSeekFromStart( ork::File &rFile, size_t iTo );
	virtual ork::EFileErrCode _doSeekFromCurrent( ork::File &rFile, size_t iOffset );
	virtual ork::EFileErrCode _doGetLength( ork::File &rFile, size_t &riLength );

	virtual ork::EFileErrCode write( ork::File &rFile, const void *pFrom, size_t iSize );
	virtual ork::EFileErrCode getCurrentDirectory( file::Path::NameType& directory );
	virtual ork::EFileErrCode setCurrentDirectory( const file::Path::NameType& directory );

	virtual bool doesFileExist( const file::Path& filespec );
	virtual bool doesDirectoryExist( const file::Path& filespec );
	virtual bool isFileWritable( const file::Path& filespec );

  orkmap< ork::File *, ork::FileH > _fileHandleMap;
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////


