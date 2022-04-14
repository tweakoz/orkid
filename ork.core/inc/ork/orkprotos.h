////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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

void orkprintf( const char *formatstring, ... );
void orkerrorlog( const char *formatstring, ... );

//#ifdef __cplusplus
namespace ork {
//#endif

void msleep( int millisec );
void usleep( int microsec );

void fatalerror( const char *formatstring, ... );

std::string getfilename( const std::string & filterdesc, const std::string &filter, const std::string & title, const std::string & initdir );

void win_messageh( const char *formatstring, ... );

orkvector< std::string > getfilenames_multiple( char * filter, char * title, char * initdir );
void orkmessageh( const char *formatstring, ... );
void orkmessageh( U32 chanid, const char *formatstring, ... );
void SplitString( const std::string &instr, orkvector<std::string> &splitvect, const char *pdelim );

///////////////////////////////////////////////////////////////////////////////

//#ifdef __cplusplus
}
