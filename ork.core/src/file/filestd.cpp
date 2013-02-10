////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/kernel/string/string.h>

#if defined(_WIN32) || defined(IX)

#include <ork/file/filestd.h>

using ork::CFileDev;
using ork::CFile;
using ork::FileH;
using ork::EFileErrCode;

#include <errno.h>
#include <algorithm>

#if defined (_WIN32)
# include <io.h> /* _findfirst and _findnext set errno iff they return -1 */
#elif defined( IX )
# include <unistd.h>
# include <glob.h>
# include <fts.h>
# include <string.h>
#endif

#if defined( WII )
#include <revolution/os.h>
#endif

#if defined(_XBOX)
void XboxFileErrorHandler(DWORD derror);
#endif


///////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
BOOL MyGetFileAttributesEx(
	__in   LPCTSTR lpFileName,
	__in   GET_FILEEX_INFO_LEVELS fInfoLevelId,
	__out  LPVOID fileInfo
						   )
{
	std::string outstr = ork::CreateFormattedString( "GetFileAttributesEx<%s>\n", lpFileName );
	//OutputDebugString(outstr.c_str());
#if defined(_XBOX)
	XboxFileErrorHandler(0);
#endif
	return GetFileAttributesEx(lpFileName, fInfoLevelId, (void*)fileInfo);
}

HANDLE MyCreateFile(
__in      LPCTSTR lpFileName,
  __in      DWORD dwDesiredAccess,
  __in      DWORD dwShareMode,
  __in_opt  LPSECURITY_ATTRIBUTES lpSecurityAttributes,
  __in      DWORD dwCreationDisposition,
  __in      DWORD dwFlagsAndAttributes,
  __in_opt  HANDLE hTemplateFile,
  bool	bREQUIRED
)
{
	std::string outstr = ork::CreateFormattedString( "CreateFile<%s>\n", lpFileName );
	//OutputDebugString(outstr.c_str());

	HANDLE h = 0;

#if defined(_XBOX)
	XboxFileErrorHandler(0);
#endif

	if( bREQUIRED )
	{
		while( 0 == h )
		{
			h = CreateFile(	lpFileName,
							dwDesiredAccess,
							dwShareMode,
							lpSecurityAttributes,
							dwCreationDisposition,
							dwFlagsAndAttributes,
							hTemplateFile );

			if( 0 == h )
			{
#if defined(_XBOX)
				XboxFileErrorHandler(GetLastError());
#endif
			}
		}
	}
	else
	{
		h = CreateFile(	lpFileName,
						dwDesiredAccess,
						dwShareMode,
						lpSecurityAttributes,
						dwCreationDisposition,
						dwFlagsAndAttributes,
						hTemplateFile );
	}

	return h;
}

BOOL MyReadFile(
  __in         HANDLE hFile,
  __out        LPVOID lpBuffer,
  __in         DWORD nNumberOfBytesToRead,
  __out_opt    LPDWORD lpNumberOfBytesRead,
  __inout_opt  LPOVERLAPPED lpOverlapped
				)
{
	BOOL bRES = FALSE;
	while( FALSE == bRES )
	{
		bRES = ReadFile(hFile,lpBuffer,nNumberOfBytesToRead,lpNumberOfBytesRead,lpOverlapped);
		if( FALSE==bRES )
		{
#if defined(_XBOX)
				XboxFileErrorHandler(GetLastError());
#endif
		}
	}
	return bRES;
}
#endif

///////////////////////////////////////////////////////////////////////////////

