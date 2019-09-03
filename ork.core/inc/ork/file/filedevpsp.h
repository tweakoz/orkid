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

class FileDevPSP : public FileDev
{
public:

	FileDevPSP();

	virtual EFileErrCode OpenFile( File& rFile );
	virtual EFileErrCode CloseFile( File& rFile );
	virtual EFileErrCode Read( File& rFile, void* pTo, int iSize );
	virtual EFileErrCode Write( File& rFile, const void* pFrom, int iSize );
	virtual EFileErrCode SeekFromStart( File& rFile, int iTo );
	virtual EFileErrCode SeekFromCurrent( File& rFile, int iOffset );
	virtual EFileErrCode GetLength( File& rFile, int& riLength );
	virtual EFileErrCode GetCurrentDirectory( std::string& directory );
	virtual EFileErrCode SetCurrentDirectory( const std::string& directory );
	virtual bool DoesFileExist( const std::string& filespec );
	virtual bool DoesDirectoryExist( const std::string& filespec ) { return false; }

private:
	std::string GetAbsolutePath(const std::string& path);
	std::string GetDeviceFilename(const std::string& unqualifiedName);
	EFileErrCode InternalSeek(File& rFile, int iTo, int sceWhence); 
	static EFileErrCode TranslateError(int sceError);
	static const SceMode kmsDefaultMode;
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#endif // _FILE_PSP_H

