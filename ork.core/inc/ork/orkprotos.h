////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/orkconfig.h>
#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/orkstl.h>
#include <string>
#include <ork/util/Context.h>

void OrkHeapCheck();

#if defined(WII)
	//#if !defined(_DEBUG)
	//	#define orkprintf(...) ((void)0)
		#define printf(...) ((void)0)
	//#else
		#define orkprintf OSReport
		#define orkerrorlog OSReport
	//#endif

#elif defined(RETAIL)
	#define orkprintf(...) ((void)0);
	#define orkerrorlog(...) ((void)0);
	#define printf(...) ((void)0);
#else
	void orkprintf( const char *formatstring, ... );
	void orkerrorlog( const char *formatstring, ... );
#endif

#ifdef __cplusplus
namespace ork {
#endif

void msleep( int millisec );
void usleep( int microsec );

void fatalerror( const char *formatstring, ... );

std::string getfilename( const std::string & filterdesc, const std::string &filter, const std::string & title, const std::string & initdir );

void win_messageh( const char *formatstring, ... );

#ifdef NITRO
# define lite_printf(...) OS_TPrintf(__VA_ARGS__)
#else
# define lite_printf(...) orkprintf(__VA_ARGS__)
#endif

orkvector< std::string > getfilenames_multiple( char * filter, char * title, char * initdir );
void orkmessageh( const char *formatstring, ... );
void orkmessageh( U32 chanid, const char *formatstring, ... );
void SplitString( const std::string &instr, orkvector<std::string> &splitvect, const char *pdelim );

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

#if defined ( IX )
template <typename T> T min( T a, T b ) { return (a<b) ? a : b; }
#endif

#ifdef __cplusplus
}


struct CheckPointContext : public ork::util::ContextTLS<CheckPointContext>
{
	char mBuffer[256];
	CheckPointContext(const char *formatstring, ...);
	~CheckPointContext();
};

#if 1
# define CPC_CONCAT(x,y) CPC_CONCAT_(x,y)
# define CPC_CONCAT_(x,y) x ## y
# define MCheckPointContext(...)  CheckPointContext CPC_CONCAT(cpc_, __LINE__)(__VA_ARGS__)
#else
# define MCheckPointContext(...)  ((void)0)
#endif

#endif

