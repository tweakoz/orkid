////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _AUD_DATAFILTER_SF2TOSPU_H
#define _AUD_DATAFILTER_SF2TOSPU_H

#if 1 //defined(_USE_SOUNDFONT)
#define MSGPANE_DEBUG 0

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {

class SF2PXVAssetFilterInterface 
{
public:

	bool ConvertAsset( const std::string & inf, const std::string & outf );

	SF2PXVAssetFilterInterface();

};

class CSF2PXVFilter 
{
	public: //

	////////////////////////////////////

	CSF2PXVFilter( );

	////////////////////////////////////

	static void PostProcess( CSoundFont *pSF2 );
	static void PostProcessPresets( CSoundFont *pSF2 );
	static void PostProcessInstruments( CSoundFont *pSF2 );

	////////////////////////////////////

};

////////////////////////////////////////////////////////////////////////////////

#pragma warning( disable : 4355 )  
#pragma warning( disable : 4099 )  
#pragma warning( disable : 4786 )  
#pragma warning( disable : 4800 )  

////////////////////////////////////////////////////////////////////////////////

} }

#endif
#endif
