////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#if defined(_PSP)

#include <ork/file/filedevpsp.h>
#include <iofilemgr.h>
#include <iofilemgr_dirent.h>
#include <iofilemgr_stat.h>
#include <kerror.h>
#include <sysmem.h>

const std::string deviceName = "host0:";

// User read/write, group read, other read
const SceMode CFileDevPSP::kmsDefaultMode(0x0644);

///////////////////////////////////////////////////////////////////////////////

CFileDevPSP::CFileDevPSP()
	: CFileDev("PSPKernel", "", EFDF_CAN_READ | EFDF_CAN_WRITE)
{
	int error = sceIoChdir(deviceName.c_str());
	OrkAssert(error == 0);
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevPSP::OpenFile(CFile& rFile)
{
	rFile.miFilePos = 0;
	rFile.miSeekPos = 0;

	//e Invaidate Cache
	rFile.micache_size = 0;
	rFile.micache_pos = 0;

	int flags = 0;

	if(rFile.Reading() && rFile.Writing())
	{
		flags = SCE_O_RDWR;
	}
	else if(rFile.Reading())
	{
		flags = SCE_O_RDONLY;
	}
	else if(rFile.Writing())
	{
		flags = SCE_O_WRONLY | SCE_O_CREAT;
	}
		
	if(rFile.Appending())
	{
		OrkAssert(rFile.Writing());
		flags |= SCE_O_APPEND;
	}

	orkprintf("Opening file [%s]...", rFile.GetFileName().c_str());
	SceUID fileDescriptor = sceIoOpen(rFile.GetFileName().c_str(), flags, kmsDefaultMode);

	if(fileDescriptor >= 0)
	{
        orkprintf("success!\n");
		rFile.mHandle = fileDescriptor;

		int filesize;
		int error = GetLength(rFile, filesize);
		OrkAssert(error == EFEC_FILE_OK);
		
		rFile.miFileLen = filesize;
		return EFEC_FILE_OK;
	}
	else
	{
        orkprintf("OpenFile() failed!\n");
		return TranslateError(fileDescriptor);
	}
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevPSP::CloseFile( CFile &rFile )
{
	OrkAssert(rFile.mHandle != 0);

	int error = sceIoClose(rFile.mHandle);
	return TranslateError(error);
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevPSP::Read( CFile& rFile, void* pTo, int iSize )
{
	OrkAssert(pTo && iSize > 0);

	// Bad pointer...
	if(NULL == pTo)
	{
		return EFEC_FILE_INVALID_ADDRESS;
	}

	// Make sure that size is positive and current size + size is not past
	// the bounds of the file.
	if(iSize < 1 || ((rFile.miFilePos + iSize) > rFile.miFileLen))
	{
		return EFEC_FILE_INVALID_SIZE;
	}

	if(!rFile.Reading())
	{
		return EFEC_FILE_INVALID_MODE;
	}

	SceSSize bytesRead = sceIoRead(rFile.mHandle, pTo, iSize);
	
	// If the return value is a positive value, then it should be the number ofi
	// bytes read.
	if(bytesRead >= 0)
	{
		OrkAssert(iSize == bytesRead);

		// Update the internal state
		rFile.miFilePos += bytesRead;
		rFile.miSeekPos = rFile.miFilePos;

		return EFEC_FILE_OK;
	}
	else
	{
		return TranslateError(bytesRead);
	}
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevPSP::Write( CFile &rFile, const void *pFrom, int iSize )
{
	// Bad pointer... 
    if(NULL == pFrom) 
    { 
        return EFEC_FILE_INVALID_ADDRESS;
    }

	// Check for a positive size
	if(iSize < 1)
	{
		return EFEC_FILE_INVALID_SIZE;
	}

	if(!rFile.IsOpen())
	{
		return EFEC_FILE_NOT_OPEN;
	}

	if(!rFile.Writing())
	{
		return EFEC_FILE_INVALID_MODE;
	}

	SceSSize bytesWritten = sceIoWrite(rFile.mHandle, pFrom, iSize);

	if(bytesWritten > 0)
	{
		OrkAssert(iSize == bytesWritten);

		rFile.miFilePos += bytesWritten;
	    rFile.miSeekPos = rFile.miFilePos;
			
        return EFEC_FILE_OK;
	}
	else 
    { 
		return TranslateError(bytesWritten);
    } 
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevPSP::SeekFromStart(CFile &rFile, int iTo)
{
	return InternalSeek(rFile, iTo, SCE_SEEK_SET);
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevPSP::SeekFromCurrent( CFile &rFile, int iOffset )
{
	return InternalSeek(rFile, iOffset, SCE_SEEK_CUR);
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevPSP::GetLength( CFile &rFile, int &riLen )
{
	SceIoStat ioStatus; 
    int error = sceIoGetstat(rFile.GetFileName().c_str(), &ioStatus);
    if(error != 0) 
    { 
        return TranslateError(error);
    } 
 
    riLen = ioStatus.st_size;
    return EFEC_FILE_OK; 
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevPSP::GetCurrentDirectory( std::string& directory )
{
	// TODO: This looks shady...
	//std::string temp = deviceName + std::string(":.");
	//SceUID rootDirectory = sceIoDopen("host0:/");
	//SceUID currentDirectory = sceIoDopen("host0:");
	//currentDirectory = sceIoDopen("host0:.");
	////int error = sceIoChdir("host0:/");
	////currentDirectory = sceIoDopen("host0:.");
	//if(currentDirectory >= 0)
	//{
	//	SceIoDirent dirent;
	//	int numberOfDirectoriesRead = sceIoDread(currentDirectory, &dirent);
	//	OrkAssert(numberOfDirectoriesRead > 0);

	//	directory = dirent.d_name;

	//	return EFEC_FILE_OK;
	//}
	//else
	//{
	//	return TranslateError(currentDirectory);
	//}

	directory = "";
	return EFEC_FILE_OK;

	//return EFEC_FILE_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevPSP::SetCurrentDirectory( const std::string& directory )
{
	int error = sceIoChdir(directory.c_str());
	return TranslateError(error);
}

///////////////////////////////////////////////////////////////////////////////

bool CFileDevPSP::DoesFileExist(const std::string& filespec)
{
	SceUID fd = sceIoOpen(filespec.c_str(), SCE_O_RDONLY, kmsDefaultMode);

	if(fd >= 0)
	{
		int error = sceIoClose(fd);
		OrkAssert(error == SCE_KERNEL_ERROR_OK);
		return true;
	}
	else
	{
		return false;
	}
}

///////////////////////////////////////////////////////////////////////////////

std::string CFileDevPSP::GetAbsolutePath(const std::string& path)
{
	bool pathIsAbsolute = CFileEnv::filespec_isabs(path);

	std::string fullFilename;
	if(GetPrependFilesystemBase() && (false == pathIsAbsolute))
	{
		fullFilename = GetFilesystemBase() + path;
	}
	else
	{
		fullFilename = path;
	}

	return CFileEnv::filespec_to_native(fullFilename);
}

///////////////////////////////////////////////////////////////////////////////

std::string CFileDevPSP::GetDeviceFilename(const std::string& unqualifiedName)
{
	return deviceName + GetAbsolutePath(unqualifiedName);
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevPSP::InternalSeek(CFile& rFile, int iTo, int sceWhence)
{
    if(!rFile.IsOpen())
    {
        return EFEC_FILE_NOT_OPEN;
    }

    if(iTo > rFile.miFileLen) 
    { 
        return EFEC_FILE_INVALID_SIZE; 
    } 
 
    SceOff offset = sceIoLseek(rFile.mHandle, iTo, sceWhence);
    if(offset >= 0) 
    { 
        rFile.miSeekPos = iTo; 
        rFile.miFilePos = iTo; 
 
        return EFEC_FILE_OK; 
    } 
    else 
    { 
		return TranslateError(offset);
    }
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevPSP::TranslateError(int sceError)
{
	switch(sceError)
	{
		case SCE_KERNEL_ERROR_OK:
		{
			return EFEC_FILE_OK;
		}
		case SCE_KERNEL_ERROR_BADF:
		{
			return EFEC_FILE_INVALID_ADDRESS;
		}
		case SCE_KERNEL_ERROR_UNSUP:
		{
			return EFEC_FILE_UNSUPPORTED;
		}
		case SCE_KERNEL_ERROR_ILLEGAL_PRIORITY:
		{
			return EFEC_FILE_UNKNOWN;
		}
		case SCE_KERNEL_ERROR_INVAL:
		{
			return EFEC_FILE_UNKNOWN;
		}
		case SCE_KERNEL_ERROR_MFILE:
		{
			return EFEC_FILE_UNKNOWN;
		}
		case SCE_KERNEL_ERROR_NODEV:
		{
			return EFEC_FILE_UNKNOWN;
		}
		case SCE_KERNEL_ERROR_XDEV:
		{
			return EFEC_FILE_UNKNOWN;
		}
		case SCE_KERNEL_ERROR_ALIAS_USED:
		{
			return EFEC_FILE_UNKNOWN;
		}
		case SCE_KERNEL_ERROR_CANNOT_MOUNT:
		{
			return EFEC_FILE_UNKNOWN;
		}
		case SCE_KERNEL_ERROR_DRIVER_DELETED:
		{
			return EFEC_FILE_UNKNOWN;
		}
		case SCE_KERNEL_ERROR_ASYNC_BUSY:
		{
			return EFEC_FILE_UNKNOWN;
		}
		case SCE_KERNEL_ERROR_NOASYNC:
		{
			return EFEC_FILE_UNKNOWN;
		}
		case SCE_KERNEL_ERROR_NOCWD:
		{
			return EFEC_FILE_UNKNOWN;
		}
		case SCE_KERNEL_ERROR_NAMETOOLONG:
		{
			return EFEC_FILE_UNKNOWN;
		}
		default:
		{
			return EFEC_FILE_UNKNOWN;
		}
	}
}

#endif // _PSP
