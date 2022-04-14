////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/file/file.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class FileDevStd : public ork::FileDev
{
protected:

	orkmap< ork::File *, ork::FileH >	mmFileHandleMap;

public:

	FileDevStd( );

private:
	virtual ork::EFileErrCode DoOpenFile( ork::File &rFile );
	virtual ork::EFileErrCode DoCloseFile( ork::File &rFile );
	virtual ork::EFileErrCode DoRead( ork::File &rFile, void *pTo, size_t iSize, size_t& actualread );
	virtual ork::EFileErrCode DoSeekFromStart( ork::File &rFile, size_t iTo );
	virtual ork::EFileErrCode DoSeekFromCurrent( ork::File &rFile, size_t iOffset );
	virtual ork::EFileErrCode DoGetLength( ork::File &rFile, size_t &riLength );

	virtual ork::EFileErrCode Write( ork::File &rFile, const void *pFrom, size_t iSize );
	virtual ork::EFileErrCode GetCurrentDirectory( file::Path::NameType& directory );
	virtual ork::EFileErrCode SetCurrentDirectory( const file::Path::NameType& directory );

	virtual bool DoesFileExist( const file::Path& filespec );
	virtual bool DoesDirectoryExist( const file::Path& filespec );
	virtual bool IsFileWritable( const file::Path& filespec );
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////


