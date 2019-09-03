////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/file/file.h>
#include <ork/kernel/string/string.h>

#if defined( WII )
#include <ork/mem/wii_mem.h>
#endif

///////////////////////////////////////////////////////////////////////////////

namespace ork
{

static int iqbidx = 0;
const size_t QueueBuffer::kreadblocksize = 256<<10;
const size_t QueueBuffer::kmaxbuffersize = 1024<<10;
static int MAX_FILE_CONTEXT = 8;

#if defined(WII)
static unsigned char* pqbuf = (unsigned char* ) wii::MEM2Alloc( ork::wii::kfilbufsize ); //
#else
static unsigned char* pqbuf = (unsigned char* ) malloc( QueueBuffer::kmaxbuffersize*MAX_FILE_CONTEXT );
#endif

///////////////////////////////////////////////////////////////////////////////

QueueBuffer::QueueBuffer()
	: mireadidx(0)
	, miwriteidx(0)
	, miReadFromQueue(0)
	, minumqueued(0)
	, mData( 0 )
{
	//OrkAssert( iqbidx<MAX_FILE_CONTEXT );
	//mData = pqbuf+(kmaxbuffersize*iqbidx);
	//iqbidx++;
}

///////////////////////////////////////////////////////////////////////////////

QueueBuffer::~QueueBuffer()
{
	//iqbidx--;
	//delete mData;
}

///////////////////////////////////////////////////////////////////////////////

File::File(FileDev* pdev)
	: mpDevice(pdev)
	, msFileName("NoFile")
	, meFileMode(EFM_READ)
	, miFileLen(0)
	, miUserPos(0)
	, mHandle(0)
	, miPhysicalPos(0)
    , mbEnableBuffering(true)
{
	if(NULL == mpDevice)
		mpDevice = FileEnv::GetRef().GetDefaultDevice();
}

///////////////////////////////////////////////////////////////////////////////

File::File( const char* sFileName, EFileMode eMode, FileDev* pdev )
	: mpDevice(pdev)
	, msFileName(sFileName)
	, meFileMode(eMode)
	, miFileLen(0)
	, miUserPos(0)
	, mHandle(0)
	, miPhysicalPos(0)
    , mbEnableBuffering(true)
{
	if(NULL == mpDevice)
		mpDevice = FileEnv::GetRef().GetDeviceForUrl(msFileName);

	OpenFile( sFileName, eMode );
}

File::File(const file::Path &sFileName, EFileMode eMode, FileDev *pdev)
	: mpDevice(pdev)
	, msFileName(sFileName)
	, meFileMode(eMode)
	, miFileLen(0)
	, miUserPos(0)
	, mHandle(0)
	, miPhysicalPos(0)
    , mbEnableBuffering(true)
{
	if(NULL == mpDevice)
		mpDevice = FileEnv::GetRef().GetDeviceForUrl(msFileName);

	OpenFile(sFileName, eMode);
}

File::~File()
{
	if(IsOpen())
		Close();
}

bool File::IsOpen() const
{
	if( ork::FileEnv::GetLinFileMode() == ork::ELFM_READ )
	{
		if( ork::FileEnv::GetLinFile() != this )
		{
			return miFileLen!=0;
		}
	}
	return mHandle != 0;
}

///////////////////////////////////////////////////////////////////////////////

EFileErrCode File::Open()
{
	float ftime = ork::OldSchool::GetRef().GetLoResRelTime();

//	std::string filename = ork::CreateFormattedString(
//		"Time<%f> Opening<%s>\n", ftime, this->msFileName.c_str() );
//	orkprintf( filename.c_str() );

	return mpDevice->OpenFile( *this );
}

EFileErrCode File::OpenFile( const file::Path& fname, EFileMode eMode )
{
	meFileMode = eMode;
	msFileName = fname;
	return Open();
}

EFileErrCode File::Load( void **filebuffer, size_t &size )
{
	OrkAssert( meFileMode & EFM_READ );
	OrkAssert( IsOpen() );

	EFileErrCode result = Open();
	
	if(EFEC_FILE_OK == result)
	{
		size_t length = 0;
		result = GetLength( length );
		if(EFEC_FILE_OK == result)
		{
			if(*filebuffer)
			{
				if(size > length)
					size = length;
				result = Read( *filebuffer, size );
				if(EFEC_FILE_OK != result)
					size = 0;
			}
			else
			{
				size = length;
				char* charBuffer = new char[size];
				*filebuffer = charBuffer;
				result = Read( *filebuffer, size );
				if(EFEC_FILE_OK != result)
				{
					size = 0;
					OrkAssertI(false, "Stacked allocations cannot be deleted.");
					delete[] charBuffer;
					*filebuffer = NULL;
				}
			}
		}
	}

	Close( );

	return result;
}

}
