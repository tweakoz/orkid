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
#include <ork/kernel/string/StringBlock.h>
#include <ork/kernel/orklut.hpp>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/FileAssetNamer.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/math/audiomath.h>
#include <ork/application/application.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/BinaryDeserializer.h>
#include <ork/reflect/serialize/BinarySerializer.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/reflect/serialize/ShallowSerializer.h>
#include <ork/reflect/serialize/ShallowDeserializer.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::AudioGraph, "lev2::aud::AudioGraph");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::AudioModule, "lev2::aud::AudioModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::AudioGlobalModule, "lev2::aud::AudioGlobalModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::AudioLfoModule, "lev2::aud::AudioLfoModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::AudioOp2Module, "lev2::aud::AudioOp2Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::AudioExtConnectorModule, "lev2::aud::AudioExtConnectorModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::AudioHwSinkModule, "lev2::aud::AudioHwSinkModule");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::AudioKRateFilterModule, "lev2::aud::AudioKRateFilterModule");

using namespace ork::audiomath;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// modular audio playback
///////////////////////////////////////////////////////////////////////////////
void AudioModule::Describe()	{}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AudioGlobalModule::Describe()
{
}
AudioGlobalModule::AudioGlobalModule()
	: ConstructOutPlug(Time, dataflow::EPR_UNIFORM)
	, ConstructOutPlug(TimeDiv10, dataflow::EPR_UNIFORM)
	, ConstructOutPlug(TimeDiv100, dataflow::EPR_UNIFORM)
	, ConstructOutPlug(Random, dataflow::EPR_UNIFORM)
	, ConstructOutPlug(SlowNoise, dataflow::EPR_UNIFORM)
	, ConstructOutPlug(Noise, dataflow::EPR_UNIFORM)
	, ConstructOutPlug(FastNoise, dataflow::EPR_UNIFORM)
	, mPlugInpTimeScale( this, dataflow::EPR_UNIFORM, mfTimeScale, "ts" )
	, mfTimeScale(1.0f)
	, mOutDataRandom(0.0f)
	, mOutDataNoise(0.0f)
	, mfNoiseTim(0.0f)
	, mfSlowNoiseTim(0.0f)
	, mfFastNoiseTim(0.0f)
	, mfNoisePrv(0.0f)
	, mfSlowNoisePrv(0.0f)
	, mfFastNoisePrv(0.0f)
	, mfNoiseBas(0.0f)
	, mfSlowNoiseBas(0.0f)
	, mfFastNoiseBas(0.0f)
{
}
void AudioGlobalModule::Compute(float fdt)
{
	float ftime = ork::OldSchool::GetRef().GetLoResTime()*mPlugInpTimeScale.GetValue();
	mOutDataTime = ftime;
	mOutDataTimeDiv10 = ftime*0.1f;
	mOutDataTimeDiv100 = ftime*0.01f;
	mOutDataRandom = float(std::rand()%32767)/32768.0f;
	/////////////////////////////////////////
	// compute band limited noise
	/////////////////////////////////////////
	if( mfNoiseTim>=mfNoisePrv )
	{	mfNoiseBas = mOutDataNoise;
		mfNoiseTim = float(std::rand()%32767)/32768.0f;
		mfNoiseNew = float(std::rand()%32767)/32768.0f;
		mfNoiseRat = (mfNoiseNew-mfNoiseBas)/mfNoiseTim;
		mfNoiseTim = 0.0f;

	}
	mOutDataNoise = mfNoiseBas+(mfNoiseRat*mfNoiseTim);
	if( mfNoiseNew > mfNoiseBas )
	{
		if( mOutDataNoise > mfNoiseNew ) mOutDataNoise=mfNoiseNew;
	}
	if( mfNoiseNew < mfNoiseBas )
	{
		if( mOutDataNoise < mfNoiseNew ) mOutDataNoise=mfNoiseNew;
	}
	mfNoiseTim += fdt;
	/////////////////////////////////////////
}
dataflow::outplugbase* AudioGlobalModule::GetOutput(int idx)
{	dataflow::outplugbase* rval = 0;
	switch( idx )
	{	case 0:	rval = & OutPlugName(Time);			break;
		case 1:	rval = & OutPlugName(TimeDiv10);	break;
		case 2:	rval = & OutPlugName(TimeDiv100);	break;
		case 3:	rval = & OutPlugName(Random);		break;
		case 4:	rval = & OutPlugName(Noise);		break;
		case 5:	rval = & OutPlugName(FastNoise);	break;
		case 6:	rval = & OutPlugName(SlowNoise);	break;
	}
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
void AudioExtConnectorModule::Describe()
{	
	ork::reflect::RegisterMapProperty( "FloatPlugs", & AudioExtConnectorModule::mFloatPlugs );
	ork::reflect::annotatePropertyForEditor< AudioExtConnectorModule >("FloatPlugs", "editor.factorylistbase", "dflow/outplug<float>" );
}
AudioExtConnectorModule::AudioExtConnectorModule()
{
}
int AudioExtConnectorModule::GetNumOutputs() const
{	int iret = 0;
	size_t inump = mFloatPlugs.size();
	for( size_t i=0; i<inump; i++ )
	{
		const std::pair<PoolString,ork::dataflow::outplug<float>*>& pr = mFloatPlugs.GetItemAtIndex((int)i);
		ork::dataflow::outplug<float>* pplug = pr.second;
		if( pplug )
		{
			pplug->SetModule(const_cast<AudioExtConnectorModule*>(this));
			pplug->SetName( pr.first );
			pplug->SetRate( ork::dataflow::EPR_UNIFORM );
			
			static const float kzero = 0.0f;

			iret++;
		}
	}
	return iret;
}
dataflow::outplugbase* AudioExtConnectorModule::GetOutput(int idx)
{
	int iret = 0;
	size_t inump = mFloatPlugs.size();
	for( size_t i=0; i<inump; i++ )
	{
		ork::dataflow::outplug<float>* pplug = mFloatPlugs.GetItemAtIndex((int)i).second;
		if( pplug )
		{
			if( iret == idx )
			{
				return pplug;
			}
			iret++;
		}
	}
	return 0;
}
void AudioExtConnectorModule::BindConnector( dataflow::dyn_external* pconnector )
{	size_t inump = mFloatPlugs.size();
	for( size_t i=0; i<inump; i++ )
	{	ork::dataflow::outplug<float>* pplug = mFloatPlugs.GetItemAtIndex((int)i).second;
		if( pplug )
		{	const float* pfloat = 0;
			if( pconnector ) 
			{	const orklut<PoolString,dataflow::dyn_external::FloatBinding>& float_bindings = pconnector->GetFloatBindings();
				orklut<PoolString,dataflow::dyn_external::FloatBinding>::const_iterator it=float_bindings.find(pplug->GetName());
				if( it != float_bindings.end() )
				{	pfloat = it->second.mpSource;
				}
			}
			pplug->ConnectData( pfloat );
		}
	}
}

void AudioExtConnectorModule::Compute( float dt )
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AudioLfoModule::Describe()
{
	RegisterFloatXfPlug( AudioLfoModule, LfoFrequency, -100, 100.0f, ged::OutPlugChoiceDelegate );
	RegisterFloatXfPlug( AudioLfoModule, LfoBias, -100, 100.0f, ged::OutPlugChoiceDelegate );
	RegisterFloatXfPlug( AudioLfoModule, LfoAmplitude, -100, 100.0f, ged::OutPlugChoiceDelegate );
}
AudioLfoModule::AudioLfoModule()
	: ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
	, mPlugInpLfoFrequency( this, dataflow::EPR_UNIFORM, mfLfoFrequency, "frq" )
	, mPlugInpLfoBias( this, dataflow::EPR_UNIFORM, mfLfoBias, "bia" )
	, mPlugInpLfoAmplitude( this, dataflow::EPR_UNIFORM, mfLfoAmplitude, "amp" )
{
	mfLfoFrequency = 0.0f;
	mfLfoBias = 0.0f;
	mfLfoAmplitude = 0.0f;
}
dataflow::inplugbase* AudioLfoModule::GetInput(int idx)
{	dataflow::inplugbase* rval = 0;
	switch(idx)
	{
		case 0:
			rval = & mPlugInpLfoFrequency;
			break;
		case 1:
			rval = & mPlugInpLfoBias;
			break;
		case 2:
			rval = & mPlugInpLfoAmplitude;
			break;
	}
	return rval;
}
dataflow::outplugbase* AudioLfoModule::GetOutput(int idx)
{
	return & mPlugOutOutput;
}
void AudioLfoModule::Compute(float fdt)
{
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AudioKRateFilterModule::Describe()
{
	reflect::RegisterProperty( "Window", & AudioKRateFilterModule::mfWindow );
	RegisterFloatXfPlug( AudioKRateFilterModule, ControlInput, -100, 100.0f, ged::OutPlugChoiceDelegate );
}
AudioKRateFilterModule::AudioKRateFilterModule()
	: ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
	, mPlugInpControlInput( this, dataflow::EPR_UNIFORM, mfControlInput, "inp" )
	, mfWindow(0.0f)
	, mfControlInput(0.0f)
{
}
dataflow::inplugbase* AudioKRateFilterModule::GetInput(int idx)
{	dataflow::inplugbase* rval = 0;
	switch(idx)
	{
		case 0:
			rval = & mPlugInpControlInput;
			break;
	}
	return rval;
}
dataflow::outplugbase* AudioKRateFilterModule::GetOutput(int idx)
{
	return & mPlugOutOutput;
}
void AudioKRateFilterModule::Compute(float fdt)
{
	float input = mPlugInpControlInput.GetValue();
	mFilter.SetWindow(mfWindow);
	mFilter.mbEnable = (mfWindow!=0.0f);
	mOutDataOutput = mFilter.compute( input, fdt );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AudioOp2Module::Describe()
{
	RegisterFloatXfPlug( AudioOp2Module, InputA, -100, 100.0f, ged::OutPlugChoiceDelegate );
	RegisterFloatXfPlug( AudioOp2Module, InputB, -100, 100.0f, ged::OutPlugChoiceDelegate );
}
AudioOp2Module::AudioOp2Module()
	: ConstructOutPlug(Output, dataflow::EPR_UNIFORM)
	, mPlugInpInputA( this, dataflow::EPR_UNIFORM, mfInputA, "ina" )
	, mPlugInpInputB( this, dataflow::EPR_UNIFORM, mfInputB, "inb" )
	, meOp( EAO2_ADD )
{
	mfInputA = 0.0f;
	mfInputB = 0.0f;
}
dataflow::inplugbase* AudioOp2Module::GetInput(int idx)
{	dataflow::inplugbase* rval = 0;
	switch(idx)
	{
		case 0:
			rval = & mPlugInpInputA;
			break;
		case 1:
			rval = & mPlugInpInputB;
			break;
	}
	return rval;
}
dataflow::outplugbase* AudioOp2Module::GetOutput(int idx)
{
	return & mPlugOutOutput;
}
void AudioOp2Module::Compute(float fdt)
{
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AudioHwSinkModule::Describe()
{
	RegisterFloatXfPlug( AudioHwSinkModule, Amplitude, -100, 100.0f, ged::OutPlugChoiceDelegate );
	RegisterFloatXfPlug( AudioHwSinkModule, Pitch, -100, 100.0f, ged::OutPlugChoiceDelegate );
	RegisterFloatXfPlug( AudioHwSinkModule, Cutoff, -100, 100.0f, ged::OutPlugChoiceDelegate );
	RegisterFloatXfPlug( AudioHwSinkModule, Resonance, -100, 100.0f, ged::OutPlugChoiceDelegate );
	RegisterFloatXfPlug( AudioHwSinkModule, Pan, -100, 100.0f, ged::OutPlugChoiceDelegate );
}
AudioHwSinkModule::AudioHwSinkModule()
	: mPlugInpAmplitude( this, dataflow::EPR_UNIFORM, mfAmplitude, "amp" )
	, mPlugInpPitch( this, dataflow::EPR_UNIFORM, mfPitch, "pit" )
	, mPlugInpCutoff( this, dataflow::EPR_UNIFORM, mfCutoff, "cut" )
	, mPlugInpResonance( this, dataflow::EPR_UNIFORM, mfResonance, "res" )
	, mPlugInpPan( this, dataflow::EPR_UNIFORM, mfPan, "pan" )
	, mZonePB( 0 )
{
	mfAmplitude = 0.0f;
	mfPitch = 0.0f;
	mfCutoff = 0.0f;
	mfResonance = 0.0f;
	mfPan = 0.0f;
}
dataflow::inplugbase* AudioHwSinkModule::GetInput(int idx)
{	dataflow::inplugbase* rval = 0;
	switch(idx)
	{
		case 0:
			rval = & mPlugInpAmplitude;
			break;
		case 1:
			rval = & mPlugInpPitch;
			break;
		case 2:
			rval = & mPlugInpCutoff;
			break;
		case 3:
			rval = & mPlugInpResonance;
			break;
		case 4:
			rval = & mPlugInpPan;
			break;
	}
	return rval;
}
void AudioHwSinkModule::Compute( float fDT )
{
	if( mZonePB )
	{
		float famp = mPlugInpAmplitude.GetValue();
		float fpitch = mPlugInpPitch.GetValue();
		float fcutoff = mPlugInpCutoff.GetValue();
		float freso = mPlugInpResonance.GetValue();
		float fpan = mPlugInpPan.GetValue();

		mZonePB->SetPitchOffset( fpitch );

		//orkprintf( "sink<%p> pitch<%f>\n", this, fpitch );

		mZonePB->SetAmplitude( famp );
	}
}
void AudioModularZonePlayback::Update( float fDT )
{
	AudioInstrumentZoneContext& izc = GetContext();
	AudioSample* psamp = GetSample();
	AudioInstrumentZone* pizone = GetZone();
	//const AudioIntrumentPlayParam& pbparam = GetPlaybackParam();
	
	if( 0 == pizone ) return;

	///////////////////////////////////////////////////
	// update pitch
	///////////////////////////////////////////////////

	float fbaserate = float(psamp->GetSampleRate());
	int isamprootkey = psamp->GetRootKey();
	int irootkeyoverride = pizone->GetSampleRootOverride();
	int irootkey = irootkeyoverride;
	int itunesemis = pizone->GetTuneSemis();
	int itunecents =	pizone->GetTuneCents()
						+ int(mfPitchOffset*100.0f);
	int isemidelta = itunesemis+(mibasekey-irootkey); //-itunesemis; //+(mibasekey-irootkey);
	float fcents = float(isemidelta*100.0f)+float(itunecents);
	float fratio = cents_to_linear_freq_ratio(fcents);
	float fnewrate = fbaserate*fratio;
	SetPBSampleRate( fnewrate );

	///////////////////////////////////////////////////
	// update pan / amp
	///////////////////////////////////////////////////

	int ivel = GetInsPlayback()->GetPlaybackParam().miVelocity;
	if( ivel == -1 ) ivel = 127;

	const float kvelsc = (96.0f/127.0f);
	float atten_velocity = float(127-ivel)*kvelsc;

	float atten_centibel	= pizone->GetAttenCentibels();
	float atten_linear		= decibel_to_linear_amp_ratio( -atten_centibel*0.1f );
	float atten_linear_vel	= decibel_to_linear_amp_ratio( -atten_velocity );
	float atten_modlin		= mfAmplitude;
	izc.mfLinearAmplitude = atten_linear*atten_modlin*atten_linear_vel;

	izc.mfPan = pizone->GetPan()*0.005f; // limit pan spread to 50% for better spatialization while still allowing for stereo offset

}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void AudioGraph::Describe()
{
}
AudioGraph::AudioGraph()
	: mdflowregisters("audio",16)
{
}
AudioGraph::~AudioGraph()
{
}
void AudioGraph::operator=( const AudioGraph& oth )
{
}

bool AudioGraph::CanConnect( const ork::dataflow::inplugbase* pin, const ork::dataflow::outplugbase* pout ) const
{
	bool brval = false;
	brval |= (&pin->GetDataTypeId()==&typeid(float))&&(&pout->GetDataTypeId()==&typeid(float));
	return brval;
}
void AudioGraph::Update( float fDT )
{
	const orklut<int,ork::dataflow::dgmodule*>& Topos = LockTopoSortedChildrenForWrite(102);
	for( orklut<int,ork::dataflow::dgmodule*>::const_iterator it=Topos.begin(); it!= Topos.end(); it++ )
	{	ork::dataflow::dgmodule* pmodule = it->second;
		AudioModule* audmod = rtti::autocast(pmodule);
		audmod->Compute(fDT);
	}
	UnLockTopoSortedChildren();
	//mfElapsed+= fdt;
}
void AudioGraph::PrepForStart()
{
	///////////////////////////////////////////////////
	
	ork::dataflow::dyn_external* pdyn = GetExternal();

	size_t inummods = GetNumChildren();
	for( size_t i=0; i<inummods; i++ )
	{
		AudioExtConnectorModule* pext = rtti::autocast( GetChild((int)i) );
		if( pext )
		{
			pext->BindConnector(pdyn);
		}
		AudioHwSinkModule* psink = rtti::autocast( GetChild((int)i) );
		if( psink )
		{
			psink->SetZonePB( 0 );
		}
	}

	///////////////////////////////////////////////////

	mdflowctx.Clear();
	RefreshTopology( mdflowctx );
	Reset();
}
void AudioGraph::Reset()
{
	const orklut<int,ork::dataflow::dgmodule*>& Topos = LockTopoSortedChildrenForWrite(103);
	{
		for( orklut<int,ork::dataflow::dgmodule*>::const_iterator it=Topos.begin(); it!= Topos.end(); it++ )
		{	ork::dataflow::dgmodule* pmodule = it->second;
			AudioModule* paudmod = rtti::autocast(pmodule);
			paudmod->Reset();
		}
	}
	UnLockTopoSortedChildren();
	//mfElapsed = 0.0f;
	//mbEmitEnable = true;
}
void AudioGraph::compute()
{
}
void AudioGraph::BindZonePB( AudioModularZonePlayback* pb )
{
	size_t inummods = GetNumChildren();
	int isink = 0;
	int izone = pb->GetChannel();
	for( size_t i=0; i<inummods; i++ )
	{
		AudioHwSinkModule* psink = rtti::autocast( GetChild((int)i) );
		if( psink )
		{
			if( izone == isink )
			{
				psink->SetZonePB( pb );
			}
			isink++;
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
AudioGraphPool::AudioGraphPool()
	: mTemplate(0)
	, mGraphPool(0)
{
}
AudioGraphPool::~AudioGraphPool()
{
	if( mGraphPool )
		delete mGraphPool;
}
AudioGraph* AudioGraphPool::Allocate()
{	AudioGraph* pinstance = mGraphPool->allocate();
	if( pinstance )
	{
		pinstance->BindExternal(0);
	}
	return pinstance;
}
void AudioGraphPool::Free(AudioGraph*pgraph)
{	if( pgraph  )
	{	mGraphPool->deallocate(pgraph);
		pgraph->BindExternal(0);
	}
}
void AudioGraphPool::BindTemplate( const AudioGraph& InTemplate, int inumvoices )
{
	if( mGraphPool ) delete mGraphPool;
	mGraphPool = new ork::pool<AudioGraph>( inumvoices );
	////////////////////////////////////////////
	ork::ResizableString str;
	ork::stream::ResizableStringOutputStream ostream(str);
	ork::reflect::serialize::BinarySerializer binoser(ostream);
	InTemplate.GetClass()->Description().SerializeProperties(binoser, & InTemplate);
	////////////////////////////////////////////
	for( int i=0; i<inumvoices; i++ )
	{	AudioGraph& clone = mGraphPool->direct_access(i);
		//clone.ReInit();
		//clone.~AudioGraph();
		//new (&clone) AudioGraph();
		ork::stream::StringInputStream istream(str);
		ork::reflect::serialize::BinaryDeserializer biniser(istream);
		InTemplate.GetClass()->Description().DeserializeProperties(biniser, &clone);
	}
	mTemplate = & InTemplate;
}
///////////////////////////////////////////////////////////////////////////////
// Play sound using modular control graph
///////////////////////////////////////////////////////////////////////////////
AudioInstrumentPlayback * AudioDevice::PlaySound(	AudioProgram* Program, 
													const AudioIntrumentPlayParam & PlaybackParams,
													AudioGraph* controlgraph )
{	AudioInstrumentPlayback *PlaybackHandle = GetFreePlayback();
	bool pbOK = (PlaybackHandle != 0);
	if( pbOK )
	{	PlaybackHandle->ReInit();
		if( Program )
		{	int inote = PlaybackParams.miNote;
			int ivel = PlaybackParams.miVelocity;
			int ipan = 64+int(PlaybackParams.mPan*63.0f);
			//////////////////////////////////////////////////////////
			PlaybackHandle->SetParams( PlaybackParams );
			PlaybackHandle->SetGraph( controlgraph );
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
							{	AudioModularZonePlayback* ZonePB = mModPlaybacks.LockForWrite().allocate();
								mModPlaybacks.UnLock();
								ZonePB->BindAudioGraph( controlgraph );
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
								ZonePB->SetChannel( ichanidx );
								controlgraph->BindZonePB( ZonePB );
								PlaybackHandle->mpProgram = Program;
								PlaybackHandle->minumchannels++;
								PlaybackHandle->SetSubMix( PlaybackParams.mSubMixGroup );
								ichanidx++;
							}
						}
					}
				}
			}
			DoPlaySound( PlaybackHandle );
		}
	}
	else
	{	//PlaybackHandle->SetNumActiveChannels(0);//SetVoiceState( AudioInstrumentPlayback::ESTATE_INACTIVE );
	}
	return PlaybackHandle;
}
void AudioModularZonePlayback::Describe(){}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
