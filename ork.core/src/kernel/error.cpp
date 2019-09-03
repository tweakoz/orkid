	////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>
#include <ork/orkconfig.h>
#include <ork/orkprotos.h>

#include <ork/kernel/csystem.h>
#include <ork/kernel/mutex.h>

#include <ork/file/file.h>
#include <ork/util/stl_ext.h>
#include <ork/loc/lprintf.h>
#include <ork/kernel/string/string.h>

#include <ork/util/Context.hpp>
#include <ork/kernel/debug.h>

///////////////////////////////////////////////////////////////////////////////

static const int kgchkpointbuffersize = 64;
static const int kgchkpointnumbuffers = 64;
static const int kerrorbuffersize = 4096;

char errorbuffer[kerrorbuffersize];

template class ork::util::Context<ork::OldSchool::LogPolicy>;
template class ork::util::ContextTLS<CheckPointContext>;

char gchkpointbuffer[kgchkpointnumbuffers][kgchkpointbuffersize];
int gchkpointbufferindex = 0;
ork::mutex					gMemMutex( "yo" );

///////////////////////////////////////////////////////////////////////////////

static void OrkCheckPoint( char *setstring )
{
#if 0
	gMemMutex.Lock();
	{
		if( setstring )
		{
			OrkAssert( strlen( setstring ) < kgchkpointbuffersize );
			strcpy( gchkpointbuffer[gchkpointbufferindex], setstring );
			gchkpointbufferindex = (gchkpointbufferindex+1)%kgchkpointnumbuffers;
		}
	}
	gMemMutex.UnLock();
#endif

}

///////////////////////////////////////////////////////////////////////////////

CheckPointContext::CheckPointContext(const char *formatstring, ...)
{
#if 0
	mBuffer[0] = 'b';
	mBuffer[1] = 'e';
	mBuffer[2] = 'g';
	mBuffer[3] = ' ';

	va_list argp;
	va_start(argp, formatstring);
	vsnprintf(&mBuffer[4], sizeof(mBuffer), formatstring,argp);

	OrkCheckPoint( & mBuffer[0] );
#endif
}

///////////////////////////////////////////////////////////////////////////////

CheckPointContext::~CheckPointContext()
{
#if 0
	mBuffer[0] = 'e';
	mBuffer[1] = 'n';
	mBuffer[2] = 'd';
	mBuffer[3] = ' ';

	OrkCheckPoint( & mBuffer[0] );
#endif
}

///////////////////////////////////////////////////////////////////////////////

