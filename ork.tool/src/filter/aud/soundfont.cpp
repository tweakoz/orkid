///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/file/file.h>
#include <ork/math/audiomath.h>
#include <ork/lev2/aud/audiobank.h>
#include "soundfont.h"
#include "sf2tospu.h"
#include <ork/kernel/string/string.h>

#if defined(_USE_SOUNDFONT)
void	OrkHeapCheck( void );

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::SF2XABFilter,"SF2XABFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::SF2GABFilter,"SF2GABFilter");

////////////////////////////////////////////////////////////////////////////////
using namespace ork::audiomath;

namespace ork { namespace tool
{

std::set<std::string> SoundFont::mSearchPaths;

////////////////////////////////////////////////////////////////////////////////

void SF2XABFilter::Describe() {}

SF2XABFilter::SF2XABFilter( )
{
}

bool SF2XABFilter::ConvertAsset( const tokenlist& toklist )
{
	tokenlist::const_iterator it = toklist.begin();
	const std::string& inf = *it++;
	const std::string& outf = *it++;
	SF2PXVAssetFilterInterface iface;
	return iface.ConvertAsset( inf, outf );
}

/////////////////////////////////////////////////////////////////////////////////

void SF2GABFilter::Describe() {}

SF2GABFilter::SF2GABFilter( )
{
}

bool SF2GABFilter::ConvertAsset( const tokenlist& toklist )
{
	tokenlist::const_iterator it = toklist.begin();
	const std::string& inf = *it++;
	const std::string& outf = *it++;
	SF2PXVAssetFilterInterface iface;
	return iface.ConvertAsset( inf, outf );
}
///////////////////////////////////////////////////////////////////////////////

SF2Program::SF2Program()
{
	preset = 0;
	bank = 0;
	pbag_base = 0;
	library = 0;
	genre = 0;
	morphology = 0;
	num_pbags = 0;
	mapped_preset = 0xffffffff;
//	create_control_callback = create_pxmpresetov;

	std::string name = "preset";

	//add_attributeSTRING( 0, name, ATTR_RW, ATTR_SHOW, "Preset" );

}

////////////////////////////////////////////////////////////////////////////////

SoundFontConversionEngine::SoundFontConversionEngine( const std::string & SoundFontName )
	: mSoundFontName( SoundFontName )
	, numinst( 0 )
	, numizones( 0 )
	, numigen( 0 )
	, numsamples( 0 )
	, sampledata( 0 )
	, misampleblockdatalen( 0 )
{
	std::string filename = SoundFontName;

	RIFFFile RiffFile;
	RiffFile.OpenFile( filename );
	RiffFile.LoadChunks();

	GetSBFK( RiffFile.GetChunk( "ROOT" ) );
	Process();
}

SoundFontConversionEngine::~SoundFontConversionEngine()
{
	for( auto item : mPXMPrograms ) delete item;
	for( auto item : mPXMProgramZones ) delete item;
	for( auto item : mPXMInstruments ) delete item;
	for( auto item : mPXMInstrumentZones ) delete item;
	for( auto item : mPXMSamples ) delete item;
	for( auto item : mDynamicChunks ) delete item;
}


////////////////////////////////////////////////////////////////////////////////

void SoundFontConversionEngine::AddProgram( Ssfontpreset *preset )
{
	SF2Program *cpre = new SF2Program;

	cpre->bank = preset->wBank;
	cpre->genre = preset->dwGenre;
	cpre->library = preset->dwLibrary;
	cpre->morphology = preset->dwMorphology;
	cpre->pbag_base = preset->wPresetBagNdx;
	cpre->preset = preset->wPreset;
	cpre->mapped_preset = cpre->preset;
	
	cpre->SetName( preset->GetName() );

	orkmessageh( "pbase: %d preset %d mapped %d name %s\n", cpre->pbag_base, cpre->preset, cpre->mapped_preset, cpre->GetName().c_str() );

	mPXMPrograms.push_back( cpre );

}

////////////////////////////////////////////////////////////////////////////////

void SoundFontConversionEngine::AddSample( Ssfontsample *sample )
{
	SF2Sample *pxsample = new SF2Sample( sample );
	mPXMSamples.push_back( pxsample );
}	

////////////////////////////////////////////////////////////////////////////////

void SoundFontConversionEngine::AddPresetGen( SSoundFontGenerator *pgn )
{
	mPXMPresetGen.push_back( pgn );
}

////////////////////////////////////////////////////////////////////////////////

void SoundFontConversionEngine::AddInstrumentZone( Ssfontinstbag *ibg )
{
	SF2InstrumentZone *pxmi = new SF2InstrumentZone;
	pxmi->base_generator = ibg->wInstGenNdx;
	pxmi->SetBaseModulator( ibg->wInstModNdx );
	mPXMInstrumentZones.push_back( pxmi );
}

////////////////////////////////////////////////////////////////////////////////

void SoundFontConversionEngine::AddInstrument( Ssfontinst *inst )
{
	SF2Instrument *pxmi = new SF2Instrument;
	pxmi->izone_base = inst->wInstBagNdx;
	//strncpy( (char *) & pxmi->name[0], (char *) & inst->achInstName[0], 20 );
	pxmi->SetName( inst->GetName() );
	pxmi->SetIndex( mPXMInstruments.size() );
	mPXMInstruments.push_back( pxmi );
}

////////////////////////////////////////////////////////////////////////////////

void SoundFontConversionEngine::AddInstrumentGen( SSoundFontGenerator *igen )
{
	mPXMInstrumentGen.push_back( igen );
}

////////////////////////////////////////////////////////////////////////////////

void SoundFontConversionEngine::AddProgramZone( Ssfontprebag *pbg )
{
	SF2ProgramZone *pxmp = new SF2ProgramZone;
	pxmp->base_generator = pbg->wInstGenNdx;
	mPXMProgramZones.push_back( pxmp );
}

////////////////////////////////////////////////////////////////////////////////

void SF2InstrumentZone::ApplyGenerator( ESF2Generators egen, S16 GenVal )
{
	const float kfrqbase = 8.1757989156f;

	switch( egen )
	{
		////////////////////////////////////////////////////
		case ESF2GEN_KEYRANGE:
		{	SetKeyMax( (GenVal&0xff00)>>8 );
			SetKeyMin( (GenVal&0x00ff) );
			break;
		}
		case ESF2GEN_VELRANGE:
		{	SetKeyMax((GenVal&0xff00)>>8);
			SetKeyMin(GenVal&0x00ff);
			break;
		}
		////////////////////////////////////////////////////
		case ESF2GEN_OVERRIDEROOTKEY: //	overriding ROOT key
		{
			SetSampleRootOverride( (u8) GenVal );
			break;
		}
		case ESF2GEN_TUNE_SEMIS: //	coarse tune (semitones)
		{	SetTuneSemis(GenVal);
			break;
		}
		case ESF2GEN_TUNE_CENTS://	fine tune (cents)
		{
			SetTuneCents(GenVal);
			break;
		}
		////////////////////////////////////////////////////
		case ESF2GEN_SAMPLE_LOOPMODE: //	loopsample
		{	loop = GenVal;
			//OrkAssert( IsGlobalZone() == false );
			break;
		}
		////////////////////////////////////////////////////
		case ESF2GEN_BASE_ATTENUATION: //	initial Attenuation (centibels)
		{	
			float fatten = (float(GenVal)/2.6f);
			SetAttenCentibels(short(fatten));
			break;
		}
		////////////////////////////////////////////////////
		case ESF2GEN_ZONE_PAN: //	Zone Pan
		{	ipan = GenVal;
			SetPan( float(GenVal) * 0.1f );
			break;
		}
		////////////////////////////////////////////////////
		case ESF2GEN_BASE_FILTER_CUTOFF: 
		{	
			float frqratio = cents_to_linear_freq_ratio(float(GenVal));
			float freq = kfrqbase*frqratio;
			SetFilterCutoff(freq);
			break;
		}
		case ESF2GEN_BASE_FILTER_Q: 
		{	
			//float flevel = centibel_to_linear_amp_ratio(float(GenVal));
			SetFilterQ( float(GenVal) );
			break;
		}
		////////////////////////////////////////////////////
		case ESF2GEN_MODLFO_DELAY: 
		{	
			float ftime = timecent_to_linear_time(GenVal);
			SetModLfoDelay(ftime);
			break;
		}
		case ESF2GEN_MODLFO_FREQ: 
		{	
			float frqratio = cents_to_linear_freq_ratio(float(GenVal));
			float freq = kfrqbase*frqratio;
			SetModLfoFrequency(freq);
			break;
		}
		case ESF2GEN_MODLFO_TO_PITCH: 
		{	
			float fval = float(GenVal);
			SetModLfoToPitch(fval);					
			break;
		}
		case ESF2GEN_MODLFO_TO_FC: 
		{	
			float fval = float(GenVal);
			SetModLfoToCutoff(fval);					
			break;
		}
		case ESF2GEN_MODLFO_TO_AMP: 
		{	
			float fval = float(GenVal);
			SetModLfoToAmp(fval);					
			break;
		}
		////////////////////////////////////////////////////
		case ESF2GEN_VIBLFO_DELAY: 
		{	
			float ftime = timecent_to_linear_time(GenVal);
			SetVibLfoDelay(ftime);
			break;
		}
		case ESF2GEN_VIBLFO_FREQ: 
		{	
			float frqratio = cents_to_linear_freq_ratio(float(GenVal));
			float freq = kfrqbase*frqratio;
			SetVibLfoFrequency(freq);
			break;
		}
		case ESF2GEN_VIBLFO_TO_PITCH: 
		{	
			float fval = float(GenVal);
			SetVibLfoToPitch(fval);					
			break;
		}
		////////////////////////////////////////////////////
		case ESF2GEN_AMPENV_DELAY: // (timecents)
		{
			float ftime = timecent_to_linear_time(GenVal);
			SetAmpEnvDelay(ftime);
			break;
		}
		case ESF2GEN_AMPENV_ATTACK: // (timecents)
		{	
			float ftime = timecent_to_linear_time(GenVal);
			SetAmpEnvAttack(ftime);
			break;
		}
		case ESF2GEN_AMPENV_HOLD: // (timecents)
		{	
			float ftime = timecent_to_linear_time(GenVal);
			SetAmpEnvHold(ftime);
			break;
		}
		case ESF2GEN_AMPENV_DECAY: // (timecents)
		{	
			float ftime = timecent_to_linear_time(GenVal);
			SetAmpEnvDecay(ftime);
			break;
		}
		case ESF2GEN_AMPENV_SUSTAIN: // (centibels)
		{
			float flevel = centibel_to_linear_amp_ratio(-float(GenVal));
			SetAmpEnvSustain(flevel);
			break;
		}
		case ESF2GEN_AMPENV_RELEASE: // (timecents)
		{	
			float ftime = timecent_to_linear_time(GenVal);
			SetAmpEnvRelease(ftime);
			break;
		}
		case ESF2GEN_AMPENV_KEYNUMTOHOLD: 
		{	
			float fval = float(GenVal);
			SetAmpEnvKeyNumToHold(fval);					
			break;
		}
		case ESF2GEN_AMPENV_KEYNUMTODECAY: 
		{	
			float fval = float(GenVal);
			SetAmpEnvKeyNumToDecay(fval);					
			break;
		}
		////////////////////////////////////////////////////
		case ESF2GEN_MODENV_DELAY: // (timecents)
		{	
			float ftime = timecent_to_linear_time(GenVal);
			SetAmpEnvDelay(ftime);
			break;
		}
		case ESF2GEN_MODENV_ATTACK: // (timecents)
		{	
			float ftime = timecent_to_linear_time(GenVal);
			SetAmpEnvAttack(ftime);
			break;
		}
		case ESF2GEN_MODENV_HOLD: // (timecents)
		{	
			float ftime = timecent_to_linear_time(GenVal);
			SetAmpEnvHold(ftime);
			break;
		}
		case ESF2GEN_MODENV_DECAY: // (timecents)
		{	
			float ftime = timecent_to_linear_time(GenVal);
			SetAmpEnvDecay(ftime);
			break;
		}
		case ESF2GEN_MODENV_SUSTAIN: // (-.1% units, 0==100%)
		{	int ival = GenVal;
			float flevel = 1.0f - float(ival)*0.001f;
			SetModEnvSustain(flevel);
			break;
		}
		case ESF2GEN_MODENV_RELEASE: // (timecents)
		{	
			float ftime = timecent_to_linear_time(GenVal);
			SetAmpEnvRelease(ftime);
			break;
		}
		case ESF2GEN_MODENV_KEYNUMTOHOLD: 
		{	
			float fval = float(GenVal);
			SetModEnvKeyNumToHold(fval);					
			break;
		}
		case ESF2GEN_MODENV_KEYNUMTODECAY: 
		{	
			float fval = float(GenVal);
			SetModEnvKeyNumToDecay(fval);					
			break;
		}
		////////////////////////////////////////////////////
		case ESF2GEN_MUTEX_GROUP: 
		{	SetMutexGroup( GenVal );
			break;
		}
		////////////////////////////////////////////////////
		case ESF2GEN_SAMPLE_ID: //	sampleID (terminal generator)
		{	sampleID = GenVal;
			break;
		}
		////////////////////////////////////////////////////
	}
}

////////////////////////////////////////////////////////////////////////////////

void SoundFontConversionEngine::ProcessInstruments( void )
{
	numinst = (int) mPXMInstruments.size();
	numizones = (int) mPXMInstrumentZones.size();
	numigen = (int) mPXMInstrumentGen.size();
	numsamples = (int) mPXMSamples.size();

	OrkHeapCheck();

	//////////////////////////////////////////////////
	// scan Instrument Zone Generators

	for( int j=1; j<=numizones; j++ )
	{	int i = j-1;
		
		SF2InstrumentZone *izone = mPXMInstrumentZones[i];
		//SF2InstrumentZone *izone2 = mPXMInstrumentZones[j];
		
		if( j==849 )
		{
			orkprintf( "yo\n" );
		}

		int iBASEGEN = izone->base_generator;
		int iLASTGEN = ( j<=(numizones-1) ) ? mPXMInstrumentZones[j]->base_generator : numigen;

		izone->num_generators = (iLASTGEN-iBASEGEN);

		izone->index = i;
		izone->sampleID = 0xffff;
				
		int isize = (int) mPXMInstrumentGen.size();

		for( int g=0; g<izone->num_generators; g++ )
		{	
			int iGenINDEX = iBASEGEN + g;
			
			SSoundFontGenerator *igen = mPXMInstrumentGen[iGenINDEX];
			
			ESF2Generators egen = ESF2Generators(igen->muGeneratorID);

			orkmap<ESF2Generators,S16>::const_iterator it=izone->mGenerators.find(egen);
			OrkAssert(it==izone->mGenerators.end());

			izone->mGenerators.insert(std::make_pair(egen,igen->miGeneratorValue));

			///////////////////////////////////////////
			// is keyrange first generator ?
			///////////////////////////////////////////

			if( egen == ESF2GEN_KEYRANGE )
			{
				//OrkAssert(g==0);
			}

			///////////////////////////////////////////
			// is sampleid last generator ?
			///////////////////////////////////////////

			if( egen == ESF2GEN_SAMPLE_ID )
			{
				OrkAssert(g==(izone->num_generators-1));
			}

			///////////////////////////////////////////
			// is this a global zone ?
			///////////////////////////////////////////

			if( g==izone->num_generators-1 )
			{
				if( egen != ESF2GEN_SAMPLE_ID )
				{
					izone->SetGlobalZone();
				}
			}

			///////////////////////////////////////////
		}

		for( orkmap<ESF2Generators,S16>::const_iterator it=izone->mGenerators.begin(); it!=izone->mGenerators.end(); it++ )
		{	
			ESF2Generators egen = it->first;
			S16 GenVal = it->second;
			izone->ApplyGenerator( egen, GenVal );
		}
	}
	
	OrkHeapCheck();

	//////////////////////////////////////////////////
	// setup Instruments

	for( int j=1; j<=numinst; j++ )
	{	
		int i=j-1;
				
		SF2Instrument *inst = mPXMInstruments[i];
		
		int iz1 = (int) inst->izone_base;
		int iz2 = (j<=(numinst-1)) ? (int) mPXMInstruments[j]->izone_base : (numizones);
		
		inst->num_izones = (iz2-iz1);

		/////////////////////////////////////////////////////////
		// find wacky SF2 "Global Split" Data
		/////////////////////////////////////////////////////////

		SF2InstrumentZone* pGlobalIZone = 0;
		bool bHasGlobalZone = false;

		for( size_t iz=0; iz<inst->num_izones; iz++ )
		{
			int izone_index = int(inst->izone_base) + iz ;

			SF2InstrumentZone *pZ = mPXMInstrumentZones[ izone_index ];

			if( pZ->IsGlobalZone() )
			{
				OrkAssert( bHasGlobalZone==false );
				bHasGlobalZone = true;
				pGlobalIZone = pZ;
			}
		}

		/////////////////////////////////////////////////////////
		// Distrubute wacky SF2 "Global Split" Data
		/////////////////////////////////////////////////////////

		for( size_t iz=0; iz<inst->num_izones; iz++ )
		{
			SF2InstrumentZone *pZ = mPXMInstrumentZones[ inst->izone_base + iz ];

			if( bHasGlobalZone )
			{
				if( pZ != pGlobalIZone )
				{
					for( orkmap<ESF2Generators,S16>::const_iterator it = pGlobalIZone->mGenerators.begin();
																	it != pGlobalIZone->mGenerators.end();
																	it++ )
					{
						ESF2Generators egen = it->first;
						S16 GenVal = it->second;

						orkmap<ESF2Generators,S16>::const_iterator itf = pZ->mGenerators.find(egen);

						if( itf==pZ->mGenerators.end() )
						{
							pZ->ApplyGenerator( egen, GenVal );	
							pZ->mGenerators.insert( std::make_pair( egen, GenVal ) );
						}
					}

					inst->mIZones.push_back( *pZ );
				}
			}
			else
			{
				inst->mIZones.push_back( *pZ );
			}
		}

		/////////////////////////////////////////////////////////

		//orkmessageh( "inst %d [%s]\n", i, inst->GetName().c_str() );
	}

	OrkHeapCheck();

	//////////////////////////////////////////////////
	// Mark samples as looped
	
	for( int i=0; i<numizones; i++ )
	{	SF2InstrumentZone *izone = mPXMInstrumentZones[i];
		U32 smpID = izone->sampleID;
		
		int loopi = (izone->loop & 1);
		if( loopi && (smpID != 0xffff) )
		{	SF2Sample *smp = mPXMSamples[ smpID ];
			smp->loop = 1;
		}
	}

	//////////////////////////////////////////////////
	// Finish Up Samples

	U32 samplen = 0;

    int imaxsamp = (int) mPXMSamples.size();

    orkprintf( "NumSamples %d : %d\n", numsamples, imaxsamp );

	OrkHeapCheck();

	for( int i=0; i<numsamples; i++ )
	{
		SF2Sample *smp = mPXMSamples[ i ];

		int ilen = smp->end - smp->start;

		int ismpbase = (int) mSampleData.size();

		for( int iv=0; iv<ilen; iv++ )
		{
			mSampleData.push_back( sampledata[ smp->start+iv ] );
		}

		smp->SetNumSamples( ilen );

		U32 sampr = smp->samplerate;
		F32 time = F32( smp->GetNumSamples() )/ F32( sampr );
		samplen += smp->GetNumSamples();


		S16 *p16src = & sampledata[ smp->start ];
		S16 *p16dst = new S16[ ilen ];
		smp->SetSampleData( p16dst );
		memcpy( p16dst, p16src, 2*ilen );

		OrkAssert( (smp->start+ilen) < misampleblockdatalen );

		///////////////////////////////////

		int ilstart = smp->GetLoopStart() - smp->start;
		int ilend =   smp->GetLoopEnd()   - smp->start;

		smp->SetLoopStart( ilstart );
		smp->SetLoopEnd( ilend-1 );

		smp->start = ismpbase;
		smp->end = ismpbase+ilen;

		///////////////////////////////////

		//smp->VagCheckResample();

		//orkmessageh( "Sample %d %08x %s [len %d] [res_len %d] [start %d] [end %d] [loopstart %d] [loopend %d] [res_loopstart %d] [res_loopend %d] [loopena %d]\n", i, smp, smp->name.c_str(), smp->len, smp->resample_len, smp->start, smp->end, smp->loopstart, smp->loopend, smp->resample_loopstart, smp->resample_loopend, smp->loop );

	}

	//////////////////////////////////////////////////
}

void SoundFontConversionEngine::ProcessPresets( void )
{
	int inumpresets = (int) mPXMPrograms.size();
	int inumprez = (int) this->mPXMProgramZones.size();
	int inumpgen = (int) this->mPXMPresetGen.size();

	orkmessageh( "post_process_pzones()\n" );

	////////////////////////
	
	for( int j=1; j<=inumprez; j++ )
	{	
		int i = j-1;
		
		SF2ProgramZone *pre = mPXMProgramZones[i];
		
		int pg1 = pre->base_generator;
		int pg2 = ( j<=inumprez-1 ) ? mPXMProgramZones[j]->base_generator : inumpgen;

		pre->num_generators = (pg2-pg1);
		pre->instrumentID = -1;

		orkmessageh( "//////////////////////////////\n" );
		orkmessageh( "// pzone %d numpregens %d genbase %d pg1 %d pg2 %d\n", i, pre->num_generators, pre->base_generator, pg1, pg2 );

		for( int g=0; g<pre->num_generators; g++ )
		{	
			int gnum = g+pre->base_generator;
			SSoundFontGenerator *pgen = mPXMPresetGen[gnum];
			
			if( pgen->muGeneratorID == 41 ) // instrument
			{
				pre->instrumentID = pgen->miGeneratorValue;
			}
			else if( pgen->muGeneratorID == 43 ) // key range
			{
				pre->key_max = (pgen->miGeneratorValue&0xff00)>>8;
				pre->key_min = (pgen->miGeneratorValue&0x00ff);
			}
			else if( pgen->muGeneratorID == 44 ) // vel range
			{
				pre->vel_max = (pgen->miGeneratorValue&0xff00)>>8;
				pre->vel_min = (pgen->miGeneratorValue&0x00ff);
				
				if( (pre->vel_min == 0) && (pre->vel_max == 0) ) // 00 .. 00 really means 00 .. 7f ?
				{
					pre->vel_max = 0x7f;
				}

			}
			else
			{
				//orkmessageh( "pgen: %d val: %d\n", pgen->muGeneratorID, pgen->miGeneratorValue );
			}
		}

		orkmessageh( "pzone: %d instrument %d keymin: %d keymax: %d\n", i, pre->instrumentID, pre->key_min, pre->key_max ); 
	}
	////////////////////////

	orkmessageh( "post_process_presets()\n" );
			
	for( int j=1; j<=inumpresets; j++ )
	{
		int i=j-1;

		SF2Program *preset = mPXMPrograms[i];
		U32 pr1 = preset->pbag_base;
		U32 pr2 = ( j<=inumpresets-1 ) ? mPXMPrograms[j]->pbag_base : numprebags;

		preset->num_pbags = (pr2-pr1);
				
		orkmessageh( "//////////////////////////\n" );
		orkmessageh( "// program: %03d <%s> pbag_base %d num_pbags %d pr1 %d pr2 %d mapped %d\n", i, preset->GetName().c_str(), preset->pbag_base, preset->num_pbags, pr1, pr2, preset->mapped_preset );
		
//		SF2Program *mapped = mPXMPrograms[ preset->mapped_preset ];

		for( int ipbag=0; ipbag<preset->num_pbags; ipbag++ )
		{	
			int zoneidx = preset->pbag_base + ipbag;
			
			SF2ProgramZone *pzone = mPXMProgramZones[zoneidx];

			if( pzone->instrumentID>=0 )
			{
				SF2Instrument *inst = mPXMInstruments[ pzone->instrumentID ];
				preset->AddZone( *pzone );
				orkmessageh( "//	[zone %d of %d] [zoneID %d] [name %s]\n", ipbag, preset->num_pbags, pzone->instrumentID, inst->GetName().c_str() );
			}
		}
	}

}

////////////////////////////////////////////////////////////////////////////////

SoundFont::SoundFont( ) 
	: mpSampleData( 0 )
	, miSampleBlockLength( 0 )
{
}

////////////////////////////////////////////////////////////////////////////////

SoundFont::SoundFont( SoundFontConversionEngine *pengine ) 
	: mpSampleData( 0 )
	, miSampleBlockLength( 0 )
{

	if( pengine )
	{
		int isampblksize = (int) pengine->GetSampleBlockSize();
		
		//const s16 *pdata = pengine->GetSampleBlock();

		miSampleBlockLength = isampblksize;
		mpSampleData = new s16[ isampblksize ];

		for( int i=0; i<isampblksize; i++ )
		{
			mpSampleData[i] = pengine->GetSampleData( i );
		}
		//memcpy( mpSampleData, pdata, sizeof(s16)*isampblksize );

		int inuminstruments = (int) pengine->GetNumInstruments();
		int inumsamples = (int) pengine->GetNumSamples();

		for( int i=0; i<inuminstruments; i++ )
		{
			const SF2Instrument *pinst = pengine->GetInstrument( i );
			
			mSF2Instruments.push_back( *pinst );


			const SF2Instrument *psf2i = & mSF2Instruments[ i ];


			std::string InstrumentName = CreateFormattedString( "/%s/[%d]%s", mSoundFontName.c_str(), pinst->GetIndex(), pinst->GetName().c_str() );
			std::string InstrumentShortName = CreateFormattedString( "/[%d] %s", pinst->GetIndex(), pinst->GetName().c_str() );
		//	SoundFont::mSF2IntrumentChoices.add( AttrChoiceValue( InstrumentName, (u32) i, InstrumentShortName ) );

		}

		for( int i=0; i<inumsamples; i++ )
		{
			const SF2Sample *psamp = pengine->GetSample( i );
			mSF2Samples.push_back( *psamp );

		}

	}

}

const SF2Instrument *SoundFont::GetInstrument( int idx ) const
{
	return & mSF2Instruments[ idx ];
}

const SF2Sample * SoundFont::GetSample( int idx ) const
{
	OrkAssert( idx < int(mSF2Samples.size()) );
	return & mSF2Samples[ idx ];
}

///////////////////////////////////////////////////////////////////////////////

void SoundFont::LoadSearchPaths( void )
{
	mSearchPaths.clear();

/*	file::Path ConfigFile( "data://soundfonts/soundfonts.cfg" );

	if( FileEnv::DoesFileExist( ConfigFile ) )
	{
		CScannerParser Parser( ConfigFile );

		Parser.Tokenize();

		if( ORKPARSE_MATCH == Parser.CompareToken( 0, "SoundFontPaths" ) )
		{
			if( ORKPARSE_MATCH == Parser.CompareToken( 1, "Count" ) )
			{
				int icount = atoi( Parser.GetToken(2) );

				Parser.AdvanceToken(4);

				for( int ic=0; ic<icount; ic++ )
				{
					if( ORKPARSE_MATCH == Parser.CompareToken( 0, "path" ) )
					{
						const char *ppath = Parser.GetToken( 1 );
						OrkSTXSetInsert( mSearchPaths, (std::string) ppath );
						Parser.AdvanceToken( 2 );
					}
				}
			}
		}

	}*/
}

///////////////////////////////////////////////////////////////////////////////

void SoundFont::AddSearchPath( const std::string & searchpath )
{
	if( OrkSTXSetInsert( mSearchPaths, searchpath ) )
	{
		file::Path ConfigFile( "soundfonts.cfg" );
		
		File OutFile( ConfigFile, EFM_WRITE );
		OutFile.Open();

		std::string OutData;

		OutData += CreateFormattedString( "SoundFontPaths Count %d {\n", mSearchPaths.size() );
		
		for( std::set<std::string>::iterator it=mSearchPaths.begin(); it!=mSearchPaths.end(); it++ )
		{
			OutData += CreateFormattedString( "	path \"%s\"\n", (*it).c_str() );
		}

		OutData += CreateFormattedString( "}\n\n" );

		OutFile.Write( OutData.c_str(), OutData.size() );

		OutFile.Close();
	}
	
}

///////////////////////////////////////////////////////////////////////////////

std::string SoundFont::FindSoundFont( const std::string & named )
{
	std::string rval;

	for( std::set<std::string>::iterator it=mSearchPaths.begin(); it!=mSearchPaths.end(); it++ )
	{
		std::string path = (*it);

		std::string filename = path + named + ".sf2";

		if( FileEnv::DoesFileExist( file::Path(filename.c_str()) ) )
		{
			rval = filename;
		}
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

int SF2Sample::GetLoopStart( void ) const 
{
	return miloopstart;
}

int SF2Sample::GetLoopEnd( void ) const 
{
	return miloopend;
}

S16 SF2Sample::GetSample( int idx ) const
{
	OrkAssert( idx < misamplelen );
	return mpsampledata[ idx ];
}

} }
#endif
