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
	orkprintf("error<%s>\n", fmtstr);
	orkprintf("crashing....\n");
	std::string bt = ork::get_backtrace();
	printf( "BACKTRACE\n%s\n", bt.c_str() );
	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf( "/////////////////////////////////////////////\n" );
	orkprintf( "/////////////////////////////////////////////\n" );
	fflush(stdout);

	ork::msleep(500);

	char *pFUCKED = nullptr;
	*pFUCKED = 0;

	std::exception a;
	throw a;

	//break 3;


	while(1){}

	//assert(false);
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