void OrkHeapCheck()
{
#if defined(WII) && defined(_DEBUG) && 0
	///////////////////////////////////////////
	// stack overflow check
	///////////////////////////////////////////
	OSThread* pthread = OSGetCurrentThread();
	OrkAssert( *(pthread->stackEnd) == OS_THREAD_STACK_MAGIC );
	///////////////////////////////////////////
	long val = OSCheckHeap(0);
	OrkAssert(val>=0);
	//OSDumpHeap(0);
#elif defined(_MSVC) && defined(_DEBUG) && 0
	//chkstack();
	int ival = _CrtCheckMemory();
	if( 0 == ival )
	{
		orkprintf( "HEAP CORRUPTED\n" );
		//OutputDebugString( "HEAP CORRUPTED\n" );
		__asm int 3;
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////

void OrkNonFatalAssertFunction(const char *fmtstr, ...)
{
	va_list argp;
	va_start(argp, fmtstr);
	vsnprintf(&errorbuffer[0], sizeof(errorbuffer), fmtstr, argp);
	va_end(argp);

#if defined( WIN32 ) && ! defined( _XBOX )
	MessageBox( 0, errorbuffer, "Non Fatal Error", MB_ICONERROR|MB_OK|MB_TASKMODAL );
#else
	orkprintf(errorbuffer);
#endif
}

void OrkAssertFunction(const char *fmtstr, ...)
{
#if defined(IX)
	va_list argp;
	va_start( argp, fmtstr );
	vsprintf( &errorbuffer[0], fmtstr, argp );
	va_end( argp );

#elif defined(WII)
	va_list argp;
	va_start(argp, fmtstr);
	std::vsnprintf(&errorbuffer[0], sizeof(errorbuffer), fmtstr,argp);
	va_end(argp);
#else
	va_list argp;
	va_start(argp, fmtstr);
	vsnprintf(&errorbuffer[0], sizeof(errorbuffer), fmtstr, argp);
	va_end(argp);
#endif

#if defined(_PS2)
	OrkCheckPoint("OrkAssert: %s\n", errorbuffer);
#elif defined( WIN32 ) || defined(_XBOX) || defined( IX )

	//MessageBox( 0, errorbuffer, "Fatal Error", MB_ICONERROR|MB_OK|MB_TASKMODAL );
	//assert( false );

	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf("error<%s>\n", fmtstr);
	orkprintf("crashing....\n");
	std::string bt = ork::get_backtrace();
	printf( "BACKTRACE\n%s\n", bt.c_str() );
	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf( "/////////////////////////////////////////////\n" );
	fflush(stdout);

	ork::msleep(500);

#if defined( _XBOX )
	*((float*)1) = 0; // misaligned FP memory access will cause exception but not bork the xbox
	while(1){} // incase that worked
#endif

	char *pFUCKED = nullptr;
	*pFUCKED = 0;

	std::exception a;
	throw a;

	//break 3;

# elif defined( WII )

	orkprintf("crashing....\n");
	orkprintf("%s\n", errorbuffer);
	fflush(stdout);
	//std::exception a;
	//throw a;
	//int *pFUCKED = reinterpret_cast<int*>(0xffffffff); // unaligned
	//*pFUCKED = 0;
	//assert(false);
#if ! defined(_DEBUG)
	WiiPanic(__FILE__, __LINE__, errorbuffer);
#endif
	ASSERT(0);
	PPCHalt();
#elif defined(_DS)

	lite_printf("crashing....\n");
	lite_printf("%s\n", errorbuffer);

	ARMStackTrace();

	fflush(stdout);

	int *pFUCKED = 0;
	*pFUCKED = 0;

# endif

	while(1){}

	//assert(false);
}

///////////////////////////////////////////////////////////////////////////////

#ifdef WIN32
# define vsnprintf _vsnprintf
#endif

#if defined( IX )
void orkprintf( const char *formatstring, ...)
{
	va_list argp;
	va_start( argp, formatstring );
	vsprintf( &errorbuffer[0], formatstring, argp );
	va_end( argp );
    //OutputDebugString( errorbuffer );
	printf( "%s", errorbuffer );
}
void orkerrorlog(const char *formatstring, ...)
{
	char aPrintBuffer[1024];
	va_list argp;
	va_start(argp, formatstring);
	vsnprintf(&aPrintBuffer[0], sizeof(aPrintBuffer), formatstring, argp);
	va_end(argp);

	aPrintBuffer[sizeof(aPrintBuffer)-1] = '\0';
	fprintf( stderr, "%s", aPrintBuffer);
	//OutputDebugString(aPrintBuffer);
	fflush(stdout);
}
#elif defined( _PS2 )
void orkprintf( const char *format, ...)
{
#if defined( _DEBUG )
    va_list args;
    int rv;

    va_start(args, format);
    rv = vsnprintf(errorbuffer, 4096, format, args);

	int nputs(char *buffer);
	nputs(errorbuffer);
#endif
}
#elif defined( _XBOX ) && ! defined(RETAIL)
void orkprintf( const char *formatstring, ...)
{
	va_list argp;
	va_start( argp, formatstring );
	vsprintf( &errorbuffer[0], formatstring, argp );
	va_end( argp );
    //OutputDebugString( errorbuffer );
	//orkprintf( "%s", errorbuffer );
}
void orkerrorlog(const char *formatstring, ...)
{
	char aPrintBuffer[1024];
	va_list argp;
	va_start(argp, formatstring);
	vsnprintf(&aPrintBuffer[0], sizeof(aPrintBuffer), formatstring, argp);
	va_end(argp);

	aPrintBuffer[sizeof(aPrintBuffer)-1] = '\0';
	fprintf( stderr, "%s", aPrintBuffer);
	//OutputDebugString(aPrintBuffer);
	fflush(stdout);
}
#elif defined(WIN32) && ! defined(RETAIL)
void orkprintf(const char *formatstring, ...)
{
	char aPrintBuffer[1024];
	va_list argp;
	va_start(argp, formatstring);
	vsnprintf(&aPrintBuffer[0], sizeof(aPrintBuffer), formatstring, argp);
	va_end(argp);

	aPrintBuffer[sizeof(aPrintBuffer)-1] = '\0';
	printf("%s", aPrintBuffer);
	//OutputDebugString(aPrintBuffer);
	fflush(stdout);
}
void orkerrorlog(const char *formatstring, ...)
{
	char aPrintBuffer[1024];
	va_list argp;
	va_start(argp, formatstring);
	vsnprintf(&aPrintBuffer[0], sizeof(aPrintBuffer), formatstring, argp);
	va_end(argp);

	fprintf( stderr, "%s", aPrintBuffer);
	//OutputDebugString(aPrintBuffer);

	fflush(stdout);
}
#endif

namespace ork {

///////////////////////////////////////////////////////////////////////////////

void fatalerror( const char *formatstring, ... )
{	va_list argp;
	va_start( argp, formatstring );
	vsnprintf( &errorbuffer[0], sizeof(errorbuffer), formatstring, argp );
	va_end( argp );
	orkprintf( "[FATALERROR] %s", errorbuffer );
	std::exit( -1 );
}

///////////////////////////////////////////////////////////////////////////////

void orkmessageh( const char *formatstring, ... )
{
	va_list argp;
	va_start( argp, formatstring );
	vsnprintf( &errorbuffer[0], sizeof(errorbuffer), formatstring, argp );
	va_end( argp );
#if defined(_XBOX)
	//OutputDebugString( errorbuffer );
#else
	orkprintf( "%s", errorbuffer );
#endif
}

///////////////////////////////////////////////////////////////////////////////

void orkmessageh( U32 chanid, const char *formatstring, ... )
{
	va_list argp;
	va_start( argp, formatstring );
	vsnprintf( &errorbuffer[0], sizeof(errorbuffer), formatstring, argp );
	va_end( argp );
#if defined(_XBOX)
	//OutputDebugString( errorbuffer );
#else
	orkprintf( "%s\n", errorbuffer );
#endif
}

///////////////////////////////////////////////////////////////////////////////

static OldSchool::LogPolicy sDefaultPolicy;

void OldSchool::Log(LOG_SEVERITY severity, const std::string &chanid, char *formatstring, ...)
{
	va_list argp;
	va_start( argp, formatstring );
	vsnprintf( &errorbuffer[0], sizeof(errorbuffer), formatstring, argp );
	va_end( argp );

	static const char *SEVERITY_TEXT[] = { "INFO: ", "WARNING: ", "ERROR: ", "FATAL: " };

#if !defined(NITRO) && !defined(WII)
	if(OldSchool::LogPolicy::GetContext()->mFileOut)
	{
		File *pFile = new File((chanid + ".Log").c_str(), EFM_APPEND);
		EFileErrCode eFileErr = pFile->Open();
		if(eFileErr == EFEC_FILE_OK)
		{
			pFile->Write(SEVERITY_TEXT[severity], strlen(SEVERITY_TEXT[severity]));
			pFile->Write(errorbuffer, strlen(errorbuffer));
			pFile->Close();

			delete pFile;
			pFile = NULL;
		}
	}
#endif

	if(OldSchool::LogPolicy::GetContext()->mAllChannelsToStdOut
			|| OldSchool::LogPolicy::GetContext()->mChannelsToStdOut.find(chanid) != OldSchool::LogPolicy::GetContext()->mChannelsToStdOut.end())
		orkprintf("[%s] %s%s\n", chanid.c_str(), SEVERITY_TEXT[severity], errorbuffer);
}

OldSchool::~OldSchool()
{
	for( orkmap<std::string, File*>::iterator it=mvLogChannels.begin(); it!=mvLogChannels.end(); it++ )
	{	std::pair<std::string, File*> pr = *it;
		File *pFile = pr.second;
		if( pFile )
			pFile->Close();
	}
}

///////////////////////////////////////////////////////////////////////////////

void win_messageh( const char *formatstring, ... )
{
	va_list argp;
	va_start( argp, formatstring );
	vsnprintf( &errorbuffer[0], sizeof(errorbuffer), formatstring, argp );
	va_end( argp );

}

} // namespace ork
