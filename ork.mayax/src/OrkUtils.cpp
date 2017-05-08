////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include"OrkUtils.h"
#include"UvRail.h"

#include <maya/MFnPlugin.h>

///////////////////////////////////////////////////////////////////////////////
using namespace ork::maya;
///////////////////////////////////////////////////////////////////////////////

MStatus initializePlugin( MObject Obj )
{
    printf( "Loading OrkUtils maya plugin...\n");
	MStatus Status;
	MFnPlugin Plugin( Obj, "OrkUtils", "1.0", "Any");
    ///////////////////////////////////////////////////////
    // register commands
    ///////////////////////////////////////////////////////
    Status = Plugin.registerCommand("_OrkUvRailOverlapped", UvRailOverlappedCommand::instantiate);
    if (!Status)
    {
        Status.perror("registerCommand");
        return Status;
    }
    ///////////////////////////////////////////////////////
    Status = Plugin.registerCommand("_OrkUvRailUnitized", UvRailUnitizedCommand::instantiate);
    if (!Status)
    {
        Status.perror("registerCommand");
        return Status;
    }
    ///////////////////////////////////////////////////////
    return Status;
}

///////////////////////////////////////////////////////////////////////////////

MStatus uninitializePlugin(MObject Obj)
{
	MStatus   Status;
	MFnPlugin Plugin(Obj);
    ///////////////////////////////////////////////////////
	Status = Plugin.deregisterCommand("_OrkUvRailOverlapped");
    if (!Status)
    {   Status.perror("deregisterCommand");
        return Status;
    }
    ///////////////////////////////////////////////////////
	Status = Plugin.deregisterCommand("_OrkUvRailUnitized");
    if (!Status)
    {   Status.perror("deregisterCommand");
        return Status;
    }
    ///////////////////////////////////////////////////////
    return Status;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace maya {
static const int ktexbufsize = 1024;
static char textbuf[ktexbufsize];

std::string formatString( const char* formatstring, ... )
{
    va_list argp;
    va_start( argp, formatstring );
    vsnprintf( textbuf, ktexbufsize, formatstring, argp );
    va_end( argp );
    std::string rval = (std::string) textbuf;
    return rval;
}

void logMessage( const char* formatstring, ... )
{
    va_list argp;
    va_start( argp, formatstring );
    vsnprintf( textbuf, ktexbufsize, formatstring, argp );
    va_end( argp );

    MGlobal::displayInfo( textbuf );
    fflush(stdout);
}

void logError( const char* formatstring, ... )
{
    va_list argp;
    va_start( argp, formatstring );
    vsnprintf( textbuf, ktexbufsize, formatstring, argp );
    va_end( argp );

    MGlobal::displayInfo( textbuf );
    fflush(stdout);
}

///////////////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace maya {
///////////////////////////////////////////////////////////////////////////////


