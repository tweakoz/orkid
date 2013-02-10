////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#if defined(WII)

#include <ork/file/filedevwii.h>
#include <revolution/os.h>
#include <revolution/dvd.h>
#include <ork/kernel/orkpool.h>
#include <queue>
#include <ork/mem/wii_mem.h>

namespace ork
{

//static DVDFileInfo	FileInfoPool;

///////////////////////////////////////////////////////////////////////////////

struct DirEnt
{
	file::Path name;
	DVDDir		dir;
};

CFileDevWII::CFileDevWII( )
	: CFileDev( "Wii", "/", ( EFDF_CAN_READ ) )
{
	DVDInit();
	DVDSetAutoInvalidation(true);

	std::queue<DirEnt> dirstack;
	DirEnt dent;
	dent.name = "/";
	dirstack.push( dent );

	while( false == dirstack.empty() )
	{
		DirEnt top = dirstack.front();
		dirstack.pop();

		//orkprintf( "scanning dir<%s>\n", top.name.c_str() );

		BOOL bd = DVDOpenDir( top.name.c_str(), & top.dir );

		if( bd )
		{
			bool bdone = false;

			while( false == bdone )
			{
				DVDDirEntry entry;
				bd = DVDReadDir( & top.dir, & entry );

				if( bd )
				{
					bool isdir = entry.isDir;

					if( isdir )
					{
						DirEnt subdir;
						std::string bldpath =
								std::string(top.name.c_str())
							+	std::string(entry.name)
							+	std::string("/");
						std::transform( bldpath.begin(), bldpath.end(), bldpath.begin(), lower() );
						subdir.name = bldpath.c_str();
						//orkprintf( "dir <%s>\n", subdir.name.c_str() );
						dirstack.push( subdir );
					}
					else
					{
						std::string bldpath =
								std::string(top.name.c_str())
							+	std::string(entry.name);
						std::transform( bldpath.begin(), bldpath.end(), bldpath.begin(), lower() );
						file::Path pth( bldpath.c_str() );
						mTOC.insert(pth);
						//orkprintf( "file <%s>\n", pth.c_str() );
					}
				}
				else
				{
					bdone = true;
				}
			}
		}
		else
		{
			orkprintf( "ERROR could not open dir<%s>\n", top.name.c_str() );
		}
		DVDCloseDir( & top.dir );
	}
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevWII::DoOpenFile( CFile& rFile )
{
	const file::Path& fname = rFile.GetFileName();
	bool bISAbsolute = fname.IsAbsolute();

	/////////////////////////////////////////////////////////////////////////////
	// compute the filename

	file::Path fullfname = fname.ToAbsolute();

	/////////////////////////////////////////////////////////////////////////////

	DVDFileInfo* pfile = new DVDFileInfo;
	memset( pfile, 0, sizeof(DVDFileInfo) );

	BOOL result = DVDOpen( fullfname.c_str(), pfile );
	rFile.mHandle = reinterpret_cast<U32>(pfile);

	if( result )
	{
		rFile.miFileLen = (int)DVDGetLength(pfile);
#if !defined(WII)
		orkprintf( "OpenFile<%s> Length<%d>\n", fullfname.c_str(), rFile.miFileLen );
#endif
		return EFEC_FILE_OK;
	}
	else
	{
		orkprintf( "OpenFile<%s> NotFound!!\n", fullfname.c_str() );
		return EFEC_FILE_DOES_NOT_EXIST;
	}
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevWII::DoCloseFile( CFile &rFile )
{
	DVDFileInfo* pfile = reinterpret_cast<DVDFileInfo*>(rFile.mHandle);

	BOOL result = DVDClose(pfile);

	delete pfile;

	rFile.mHandle = 0;


	if( FALSE == result )
		return EFEC_FILE_UNKNOWN;

	return EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

//typedef void (*DVDCallback)(s32 result, DVDFileInfo* fileInfo);

volatile static bool boktogo = false;
volatile static int ginumread = 0;
void MyDvdCallback(s32 result, DVDFileInfo* fileInfo)
{
	boktogo = true;
	ginumread = result;
	//orkprintf( "MyDvdCallback<%08x> file<%08x>\n", result, fileInfo );


}

///////////////////////////////////////////////////////////////////////////////

int MyDvdRead( DVDFileInfo* pfile, void* pwhere, int icount, int ioffset )
{
	int ipaddedcount = OSRoundUp32B( icount );
	DVDReadAsync( pfile, pwhere, ipaddedcount, ioffset , MyDvdCallback );
	//s32 result = DVDReadAsync( pfile, alignbuffer, icount, iphyspos, MyDvdCallback );

	while( false == boktogo )
	{


		OSSleepMilliseconds( 1 );
		s32 error = DVDGetDriveStatus();
	
    	switch(error)
    	{
    	  case DVD_STATE_FATAL_ERROR:
    		break;
    	  case DVD_STATE_NO_DISK:
    	  case DVD_STATE_WRONG_DISK:
		    return(-1);
    		break;
    	  case DVD_STATE_RETRY:
			 return(-2);
    		break;
    		break;
    	  case DVD_STATE_MOTOR_STOPPED:
    		break;
		
		}
		//orkprintf("%d error\n",error);


		// DVD_STATE_NO_DISK
		//s32 stat = DVDGetDriveStatus();
		//s32 statf = DVDGetFileInfoStatus(pfile);
		//orkprintf( "dvdstatus<%08x> filestatus<%08x>\n", stat, statf );

		//DVDDumpWaitingQueue();
		//DVDResume();
	}



	int iactualread = ginumread;
	DCStoreRange(pwhere, iactualread); //e see autoinvaidate
	//ICInvalidateRange(pwhere, iactualread);
	return iactualread;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevWII::DoRead( CFile& rFile, void* pTo, int icount, int& iactualread )
{
	/////////////////////////////////////////////////////////
	int iphyspos = rFile.GetPhysicalPos();
	/////////////////////////////////////////////////////////
	//orkprintf( "DoRead<%d> to<%08x> phys<%d>\n", icount, pTo, iphyspos );
	/////////////////////////////////////////////////////////
	OrkAssert( iphyspos%kSEEKALIGN == 0 );
	OrkAssert( icount%kBUFFERALIGN == 0 );
	/////////////////////////////////////////////////////////
	iactualread = 0;
	/////////////////////////////////////////////////////////
	static const int kalignbuffersize = 128<<10;
	static char* alignbuffer = (char*) wii::MEM2Alloc( kalignbuffersize );
	DVDFileInfo* pfile = reinterpret_cast<DVDFileInfo*>(rFile.mHandle);
	U32 aligncheck = reinterpret_cast<U32>(alignbuffer);
	OrkAssert( (aligncheck%32)==0 );
	/////////////////////////////////////////////////////////
	int ifilelen = 0;
	rFile.GetLength(ifilelen);
	int iphysleft = (ifilelen-iphyspos);
	OrkAssert( icount<kalignbuffersize );
	///////////////////////////////////////////
	boktogo = false;

	if( icount <= iphysleft )
	{
		iactualread = MyDvdRead( pfile, alignbuffer, icount, iphyspos );
	}
	/////////////////////////////////////////////////////////
	else // read past end of file, so terminate read buffer with 0's
	{
		int itail = icount-iphysleft;
		iactualread = MyDvdRead( pfile, alignbuffer, iphysleft, iphyspos );
	}

	while(iactualread <0)
	{
		
		if( mWatcher ) mWatcher->Reading( & rFile, iactualread );
		 s32 error = DVDGetDriveStatus();
		 if((error == DVD_STATE_END)) 
		 {
			iactualread = ginumread;
		 }
		 else if((error == DVD_STATE_RETRY)) 
		 {
			iactualread = -2;
		 }
		 else
		 {
			iactualread = -1;
		 }
		 
	}

	if( mWatcher ) mWatcher->Reading( & rFile, 0 );
	
	//result = ginumread;

	///////////////////////////////////////////
	if(iactualread > -1)
	{
		int ioldphys = rFile.GetPhysicalPos();
		rFile.SetPhysicalPos( ioldphys+iactualread );

		memcpy( pTo, alignbuffer, icount );
		//iactualread = result;
	}
	else
	{
		orkprintf("FileError: EFEC_FILE_UNKNOWN\n");
		return EFEC_FILE_UNKNOWN;
	}

	OrkAssert( iactualread< kalignbuffersize );
	///////////////////////////////////////////
	//OrkAssert(iactualread == icount);
	//DCFlushRange(pTo, u32(iactualread));
	///////////////////////////////////////////
	return EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevWII::Write( CFile &rFile, const void *pFrom, int iSize )
{
	OrkAssert(false);
	return EFEC_FILE_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevWII::DoSeekFromStart( CFile &rFile, int iTo )
{
	DVDFileInfo* pfile = reinterpret_cast<DVDFileInfo*>(rFile.mHandle);
	OrkAssert( rFile.IsOpen() );

	OrkAssert( iTo <= rFile.miFileLen );
	if(iTo > rFile.miFileLen)
		return EFEC_FILE_INVALID_SIZE;

	OrkAssert( (iTo%4) == 0 );

	S32 result = DVDSeek( pfile, iTo );

	if( result < 0 )
		return EFEC_FILE_UNKNOWN;

	rFile.SetPhysicalPos( iTo );

	return EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevWII::DoSeekFromCurrent( CFile &rFile, int iOffset )
{
	DVDFileInfo* pfile = reinterpret_cast<DVDFileInfo*>(rFile.mHandle);
	OrkAssert( rFile.IsOpen() );

	int inext = rFile.GetPhysicalPos() + iOffset;

	OrkAssert( inext <= rFile.miFileLen );
	if(inext > rFile.miFileLen)
		return EFEC_FILE_INVALID_SIZE;

	OrkAssert( (inext%4) == 0 );

	S32 result = DVDSeek( pfile, inext );

	if( result < 0 )
		return EFEC_FILE_UNKNOWN;

	rFile.SetPhysicalPos( inext );

	return EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevWII::DoGetLength( CFile &rFile, int &riLen )
{
	DVDFileInfo* pfile = reinterpret_cast<DVDFileInfo*>(rFile.mHandle);
	OrkAssert( rFile.IsOpen() );
	const file::Path& fname = rFile.GetFileName();
	bool bISAbsolute = fname.IsAbsolute();

	/////////////////////////////////////////////////////////////////////////////
	// compute the filename

	file::Path fullfname = fname.ToAbsolute();

	/////////////////////////////////////////////////////////////////////////////

	DVDFileInfo file;
	if( DVDOpen( fullfname.c_str(), &file ) )
	{
		riLen = (int)DVDGetLength(&file);
		DVDClose(&file);

		//orkprintf( "file<%s> length<%d>\n", fullfname.c_str(), riLen );
		return EFEC_FILE_OK;
	}
	else
	{
		return EFEC_FILE_DOES_NOT_EXIST;
	}
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevWII::GetCurrentDirectory(file::Path::NameType& directory)
{
	directory = "/";
	return EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevWII::SetCurrentDirectory(const file::Path::NameType& inspec)
{
	return EFEC_FILE_UNSUPPORTED;
}

///////////////////////////////////////////////////////////////////////////////

bool CFileDevWII::DoesFileExist( const file::Path& filespec )
{
	file::Path absol = filespec.ToAbsolute();
	std::string lowstr( absol.c_str() );
	std::transform( lowstr.begin(), lowstr.end(), lowstr.begin(), lower() );

	orkset<file::Path>::const_iterator it = mTOC.find(file::Path(lowstr.c_str()));

	if( it != mTOC.end() )
	{
		//orkprintf( "DoesFileExist<%s>: true\n", absol.c_str() );
		return true;
	}
	//orkprintf( "DoesFileExist<%s>: false\n", absol.c_str() );
	return false;
}

///////////////////////////////////////////////////////////////////////////////

bool CFileDevWII::DoesDirectoryExist( const file::Path& filespec )
{
	file::Path absol = filespec.ToAbsolute();
	std::string lowstr( absol.c_str() );
	std::transform( lowstr.begin(), lowstr.end(), lowstr.begin(), lower() );

	orkset<file::Path>::const_iterator it = mTOC.find(lowstr.c_str());

	if( it != mTOC.end() )
	{
		//orkprintf( "DoesDirectoryExist<%s>: true\n", absol.c_str() );
		return true;
	}
	//orkprintf( "DoesDirectoryExist<%s>: false\n", absol.c_str() );
	return false;
}

///////////////////////////////////////////////////////////////////////////////

bool CFileDevWII::IsFileWritable( const file::Path& filespec )
{
	return false;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork

#endif // WII
