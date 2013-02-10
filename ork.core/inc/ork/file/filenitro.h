////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _FILE_FILENITRO_H
#define _FILE_FILENITRO_H

#include <file/file.h>

namespace ork {

struct MemoryFS;

class CFileDevNitro : public CFileDev
{
protected:

	orkmap< CFile *, FileH >	mmFileHandleMap;

public:

	CFileDevNitro( );

	virtual EFileErrCode OpenFile( CFile& rFile );
	virtual EFileErrCode CloseFile( CFile& rFile );
	virtual EFileErrCode Read( CFile& rFile, void* pTo, int iSize );
	virtual EFileErrCode Map( CFile& rFile, void** ppTo, int& riSize );
	virtual EFileErrCode Write( CFile& rFile, const void* pFrom, int iSize );
	virtual EFileErrCode SeekFromStart( CFile& rFile, int iTo );
	virtual EFileErrCode SeekFromCurrent( CFile& rFile, int iOffset );
	virtual EFileErrCode GetLength( CFile& rFile, int& riLength );
	virtual EFileErrCode GetCurrentDirectory( std::string& directory );
	virtual EFileErrCode SetCurrentDirectory( const std::string& directory );

	virtual bool DoesFileExist( const std::string& filespec );
	virtual bool DoesDirectoryExist( const std::string& filespec );
	virtual bool IsFileWritable( const std::string& filespec );

	static void RegisterMemoryFS(void *symbol);

private:

	static bool MemoryFSFindFile(const char *fname, void *&base, size_t &size);

	static MemoryFS *mMemoryFS;
	bool mbMcsInitialized;
};

}

#endif // _FILE_FILENITRO_H
