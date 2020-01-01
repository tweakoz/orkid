///////////////////////////////////////////////////////////////////////////////
// Orkid2
// Copyright 1996-2020, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid2/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/file/filedev.h>
# include <ork/file/filestd.h>

//#include <ork/util/crc64.h>
#include <ork/util/crc.h>
#include <ork/kernel/string/string.h>
#include <ork/util/endian.h>

#include <stdio.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork
{

const size_t READBUFFERSIZE = QueueBuffer::kreadblocksize+1024;

#if defined(WII)
const bool kPerformBuffering = true;
#else
const bool kPerformBuffering = false;
#endif

///////////////////////////////////////////////////////////////////////////////

FileDevContext::FileDevContext()
	: msFilesystemBaseAbs("")
	, msFilesystemBaseRel("")
	, mbPrependFilesystemBase( false )
	, mpFileDevice(NULL)
	, meTocMode(ETM_NO_TOC)
{
}

///////////////////////////////////////////////////////////////////////////////

FileDevContext::FileDevContext( const FileDevContext& oth )
	: msFilesystemBaseAbs(oth.msFilesystemBaseAbs)
	, msFilesystemBaseRel(oth.msFilesystemBaseRel)
	, mbPrependFilesystemBase( oth.mbPrependFilesystemBase )
	, mpFileDevice(oth.mpFileDevice)
	, mPathConverters( oth.mPathConverters )
	, meTocMode(oth.meTocMode)
	, mTOC(oth.mTOC)
{

}

///////////////////////////////////////////////////////////////////////////////

static file::Path::NameType BaseDir(){
	file::Path::NameType startupdir = file::GetStartupDirectory();

	if( getenv("ORKID_WORKSPACE_DIR")!=nullptr )
		startupdir = file::Path::NameType(getenv("ORKID_WORKSPACE_DIR"))+file::Path::NameType("/");

		return startupdir;

}
///////////////////////////////////////////////////////////////////////////////

void FileDevContext::SetFilesystemBaseAbs( file::Path::NameType base )
{
	file::Path pbase( base.c_str() );

	bool baseisabs = pbase.IsAbsolute();

	auto startupdir = BaseDir();

	file::Path::NameType nbase = baseisabs	? file::Path::NameType(base)
											: startupdir+file::Path::NameType(base);

	msFilesystemBaseAbs= nbase.c_str();

	if( (nbase.find(startupdir.c_str()) == 0) )
	{
		msFilesystemBaseRel = base;
	}
	else
	{
		msFilesystemBaseRel = nbase;
	}

	//printf( "SetFilesystemBaseAbs<%s> : abs<%s> rel<%s> baseisabs<%d>\n", base.c_str(), msFilesystemBaseAbs.c_str(), msFilesystemBaseRel.c_str(), int(baseisabs) );
}

///////////////////////////////////////////////////////////////////////////////

void FileDevContext::SetFilesystemBaseRel( file::Path::NameType base )
{
	auto startupdir = BaseDir();
	file::Path::NameType nbase = startupdir+base; //(base.c_str=="./") ? startupdir : base;
	msFilesystemBaseAbs = nbase.c_str();
	msFilesystemBaseRel = nbase.c_str();
	//std::transform( msFilesystemBaseRel.begin(), msFilesystemBaseRel.end(), msFilesystemBaseRel.begin(), dos2unixpathsep() );
	//std::transform( msFilesystemBaseAbs.begin(), msFilesystemBaseAbs.end(), msFilesystemBaseAbs.begin(), dos2unixpathsep() );
}

void FileDevContext::CreateToc(const file::Path::SmallNameType& UrlName)
{
	if( meTocMode == ETM_CREATE_TOC )
	{
		ork::file::Path::NameType searchdirdos(msFilesystemBaseAbs);
		searchdirdos.replace_in_place("\\\\", "\\");
		ork::file::Path::NameType searchdirpsx=searchdirdos;
		searchdirpsx.replace_in_place("\\", "/");
		if( searchdirpsx.c_str()[searchdirpsx.length()-1]!='/' )
		{
			searchdirpsx.append("/",1);
		}
		ork::file::Path searchdir( searchdirdos.c_str() );
		searchdir = searchdir.ToAbsoluteFolder();
		ork::file::Path::NameType wildcard = "*.*";
		//searchdir = "d:\\";
		orkset<file::Path::NameType> tempset = FileEnv::filespec_search_sorted( wildcard, searchdir );
		std::string mTocString;

		file::Path::SmallNameType::size_type colonslsl = UrlName.find_first_of( "://" );
		file::Path::SmallNameType UrlBase = UrlName.substr( 0, colonslsl );


		mTocString = ork::CreateFormattedString(
			"static void AddToToc_%s( orkmap<ork::file::Path::NameType,int>& the_map, const char* name, int isize )\n"
			"{	the_map.insert( std::make_pair<ork::file::Path::NameType,int>( name, isize ) );\n"
			"}\n", UrlBase.c_str() );
		mTocString += ork::CreateFormattedString( "void FillToc_%s( orkmap<ork::file::Path::NameType,int>& the_map ) {\n", UrlBase.c_str() );

		////////////////////////////////////////
		ork::file::Path::NameType SearchDirPosix( searchdir.c_str() );
		size_t ilen = SearchDirPosix.length();
		for( size_t i=0; i<ilen; i++ )
		{
			char ch = SearchDirPosix.c_str()[i];

			if( ch == '\\' ) ch = '/';
			else if( ch>='A' && ch<='Z' ) ch = (ch-'A')+'a';

			SearchDirPosix.SetChar( (int) i, ch );
		}
		////////////////////////////////////////

		for( orkset<file::Path::NameType>::const_iterator it=tempset.begin(); it!=tempset.end(); it++ )
		{
			file::Path::NameType tname;
			tname.replace( it->c_str(), SearchDirPosix.c_str(), "" );

			size_t ilen = tname.length();
			for( size_t i=0; i<ilen; i++ )
			{
				char ch = tname.c_str()[i];

				if( ch == '\\' ) ch = '/';
				else if( ch>='A' && ch<='Z' ) ch = (ch-'A')+'a';

				tname.SetChar( (int) i, ch );
			}

			/////////////////////////////////
			// Get File Size
			/////////////////////////////////

			int ifilesize = 0;
			std::string absname;
			if( strstr( tname.c_str(), SearchDirPosix.c_str() ) == 0 )
			{
				absname = CreateFormattedString( "%s%s", SearchDirPosix.c_str(), tname.c_str() );
			}
			else
			{
				absname = tname.c_str();
			}
			/////////////////////////////////
			ork::file::Path::NameType AbsPath( absname.c_str() );
			ilen = AbsPath.length();
			for( size_t i=0; i<ilen; i++ )
			{
				char ch = AbsPath.c_str()[i];

				if( ch == '/' ) ch = '\\';

				AbsPath.SetChar( i, ch );
			}
			/////////////////////////////////
			const char* pactualname = AbsPath.c_str();
			FILE* fin = fopen(pactualname, "rb" );
			OrkAssert(fin);
			fseek(fin,0,SEEK_END);
			ifilesize = ftell(fin);
			fclose(fin);

			/////////////////////////////////

			if( strstr( tname.c_str(), searchdirpsx.c_str() ) != 0 )
			{
				file::Path::NameType tname2;
				tname2.replace( tname.c_str(), searchdirpsx.c_str(), "" );
				tname = tname2;
			}

			/////////////////////////////////

			mTOC.insert(std::make_pair(tname,ifilesize));

			mTocString += ork::CreateFormattedString( "AddToToc_%s( the_map, \"%s\", %d );\n", UrlBase.c_str(), tname.c_str(), ifilesize );
		}
		mTocString += ork::CreateFormattedString( "}\n" );

	#if defined(_XBOX)
		std::string platbase = "xbox";
	#else
		std::string platbase = "pc";
	#endif

		std::string TocFileName = ork::CreateFormattedString( "e:\\%s_%s.toc", platbase.c_str(), UrlBase.c_str() );

		FILE *fout = fopen( TocFileName.c_str(), "wt" );
		fwrite( mTocString.c_str(), mTocString.length(), 1, fout );
		fclose(fout);
	}
}

///////////////////////////////////////////////////////////////////////////////

void QueueBuffer::Queue( const void* pwhere, size_t isize )
{
	const size_t ksize = isize;

	size_t inumq = GetNumBytesQueued();

	//orkprintf( "QueueBuffer::Queue() this<%08x> size<%d> miwriteidx<%d> inumq<%d>\n", this, isize, miwriteidx, inumq );
	size_t iend = (miwriteidx+isize);
	const U8* pu8 = (U8*) pwhere;

	if( iend <= kmaxbuffersize )
	{
		memcpy( mData+miwriteidx, pu8, isize );
	}
	else
	{
		size_t icnt = (kmaxbuffersize-miwriteidx);
		memcpy( mData+miwriteidx, pu8, icnt );
		isize -= icnt;
		memcpy( mData, pu8+icnt, isize );
		isize -= isize;
	}
	miwriteidx = (miwriteidx+ksize)%kmaxbuffersize;
	minumqueued += ksize;
}
///////////////////////////////////////////////////////////////////////////////
void QueueBuffer::Read( void* pwhere, size_t isize )
{
	const size_t ksize = isize;

	size_t inumq = GetNumBytesQueued();

	//orkprintf( "QueueBuffer::Read()  this<%08x> size<%d> mireadidx<%d> inumq<%d>\n", this, isize, mireadidx, inumq );

	if( isize==30720 && mireadidx == 0 )
	{
		//orkprintf( "yo\n" );
	}
	U8* pu8 = (U8*) pwhere;
	size_t iend = mireadidx+isize;
	if( iend <= kmaxbuffersize )
	{
		memcpy( pu8, mData+mireadidx, isize );
	}
	else
	{
		size_t icnt = (kmaxbuffersize-mireadidx);
		memcpy( pu8, mData+mireadidx, icnt );
		isize -= icnt;
		memcpy( pu8+icnt, mData, isize );
		isize -= isize;
	}
	mireadidx = (mireadidx+ksize)%kmaxbuffersize;
	miReadFromQueue += ksize;
	minumqueued -= ksize;
}
///////////////////////////////////////////////////////////////////////////////
void QueueBuffer::Flush()
{
	mireadidx = 0;
	miwriteidx = 0;
	minumqueued = 0;
}
///////////////////////////////////////////////////////////////////////////////
size_t QueueBuffer::GetNumBytesQueued() const
{
	return minumqueued;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::Read( File &rFile, void *pTo, size_t iSize )
{
	// There should be no assert here since Reading a non-open file is an error condition.
	if(!rFile.IsOpen())
	{
		orkprintf("FileError(%s): EFEC_FILE_NOT_OPEN\n", rFile.msFileName.c_str());
		return ork::EFEC_FILE_NOT_OPEN;
	}

	if(!rFile.Reading())
	{
		return ork::EFEC_FILE_INVALID_MODE;
	}

	//OrkAssert(pTo && iSize > 0);

	if( 0 == pTo )
	{
		orkprintf("FileError(%s): EFEC_FILE_INVALID_ADDRESS\n", rFile.msFileName.c_str());
		return EFEC_FILE_INVALID_ADDRESS;
	}

	if( iSize < 1 )
	{
		orkprintf("FileError(%s): EFEC_FILE_INVALID_SIZE\n", rFile.msFileName.c_str());
		return EFEC_FILE_INVALID_SIZE;
	}

	OrkAssert( (rFile.GetUserPos() + iSize) <= rFile.miFileLen );
	if((rFile.GetUserPos() + iSize) > rFile.miFileLen)
	{
		orkprintf("FileError(%s): EFEC_FILE_INVALID_SIZE (seek past file bounds)\n", rFile.msFileName.c_str());
		return ork::EFEC_FILE_INVALID_SIZE;
	}

	///////////////////////////////////////////
	// LINFILE READ
	///////////////////////////////////////////

	if( FileEnv::GetLinFileMode() == ELFM_READ && (&rFile) != FileEnv::GetLinFile() )
	{
		////OutputDebugString( "Reading From LinFile!!!\n" );
		EFileErrCode efm = FileEnv::GetLinFile()->Read(pTo, iSize);
		OrkAssert( efm == EFEC_FILE_OK );
		size_t iuserpos = rFile.GetUserPos();
		rFile.SetUserPos( iuserpos + iSize );
		return efm;
	}

	///////////////////////////////////////////

	size_t ilefttoread = iSize;
	size_t ireadctr = 0;

	EFileErrCode ecode = EFEC_FILE_OK;

	//orkprintf( "Read<%d> Phys<%d> User<%d>\n", iSize, rFile.GetPhysicalPos(), rFile.GetUserPos() );

	if( kPerformBuffering && rFile.IsBufferingEnabled() )
	{
		size_t ifsize = rFile.miFileLen;
		while( ilefttoread )
		{	//////////////////////////////////////////////////////////
			// Queue
			//////////////////////////////////////////////////////////
			size_t inumphysrem = rFile.NumPhysicalRemaining();
			size_t inumfree = rFile.mFifo.GetWriteFree();
			//////////////////////////////////////////////////////////
			while( (inumphysrem>0) && (inumfree>QueueBuffer::kreadblocksize) )
			{
				//////////////////////////////////////
				size_t icount = QueueBuffer::kreadblocksize;
				if( icount>inumphysrem ) icount=inumphysrem;
				//////////////////////////////////////
				size_t iuserpos = rFile.GetUserPos();
				size_t iphyspos = rFile.GetPhysicalPos();
				//////////////////////////////////////
				size_t iroundupcount = (icount+kBUFFERALIGNM1)&kBUFFERMASK;
				//////////////////////////////////////
				size_t iactualread = 0;
				EFileErrCode ethiscode;
				//////////////////////////////////////
				if( iphyspos == - 1 )	// user seek happened
										// now do the physical seek
				{	//////////////////////////////////////
					bool aligned_seek_pos = (0 == (iuserpos&(kSEEKALIGN-1)));
					if( aligned_seek_pos )
					{	///////////////////////////////////////
						DoSeekFromStart( rFile, iuserpos );
						ethiscode = DoRead(rFile,(void*)mReadBuffer,iroundupcount,iactualread);
						OrkAssert( iactualread < READBUFFERSIZE );
						///////////////////////////////////////
						rFile.mFifo.Queue( mReadBuffer,iactualread );
						OrkAssert(ethiscode==EFEC_FILE_OK);
						///////////////////////////////////////
					}
					else // unaligned_seek
					{	///////////////////////////////////////
						int aligned = iuserpos&kBUFFERMASK;
						int ioff = iuserpos%kBUFFERALIGN;
						iroundupcount = (icount+ioff+kBUFFERALIGNM1)&kBUFFERMASK;
						///////////////////////////////////////
						DoSeekFromStart( rFile, aligned );
						ethiscode = DoRead(rFile,(void*)mReadBuffer,iroundupcount,iactualread);
						OrkAssert( iactualread < READBUFFERSIZE );
						OrkAssert(ethiscode==EFEC_FILE_OK);
						///////////////////////////////////////
						size_t inumvalid = iactualread-ioff;
						rFile.mFifo.Queue( mReadBuffer+ioff,inumvalid );
						///////////////////////////////////////
					}
				}
				else // no seek, just read
				{	///////////////////////////////////////
					ethiscode = DoRead(rFile,(void*)mReadBuffer,iroundupcount,iactualread);
					OrkAssert( iactualread < READBUFFERSIZE );
					///////////////////////////////////////
					rFile.mFifo.Queue( mReadBuffer,iroundupcount );
					OrkAssert(ethiscode==EFEC_FILE_OK);
					///////////////////////////////////////
				}
				//////////////////////////////////////
				inumfree = rFile.mFifo.GetWriteFree();
				inumphysrem = rFile.NumPhysicalRemaining();
				//////////////////////////////////////
			}
			//////////////////////////////////////////////////////////
			// UnQueue
			//////////////////////////////////////////////////////////
			size_t inumqueued = rFile.mFifo.GetNumBytesQueued();
			if( inumqueued )
			{
				size_t ithiscnt = inumqueued;
				if( ithiscnt>ilefttoread ) ithiscnt=ilefttoread;
				char* pthis = ((char*)pTo)+ireadctr;
				rFile.mFifo.Read( pthis, ithiscnt );
				/////////////////////////////////////////////////
				if( FileEnv::GetLinFileMode() == ELFM_WRITE )
				{
					FileEnv::GetLinFile()->Write(pthis, ithiscnt);
				}
				/////////////////////////////////////////////////
				ireadctr += ithiscnt;
				ilefttoread -= ithiscnt;
			}
			//////////////////////////////////////////////////////////
		}
	}
	else
	{
		size_t iactualread = 0;
		ecode = DoRead(rFile,pTo,iSize,iactualread);
		/////////////////////////////////////////////////
		if( FileEnv::GetLinFileMode() == ELFM_WRITE )
		{
			FileEnv::GetLinFile()->Write(pTo, iSize);
		}
		/////////////////////////////////////////////////
	}

	//boost::Crc64 crc64A = boost::crc64( (const void *) & pTo, sizeof(iSize) );
	//U32 *pu32 = (U32*) & crc64A.crc0;
	//orkprintf( "FILECK file<%s> size<%d> <%08x::%08x>\n", rFile.GetFileName().c_str(), iSize, pu32[0],pu32[1] );

	size_t iuserpos = rFile.GetUserPos();

	rFile.SetUserPos( iuserpos + iSize );

	///////////////////////////////////////////

	return ecode;

}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::OpenFile( File &rFile )
{
	rFile.mFifo.Flush();
	rFile.SetPhysicalPos(0);
	rFile.SetUserPos(0);

	static const u32 OpenFileCode = 0xc0de0001;

	if( mWatcher ) mWatcher->BeginFile( & rFile );

	if(CheckFileDevCaps(rFile) == EFEC_FILE_UNSUPPORTED)
	{
		return EFEC_FILE_UNSUPPORTED;
	}

	///////////////////////////////////////////////////////////
	// BEGIN Linfile Support
	///////////////////////////////////////////////////////////

	File* pLinFile = ork::FileEnv::GetLinFile();
	if( pLinFile && (&rFile!=pLinFile) )
	{
		switch( ork::FileEnv::GetLinFileMode()  )
		{
            case ork::ELFM_NONE:
                break;
			case ork::ELFM_READ:
			{
				OrkAssert(rFile.Reading());
				ork::FixedString<256> fxstr;
				fxstr.format( "Open:%s", rFile.GetFileName().c_str() );
				char buffer[256];
				////////////////////////////////////////
				u32 ReadCode = 0;
				pLinFile->Read( & ReadCode, sizeof( ReadCode ) );
				if( ReadCode != OpenFileCode )
				{
					//OutputDebugString( "ERROR: LinFileStreamError(ReadCode)!!\n" );
					OrkAssert(false);
				}
				////////////////////////////////////////
				int isize = 0;
				pLinFile->Read( & isize, sizeof( isize ) );
				////////////////////////////////////////
				pLinFile->Read( buffer, isize );
				buffer[isize] = 0;
				if( 0 != strcmp( buffer, fxstr.c_str() ) )
				{
					ork::FixedString<256> fxstr2;
					fxstr2.format( "ERROR: LinFileStreamError(Filename) <%s>!=<%s>\n", &buffer[0], fxstr.c_str() );
					//OutputDebugString( fxstr2.c_str() );
					OrkAssert(false);
				}
				////////////////////////////////////////
				break;
			}
			case ork::ELFM_WRITE:
			{
				OrkAssert(rFile.Reading());
				ork::FixedString<256> fxstr;
				fxstr.format( "Open:%s", rFile.GetFileName().c_str() );
				size_t isize = fxstr.length();
				////////////////////////////////////////
				pLinFile->Write( & OpenFileCode, sizeof( OpenFileCode ) );
				////////////////////////////////////////
				pLinFile->Write( & isize, sizeof( isize ) );
				////////////////////////////////////////
				pLinFile->Write( fxstr.c_str(), isize );
				////////////////////////////////////////
				break;

			}
		}
	}

	///////////////////////////////////////////////////////////
	// END Linfile Support
	///////////////////////////////////////////////////////////

	//orkprintf( "Opening<%s>\n", rFile.GetFileName().c_str() );

	//OrkHeapCheck();

	return DoOpenFile( rFile );
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::CloseFile( File &rFile )
{
	if( mWatcher ) mWatcher->EndFile( & rFile );

	if(!rFile.IsOpen())
	{
		return ork::EFEC_FILE_NOT_OPEN;
	}
	rFile.mFifo.Flush();
	EFileErrCode ecode = DoCloseFile( rFile );
	rFile.mHandle = 0;
//	OrkHeapCheck();
	return ecode;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::SeekFromStart( File &rFile, size_t iTo )
{	if(!rFile.IsOpen())
	{
		return ork::EFEC_FILE_NOT_OPEN;
	}
	EFileErrCode ecode = ork::EFEC_FILE_OK;// DoSeekFromStart( rFile, iTo );

	if( kPerformBuffering && rFile.IsBufferingEnabled()  )
	{
		rFile.SetPhysicalPos( 0 );
	}
	else
	{
		DoSeekFromStart( rFile, iTo );
	}
	rFile.SetUserPos(iTo);
	rFile.mFifo.Flush();
	//orkprintf( "SeekFromStart<%d>\n", iTo );
	return ecode;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::SeekFromCurrent( File &rFile, size_t iOffset )
{	if(!rFile.IsOpen())
	{
		return ork::EFEC_FILE_NOT_OPEN;
	}
	EFileErrCode ecode = ork::EFEC_FILE_OK;// DoSeekFromStart( rFile, iTo );
	//EFileErrCode ecode = DoSeekFromCurrent( rFile, iOffset );
	size_t inewpos = rFile.GetUserPos() + iOffset;
	if( kPerformBuffering && rFile.IsBufferingEnabled() )
	{
		rFile.SetPhysicalPos( 0 );
	}
	else
	{
		DoSeekFromCurrent( rFile, iOffset );
	}
	rFile.SetUserPos( inewpos );
	rFile.mFifo.Flush();
	//orkprintf( "SeekFromCurrent<%d>\n", iOffset );
	return ecode;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::GetLength( File &rFile, size_t& riLength )
{
	EFileErrCode ecode = EFEC_FILE_OK;
	if( rFile.IsOpen() )
	{
		riLength = rFile.miFileLen;
	}
	else
	{
		ecode = DoGetLength( rFile, riLength );
		rFile.SetPhysicalPos( 0 );
		rFile.SetUserPos( 0 );
		rFile.mFifo.Flush();
	}
	return ecode;
}

///////////////////////////////////////////////////////////////////////////////

/*void FileDev::PushParamContext( void )
{
	OrkAssert( mFileDevContextStackDepth < (kFileDevContextStackMax-1) );
	mFileDevContextStackDepth++;
}

///////////////////////////////////////////////////////////////////////////////

void FileDev::PopParamContext( void )
{
	OrkAssert( mFileDevContextStackDepth >= 1 );
	mFileDevContextStackDepth--;
}*/

///////////////////////////////////////////////////////////////////////////////

EFileErrCode FileDev::CheckFileDevCaps( File &rFile )
{
	if( rFile.Appending() || rFile.Writing() )
	{
		bool canw = CanWrite();
		OrkAssert(canw);
		if(!canw)
			return EFEC_FILE_INVALID_CAPS;
	}
	if( rFile.Reading() )
	{
		bool canr = CanRead();
		OrkAssert(canr);
		if(!canr)
			return EFEC_FILE_INVALID_CAPS;
	}
	return EFEC_FILE_OK;
}

///////////////////////////////////////////////////////////////////////////////

FileDev::FileDev( file::Path::NameType devicename, file::Path fsbase, U32 devcaps )
	: msDeviceName( devicename )
	, muDeviceCaps( devcaps )
	, mFileDevContextStackDepth( 0 )
	, mReadBuffer( new u8[READBUFFERSIZE] )
	, mWatcher( 0 )
{
	SetFileSystemBaseAbs( fsbase.c_str() );
	SetPrependFilesystemBase( true );
}


}
