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

class CFileDevWII : public CFileDev
{
	virtual EFileErrCode DoOpenFile( CFile& rFile );
	virtual EFileErrCode DoCloseFile( CFile& rFile );
	virtual EFileErrCode DoRead( CFile& rFile, void* pTo, int iSize, int& iactualread );
	virtual EFileErrCode DoSeekFromStart( CFile& rFile, int iTo );
	virtual EFileErrCode DoSeekFromCurrent( CFile& rFile, int iOffset );
	virtual EFileErrCode DoGetLength( CFile& rFile, int& riLength );

	//orkmap< CFile *, FileH >	mmFileHandleMap;

	orkset<file::Path>	mTOC;

public:

	CFileDevWII( );
	
	virtual EFileErrCode Write( CFile& rFile, const void* pFrom, int iSize );
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
