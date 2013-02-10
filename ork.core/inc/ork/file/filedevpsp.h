////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _FILE_FILEPSP_H
#define _FILE_FILEPSP_H

///////////////////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <kerneltypes.h> // For SceMode

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class CFileDevPSP : public CFileDev
{
public:

	CFileDevPSP();

	virtual EFileErrCode OpenFile( CFile& rFile );
	virtual EFileErrCode CloseFile( CFile& rFile );
	virtual EFileErrCode Read( CFile& rFile, void* pTo, int iSize );
	virtual EFileErrCode Write( CFile& rFile, const void* pFrom, int iSize );
	virtual EFileErrCode SeekFromStart( CFile& rFile, int iTo );
	virtual EFileErrCode SeekFromCurrent( CFile& rFile, int iOffset );
	virtual EFileErrCode GetLength( CFile& rFile, int& riLength );
	virtual EFileErrCode GetCurrentDirectory( std::string& directory );
	virtual EFileErrCode SetCurrentDirectory( const std::string& directory );
	virtual bool DoesFileExist( const std::string& filespec );
	virtual bool DoesDirectoryExist( const std::string& filespec ) { return false; }

private:
	std::string GetAbsolutePath(const std::string& path);
	std::string GetDeviceFilename(const std::string& unqualifiedName);
	EFileErrCode InternalSeek(CFile& rFile, int iTo, int sceWhence); 
	static EFileErrCode TranslateError(int sceError);
	static const SceMode kmsDefaultMode;
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#endif // _FILE_PSP_H

