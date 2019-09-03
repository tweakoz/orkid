////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _FILE_FILERVL_H
#define _FILE_FILERVL_H

///////////////////////////////////////////////////////////////////////////////

#include <ork/file/file.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class FileDevWII : public FileDev
{
	virtual EFileErrCode DoOpenFile( File& rFile );
	virtual EFileErrCode DoCloseFile( File& rFile );
	virtual EFileErrCode DoRead( File& rFile, void* pTo, int iSize, int& iactualread );
	virtual EFileErrCode DoSeekFromStart( File& rFile, int iTo );
	virtual EFileErrCode DoSeekFromCurrent( File& rFile, int iOffset );
	virtual EFileErrCode DoGetLength( File& rFile, int& riLength );

	//orkmap< File *, FileH >	mmFileHandleMap;

	orkset<file::Path>	mTOC;

public:

	FileDevWII( );
	
	virtual EFileErrCode Write( File& rFile, const void* pFrom, int iSize );
	virtual EFileErrCode GetCurrentDirectory( file::Path::NameType& directory );
	virtual EFileErrCode SetCurrentDirectory( const file::Path::NameType& directory );

	virtual bool DoesFileExist( const file::Path& filespec );
	virtual bool DoesDirectoryExist( const file::Path& filespec );
	virtual bool IsFileWritable( const file::Path& filespec );
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#endif // _FILE_FILERVL_H
