////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#if defined(WIN32) && ! defined( _XBOX )
#include <shlobj.h> // For BROWSEINFO
#endif
#include <ork/file/fileenv.h>
#include <ork/kernel/slashnode.h>
#include <ork/orkstd.h> // For OrkAssert

#if defined(NITRO)
# include <ork/file/filenitro.h>
#elif defined(WII)
# include <ork/file/filedevwii.h>
# include <ork/file/filestd.h>
#elif defined(_PSP)
# include <ork/file/filedevpsp.h>
#else
# include <ork/file/filestd.h>
#endif
#if defined(IX)
# include <unistd.h>
#endif

namespace ork
{

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
namespace file {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

file::Path::NameType GetStartupDirectory()
{
#if defined(_XBOX)
	return GetCurDir()+file::Path::NameType("\\");
#else
	return GetCurDir()+file::Path::NameType("/");
#endif
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode SetCurDir( const file::Path::NameType & inspec )
{
	return EFEC_FILE_UNSUPPORTED; //CFileEnv::GetRef().mpDefaultDevice->SetCurrentDirectory(inspec);
}

///////////////////////////////////////////////////////////////////////////////

file::Path::NameType GetCurDir() // the curdir of the process, not the CFileDevice
{
	file::Path::NameType outspec;
#if defined(_XBOX)
	outspec = "d:\\";
#elif defined( WIN32 )
	char dirbuf[256];
	dirbuf[0] = 0;
	DWORD len = ::GetCurrentDirectory(256, dirbuf);
	outspec = file::Path::NameType(dirbuf);
#elif defined(WII) || defined(NITRO)
	return file::Path::NameType("");
#elif defined(IX)
	char cwdbuf[4096];
	const char* cwdr = getcwd(cwdbuf, sizeof(cwdbuf));
	OrkAssert(cwdr!=0);
	printf( "cwdbuf<%s>\n", cwdbuf );
	outspec = cwdr;
#else
	// Not implemented for this platform!
	OrkAssert(false);
#endif
	printf( "aa\n");
	file::Path mypath( outspec.c_str() );
	printf( "ab\n");
	//std::transform( outspec.begin(), outspec.end(), outspec.begin(), dos2unixpathsep() );
	mypath = mypath.c_str();
	return outspec;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CFileEnv::CFileEnv() : NoRttiSingleton<CFileEnv>(), mpDefaultDevice(NULL)
{
#if defined(NITRO)
	CFileDev* fileEnvironment = new CFileDevNitro;
#elif defined(WII)
	CFileDev* fileEnvironment = new CFileDevWII;
#elif defined(_PSP)
	CFileDev* fileEnvironment = new CFileDevPSP;
#else
	CFileDev* fileEnvironment = new CFileDevStd;
#endif
	SetDefaultDevice(fileEnvironment);
}

///////////////////////////////////////////////////////////////////////////////

CFileDev* CFileEnv::GetDeviceForUrl(const file::Path &fileName) const
{
	ork::file::Path::SmallNameType urlbase = CFileEnv::GetRef().UrlNameToBase(fileName.GetUrlBase().c_str()).c_str();

	orkmap<ork::file::Path::SmallNameType, SFileDevContext>::const_iterator it
		= CFileEnv::GetRef().RefUrlRegistry().find(urlbase);
	if(it != CFileEnv::GetRef().RefUrlRegistry().end())
		if(it->second.GetFileDevice())
			return it->second.GetFileDevice();

	return GetRef().mpDefaultDevice;
}

///////////////////////////////////////////////////////////////////////////////

bool CFileEnv::CanRead( void ) { return GetRef().mpDefaultDevice->CanRead(); }
bool CFileEnv::CanWrite( void ) { return GetRef().mpDefaultDevice->CanWrite(); }
bool CFileEnv::CanReadAsync( void ) { return GetRef().mpDefaultDevice->CanReadAsync(); }

///////////////////////////////////////////////////////////////////////////////

void CFileEnv::SetFilesystemBase( file::Path::NameType FSBase )
{
    GetRef().mpDefaultDevice->SetFileSystemBaseAbs( FSBase );
}

///////////////////////////////////////////////////////////////////////////////

const file::Path::NameType & CFileEnv::GetFilesystemBase( void )
{
    return GetRef().mpDefaultDevice->GetFilesystemBaseAbs();
}

///////////////////////////////////////////////////////////////////////////////

const SFileDevContext & CFileEnv::UrlBaseToContext( const file::Path::SmallNameType &UrlName )
{
	static SFileDevContext DefaultContext;
	orkmap<ork::file::Path::SmallNameType, SFileDevContext> & Map = GetRef().mUrlRegistryMap;

	file::Path::SmallNameType strip_name;
	strip_name.replace( UrlName.c_str(), "://", "" );

	orkmap<ork::file::Path::SmallNameType, SFileDevContext>::const_iterator it=Map.find(strip_name);
	
	if( it!=Map.end() )
	{
		return it->second;
	}
	return DefaultContext;
}

///////////////////////////////////////////////////////////////////////////////

file::Path::SmallNameType CFileEnv::UrlNameToBase(const file::Path::NameType& UrlName)
{
	file::Path::SmallNameType urlbase = "";
	file::Path::NameType::size_type find_url_colon = UrlName.cue_to_char(':', 0);
	if(UrlName.npos != find_url_colon)
	{
		if( int(UrlName.size()) > (find_url_colon+2) )
		{
			if( (UrlName[find_url_colon+1] == '/') || (UrlName[find_url_colon+2] == '/') )
			{
				urlbase = UrlName.substr( 0, find_url_colon ).c_str();
			}
		}
	}

	return urlbase;
}

///////////////////////////////////////////////////////////////////////////////

file::Path::NameType CFileEnv::UrlNameToPath( const file::Path::NameType& UrlName )
{
	file::Path::NameType path = "";
	file::Path::NameType::size_type find_url_colon = UrlName.cue_to_char( ':', 0 );
	if( UrlName.npos != find_url_colon )
	{
		if( int(UrlName.size()) > (find_url_colon+3) )
		{
			if( (UrlName[find_url_colon+1] == '/') || (UrlName[find_url_colon+2] == '/') )
			{
				file::Path::NameType urlbase = UrlName.substr( 0, find_url_colon );

				file::Path::SmallNameType urlp = urlbase.c_str();

				if( OrkSTXIsInMap( GetRef().mUrlRegistryMap, urlp ) )
				{				
					file::Path::NameType::size_type ipathbase = find_url_colon+3;
					path = UrlName.substr(ipathbase, UrlName.size() - ipathbase).c_str();
				}
			}
		}
	}
	
	if( path.length()==0 )
	{
		path = UrlName;
	}

	return path;
}

///////////////////////////////////////////////////////////////////////////////

ork::file::Path CFileEnv::GetPathFromUrlExt( const file::Path::NameType& UrlName, const file::Path::NameType & subfolder, const file::Path::SmallNameType & ext )
{
	file::Path::NameType Base = UrlNameToBase( UrlName ).c_str();
	file::Path::NameType Tail = UrlNameToPath( UrlName );

	if( Tail==(Base+"://") ) Tail="";

	const SFileDevContext & Ctx = UrlBaseToContext( Base.c_str() );

	file::Path::NameType path;

	if( Ctx.GetPrependFilesystemBase() )
	{
		path = file::Path::NameType(Ctx.GetFilesystemBaseAbs().c_str()) + subfolder.c_str() + Tail + ext.c_str();
	}
	else
	{

		path = (subfolder + Tail + ext.c_str() );
	}

	return ork::file::Path(path.c_str());
}

///////////////////////////////////////////////////////////////////////////////

void CFileEnv::RegisterUrlBase( const file::Path::SmallNameType& UrlName, const SFileDevContext & FileContext )
{
	file::Path::SmallNameType urlbase = UrlNameToBase( UrlName.c_str() );
	orkmap<ork::file::Path::SmallNameType, SFileDevContext> & Map = GetRef().mUrlRegistryMap;
	std::pair<ork::file::Path::SmallNameType, SFileDevContext> the_pair(urlbase.c_str(), FileContext);
	Map.insert( the_pair );

	SFileDevContext& nc = const_cast<SFileDevContext&>(FileContext);
	nc.CreateToc(UrlName);
}

///////////////////////////////////////////////////////////////////////////////

bool CFileEnv::PathIsUrlForm( const file::Path& PathName )
{
	return PathName.HasUrlBase();
}

///////////////////////////////////////////////////////////////////////////////

bool CFileEnv::IsUrlBaseRegistered(const file::Path::SmallNameType& urlBase)
{
	return OrkSTXIsInMap(GetRef().mUrlRegistryMap, urlBase);
}

file::Path::NameType CFileEnv::StripUrlFromPath(const file::Path::NameType& urlName)
{
	file::Path::NameType urlStr = urlName.c_str();
	file::Path::NameType path = "";
	file::Path::NameType::size_type find_url_colon = urlStr.cue_to_char(':', 0);
	if(urlStr.npos != find_url_colon)
	{
		if(int(urlStr.size()) > (find_url_colon + 3))
		{
			if((urlStr[find_url_colon + 1] == '/') || (urlStr[find_url_colon + 2] == '/'))
			{
				file::Path::NameType::size_type ipathbase = find_url_colon + 3;
				return urlStr.substr(ipathbase, urlStr.size() - ipathbase);
			}
		}
	}

	return urlStr;
}

///////////////////////////////////////////////////////////////////////////////
	
bool CFileEnv::DoesFileExist(  const file::Path& filespec )
{
	return CFileEnv::GetRef().GetDeviceForUrl(filespec)->DoesFileExist(filespec);
}

///////////////////////////////////////////////////////////////////////////////

bool CFileEnv::DoesDirectoryExist( const file::Path& filespec )
{
	return CFileEnv::GetRef().GetDeviceForUrl(filespec)->DoesDirectoryExist(filespec);
}

///////////////////////////////////////////////////////////////////////////////

bool CFileEnv::IsFileWritable( const file::Path& filespec )
{
	return CFileEnv::GetRef().GetDeviceForUrl(filespec)->IsFileWritable(filespec);
}

///////////////////////////////////////////////////////////////////////////////

void CFileEnv::SetPrependFilesystemBase(bool setting)
{
	CFileEnv::GetRef().mpDefaultDevice->SetPrependFilesystemBase(setting);
}

///////////////////////////////////////////////////////////////////////////////

bool CFileEnv::GetPrependFilesystemBase(void)
{
	return(CFileEnv::GetRef().mpDefaultDevice->GetPrependFilesystemBase());
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

bool CFileEnv::filespec_isdos( const file::Path::NameType & inspec )
{
	bool rval = true;
	
	file::Path::NameType::size_type len = inspec.size();
	
	if( inspec[1] == ':' )
	{
		return true;
	}
	
	// Windows-style network paths
	if( (inspec[0] == '\\') && (inspec[1] == '\\') )
	{
		return true;
	}

	for(file::Path::NameType::size_type i = 0; i < len; ++i)
	{	
		const char tch = inspec[i];
		
		if( !IsCharDos(tch) )
		{	
			rval = false;
			break;
		}
	}

	return rval;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

bool CFileEnv::filespec_isunix( const file::Path::NameType & inspec )
{
	bool rval = true;

	file::Path::NameType::size_type len = inspec.size();
	
	for(file::Path::NameType::size_type i = 0; i < len; ++i)
	{	
		const char tch = inspec[i];
		
		if( !IsCharUnix(tch) )
		{	
			rval = false;
			break;
		}
	}

	return rval;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

file::Path::NameType CFileEnv::filespec_to_extension( const file::Path::NameType & inspec )
{	file::Path::NameType::size_type i1stDot = inspec.find_first_of(".");
	file::Path::NameType::size_type iLstDot = inspec.find_last_of(".");
	//OrkAssert( i1stDot==iLstDot );
	file::Path::NameType::size_type iOldLength = inspec.length();
	file::Path::NameType::size_type iNewLength = (iOldLength-iLstDot)-1;
	file::Path::NameType outstr = inspec.substr(iLstDot+1,iNewLength);

	if( (i1stDot == file::Path::NameType::npos) && (i1stDot == file::Path::NameType::npos) )
	{
		outstr = "";
	}
	//transform( outstr.begin(), outstr.end(), outstr.begin(), lower() );
	return outstr;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

file::Path::NameType CFileEnv::filespec_no_extension( const file::Path::NameType & inspec )
{	file::Path::NameType::size_type i1stDot = inspec.find_first_of(".");
	file::Path::NameType::size_type iLstDot = inspec.find_last_of(".");
	//OrkAssert( i1stDot==iLstDot );
	file::Path::NameType::size_type iOldLength = inspec.length();
	file::Path::NameType outstr = inspec.substr(0,iLstDot);
	return outstr;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

file::Path::NameType CFileEnv::filespec_strip_base( const file::Path::NameType & inspec, const file::Path::NameType & base )
{
	file::Path::NameType outstr = inspec;
	std::transform( outstr.begin(), outstr.end(), outstr.begin(), dos2unixpathsep() );
	if(outstr.find(base.c_str()) == 0)
		return outstr.substr(base.length());
	else
		return outstr;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

orkvector< file::Path::NameType > CFileEnv::filespec_separate_terms( const file::Path::NameType & inspec )
{
	file::Path::NameType _instr = inspec;
	std::transform( _instr.begin(), _instr.end(), _instr.begin(), dos2unixpathsep() );
	orkvector<file::Path::NameType> outvec;
	const file::Path::NameType delims( "/" );
	file::Path::NameType::size_type idx, len, ilen;
	ilen = _instr.size();
	idx = _instr.cue_to_char( '/', 0 );
	int word = 0;
	outvec.clear();
	bool bDone = false;
	file::Path::NameType::size_type Nidx;
	while( (idx < ilen) && (!bDone) )
	{	
		Nidx = _instr.cue_to_char( '/', int(idx)+1 );
		bool bAnotherSlash = (Nidx!=-1);
		len = Nidx - idx;
		file::Path::NameType newword = _instr.substr( idx+1, len-1 );
		outvec.push_back( newword );
		idx = Nidx;
		word++;
	}
	return outvec;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

file::Path::NameType CFileEnv::FilespecToContainingDirectory(const file::Path::NameType& path)
{
	file::Path::NameType rval("");
	bool isurl = CFileEnv::PathIsUrlForm(ork::file::Path(path.c_str()));
	file::Path::SmallNameType urlbase = CFileEnv::UrlNameToBase(path.c_str());
	// This nonsense is so we can work with URLs too...
	file::Path::NameType UrlStrippedPath = path;
	if(isurl)
	{
		UrlStrippedPath = StripUrlFromPath(path.c_str());
	}

	bool isunix = filespec_isunix(UrlStrippedPath);
	bool isdos = filespec_isdos(UrlStrippedPath);

	// Precondition
	OrkAssert(isunix || isdos);

	std::transform( UrlStrippedPath.begin(), UrlStrippedPath.end(), UrlStrippedPath.begin(), dos2unixpathsep() );

//	printf( "FilespecToContainingDirectory<%s>\n", path.c_str() );
//	printf( "ix:UrlStrippedPath<%s>\n", UrlStrippedPath.c_str() );
	int idx = UrlStrippedPath.find_last_of("/");
//	printf( "idx<%d>\n", idx );
	if( idx!=file::Path::NameType::npos )
	{
		file::Path::NameType tmp =  UrlStrippedPath.substr(0,idx);
		rval = tmp;
		
	}

	//rval=TruncateAtFirstCharFromSet(UrlStrippedPath, "/");
//	printf( "UrlStrippedPath<%s>\n", UrlStrippedPath.c_str() );
//	printf( "rval<%s>\n", rval.c_str() );
	if(isurl)
	{
		rval=file::Path::NameType(urlbase.c_str())+file::Path::NameType("://")+rval;
	}
	//std::transform( rval.begin(), rval.end(), rval.begin(), dos2unixpathsep() );
//	printf( "rval<%s>\n", rval.c_str() );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////
/// DEPRECATED (ork::file::Path will be replacement)

file::Path::NameType CFileEnv::TruncateAtFirstCharFromSet(const file::Path::NameType& stringToTruncate, const file::Path::NameType& setOfChars)
{
	size_t isetsize = setOfChars.length();

	file::Path::NameType::size_type ilchar = file::Path::NameType::npos;

	for( size_t i=0; i<isetsize; i++ )
	{
		char buffer[2] = { setOfChars.c_str()[0], 0 };

		file::Path::NameType::size_type indexOfLastSeparator = stringToTruncate.find_last_of(& buffer[0] );
		
		if(indexOfLastSeparator > ilchar)
		{
			ilchar = indexOfLastSeparator;
		}
	}

	if( ilchar == file::Path::NameType::npos )
	{
		return "";
	}
	else
	{
		return stringToTruncate.substr(0, ilchar);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if defined(WIN32) && ! defined(_XBOX)

std::string getdirectory( const std::string & title, const std::string & initial )
{
	char * rval;

	char textbuffer[1024];
	char filebuffer[1024];   // usually 1024 will do

	BROWSEINFO binfo;

	//binfo.hwndOwner = fl_xid( win );
	binfo.hwndOwner = NULL;
	binfo.pidlRoot = NULL;
	binfo.pszDisplayName = filebuffer;
	binfo.lpszTitle = title.c_str();
	binfo.ulFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS;
	binfo.lpfn = NULL;
	binfo.lParam = NULL;
	binfo.iImage = 0;

	LPITEMIDLIST itlist = SHBrowseForFolder( & binfo );
	SHGetPathFromIDList( itlist, (char *) textbuffer );
	sprintf( filebuffer, "%s\\", textbuffer );

	size_t slen = strlen( filebuffer );
	OrkAssert( slen<sizeof(filebuffer) );
	for( size_t i=0; i<slen; i++ )
	{
		if( filebuffer[i] == '\\' )
			filebuffer[i] = '/';

	}

	rval = filebuffer;

	return rval;
}

#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CFileEnv::BeginLinFile( const file::Path& lfn, ELINFILEMODE emode )
{
	if( emode==ELFM_WRITE )
	{
#if defined(_XBOX)
		std::string lfile1 = std::string("e:\\")+std::string(lfn.c_str());
#else
		std::string lfile1 = std::string(lfn.c_str());
#endif
		GetRef().mpLinFile = new CFile( lfile1.c_str(), ork::EFM_WRITE );
		GetRef().meLinFileMode = emode;
	}
	else if( emode==ELFM_READ )
	{
#if defined(_XBOX)
		std::string lfile1 = std::string("d:\\data\\")+std::string(lfn.c_str());
		std::string lfile2 = std::string("d:\\")+std::string(lfn.c_str());

		if( ork::CFileEnv::DoesFileExist(lfile1.c_str()) )
		{
			//OutputDebugString( "OpeningLinFile!!!\n" );
			GetRef().mpLinFile = new CFile( lfile1.c_str(), ork::EFM_READ );
			GetRef().meLinFileMode = emode;
		}
		else if( ork::CFileEnv::DoesFileExist(lfile2.c_str()) )
		{
			//OutputDebugString( "OpeningLinFile!!!\n" );
			GetRef().mpLinFile = new CFile( lfile2.c_str(), ork::EFM_READ );
			GetRef().meLinFileMode = emode;
		}
		else
		{
			//OutputDebugString( "NoLinFile Found!!!\n" );
		}
#else
		std::string lfile3 = std::string("data/")+std::string(lfn.c_str());
		if( ork::CFileEnv::DoesFileExist(lfile3.c_str()) )
		{
			//OutputDebugString( "OpeningLinFile!!!\n" );
			GetRef().mpLinFile = new CFile( lfile3.c_str(), ork::EFM_READ );
			GetRef().meLinFileMode = emode;
		}
#endif		

	}
}

///////////////////////////////////////////////////////////////////////////////

void CFileEnv::EndLinFile()
{
	if( GetRef().mpLinFile )
		delete GetRef().mpLinFile;

	GetRef().mpLinFile = 0;

	GetRef().meLinFileMode = ork::ELFM_NONE;

}

//////////////////////////////////////////////////////////

}
