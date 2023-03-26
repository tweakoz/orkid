////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/file/filedevram.h>

namespace ork
{

FileDevRam::FileDevRam() 
	: FileDev( "ram", "/", EFDF_CAN_READ)
{
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDevRam::_doOpenFile(File &rFile)
{
	OrkAssert(!rFile.IsOpen());

	orkmap<file::Path, RamFile *>::const_iterator it = mTOC.find(rFile.GetFileName().toAbsolute());
	if(it != mTOC.end())
	{
		rFile.mHandle = reinterpret_cast<FileH>(it->second);
		rFile.miFileLen = it->second->miSize;
		rFile.SetUserPos(0);
		rFile.SetPhysicalPos(0);
		return EFEC_FILE_OK;
	}

	return EFEC_FILE_DOES_NOT_EXIST;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDevRam::_doCloseFile(File &rFile)
{
	OrkAssert(rFile.IsOpen());

	rFile.mHandle = 0;
	rFile.miFileLen = 0;
	return EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDevRam::_doRead(File &rFile, void *pTo, size_t icount, size_t &iactualread)
{
	OrkAssert(rFile.IsOpen());

	OrkAssert(pTo);

	RamFile *ramfile = reinterpret_cast<RamFile *>(rFile.mHandle);

	size_t iphyspos = rFile.GetPhysicalPos();

	iactualread = 0;
	size_t ifilelen = 0;
	rFile.GetLength(ifilelen);
	size_t iphysleft = ifilelen - iphyspos;

	if(icount <= iphysleft)
	{
		memcpy(pTo, &ramfile->mpBuffer[iphyspos], icount);
		rFile.SetPhysicalPos(iphyspos + icount);
		iactualread = icount;
	}
	else if(iphysleft > 0) // read past end of file, so terminate read buffer with 0's
	{
		memcpy(pTo, &ramfile->mpBuffer[iphyspos], iphysleft);
		rFile.SetPhysicalPos(iphyspos + iphysleft);
		memset(((U8 *)pTo) + iphysleft, 0, icount - iphysleft);
	}
	else
		memset(pTo, 0, icount);

	return EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDevRam::write(File &rFile, const void *pFrom, size_t iSize)
{
	OrkAssertNotImpl();
	return EFEC_FILE_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDevRam::_doSeekFromStart(File &rFile, size_t iTo)
{
	OrkAssertNotImpl();

	/*OrkAssert(rFile.IsOpen());

	OrkAssert(iTo <= rFile.miFileLen);
	if(iTo > rFile.miFileLen)
		return EFEC_FILE_INVALID_SIZE;

	OrkAssert((iTo % 4) == 0);

	//S32 result = DVDSeek(pfile, iTo);
	//if(result < 0)
	//	return EFEC_FILE_UNKNOWN;

	rFile.SetPhysicalPos(iTo);*/

	return EFEC_FILE_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDevRam::_doSeekFromCurrent(File &rFile, size_t iOffset)
{
	OrkAssertNotImpl();

	/*OrkAssert(rFile.IsOpen());

	int inext = rFile.GetPhysicalPos() + iOffset;

	OrkAssert(inext <= rFile.miFileLen);
	if(inext > rFile.miFileLen)
		return EFEC_FILE_INVALID_SIZE;

	OrkAssert((inext % 4) == 0);

	//S32 result = DVDSeek(pfile, inext);
	//if(result < 0)
	//	return EFEC_FILE_UNKNOWN;

	rFile.SetPhysicalPos(inext);*/

	return EFEC_FILE_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDevRam::_doGetLength(File &rFile, size_t &riLen)
{
	OrkAssert(rFile.IsOpen());

	orkmap<file::Path, RamFile *>::const_iterator it = mTOC.find(rFile.GetFileName().toAbsolute());
	if(it != mTOC.end())
	{
		riLen = it->second->miSize;
		return EFEC_FILE_OK;
	}

	return EFEC_FILE_DOES_NOT_EXIST;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDevRam::getCurrentDirectory(file::Path::NameType &directory)
{
	directory = "/";
	return EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDevRam::setCurrentDirectory(const file::Path::NameType &inspec)
{
	return EFEC_FILE_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////

bool FileDevRam::doesFileExist(const file::Path &filespec)
{
	//orkprintf("FileDevRam::DoesFileExist(%s)\n", filespec.c_str());
	//orkprintf("  filespec.toAbsolute() = %s\n", filespec.toAbsolute().c_str());

	//for(orkmap<file::Path, RamFile *>::const_iterator ittmp = mTOC.begin(); ittmp != mTOC.end(); ittmp++)
	// 	orkprintf("  :  = %s\n", ittmp->first.c_str());

	orkmap<file::Path, RamFile *>::const_iterator it = mTOC.find(filespec.toAbsolute());
	if(it != mTOC.end())
		return true;
	return false;
}

///////////////////////////////////////////////////////////////////////////////

bool FileDevRam::doesDirectoryExist(const file::Path &filespec)
{
	return false;
}

bool FileDevRam::isFileWritable(const file::Path &filespec)
{
	return false;
}

void FileDevRam::RegisterRamFile(const file::Path &path, const char *pBuffer, size_t isize)
{
	OrkAssert(pBuffer);
	OrkAssert(isize >= 0);

	file::Path absol = path.toAbsolute();

	orkmap<file::Path, RamFile *>::const_iterator it = mTOC.find(absol);
	if(it == mTOC.end())
		mTOC.insert(std::make_pair(absol, new RamFile(pBuffer, isize)));
	else
	{
		it->second->mpBuffer = pBuffer;
		it->second->miSize = isize;
	}
}

} // namespace ork
