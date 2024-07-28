	////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/kernel/string/deco.inl>

///////////////////////////////////////////////////////////////////////////////

static const int kgchkpointbuffersize = 64;
static const int kgchkpointnumbuffers = 64;
static const int kerrorbuffersize = 4096;

char errorbuffer[kerrorbuffersize];

template class ork::util::Context<ork::OldSchool::LogPolicy>;

ork::mutex					gMemMutex( "yo" );

///////////////////////////////////////////////////////////////////////////////

void OrkHeapCheck(){}

///////////////////////////////////////////////////////////////////////////////

void OrkNonFatalAssertFunction(const char *fmtstr, ...)
{
	va_list argp;
	va_start(argp, fmtstr);
	vsnprintf(&errorbuffer[0], sizeof(errorbuffer), fmtstr, argp);
	va_end(argp);

	orkprintf(errorbuffer);
}

void OrkAssertFunction(const char *fmtstr, ...)
{
	va_list argp;
	va_start( argp, fmtstr );
	vsprintf( &errorbuffer[0], fmtstr, argp );
	va_end( argp );

	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf( "%s\n", fmtstr);
	orkprintf("crashing....\n");
	std::string bt = ork::get_backtrace();
	printf( "BACKTRACE\n%s\n", bt.c_str() );
	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf( "/////////////////////////////////////////////\n" );
	fflush(stdout);

	ork::msleep(500);

	char *pKILL = nullptr; // force kill process via segfault
	*pKILL = 0;

	std::exception a; // just in case we are on a platform 
	throw a;          // that allows nullptr deref without seg-faulting..

	//break 3;


	while(1){} // and as a last resort, ...

	//assert(false);
}

void _format_and_assert(const char* file, int line, const char* fmtstr, ...) {
    char formatbuffer[512];
    char assertbuffer[1024];
    va_list args;
    va_start(args, fmtstr);
    vsnprintf(formatbuffer, sizeof(formatbuffer), fmtstr, args);
    va_end(args);

    auto a1 = ork::deco::decorate(ork::fvec3(1), "Assertion At:\n  File: ");
    auto a2 = ork::deco::decorate(ork::fvec3(1), "\n  Line: ");
    auto a3 = ork::deco::decorate(ork::fvec3(1), "\n  Reason: ");
    auto a4 = ork::deco::decorate(ork::fvec3(1), "");
    auto b1 = ork::deco::format(255,192,255, "%s", file );
    auto b2 = ork::deco::format(255,128,255, "%d", line );
    auto b3 = ork::deco::format(255,255,0, "%s", formatbuffer );

    snprintf( assertbuffer, 
              sizeof(assertbuffer), 
              "%s%s%s%s%s%s%s",
              a1.c_str(), 
              b1.c_str(),
              a2.c_str(), 
              b2.c_str(), 
              a3.c_str(), 
              b3.c_str(), 
              a4.c_str() );

    OrkAssertFunction(assertbuffer);
}

///////////////////////////////////////////////////////////////////////////////

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
	orkprintf( "%s", errorbuffer );
}

///////////////////////////////////////////////////////////////////////////////

void orkmessageh( U32 chanid, const char *formatstring, ... )
{
	va_list argp;
	va_start( argp, formatstring );
	vsnprintf( &errorbuffer[0], sizeof(errorbuffer), formatstring, argp );
	va_end( argp );
	orkprintf( "%s\n", errorbuffer );
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

	if(OldSchool::LogPolicy::context()->mFileOut)
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

	if(OldSchool::LogPolicy::context()->mAllChannelsToStdOut
			|| OldSchool::LogPolicy::context()->mChannelsToStdOut.find(chanid) != OldSchool::LogPolicy::context()->mChannelsToStdOut.end())
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
