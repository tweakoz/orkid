////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/audiobank.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.hpp>
#include <ork/kernel/string/StringBlock.h>
#include <ork/kernel/orklut.hpp>
#include <ork/asset/FileAssetLoader.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/math/audiomath.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::AudioBank, "lev2::audiobank" );

using namespace ork::audiomath;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
AudioIntrumentPlayParam AudioIntrumentPlayParam::DefaultParams;
///////////////////////////////////////////////////////////////////////////////

class AudioBankLoader : public ork::asset::FileAssetLoader
{
public:

	AudioBankLoader();

	/*virtual*/ bool LoadFileAsset(asset::Asset *pAsset, ConstString filename);
	/*virtual*/ void DestroyAsset(asset::Asset *pAsset)
	{
		AudioBank* pbank = rtti::autocast(pAsset);
		AudioDevice::GetDevice()->DestroyBank( pbank );
		
		//delete compasset->GetComponent();
		//compasset->SetComponent(NULL);
	}
};

///////////////////////////////////////////////////////////////////////////////

AudioBankLoader::AudioBankLoader()
	:  FileAssetLoader( AudioBank::GetClassStatic() )
{
	AddLocation("data://",".xab");
}

///////////////////////////////////////////////////////////////////////////////

bool AudioBankLoader::LoadFileAsset(asset::Asset *pAsset, ConstString filename)
{
	AudioBank* pbank = rtti::autocast(pAsset);

	bool bOK = AudioDevice::GetDevice()->LoadSoundBank( pbank,filename );
	OrkAssert( bOK );
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void AudioBank::Describe()
{
	auto loader = new AudioBankLoader;
	GetClassStatic()->AddLoader(loader);
	GetClassStatic()->SetAssetNamer("data://audio/banks");
	GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("lev2::audiobank"));
}

///////////////////////////////////////////////////////////////////////////////
struct AudioBankAllocator
{	//////////////////////////////
	// per chunk allocation policy
	//////////////////////////////
	void* alloc( const char* pchkname, int ilen )
	{	void* pmem = 0;
		if( 0 == strcmp( pchkname, "header" ) )
		{
			pmem = new char[ilen];
		}
		else if( 0 == strcmp( pchkname, "progdata" ) )
		{
			pmem = new char[ilen];
		}
		else if( 0 == strcmp( pchkname, "wavedata" ) )
		{
#if defined(WII)
		pmem = wii::LockSharedMem2Buf(ilen);
#else
		pmem = new char[ilen];
#endif
		}
		return pmem;
	}
	//////////////////////////////
	// per chunk deallocation policy
	//////////////////////////////
	void done( const char* pchkname, void* pdata )
	{
		if( 0 == strcmp( pchkname, "header" ) ) {}			// audio banks keep this resident
		else if( 0 == strcmp( pchkname, "progdata" ) ) {}	// audio banks keep this resident
		else if( 0 == strcmp( pchkname, "wavedata" ) )
		{
#if defined(WII)
			wii::UnLockSharedMem2Buf();
#endif
		}	// audio banks keep this resident
	}
};