namespace ork {

///////////////////////////////////////////////////////////////////////////////

CFileDevStd::CFileDevStd( void )
	: CFileDev( "StandardIO",
				"./",
				( ork::EFDF_CAN_READ| ork::EFDF_CAN_WRITE ) )
{
# if defined( _OSX )
	char buffer[256];

	CFBundleRef Bundle = CFBundleGetMainBundle ();
	CFURLRef URL = CFBundleCopyBundleURL ( Bundle );
    CFStringRef PathStr = CFURLCopyFileSystemPath ( URL, kCFURLPOSIXPathStyle );
    Boolean bval = CFStringGetCString ( PathStr, buffer, 256, kCFStringEncodingASCII );

	orkprintf( "cwd %s\n", buffer );

	chdir( buffer );
# endif
}

///////////////////////////////////////////////////////////////////////////////

static int GetLengthFromToc(const ork::file::Path& fname)
{
#if defined(_WIN32)
	file::Path::SmallNameType url = fname.GetUrlBase();
	const SFileDevContext& ctx = ork::CFileEnv::UrlBaseToContext(url);
	if( ctx.GetTocMode() == ork::ETM_USE_TOC )
	{
		const orkmap<file::Path::NameType,int>& toc = ctx.GetTOC();

		file::Path testf = fname;
		testf.SetUrlBase("");
		file::Path::NameType testn(testf.c_str());

		size_t ilen = testn.length();
		for( size_t i=0; i<ilen; i++ )
		{
			char ch = testn.c_str()[i];

			if( ch == '\\' ) ch = '/';
			else if( ch>='A' && ch<='Z' ) ch = (ch-'A')+'a';
			
			testn.SetChar( i, ch );
		}
		orkmap<file::Path::NameType,int>::const_iterator it = toc.find( testn.c_str() );

		if( it != toc.end() )
		{
			return int(it->second);
		}
	}

	WIN32_FILE_ATTRIBUTE_DATA   fileInfo;
	ork::file::Path fullfname = fname.ToAbsolute();

	BOOL bOK = MyGetFileAttributesEx(fullfname.c_str(), GetFileExInfoStandard, (void*)&fileInfo);
	if( bOK )
	{
		return int(fileInfo.nFileSizeLow);
	}
#endif
	return -1;
}


///////////////////////////////////////////////////////////////////////////////

int ifilecount = 0;

EFileErrCode CFileDevStd::DoOpenFile( CFile &rFile )
{
	ifilecount++;

	const ork::file::Path& fname = rFile.GetFileName();

	bool breading = (false==rFile.Writing());
	bool bwriting = rFile.Writing();

//    printf( "OpenFile<%s> Read<%d> Write<%d>\n", fname.c_str(), int(breading), int(bwriting) );
    
	/////////////////////////////////////////////////////////////////////////////
	// compute the filename

	ork::file::Path fullfname = fname.ToAbsolute();

	if( breading )
	{
		bool bexists = this->DoesFileExist( fname );

		if( false == bexists )
		{
			return ork::EFEC_FILE_DOES_NOT_EXIST;
		}

		// reading and it exists

		if( CFileEnv::GetLinFileMode() == ELFM_READ )
		{
			int ilen = GetLengthFromToc(fname);
			rFile.miFileLen = ilen;
			rFile.mHandle = 0;
			return ork::EFEC_FILE_OK;

		}

	}

	/////////////////////////////////////////////////////////////////////////////

#if defined(WIN32)
	

	HANDLE h = MyCreateFile(	fullfname.c_str(),
							bwriting ? GENERIC_WRITE : GENERIC_READ,
							FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
							NULL,
							bwriting ? CREATE_ALWAYS : OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL,
							NULL,
							true );

	bool bv = (HANDLE(-1)!=h);
	if( bv )
	{
		file::Path::SmallNameType url = fname.GetUrlBase();
		const SFileDevContext& ctx = ork::CFileEnv::UrlBaseToContext(url);
		bool bTRYTOC = false;
		bool bEXISTSINTOC = false;
		int iTOCSIZE = -1;
		if( ctx.GetTocMode() == ork::ETM_USE_TOC )
		{
			bTRYTOC = true;

			const orkmap<file::Path::NameType,int>& toc = ctx.GetTOC();

			file::Path testf = fname;
			testf.SetUrlBase("");
			file::Path::NameType testn(testf.c_str());

			size_t ilen = testn.length();
			for( size_t i=0; i<ilen; i++ )
			{
				char ch = testn.c_str()[i];

				if( ch == '\\' ) ch = '/';
				else if( ch>='A' && ch<='Z' ) ch = (ch-'A')+'a';
				
				testn.SetChar( i, ch );
			}
			orkmap<file::Path::NameType,int>::const_iterator it = toc.find( testn.c_str() );

			if( it != toc.end() )
			{
				iTOCSIZE = it->second;
				bEXISTSINTOC = true;
#if ! defined(TEST_TOC)
				rFile.miFileLen = iTOCSIZE;
				rFile.mHandle = reinterpret_cast<FileH>(h);
				return ork::EFEC_FILE_OK;
#endif
			}
			else
			{
#if ! defined(TEST_TOC)
				rFile.mHandle = 0;
				return ork::EFEC_FILE_DOES_NOT_EXIST;
#endif
			}
		}
	
		WIN32_FILE_ATTRIBUTE_DATA   fileInfo;

		BOOL fOk = MyGetFileAttributesEx(fullfname.c_str(), GetFileExInfoStandard, (void*)&fileInfo);
		OrkAssert(fOk==TRUE);
		OrkAssert(0 == fileInfo.nFileSizeHigh);
		rFile.miFileLen = fileInfo.nFileSizeLow;
		rFile.mHandle = reinterpret_cast<FileH>(h);


#if defined(TEST_TOC)
		if( bEXISTSINTOC )
		{
			OrkAssert( iTOCSIZE == rFile.miFileLen );
		}
#endif

		return ork::EFEC_FILE_OK;
	}
	rFile.mHandle = 0;
	return ork::EFEC_FILE_DOES_NOT_EXIST;

#else
	const char * strMode = 0;
	if( rFile.Appending() ) strMode = "at+";
	else if( rFile.Writing() ) strMode = "wb";
	else if( rFile.Reading() ) strMode = "rb";
	//if ( !rFile.Ascii() ) {	strMode += "b"; }

	rFile.mHandle = reinterpret_cast<FileH>( fopen( fullfname.c_str(), strMode ) );

	if( rFile.mHandle )
	{
		FILE *pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
		fseek( pFILE, 0, SEEK_END );
		rFile.miFileLen = ftell( pFILE );
		fseek( pFILE, 0, SEEK_SET );

		return ork::EFEC_FILE_OK;
	}
	else
		return ork::EFEC_FILE_DOES_NOT_EXIST;
#endif
}

///////////////////////////////////////////////////////////////////////////////

ork::EFileErrCode CFileDevStd::DoCloseFile( CFile &rFile )
{
	ifilecount--;
#if defined(WIN32)
	HANDLE h = reinterpret_cast<HANDLE>(rFile.mHandle);
	if(rFile.mHandle!=0)
	{
		BOOL bOK = CloseHandle(h);
#if defined(_XBOX)
		if( FALSE == bOK )
		{
			XboxFileErrorHandler(GetLastError());
		}
#endif
	}
#else
	FILE *pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
	if(pFILE )	fclose( pFILE );
#endif
	return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

ork::EFileErrCode CFileDevStd::DoRead( CFile &rFile, void *pTo, size_t icount, size_t& iactualread )
{
#if defined(WIN32)
	HANDLE hfile = reinterpret_cast<HANDLE>(rFile.mHandle);
	size_t iphyspos = rFile.GetPhysicalPos();
#else
	FILE *pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
	int iphyspos = ftell( pFILE );
    if( rFile.GetPhysicalPos() != iphyspos )
    {
        printf( "rFile.GetPhysicalPos()<%d> iphyspos<%d>\n", int(rFile.GetPhysicalPos()), iphyspos );
    }
	OrkAssert( rFile.GetPhysicalPos() == iphyspos );
#endif
	/////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////
	//orkprintf( "DoRead<%d> tell<%d> phys<%d>\n", icount, iphyspos, rFile.GetPhysicalPos() );
	/////////////////////////////////////////////////////////

	if( kPerformBuffering && rFile.IsBufferingEnabled()  )
	{
		OrkAssert( iphyspos%kSEEKALIGN == 0 );
		OrkAssert( icount%kBUFFERALIGN == 0 );
	}
	/////////////////////////////////////////////////////////
	iactualread = 0;
	/////////////////////////////////////////////////////////
	size_t ifilelen = 0;
	rFile.GetLength(ifilelen);
	size_t iphysleft = (ifilelen-iphyspos);
	/////////////////////////////////////////////////////////
	if( icount <= iphysleft )
	{
		///////////////////////////////
		#if defined(WIN32)
		///////////////////////////////
		DWORD outb = 0;
		BOOL BOK = MyReadFile(hfile,pTo,(int)icount,&outb,0);
		OrkAssert(BOK);
		//OrkAssert(outb==icount);
		iactualread = size_t(outb);
		///////////////////////////////
		#else
		///////////////////////////////
		iactualread = fread( pTo, 1, icount, pFILE );
		///////////////////////////////
		#endif
		///////////////////////////////
		#if defined(WII)
		DCStoreRange(pTo, icount);
		ICInvalidateRange(pTo, icount);
		#endif
		///////////////////////////////
	}
	/////////////////////////////////////////////////////////
	else // read past end of file, so terminate read buffer with 0's
	{
		///////////////////////////////
		#if defined(WIN32)
		///////////////////////////////
		DWORD outb = 0;
		BOOL BOK = MyReadFile(hfile,pTo,(int)iphysleft,&outb,0);
		OrkAssert(BOK);
		//OrkAssert(outb==icount);
		iactualread = size_t(outb);
		///////////////////////////////
		#else
		///////////////////////////////
		iactualread = fread( pTo, 1, iphysleft, pFILE );
		///////////////////////////////
		#endif
		#if defined(WII)
		DCStoreRange(pTo, iactualread);
		ICInvalidateRange(pTo, iactualread);
		#endif
		intptr_t itail = icount-iphysleft;
		memset( ((char*)pTo)+iphysleft, 0, itail );
	}

	if( mWatcher ) mWatcher->Reading( & rFile, iactualread );

	/////////////////////////////////////////////////////////
	//orkprintf( "PostDoRead<%d> tell<%d> phys<%d>\n", iSize, ipos, rFile.GetPhysicalPos() );
	/////////////////////////////////////////////////////////
	if( iactualread > 0)
	{
		size_t ioldphys = rFile.GetPhysicalPos();
		rFile.SetPhysicalPos( ioldphys+iactualread );
	}
	else
	{
		perror("CFileDevStd::DoReadFailed: ");
		return ork::EFEC_FILE_UNKNOWN;
	}
	/////////////////////////////////////////////////////////
	return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

ork::EFileErrCode CFileDevStd::Write( CFile &rFile, const void *pFrom, size_t iSize )
{
	if(!rFile.IsOpen())
	{
		return ork::EFEC_FILE_NOT_OPEN;
	}

	if(!rFile.Writing() && !rFile.Appending())
	{
		return ork::EFEC_FILE_INVALID_MODE;
	}

	///////////////////////////////
	///////////////////////////////

	#if defined(WIN32)
	///////////////////////////////
	HANDLE hfile = reinterpret_cast<HANDLE>(rFile.mHandle);
	OrkAssert( hfile!=HANDLE(0) );
	DWORD numwrote = 0;
	BOOL BOK = WriteFile(hfile,pFrom,(int)iSize,&numwrote,0);
	OrkAssert(BOK);
	///////////////////////////////
	#else
	///////////////////////////////
    //printf( "write<%d> bytes\n", int(iSize) );
	FILE *pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
	OrkAssert( pFILE );
	fwrite( pFrom, 1, iSize, pFILE );
	fflush( pFILE );
	///////////////////////////////
	#endif

	size_t inewpos = rFile.GetPhysicalPos() + iSize;
	rFile.SetPhysicalPos( inewpos );
	rFile.SetUserPos(inewpos);

	return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevStd::DoSeekFromStart( CFile &rFile, size_t iTo )
{
	#if defined(WIN32)
	///////////////////////////////
	HANDLE hfile = reinterpret_cast<HANDLE>(rFile.mHandle);
	OrkAssert( hfile!=HANDLE(0) );
	DWORD res = SetFilePointer(hfile,(int)iTo,0,FILE_BEGIN);
	///////////////////////////////
	#else
	///////////////////////////////
	FILE *pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
	OrkAssert( pFILE );
	fseek( pFILE, iTo, SEEK_SET );
	#endif
	///////////////////////////////
    //printf( "CFileDevStd::DoSeekFromStart( iTo<%d> )\n", int(iTo) );
	rFile.SetPhysicalPos( iTo );
	return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

ork::EFileErrCode CFileDevStd::DoSeekFromCurrent( CFile &rFile, size_t iOffset )
{
	#if defined(WIN32)
	///////////////////////////////
	HANDLE hfile = reinterpret_cast<HANDLE>(rFile.mHandle);
	OrkAssert( hfile!=HANDLE(0) );
	DWORD res = SetFilePointer(hfile,(int)iOffset,0,FILE_CURRENT);
	///////////////////////////////
	#else
	///////////////////////////////
	FILE *pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
	OrkAssert( pFILE );
	fseek( pFILE, iOffset, SEEK_CUR );
	#endif
	///////////////////////////////
	rFile.SetPhysicalPos( rFile.GetPhysicalPos()+iOffset );
	return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode CFileDevStd::DoGetLength( CFile &rFile, size_t &riLen )
{
	riLen = 0;

	const ork::file::Path& fname = rFile.GetFileName();
	file::Path::SmallNameType url = fname.GetUrlBase();
	const SFileDevContext& ctx = ork::CFileEnv::UrlBaseToContext(url);
	bool bTRYTOC = false;
	bool bEXISTSINTOC = false;
	int iTOCSIZE = -1;
	if( ctx.GetTocMode() == ork::ETM_USE_TOC )
	{
		bTRYTOC = true;

		const orkmap<file::Path::NameType,int>& toc = ctx.GetTOC();

		file::Path testf = fname;
		testf.SetUrlBase("");
		file::Path::NameType testn(testf.c_str());

		size_t ilen = testn.length();
		for( size_t i=0; i<ilen; i++ )
		{
			char ch = testn.c_str()[i];

			if( ch == '\\' ) ch = '/';
			else if( ch>='A' && ch<='Z' ) ch = (ch-'A')+'a';
			
			testn.SetChar( i, ch );
		}
		orkmap<file::Path::NameType,int>::const_iterator it = toc.find( testn.c_str() );

		if( it != toc.end() )
		{
			iTOCSIZE = it->second;
			bEXISTSINTOC = true;
#if defined(TEST_TOC)
			rFile.miFileLen = iTOCSIZE;
#endif
		}
		else
		{
#if defined(TEST_TOC)
			rFile.mHandle = 0;
			return ork::EFEC_FILE_DOES_NOT_EXIST;
#endif
		}
	}

	///////////////////////////////
	///////////////////////////////
#if defined(WIN32)
	///////////////////////////////
	///////////////////////////////
	
	HANDLE hfile = reinterpret_cast<HANDLE>(rFile.mHandle);
	if( hfile!=HANDLE(0) )
	{
		DWORD dhisize = 0;
		riLen = GetFileSize( hfile,&dhisize);
		OrkAssert(dhisize==0);
	}
	///////////////////////////////

    #if defined(TEST_TOC)
	if( bTRYTOC )
	{
		OrkAssert( riLen == iTOCSIZE );
	}
    #endif

	///////////////////////////////
	///////////////////////////////
#else // ! WIN32
	///////////////////////////////
	///////////////////////////////

	FILE *pFILE = reinterpret_cast<FILE*>(rFile.mHandle);
	if( pFILE )
	{
		fseek( pFILE, 0, SEEK_END );
		riLen = ftell( pFILE );
		fseek( pFILE, 0, SEEK_SET );
	}
    else
    {
        return EFEC_FILE_NOT_OPEN;
    
    }
	#endif
	///////////////////////////////
	return ork::EFEC_FILE_OK;
}

///////////////////////////////////////////////
// GetCurrentDirectory/SetCurrentDirectory
// these are currently broken , they "should" be the current directory of the device,
// but what they are is the current directory of the process
///////////////////////////////////////////////

EFileErrCode CFileDevStd::GetCurrentDirectory( file::Path::NameType& directory )
{
	file::Path::NameType outspec;
#if defined( _XBOX )
	outspec = "";
#elif defined( WIN32 )
	char dirbuf[256];
	dirbuf[0] = 0;
	DWORD len = ::GetCurrentDirectory(256, dirbuf);
	outspec = file::Path::NameType(dirbuf);
#elif defined(WII)
	outspec = "";
#else
	// Not implemented for this platform!
	OrkAssert(false);
	return EFEC_FILE_UNSUPPORTED;
#endif
	directory = outspec;
	return ork::EFEC_FILE_OK;
}

EFileErrCode CFileDevStd::SetCurrentDirectory( const file::Path::NameType& directory )
{
#if defined( _XBOX )
	return ork::EFEC_FILE_OK;
#elif defined(WII)
	return ork::EFEC_FILE_OK;
#elif defined( _WIN32 )
  if(::SetCurrentDirectory(directory.c_str()))
	{
		return ork::EFEC_FILE_OK;
	}
	else
	{
	return ork::EFEC_FILE_UNKNOWN;
	}
#else
	OrkAssert(false);
	return ork::EFEC_FILE_UNKNOWN;
#endif
}

///////////////////////////////////////////////////////////////////////////////

bool CFileDevStd::DoesFileExist( const file::Path& filespec )
{
	file::Path::SmallNameType url = filespec.GetUrlBase();
	const SFileDevContext& ctx = ork::CFileEnv::UrlBaseToContext(url);

	bool bTRYTOC = false;
	bool bEXISTSINTOC = false;

    //printf( "DoesFileExist<%s>\n", filespec.c_str() );

	if( ctx.GetTocMode() == ork::ETM_USE_TOC )
	{

		bTRYTOC = true;

		const orkmap<file::Path::NameType,int>& toc = ctx.GetTOC();

		file::Path testf = filespec;
		testf.SetUrlBase("");
		file::Path::NameType testn(testf.c_str());

		size_t ilen = testn.length();
		for( size_t i=0; i<ilen; i++ )
		{
			char ch = testn.c_str()[i];

			if( ch == '\\' ) ch = '/';
			else if( ch>='A' && ch<='Z' ) ch = (ch-'A')+'a';
			
			testn.SetChar( i, ch );
		}
		orkmap<file::Path::NameType,int>::const_iterator it = toc.find( testn.c_str() );

		if( it != toc.end() )
		{
			bEXISTSINTOC = true;
#if ! defined(TEST_TOC)
				return true;
#endif
		}
#if ! defined(TEST_TOC)
		return false;
#endif
	}


	file::Path pathspec( filespec.c_str() );

	file::Path abspath = pathspec.ToAbsolute();

	const char *pFn = abspath.c_str();

    //printf( "DoesFileExist<%s> Abs<%s>\n", filespec.c_str(), abspath.c_str() );

#if defined( _WIN32 )
	HANDLE h = MyCreateFile(	pFn,
								GENERIC_READ,
								FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
								NULL,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL,
								NULL,
								false );

	bool bv = (HANDLE(-1)!=h);
	if(bv) CloseHandle(h);
#else
	FILE *fin = fopen( abspath.c_str(), "rb" );
	bool bv = (fin==0) ? false : true;
	if( fin!=0 ) fclose( fin );

	if( false == bv )
	{
		ork::CFileEnvDir * pdir = ork::CFileEnv::GetRef().OpenDir(filespec.c_str());
		bv = (pdir!=0);
		if( pdir ) ork::CFileEnv::GetRef().CloseDir( pdir );
	}
#endif

#if defined(TEST_TOC)
	if( bTRYTOC )
	{
		OrkAssert(bv==bEXISTSINTOC);
	}
#endif
	return bv;

}

///////////////////////////////////////////////////////////////////////////////

bool CFileDevStd::IsFileWritable( const file::Path& filespec )
{
	file::Path absol = filespec.ToAbsolute();

	FILE* fin = fopen(absol.c_str(), "a+");
	bool bv = (fin==0) ? false : true;
	if(fin != 0) fclose(fin);

	return bv;
}

///////////////////////////////////////////////////////////////////////////////

bool CFileDevStd::DoesDirectoryExist( const file::Path& filespec )
{
	file::Path absol = filespec.ToAbsolute();

	const char *pFn = absol.c_str();

#if defined(_XBOX)
	HANDLE h = MyCreateFile(	pFn,
								GENERIC_READ,
								FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
								NULL,
								OPEN_EXISTING,
								FILE_ATTRIBUTE_NORMAL,
								NULL,
								false );

	bool bv = (HANDLE(-1)!=h);
#elif defined( _WIN32 )
	bool bv = GetFileAttributes(absol.c_str()) != INVALID_FILE_ATTRIBUTES;   
 
	
#else
	bool bv = false;
#endif
	if( bv )
	{
#if defined( _XBOX )
		CloseHandle( h );
#endif
	}
	return bv;
}

///////////////////////////////////////////////////////////////////////////////

ork::FileStampH ork::CFileEnv::EncodeFileStamp( int year, int month, int day, int hour, int minute, int second )
{
	OrkAssert( year >= 2000 );
	OrkAssert( year <= 2063 );
	OrkAssert( month >=0 );
	OrkAssert( month <=12 );
	OrkAssert( day >=0 );
	OrkAssert( day <=31 );
	OrkAssert( hour >=0 );
	OrkAssert( hour <=24 );
	OrkAssert( minute >=0 );
	OrkAssert( minute <=60 );
	OrkAssert( second >=0 );
	OrkAssert( second <=60 );
	year -= 2000;
	U32 Result = (year<<26) + (month<<22) + (day<<17) + (hour<<12) + (minute<<6) + second;
	return static_cast< FileStampH >( Result );
}

///////////////////////////////////////////////////////////////////////////////

void ork::CFileEnv::DecodeFileStamp( ork::FileStampH stamp, int& year, int& month, int& day, int& hour, int& minute, int& second )
{
	U32 UStamp = static_cast<U32>( stamp );
	year	= ((stamp&0xfc000000)>>26) + 2000;
	month	= ((stamp&0x03c00000)>>22);
	day		= ((stamp&0x003e0000)>>17);
	hour	= ((stamp&0x0001f000)>>12);
	minute	= ((stamp&0x00000fc0)>>6);
	second	=  (stamp&0x0000003f);
}

///////////////////////////////////////////////////////////////////////////////
/*
typedef struct _SYSTEMTIME {
WORD wYear;
WORD wMonth;
WORD wDayOfWeek;
WORD wDay;
WORD wHour;
WORD wMinute;
WORD wSecond;
WORD wMilliseconds; } SYSTEMTIME, *PSYSTEMTIME;
*/

ork::FileStampH ork::CFileEnv::GetFileStamp( const file::Path::NameType & filespec )
{
	ork::FileStampH Result = 0x00000000;
#ifdef WIN32
	HANDLE hFile = MyCreateFile(
			filespec.c_str(),
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL,
			true
			);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		FILETIME FileTime;
		SYSTEMTIME SysTime;
		GetFileTime(hFile, NULL, NULL, &FileTime);
		FileTimeToSystemTime( & FileTime, & SysTime );
		CloseHandle(hFile);

		Result = EncodeFileStamp( SysTime.wYear, SysTime.wMonth, SysTime.wDay, SysTime.wHour, SysTime.wMinute, SysTime.wSecond );
	}
#endif
	return Result;
}

///////////////////////////////////////////////////////////////////////////////

ork::CFileEnvDir *ork::CFileEnv::OpenDir(const char *name)
{
    ork::CFileEnvDir *dir = 0;
#if defined( WIN32 )
    if(name && name[0])
    {
        size_t base_length = strlen(name);
        const char *all = ///* search pattern must end with suitable wildcard */
            strchr("/\\", name[base_length - 1]) ? "*" : "/*";

		u32 struct_size = sizeof(CFileEnvDir) + sizeof(_finddata_t) - 1;

		dir = (CFileEnvDir*) new u8[struct_size];
		new(dir) CFileEnvDir();

		dir->name = file::Path::NameType(name) + file::Path::NameType(all);

		WIN32_FIND_DATA FileData;

		HANDLE hList = FindFirstFile( dir->name.c_str(), & FileData );

		if( hList == HANDLE(-1) )
		{
            dir->result = "";
			if( dir ) delete[] dir;
			dir = 0;
		}
		else if (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{

		}
		else
		{
            dir->result = "";
			if( dir ) delete[] dir;
			dir = 0;
		}

    }
    else
        errno = EINVAL;

#endif
    return dir;
}

int ork::CFileEnv::CloseDir(ork::CFileEnvDir *dir)
{
    int result = -1;
#if defined( WIN32 )

    if(dir)
    {
		dir->handle = -1;
		dir->name = "";
        if(dir->handle != -1)
        {
            //result = _findclose(dir->handle);
        }
		delete dir;
	}

    if(result == -1) /* map all errors to EBADF */
    {
        errno = EBADF;
    }

#endif
    return result;
}

file::Path::NameType ork::CFileEnv::ReadDir(ork::CFileEnvDir *dir)
{
	file::Path::NameType result;
#if defined( WIN32 )

    if(dir && dir->handle != -1)
    {
        if((dir->result.length() != 0) || _findnext(dir->handle, reinterpret_cast<_finddata_t*>(&dir->info)) != -1)
        {
			result	= dir->result;
            result	= (reinterpret_cast<_finddata_t*>(&dir->info))->name;
        }
    }
    else
    {
        errno = EBADF;
    }

#endif
    return result;
}

void ork::CFileEnv::RewindDir(ork::CFileEnvDir *dir)
{
#if defined( WIN32 )
    if(dir && dir->handle != -1)
    {
        _findclose(dir->handle);
        dir->handle = (long) _findfirst(dir->name.c_str(), reinterpret_cast<_finddata_t*>(&dir->info));
        dir->result = "";
    }
    else
        errno = EBADF;
#endif
}

///////////////////////////////////////////////////////////////////////////////

int unix2winpathsep(int c)
{	int rval = c;
	if( rval == '/' ) rval = '\\';
	return rval;
}
int win2unixpathsep(int c)
{	int rval = c;
	if( rval == '\\' ) rval = '/';
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

int wildcmp( const char *wild, const char *string)
{	const char *cp = 0, *mp = 0;
	while ((*string) && (*wild != '*'))
	{	if ((*wild != *string) && (*wild != '?'))
		{	return 0;
		}
		wild++;
		string++;
	}
	while (*string)
	{	if (*wild == '*')
		{	if (!*++wild)
			{	return 1;
			}
			mp = wild;
			cp = string+1;
		}
		else if ((*wild == *string) || (*wild == '?'))
		{	wild++;
			string++;
		}
		else
		{	wild = mp;
			string = cp++;
		}
	}
	while (*wild == '*')
	{	wild++;
	}
	return !*wild;
}

///////////////////////////////////////////////////////////////////////////////

orkset<file::Path::NameType> ork::CFileEnv::filespec_search_sorted( const file::Path::NameType & wildcards, const ork::file::Path& initdir )
{
	orkvector<file::Path::NameType> files = filespec_search( wildcards, initdir );
	orkset<file::Path::NameType> rval;

	size_t inumf = files.size();

	for( size_t i=0; i<inumf; i++ )
	{
		rval.insert( files[i] );
	}

	return rval;
}

///////////////////////////////////////////////////////////////////////////////

orkvector<file::Path::NameType> ork::CFileEnv::filespec_search( const file::Path::NameType & wildcards, const ork::file::Path& initdir )
{
	orkvector<file::Path::NameType> rval;
	
	file::Path::NameType _wildcards = wildcards;
	if( _wildcards == (file::Path::NameType) "" )
		_wildcards = (file::Path::NameType) "*";

#if defined( IX )	

	const char* path = initdir.ToAbsolute(ork::file::Path::EPATHTYPE_POSIX).c_str();
	char* const paths[]= 
	{
		(char* const) path,
		0
	};

	FTS *tree = fts_open(&paths[0], FTS_NOCHDIR, 0);
	if (!tree) {
		perror("fts_open");
		OrkAssert(false);
		return rval;
	}

	FTSENT *node;
	while ((node = fts_read(tree)))
	{
		if (node->fts_level > 0 && node->fts_name[0] == '.')
			fts_set(tree, node, FTS_SKIP);
		else if (node->fts_info & FTS_F) {
			
			
			int match = wildcmp( _wildcards.c_str(), node->fts_name );
			if( match )
			{
				file::Path::NameType fullname = node->fts_accpath;
				rval.push_back( fullname );
				//orkprintf( "file found <%s>\n", fullname.c_str() );
			}

			
//			printf("got file named %s at depth %d, "
//			"accessible via %s from the current directory "
//			"or via %s from the original starting directory\n",
//			node->fts_name, node->fts_level,
//			node->fts_accpath, node->fts_path);
			/* if fts_open is not given FTS_NOCHDIR,
			* fts may change the program's current working directory */
		}
	}
	if (errno) {
		perror("fts_read");
		OrkAssert(false);
		return rval;
	}

	if (fts_close(tree)) {
		perror("fts_close");
		OrkAssert(false);
		return rval;
	}

#elif defined( WIN32 )

	file::Path::NameType srchdir( initdir.ToAbsolute(ork::file::Path::EPATHTYPE_DOS).c_str() );

	if( srchdir == (file::Path::NameType) "" )
		srchdir = (file::Path::NameType) "./";


	file::Path::NameType olddir = file::GetCurDir();
	DWORD ret;
	//HANDLE h = 0;
	std::transform( srchdir.begin(), srchdir.end(), srchdir.begin(), unix2winpathsep );

	std::stack<file::Path::NameType> dirstack;
	dirstack.push( srchdir );

	WIN32_FIND_DATA find_dirs;
	WIN32_FIND_DATA find_files;
	file::Path::NameType cd = (file::Path::NameType) ".";
	file::Path::NameType pd = (file::Path::NameType) "..";

	orkvector<file::Path::NameType> dirvect;

	while( dirstack.empty() == false )
	{	file::Path::NameType dir = dirstack.top();
		dirvect.push_back( dir );
		file::Path::NameType dirpath;
		for( orkvector<file::Path::NameType>::size_type i=0; i<dirvect.size(); i++ ) dirpath += dirvect[i];
		dirstack.pop();

		//orkmessageh( "processing dir %s\n", dir.c_str() );

		//////////////////////////////////////////////////////////////////////////////////
		// Process Files
		//////////////////////////////////////////////////////////////////////////////////

		file::Path::NameType searchspec_files = dir+wildcards;
		HANDLE h_files = FindFirstFile(searchspec_files.c_str(), &find_files);
		if( h_files==INVALID_HANDLE_VALUE )
		{	ret = GetLastError();
			if (ret != ERROR_NO_MORE_FILES)
			{	// TODO: return some error code
			}
		}
		else
		{	bool bcont = true;
			while(bcont)
			{	ork::msleep(0);
				file::Path::NameType name = find_files.cFileName;
				file::Path::NameType _8dot3name = find_files.cAlternateFileName;
				if (0 == (find_files.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
				{
					int match = wildcmp( _wildcards.c_str(), name.c_str() );
					if( match )
					{
						file::Path::NameType fullname = dirpath+name;
						rval.push_back( fullname );
						//orkprintf( "file found <%s>\n", fullname.c_str() );
					}
				}
				int err = FindNextFile(h_files, &find_files);
				bcont = (err!=0);
			}
		}
		if( h_files && h_files != HANDLE(0xffffffff) )
		{
			FindClose(h_files);
		}
		//////////////////////////////////////////////////////////////////////////////////
		// Process Dirs
		//////////////////////////////////////////////////////////////////////////////////

		file::Path::NameType searchspec_dirs = dir+"*";
		HANDLE h_dirs = FindFirstFile(searchspec_dirs.c_str(), &find_dirs);

		if( h_dirs==INVALID_HANDLE_VALUE )
		{	ret = GetLastError();
			if (ret != ERROR_NO_MORE_FILES)
			{	// TODO: return some error code
			}
		}
		else
		{	bool bcont = true;
			
			while(bcont)
			{	
				ork::msleep(0);
				file::Path::NameType name = find_dirs.cFileName;
				file::Path::NameType _8dot3name = find_dirs.cAlternateFileName;
					
				if (find_dirs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{	// Append a trailing slash to directory names...
					//strcat(selectDir->d_name, "/");
					if( (name!=cd)&&(name!=pd) )
					{	file::Path::NameType dirname = dirpath+name + (file::Path::NameType)"\\";
						dirstack.push( dirname );
						//orkmessageh( "pushing dir %s\n", dirname.c_str() );
					}
				}
				int err = FindNextFile(h_dirs, &find_dirs);
				bcont = (err!=0);
			}
		}

		if( h_dirs && h_dirs != HANDLE(0xffffffff) )
		{
			FindClose(h_dirs);
		}
		
		//////////////////////////////////////////////////////////////////////////////////
		//////////////////////////////////////////////////////////////////////////////////

		dirvect.pop_back();
	}

	////////////////////////////////////////////////////////////////////
	size_t ifiles = rval.size();
	ret = GetLastError();
	if (ret != ERROR_NO_MORE_FILES) {}	// TODO: return some error code
	////////////////////////////////////////////////////////////////////
#endif
	return rval;
}

std::string getfilename( const std::string & filterdesc, const std::string &filter, const std::string & title, const std::string & initdir )
{
#if defined( WIN32 ) && ! defined( _XBOX )
	char buffer[MAX_PATH];
	memset( buffer, 0, MAX_PATH );

	std::string AllFiles = "All Files (*.*)";
	std::string AllWildcard = "*.*";

	size_t ifdlen = filterdesc.length();
	size_t iflen  = filter.length();
	size_t iaflen = AllFiles.length();
	size_t iwclen = AllWildcard.length();

	size_t ifdbase =	0;
	size_t ifbase =	ifdbase + ifdlen + 1;
	size_t iabase =	ifbase + iflen + 1;
	size_t iwbase =	iabase + iaflen + 1;

	strcpy( & buffer[ifdbase],			filterdesc.c_str() );
	strcpy( & buffer[ifbase],			filter.c_str() );
	strcpy( & buffer[iabase],			AllFiles.c_str() );
	strcpy( & buffer[iwbase],			AllWildcard.c_str() );

	///////////////////////////////////////////////////////////

	std::string _title = title;
	std::string _initdir = initdir;

	static char filebuffer[1024];       // where we start to browse
	static char inidir[1024];       // where we start to browse
	static char my_of_title[256];      // storage for title

	///////////////////////////////////////////////////////////

	OPENFILENAME fdata;
	memset( (void *) &fdata, 0, sizeof( OPENFILENAME ) );

	fdata.lStructSize = sizeof( OPENFILENAME );
	fdata.hwndOwner = 0;
	fdata.lpstrFilter = buffer;
	fdata.lpstrFile = (char *) & filebuffer[0];
	fdata.nMaxFile = MAX_PATH;
	fdata.Flags = OFN_ENABLESIZING|OFN_NONETWORKBUTTON|OFN_NOTESTFILECREATE|OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
	fdata.lpstrDefExt = "ma4";

	BOOL of = GetOpenFileName( (tagOFNA *) & fdata );

	if( of != 0 )
		return std::string(filebuffer);
	else
		return std::string("");
/*
	OPENFILENAME ofn;
	char szFileName[MAX_PATH];

	ZeroMemory(&ofn, sizeof(ofn));
	szFileName[0] = 0;

	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = 0;
	ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0";
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrDefExt = "txt";

	GetOpenFileName(&ofn);

	*/
#else
	return std::string( "" );
#endif
}

}

#endif // !NITRO && !WII
