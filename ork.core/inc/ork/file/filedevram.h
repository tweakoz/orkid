////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef ORK_FILE_FILERAM_H
#define ORK_FILE_FILERAM_H

///////////////////////////////////////////////////////////////////////////////

#include <ork/file/file.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class FileDevRam : public FileDev
{
	virtual EFileErrCode DoOpenFile(File &rFile);
	virtual EFileErrCode DoCloseFile(File &rFile);
	virtual EFileErrCode DoRead(File &rFile, void *pTo, size_t iSize, size_t &iactualread);
	virtual EFileErrCode DoSeekFromStart(File &rFile, size_t iTo);
	virtual EFileErrCode DoSeekFromCurrent(File &rFile, size_t iOffset);
	virtual EFileErrCode DoGetLength(File &rFile, size_t &riLength);

	struct RamFile
	{
		RamFile(const char *pBuffer, size_t isize) : mpBuffer(pBuffer), miSize(isize) {}
		const char *mpBuffer;
		size_t miSize;
	};

	orkmap<file::Path, RamFile *> mTOC;

public:

	FileDevRam();
	
	virtual EFileErrCode Write(File &rFile, const void *pFrom, size_t iSize);
	virtual EFileErrCode GetCurrentDirectory(std::string &directory);
	virtual EFileErrCode SetCurrentDirectory(const std::string &directory);

	virtual bool DoesFileExist(const file::Path& filespec);
	virtual bool DoesDirectoryExist(const file::Path& filespec);
	virtual bool IsFileWritable(const file::Path& filespec);

	void RegisterRamFile(const file::Path &path, const char *pBuffer, size_t isize);
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#endif // ORK_FILE_FILERAM_H
