////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
	virtual EFileErrCode _doOpenFile(File &rFile);
	virtual EFileErrCode _doCloseFile(File &rFile);
	virtual EFileErrCode _doRead(File &rFile, void *pTo, size_t iSize, size_t &iactualread);
	virtual EFileErrCode _doSeekFromStart(File &rFile, size_t iTo);
	virtual EFileErrCode _doSeekFromCurrent(File &rFile, size_t iOffset);
	virtual EFileErrCode _doGetLength(File &rFile, size_t &riLength);

	struct RamFile
	{
		RamFile(const char *pBuffer, size_t isize) : mpBuffer(pBuffer), miSize(isize) {}
		const char *mpBuffer;
		size_t miSize;
	};

	orkmap<file::Path, RamFile *> mTOC;

public:

	FileDevRam();
	
	EFileErrCode write(File &rFile, const void *pFrom, size_t iSize) final;
	EFileErrCode getCurrentDirectory(file::Path::NameType& directory) final;
	EFileErrCode setCurrentDirectory(const file::Path::NameType &directory) final;

	bool doesFileExist(const file::Path& filespec) final;
	bool doesDirectoryExist(const file::Path& filespec) final;
	bool isFileWritable(const file::Path& filespec) final;

	void RegisterRamFile(const file::Path &path, const char *pBuffer, size_t isize);
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#endif // ORK_FILE_FILERAM_H
