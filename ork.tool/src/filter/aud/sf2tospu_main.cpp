////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>

#include <orktool/filter/filter.h>
#include "soundfont.h"
#include "sf2tospu.h"
#include <ork/file/riff.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.hpp>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/audiobank.h>

#include <ork/stream/IOutputStream.h>
#include <ork/math/audiomath.h>

#if 1 //defined(_USE_SOUNDFONT)


///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {

void init_aiff_exporter( void );

///////////////////////////////////////////////////////////////////////////////

SF2PXVFilter::SF2PXVFilter( )
{
}

///////////////////////////////////////////////////////////////////////////////

SF2PXVAssetFilterInterface::SF2PXVAssetFilterInterface()
	//: AssetFilterInterface( "yo" )
{

}

///////////////////////////////////////////////////////////////////////////////

bool SF2PXVAssetFilterInterface::ConvertAsset( const std::string & FromFileName, const std::string & ToFileName )
{
	OrkHeapCheck();

	orkmessageh( "/////////////////////////////////////\n" );
	orkmessageh( "// SoundFont2 -> Playstation SoundBank Converter\n" );
	orkmessageh( "//	SourceFile [%s]\n", FromFileName.c_str() );
	orkmessageh( "/////////////////////////////////////\n" );

//	bool bwii = (0!=strstr(ToFileName.c_str(),"wii"));
	bool bxb360 = (0!=strstr(ToFileName.c_str(),"xb360"));

	if( FromFileName == (std::string) "" )
	{
		return false;
	}

	OrkHeapCheck();

	//////////////////////////////////////////////////////////////

	init_aiff_exporter();

	//////////////////////////////////////////////////////////////

	OrkHeapCheck();

	//////////////////////////////////////////////////////////////
	// Load the Soundfont

	SoundFontConversionEngine SF2Converter( FromFileName );

	//////////////////////////////////////////////////////////////

	ork::EndianContext endianctx;
	endianctx.mendian = ork::EENDIAN_LITTLE;

	chunkfile::Writer chunkwriter( "xab" );

	chunkfile::OutputStream* HeaderStream = chunkwriter.AddStream("header");
	chunkfile::OutputStream* ProgDataStream = chunkwriter.AddStream("progdata");
	chunkfile::OutputStream* WaveDataStream = chunkwriter.AddStream("wavedata");
	
	////////////////////////////////////////////////////////////////////////////////////

	int inumsamps = (int) SF2Converter.GetNumSamples();
	int inuminst = (int) SF2Converter.GetNumInstruments();
	int inumprogs = (int) SF2Converter.GetNumPrograms();

	HeaderStream->AddItem( inumsamps );
	HeaderStream->AddItem( inuminst );
	HeaderStream->AddItem( inumprogs );

	

	////////////////////////////////////////////////////////////////////////////////////
	// Samples
	////////////////////////////////////////////////////////////////////////////////////

	orkmessageh( "/////////////////////////////////////\n" );
	orkmessageh( "/////////////////////////////////////\n" );
	orkmessageh( "//num samples <%d>\n", inumsamps );

	for( int isamp=0; isamp<inumsamps; isamp++ )
	{
		HeaderStream->AddItem( isamp );

		const SF2Sample *psamp = SF2Converter.GetSample( isamp );

		const std::string & samplename = psamp->name;

		lev2::AudioSample outputsample;
	
		outputsample.SetSampleName( (const char*) chunkwriter.GetStringIndex(samplename.c_str()) );
		outputsample.SetSampleRate( psamp->samplerate );
		outputsample.SetRootKey( psamp->originalpitch );
		outputsample.SetLoopEnable( psamp->loop );
		outputsample.SetPitchCorrection( int(psamp->pitchcorrection) );
		outputsample.SetLoopStart( psamp->GetLoopStart() );
		outputsample.SetLoopEnd( psamp->GetLoopEnd() );
		outputsample.SetDataPointer( (const void*) WaveDataStream->GetSize() );
		outputsample.SetNumSamples( psamp->GetNumSamples() );

		////////////////////////
		// Uncompressed Data
		////////////////////////
		{
			int idata_len = sizeof(S16)*psamp->GetNumSamples();
			outputsample.SetFormat( lev2::AudioSample::EDF_UNCOMPRESSED );
			WaveDataStream->Write( (const unsigned char *) psamp->GetSampleData(), idata_len );
		}

		ProgDataStream->AddItem( outputsample );

		orkmessageh( "//sample<%d> name<%s> samplerate<%d> root<%d> loop<%d>\n", isamp, samplename.c_str(), psamp->samplerate, int(psamp->originalpitch), int(psamp->loop) );

	}

	////////////////////////////////////////////////////////////////////////////////////
	// Instruments
	////////////////////////////////////////////////////////////////////////////////////

	for( int iinst=0; iinst<inuminst; iinst++ )
	{
		const SF2Instrument *pinst = SF2Converter.GetInstrument( iinst );
		const std::string & instname = pinst->GetName();
		int inumzones = (int) pinst->GetNumZones();
		//////////////////////////////////
		HeaderStream->AddItem( iinst );
		HeaderStream->AddItem( chunkwriter.GetStringIndex(instname.c_str()) );
		HeaderStream->AddItem( inumzones );
		//////////////////////////////////
		orkmessageh( "//inst<%d of %d> name<%s> numzones<%d>\n", iinst, inuminst, instname.c_str(), inumzones );
		//////////////////////////////////
		for( int izone=0; izone<inumzones; izone++ )
		{	const SF2InstrumentZone & InstrumentZone = pinst->GetIZoneFromIndex( izone );
			HeaderStream->AddItem( izone );
			//////////////////////////////////
			ork::lev2::AudioInstrumentZone tempizone( InstrumentZone );
			tempizone.SetSample( (lev2::AudioSample *) InstrumentZone.sampleID );
			ProgDataStream->AddItem( tempizone );
			//////////////////////////////////
			orkmessageh( "// izone<%d> keymin<%d> keymax<%d> velmin<%d> velmax<%d> sampleID<%d> pan<%f> tune:semis<%d> tune:cents<%d> tune.rko<%d> atten:centib<%f> delay:time<%f>\n", izone,
					InstrumentZone.GetKeyMin(), InstrumentZone.GetKeyMax(), InstrumentZone.GetVelMin(), InstrumentZone.GetVelMax(),
					InstrumentZone.sampleID,InstrumentZone.GetPan(), InstrumentZone.GetTuneSemis(), InstrumentZone.GetTuneCents(), InstrumentZone.GetSampleRootOverride(),
					InstrumentZone.GetAttenCentibels(), InstrumentZone.GetAmpEnvDelay() );
			//////////////////////////////////
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Programs
	////////////////////////////////////////////////////////////////////////////////////

	for( int iprog=0; iprog<inumprogs; iprog++ )
	{
		const SF2Program *pprog = SF2Converter.GetProgram( iprog );
		int imapped = pprog->mapped_preset;
		std::string instname = pprog->GetName();
		//////////////////////////////////
		// fixup instnames (remove trailing spaces)

		size_t ilen=instname.length();

		bool btrail = true;
		int ileading = 0;
		for( ssize_t i=ilen-1; i>=0; i-- )
		{
			char ch=instname[i];

			if( ch== ' ' )
			{
				if( btrail )
				{
					ileading++;
				}
			}
			else
			{
				btrail=false;
			}

		}

		instname = instname.substr(0,(ilen-ileading));

		//////////////////////////////////
		int inumzones = (int) pprog->GetNumZones();
		//////////////////////////////////
		HeaderStream->AddItem( iprog );
		HeaderStream->AddItem( imapped );
		HeaderStream->AddItem( chunkwriter.GetStringIndex(instname.c_str()) );
		HeaderStream->AddItem( inumzones );
		//////////////////////////////////
		orkmessageh( "//prog<%d> name<%s> numzones<%d>\n", iprog, instname.c_str(), inumzones );
		//////////////////////////////////
		for( int izone=0; izone<inumzones; izone++ )
		{	const SF2ProgramZone & pzone = pprog->GetZone( izone );
			HeaderStream->AddItem( izone );
			//////////////////////////////////
			lev2::AudioProgramZone temppzone;
			temppzone.SetKeyMin( pzone.key_min );
			temppzone.SetKeyMax( pzone.key_max );
			temppzone.SetVelMin( pzone.vel_min );
			temppzone.SetVelMax( pzone.vel_max );
			temppzone.SetInstrument( (lev2::AudioInstrument *) pzone.instrumentID );
			ProgDataStream->AddItem( temppzone );
			//////////////////////////////////
			orkmessageh( "// pzone<%d> keymin<%d> keymax<%d> velmin<%d> velmax<%d> instID<%d>\n", 
							izone, pzone.key_min, pzone.key_max, pzone.vel_min, pzone.vel_max, pzone.instrumentID );
			//////////////////////////////////
		}
	}

	////////////////////////////////////////////////////////////////////////////////////

	file::Path tofpath( ToFileName.c_str() );
	chunkwriter.WriteToFile( tofpath );

	////////////////////////////////////////////////////////////////////////////////////

	return true;
}

///////////////////////////////////////////////////////////////////////////////

} }
#endif
