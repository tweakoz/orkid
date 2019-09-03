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

class FileDevNitro : public FileDev
{
protected:

	orkmap< File *, FileH >	mmFileHandleMap;

public:

	FileDevNitro( );

	virtual EFileErrCode OpenFile( File& rFile );
	virtual EFileErrCode CloseFile( File& rFile );
	virtual EFileErrCode Read( File& rFile, void* pTo, int iSize );
	virtual EFileErrCode Map( File& rFile, void** ppTo, int& riSize );
	virtual EFileErrCode Write( File& rFile, const void* pFrom, int iSize );
	virtual EFileErrCode SeekFromStart( File& rFile, int iTo );
	virtual EFileErrCode SeekFromCurrent( File& rFile, int iOffset );
	virtual EFileErrCode GetLength( File& rFile, int& riLength );
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