///////////////////////////////////////////////////////////////////////////////
bool AudioDevice::LoadSoundBank( AudioBank* ppxvbank, ConstString fname )
{	bool rval = false;
	bool busevagd = false;
	/////////////////////////////////////////////////////////////
	AssetPath filename = ppxvbank->GetName().c_str();
	filename.SetExtension( "xab" );
	/////////////////////////////////////////////////////////////
	chunkfile::Reader<AudioBankAllocator> chunkreader( filename, "xab" );
	/////////////////////////////////////////////////////////////
	if( chunkreader.IsOk() )
	{	
		/////////////////////////////////////////////////////////
		chunkfile::InputStream* HeaderBin = chunkreader.GetStream("header");
		chunkfile::InputStream* MyDataBin = chunkreader.GetStream("progdata");
		chunkfile::InputStream* WaveStream = chunkreader.GetStream("wavedata");
		/////////////////////////////////////////////////////////
		HeaderBin->GetItem( ppxvbank->miNumSamples );
		HeaderBin->GetItem( ppxvbank->miNumInstruments );
		HeaderBin->GetItem( ppxvbank->miNumPrograms );
		/////////////////////////////////////////////////////////
		ppxvbank->mpSamples = (lev2::AudioSample*) MyDataBin->GetCurrent();
		ppxvbank->mpInstruments = new lev2::AudioInstrument[ppxvbank->miNumInstruments];
		AudioProgram* Programs = new AudioProgram[ ppxvbank->miNumPrograms ];
		/////////////////////////////////////////////////////////
		for( int isamp=0; isamp<ppxvbank->miNumSamples; isamp++ )
		{	int itsamp = 0;
			HeaderBin->GetItem( itsamp );
			OrkAssert( itsamp==isamp );
			AudioSample* psample = 0;
			MyDataBin->RefItem( psample );
			ssize_t iwaveindex = ssize_t(psample->GetDataPointer());
			psample->SetDataPointer( WaveStream->GetDataAt(iwaveindex) );
			psample->SetSampleName( chunkreader.GetString( ssize_t(psample->GetSampleName()) ) );
		}
#if defined(_XBOX) // temp, move me to converter
		int inumpcmsamps = WaveStream->GetLength()/2;
		S16* ps16 = (S16*) WaveStream->mpbase;
		for( int is=0; is<inumpcmsamps; is++ )
		{
			swapbytes_from_little( ps16[is] );
		}
#endif
		/////////////////////////////////////////////////////////
		int iii = 0;
		int insname = 0;	
		int inumzones = 0;	
		/////////////////////////////////////////////////////////
		for( int ii=0; ii<ppxvbank->miNumInstruments; ii++ )
		{	
			lev2::AudioInstrument& instrument = ppxvbank->mpInstruments[ii];
			HeaderBin->GetItem( iii );
			OrkAssert(iii==ii);
			HeaderBin->GetItem( insname );
			HeaderBin->GetItem( inumzones );
			const char* InstrumentName = chunkreader.GetString( insname );
			instrument.SetName( InstrumentName );
			AudioInstrumentZone* pzones = (inumzones==0) ? 0 : (AudioInstrumentZone*) MyDataBin->GetCurrent();
			instrument.SetZones( pzones, inumzones );
			for( int iz=0; iz<inumzones; iz++ )
			{	int itzone = 0;	
				HeaderBin->GetItem( itzone );
				OrkAssert(itzone==iz);
				AudioInstrumentZone* pzone = 0;
				MyDataBin->RefItem( pzone );
				if( pzone )
				{
					AudioSample*samp = ppxvbank->mpSamples+ssize_t(pzone->GetSample());
					pzone->SetSample(samp);
				}
			}
		}
		/////////////////////////////////////////////////////////
		for( int ii=0; ii<ppxvbank->miNumPrograms; ii++ )
		{	int imapped = 0;	
			int inumpzones = 0;
			HeaderBin->GetItem( iii );
			OrkAssert(iii==ii);
			HeaderBin->GetItem( imapped );
			HeaderBin->GetItem( insname );
			HeaderBin->GetItem( inumpzones );
			const char* ProgramName = chunkreader.GetString( insname );
			PoolString psname = AddPooledString(ProgramName);
			AudioProgram* Program = Programs+ii;
			Program->SetName( psname );
			ppxvbank->AddProgram( psname, Program );
			AudioProgramZone* pzones = (inumpzones==0) ? 0 : (AudioProgramZone*) MyDataBin->GetCurrent();
			Program->SetZones( pzones, inumpzones );
			for( int pz=0; pz<inumpzones; pz++ )
			{	int itzone = 0;	
				HeaderBin->GetItem( itzone );
				OrkAssert(itzone==pz);
				AudioProgramZone* pzone = 0;
				MyDataBin->RefItem( pzone );
				lev2::AudioInstrument*inst = ppxvbank->mpInstruments+ssize_t(pzone->GetInstrument());
				pzone->SetInstrument(inst);
			}
		}
		/////////////////////////////////////////////////////////
		ppxvbank->Enable();
		InitBank( *ppxvbank );
		/////////////////////////////////////////////////////////
		rval = true;
	}
	else
	{
		ppxvbank->Disable();
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

int AudioSample::GetDataLength( void ) const
{
	return GetNumChannels()*miNumSamples*sizeof(U16);
}

///////////////////////////////////////////////////////////////////////////////

AudioInstrumentPlayback::AudioInstrumentPlayback()
	: mSubMix( "none" )
	, mpAudioGraph( 0 )
	, mEmitterMatrix(0)
{
	for( int i=0; i<kmaxzonesperevent; i++ )
		mZonePlaybacks[i] = 0; // just to shut up valgrind

	ReInit();
}

///////////////////////////////////////////////////////////////////////////////

void AudioInstrumentPlayback::SetStereoPan( int ipan )
{
	mibasepan = ipan;
}

///////////////////////////////////////////////////////////////////////////////

void AudioInstrumentPlayback::Stop( void )
{
	AudioDevice::GetDevice()->StopSound( this );
	mEmitterMatrix = 0;
}

///////////////////////////////////////////////////////////////////////////////

void AudioInstrumentPlayback::Update(float fDT)
{
	if( mpAudioGraph )
	{
		mpAudioGraph->Update( fDT );
	}
	for( int iz=0; iz<kmaxzonesperevent; iz++ )
	{
		AudioZonePlayback* pzone = mZonePlaybacks[iz];
		if( pzone )
		{
			pzone->Update(fDT);
		}
	}
	//ork::lev2::AudioDevice::GetDevice()->SetSoundSpatial( this, mEmitterPos );
}

///////////////////////////////////////////////////////////////////////////////

AudioInstrumentPlayback * AudioDevice::PlaySound(	AudioProgram* Program, 
													const AudioIntrumentPlayParam & PlaybackParams )
{	AudioInstrumentPlayback *PlaybackHandle = GetFreePlayback();
	if( PlaybackHandle!=0 )
	{	PlaybackHandle->ReInit();
		if( Program )
		{	int inote = PlaybackParams.miNote;
			int ivel = PlaybackParams.miVelocity;
			int ipan = 64+int(PlaybackParams.mPan*63.0f);
			//////////////////////////////////////////////////////////
			PlaybackHandle->SetParams( PlaybackParams );
			PlaybackHandle->minumchannels = 0;
			//////////////////////////////////////////////////////////
			const char *pprogname = Program->GetName().c_str();
			int inumpzones = Program->GetNumZones();
			int ichanidx = 0;
			if( inote == -1 ) { inote = 60; }
			if( ivel == -1 ) { ivel = 127; }
			for( int ipzone=0; ipzone<inumpzones; ipzone++ )
			{	AudioProgramZone & pzone = Program->RefZone( ipzone );
				//////////////////////////////////////
				if(	   (inote >= pzone.GetKeyMin())
					&& (inote <= pzone.GetKeyMax()) )
				//////////////////////////////////////
				{	AudioInstrument *pinst = pzone.GetInstrument();
					int inumizones = pinst->GetNumZones();
					for( int iiz=0; iiz<inumizones; iiz++ )
					{	AudioInstrumentZone & izone = pinst->RefZone( iiz );
						//////////////////////////////////////
						if(	   (inote >= izone.GetKeyMin())
							&& (inote <= izone.GetKeyMax()) )
						//////////////////////////////////////
						{	if( ichanidx < kmaxzonesperevent )
							{	Sf2PlaybackPool& pool = mSf2Playbacks.LockForWrite();
								AudioSf2ZonePlayback* ZonePB = pool.allocate();
								mSf2Playbacks.UnLock();
								PlaybackHandle->SetZonePlayback(ichanidx,ZonePB);
								AudioSample * psamp = izone.GetSample();
								float fbaserate = float(psamp->GetSampleRate());
								int isamprootkey = psamp->GetRootKey();
								int irootkeyoverride = izone.GetSampleRootOverride();
								int irootkey = irootkeyoverride; //(irootkeyoverride!=isamprootkey) ? irootkeyoverride : isamprootkey;
								int itunesemis = izone.GetTuneSemis();
								int itunecents = izone.GetTuneCents();
								int isemidelta = itunesemis+(inote-irootkey);
								float fcents = float(isemidelta*100.0f)+float(itunecents);
								float fratio = cents_to_linear_freq_ratio(fcents);
								float fnewrate = fbaserate*fratio;
								ZonePB->SetPBSampleRate( fnewrate );
								ZonePB->SetSample(psamp);
								ZonePB->SetZone( & izone );
								ZonePB->SetBaseKey( inote );
								PlaybackHandle->mpProgram = Program;
								PlaybackHandle->minumchannels++;
								PlaybackHandle->SetSubMix( PlaybackParams.mSubMixGroup );
								ichanidx++;
							}
						}
					}
				}
			}
			//////////////////////////////////////////////////////////
			DoPlaySound( PlaybackHandle );
			//////////////////////////////////////////////////////////
		}
	}
	//else
	//{	PlaybackHandle->SetNumActiveChannels(0);//SetVoiceState( AudioInstrumentPlayback::ESTATE_INACTIVE );
	//}
	return PlaybackHandle;
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevice::StopSound( AudioInstrumentPlayback * phandle )
{
	if( 0 != phandle->GetNumActiveChannels() )
	{
		DoStopSound( phandle );
	}
}

void AudioDevice::FreeZonePlayback( AudioZonePlayback* pzone )
{
	AudioSf2ZonePlayback* pSF2 = rtti::autocast(pzone);
	AudioModularZonePlayback* pMOD = rtti::autocast(pzone);

	if( pSF2 )
	{
		Sf2PlaybackPool& sf2pool = mSf2Playbacks.LockForWrite();
		const Sf2PlaybackPool::pointervect_type& used = sf2pool.used();
		const Sf2PlaybackPool::pointervect_type::const_iterator it = std::find( used.begin(), used.end(), pSF2 );
		if( it!=used.end() ) sf2pool.deallocate(pSF2);
		mSf2Playbacks.UnLock();
	}
	else if( pMOD )
	{
		ModPlaybackPool& modpool = mModPlaybacks.LockForWrite();
		const ModPlaybackPool::pointervect_type& used = modpool.used();
		const ModPlaybackPool::pointervect_type::const_iterator it = std::find( used.begin(), used.end(), pMOD );
		if( it!=used.end() ) modpool.deallocate(pMOD);
		mModPlaybacks.UnLock();
	}
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevice::InitBank( AudioBank & bank )
{
	int inumsamples = bank.GetNumSamples();

	for( int isamp=0; isamp<inumsamples; isamp++ )
	{
		AudioSample & Sample = bank.RefSample( isamp );

		DoInitSample( Sample );
	}

}


///////////////////////////////////////////////////////////////////////////////

void AudioBank::SetNumSamples( int inumsamples )
{
	mpSamples = new AudioSample[ inumsamples ];
	miNumSamples = inumsamples;
}

///////////////////////////////////////////////////////////////////////////////

void AudioBank::SetNumInstruments( int inuminst )
{
	mpInstruments = new AudioInstrument[ inuminst ];
	miNumInstruments = inuminst;
}

///////////////////////////////////////////////////////////////////////////////

void AudioBank::SetNumPrograms( int inumprog )
{
	//mpPrograms = new AudioProgram[ inumprog ];
	miNumPrograms = inumprog;
}

///////////////////////////////////////////////////////////////////////////////

void AudioInstrument::SetNumZones( int inumzones )
{
	mpInstrumentZones = new AudioInstrumentZone[ inumzones ];
	miNumZones = inumzones;
}

///////////////////////////////////////////////////////////////////////////////

void AudioProgram::SetNumZones( int inumzones )
{
	mpProgramZones = new AudioProgramZone[ inumzones ];
	miNumZones = inumzones;
}

///////////////////////////////////////////////////////////////////////////////

AudioInstrumentZone::AudioInstrumentZone()
	: mikeymin( 0 )
	, mikeymax( 127 )
	, mivelmin( 0 )
	, mivelmax( 127 )
	, mpSample( 0 )
	, mfPan( 0.0f )
	, miTuneSemis(0)
	, miTuneCents(0)
	, miSampleRootOverride(0)
	, miMutexGroup(-1)
	, mfAttenCentibels(0.0f)
	, mfFilterCutoff(0.0f)
	, mfFilterQ(0.0f)

	, mfAmpEnvDelay(0.0f)
	, mfAmpEnvAttack(0.0f)
	, mfAmpEnvHold(0.0f)
	, mfAmpEnvDecay(0.0f)
	, mfAmpEnvSustain(1.0f)
	, mfAmpEnvRelease(0.0f)
	, mfAmpEnvKeyNumToHold(0.0f)
	, mfAmpEnvKeyNumToDecay(0.0f)

	, mfModEnvDelay(0.0f)
	, mfModEnvAttack(0.0f)
	, mfModEnvHold(0.0f)
	, mfModEnvDecay(0.0f)
	, mfModEnvSustain(1.0f)
	, mfModEnvRelease(0.0f)
	, mfModEnvKeyNumToHold(0.0f)
	, mfModEnvKeyNumToDecay(0.0f)

	, mfModLfoDelay(0.0f)
	, mfModLfoFrequency(8.176f)
	, mfModLfoToPitch(0.0f)
	, mfModLfoToCutoff(0.0f)
	, mfModLfoToAmp(0.0f)

	, mfVibLfoDelay(0.0f)
	, mfVibLfoFrequency(8.176f)
	, mfVibLfoToPitch(0.0f)
{

}

AudioInstrumentZone::AudioInstrumentZone(const AudioInstrumentZone& oth )
	: mikeymin( oth.mikeymin )
	, mikeymax( oth.mikeymax )
	, mivelmin( oth.mivelmin )
	, mivelmax( oth.mivelmax )
	, mpSample( oth.mpSample )
	, mfPan( oth. mfPan )
	, miTuneSemis( oth.miTuneSemis)
	, miTuneCents( oth.miTuneCents)
	, miSampleRootOverride( oth.miSampleRootOverride )
	, miMutexGroup( oth.miMutexGroup)
	, mfAttenCentibels( oth.mfAttenCentibels)
	, mfFilterCutoff(oth.mfFilterCutoff)
	, mfFilterQ(oth.mfFilterQ)

	, mfAmpEnvDelay( oth.mfAmpEnvDelay)
	, mfAmpEnvAttack( oth.mfAmpEnvAttack)
	, mfAmpEnvHold( oth.mfAmpEnvHold)
	, mfAmpEnvDecay( oth.mfAmpEnvDecay)
	, mfAmpEnvSustain( oth.mfAmpEnvSustain)
	, mfAmpEnvRelease( oth.mfAmpEnvRelease)
	, mfAmpEnvKeyNumToHold( oth.mfAmpEnvKeyNumToHold)
	, mfAmpEnvKeyNumToDecay( oth.mfAmpEnvKeyNumToDecay)

	, mfModEnvDelay( oth.mfModEnvDelay)
	, mfModEnvAttack( oth.mfModEnvAttack)
	, mfModEnvHold( oth.mfModEnvHold)
	, mfModEnvDecay( oth.mfModEnvDecay)
	, mfModEnvSustain( oth.mfModEnvSustain)
	, mfModEnvRelease( oth.mfModEnvRelease)
	, mfModEnvKeyNumToHold( oth.mfModEnvKeyNumToHold)
	, mfModEnvKeyNumToDecay( oth.mfModEnvKeyNumToDecay)

	, mfModLfoDelay(oth.mfModLfoDelay)
	, mfModLfoFrequency(oth.mfModLfoFrequency)
	, mfModLfoToPitch(oth.mfModLfoToPitch)
	, mfModLfoToCutoff(oth.mfModLfoToCutoff)
	, mfModLfoToAmp(oth.mfModLfoToAmp)

	, mfVibLfoDelay(oth.mfVibLfoDelay)
	, mfVibLfoFrequency(oth.mfVibLfoFrequency)
	, mfVibLfoToPitch(oth.mfVibLfoToPitch)

{
}

void AudioInstrumentZone::EndianSwap()
{
	swapbytes_dynamic( mpSample );

	swapbytes_dynamic( mikeymin );
	swapbytes_dynamic( mikeymax );
	swapbytes_dynamic( mivelmin );
	swapbytes_dynamic( mivelmax );

	swapbytes_dynamic( mfPan );
	swapbytes_dynamic( miTuneSemis );
	swapbytes_dynamic( miTuneCents );
	swapbytes_dynamic( miSampleRootOverride );
	swapbytes_dynamic( miMutexGroup );
	swapbytes_dynamic( mfAttenCentibels );
	swapbytes_dynamic( mfFilterCutoff );
	swapbytes_dynamic( mfFilterQ );

	swapbytes_dynamic( mfAmpEnvDelay );
	swapbytes_dynamic( mfAmpEnvAttack );
	swapbytes_dynamic( mfAmpEnvHold );
	swapbytes_dynamic( mfAmpEnvDecay );
	swapbytes_dynamic( mfAmpEnvSustain );
	swapbytes_dynamic( mfAmpEnvRelease );
	swapbytes_dynamic( mfAmpEnvKeyNumToHold );
	swapbytes_dynamic( mfAmpEnvKeyNumToDecay );

	swapbytes_dynamic( mfModEnvDelay );
	swapbytes_dynamic( mfModEnvAttack );
	swapbytes_dynamic( mfModEnvHold );
	swapbytes_dynamic( mfModEnvDecay );
	swapbytes_dynamic( mfModEnvSustain );
	swapbytes_dynamic( mfModEnvRelease );
	swapbytes_dynamic( mfModEnvKeyNumToHold );
	swapbytes_dynamic( mfModEnvKeyNumToDecay );

	swapbytes_dynamic( mfModLfoDelay );
	swapbytes_dynamic( mfModLfoFrequency );
	swapbytes_dynamic( mfModLfoToPitch );
	swapbytes_dynamic( mfModLfoToCutoff );
	swapbytes_dynamic( mfModLfoToAmp );

	swapbytes_dynamic( mfVibLfoDelay );
	swapbytes_dynamic( mfVibLfoFrequency );
	swapbytes_dynamic( mfVibLfoToPitch );

}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////

template class ork::chunkfile::Reader<ork::lev2::AudioBankAllocator>;
