////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

// Some code in here from Apple's Core Audio Play Thru sample.
//   But according to the sample code's license,
//   if it is modified, and/or not in it's entirety, then I do not need the original license text....
//

#if 0 //defined(ORK_OSX)
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/entity.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <pkg/ent/AudioAnalyzer.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/enum_serializer.inl>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioAnalysisSystemData, "AudioAnalysisSystemData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioAnalysisSystem, "AudioAnalysisSystem");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioAnalysisComponentData, "AudioAnalysisComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioAnalysisComponentInst, "AudioAnalysisComponentInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::AudioAnalysisArchetype, "AudioAnalysisArchetype");

template  ork::ent::AudioAnalysisSystem* ork::ent::Simulation::findSystem() const;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisSystemData::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

AudioAnalysisSystemData::AudioAnalysisSystemData()
{
	printf( "AudioAnalysisSystemData::AudioAnalysisSystemData()\n" );
	AudioDeviceList* pdl = new AudioDeviceList(true);
	AudioDeviceList::DeviceList &thelist = pdl->GetList();
	int index = 0;
	for (AudioDeviceList::DeviceList::iterator i = thelist.begin(); i != thelist.end(); ++i, ++index)
	{
		AudioDeviceID id = i->mID;
		mDeviceNames[id] = i->mName;
        printf( "DEVICE<%d:%s>\n", int(id), i->mName );
	}

	mAudioDeviceList = pdl;
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::System* AudioAnalysisSystemData::createSystem(ork::ent::Simulation *pinst) const
{
	return new AudioAnalysisSystem( *this, pinst );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisSystem::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

AudioAnalysisSystem::AudioAnalysisSystem( const AudioAnalysisSystemData& data, ork::ent::Simulation *pinst )
	: ork::ent::System( &data, pinst )
	, mAAMCD(data)
{
	AudioDeviceList* alist = data.GetAudioDeviceList();
}

///////////////////////////////////////////////////////////////////////////////

ent::AudioAnalysisComponentInst* AudioAnalysisSystem::GetAudioAnalysisComponentInst( int icidx ) const
{
	ent::AudioAnalysisComponentInst* rval = 0;
	int inumsc = mAACIs.size();
	if( inumsc )
	{
		int idx = icidx%inumsc;
		rval = mAACIs[idx];
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisSystem::AddAACI( AudioAnalysisComponentInst* cci )
{
	mAACIs.push_back(cci);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentData::Describe()
{
	ork::reflect::RegisterProperty("AudioInputDeviceID", &AudioAnalysisComponentData::mAudioInputDeviceID);
	reflect::annotatePropertyForEditor< AudioAnalysisComponentData >("AudioInputDeviceID", "editor.range.min", "1" );
	reflect::annotatePropertyForEditor< AudioAnalysisComponentData >("AudioInputDeviceID", "editor.range.max", "1000" );
}

///////////////////////////////////////////////////////////////////////////////

AudioAnalysisComponentData::AudioAnalysisComponentData()
	: mAudioInputDeviceID( 0 )
{
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::ComponentInst* AudioAnalysisComponentData::createComponent(ork::ent::Entity *pent) const
{
	return new AudioAnalysisComponentInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentData::DoRegisterWithScene( ork::ent::SceneComposer& sc )
{
	sc.Register<ork::ent::AudioAnalysisSystemData>();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentInst::Describe()
{

}

///////////////////////////////////////////////////////////////////////////////

bool AudioAnalysisComponentInst::DoLink(ork::ent::Simulation *psi)
{
	auto cmi = psi->findSystem<ent::AudioAnalysisSystem>();
	OrkAssert(cmi!=0);
	cmi->AddAACI(this);

	for( int i=0; i<128; i++ )
	{
		mControlValues[i] = 1.0f;
	}

	mfTimeAccum=0.0f;
	mfLastTime = 0.0f;

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentInst::DoUnLink(Simulation *psi)
{
}

///////////////////////////////////////////////////////////////////////////////

bool AudioAnalysisComponentInst::DoStart(ork::ent::Simulation *inst, const ork::fmtx4 &world)
{
	if( mCoreAudioHost )
		mCoreAudioHost->Start();

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentInst::DoUpdate(Simulation *inst)
{
	float fDT = inst->GetDeltaTime();

	mfLastTime = mfTimeAccum;
	mfTimeAccum += fDT;
}

///////////////////////////////////////////////////////////////////////////////

AudioAnalysisComponentInst::AudioAnalysisComponentInst( const AudioAnalysisComponentData& data, ork::ent::Entity *pent )
	: ork::ent::ComponentInst( &data, pent )
	, mAnalysisData(data)
	, miNote(0)
	, mfVelocity(0.0f)
	, mInPort(0)
	, mOutPort(0)
	, mMidiInputDevice(0)
	, mMidiClient(0)
	, mCoreAudioHost(0)
	, mfTimeAccum(0.0f)
{
	StartMidi();
	Simulation* psi = pent->simulation();
	mpAAMCI = psi->findSystem<ent::AudioAnalysisSystem>();
	const AudioAnalysisSystemData& AAMCD = mpAAMCI->GetAAMCD();


	int iaudiodeviceID = data.GetAudioInputDeviceID();
	AudioDeviceList* pDeviceList = AAMCD.GetAudioDeviceList();

	AudioDeviceList::DeviceList& dlist = pDeviceList->GetList();

	bool bFOUND = false;

	for( AudioDeviceList::DeviceList::const_iterator it=dlist.begin(); it!=dlist.end(); it++ )
	{
		const AudioDeviceList::Device& device = (*it);

		if( device.mID == iaudiodeviceID )
		{
			printf( "AUDIODEVICE<%d:%s> FOUND!\n", int(device.mID), & device.mName[0] );
			bFOUND = true;
		}
	}
	if( bFOUND )
		mCoreAudioHost = new CAPlayThroughHost( iaudiodeviceID, this );
}

///////////////////////////////////////////////////////////////////////////////

AudioAnalysisComponentInst::~AudioAnalysisComponentInst()
{
	StopMidi();
	if( mCoreAudioHost )
		delete mCoreAudioHost;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisArchetype::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

AudioAnalysisArchetype::AudioAnalysisArchetype()
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<ork::ent::AudioAnalysisComponentData>();
}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentInst::SendMidiPacket(const MIDIPacketList* pktlist)
{
	// send MIDI message to all MIDI output devices connected to computer:
	ItemCount nDests = MIDIGetNumberOfDestinations();
	ItemCount iDest;
	for(iDest=0; iDest<nDests; iDest++)
	{
		MIDIEndpointRef dest;
		dest = MIDIGetDestination(iDest);
		OSStatus status = MIDISend(mOutPort, dest, pktlist);
		if (status)
		{
			printf("Problem sendint MIDI data on port %d\n", int(iDest) );
			//printf("%s\n", GetMacOSStatusErrorString(status));
			exit(status);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentInst::MidiReadProc(const MIDIPacketList *pktlist, void *refCon, void *connRefCon)
{
	AudioAnalysisComponentInst* pcci = (AudioAnalysisComponentInst*) refCon;

	MIDIPacket *packet = (MIDIPacket *)pktlist->packet;
	for( int j = 0; j < pktlist->numPackets; j++)
	{
		///////////////////////////////////////
		// route message
		///////////////////////////////////////

		Byte ControlByte = packet->data[0];

		switch( ControlByte )
		{
			case 0x90: // note on TRIGGER
			{
				Byte NOTE = packet->data[1];
				Byte velocity = packet->data[2];
				float fvel = float(velocity)/127.0f;
				if( pcci )
					pcci->NoteOn( int(NOTE), fvel );
				break;
			}
			case 0xb0: // Controller
			{
				Byte controller = packet->data[1];
				Byte value = packet->data[2];
				float fval = float(value)/127.0f;
				if( pcci )
					pcci->ControlMessage( int(controller), fval );
				break;
			}
			default:
				break;
		}

		///////////////////////////////////////
		// dump message
		///////////////////////////////////////

		printf( "packet<%p> [", packet );
		int ilen = packet->length;
		for( int i=0; i<ilen; i++ )
		{	Byte CBYTE = packet->data[i];
			printf( "%02x.", int (CBYTE) );
		}
		printf( "\n" );

		///////////////////////////////////////
		// next packet
		///////////////////////////////////////

		packet = MIDIPacketNext(packet);
	}
	///////////////////////////////////////
	// echo message
	///////////////////////////////////////
	pcci->SendMidiPacket( pktlist );
}

///////////////////////////////////////////////////////////////////////////////

void EnumerateMidiDevices()
{
	int n = MIDIGetNumberOfExternalDevices();
	MIDIEntityRef entity = NULL;

	CFStringRef pname, pmanuf, pmodel;
	char name[64], manuf[64], model[64];
	for (int i = 0; i < n; ++i)
	{
		MIDIDeviceRef dev = MIDIGetExternalDevice(i);

		MIDIObjectGetStringProperty(dev, kMIDAbstractPropertyName, &pname);
		MIDIObjectGetStringProperty(dev, kMIDAbstractPropertyManufacturer, &pmanuf);
		MIDIObjectGetStringProperty(dev, kMIDAbstractPropertyModel, &pmodel);
		CFStringGetCString(pname, name, sizeof(name), 0);
		CFStringGetCString(pmanuf, manuf, sizeof(manuf), 0);
		CFStringGetCString(pmodel, model, sizeof(model), 0);

		printf( "MidiDevice<%d> Name<%s>\n", i, name );
		CFRelease(pname);
		CFRelease(pmanuf);
		CFRelease(pmodel);
	}

}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentInst::StartMidi()
{
	printf( "STARTMIDI\n" );
	/////////////////////////////////////////
	// create client
	/////////////////////////////////////////
	mMidiClient = NULL;
	OSStatus status = MIDIClientCreate(CFSTR("MiniOrk"), NULL, NULL, &mMidiClient);
	if (status)
	{	printf("Error trying to create MIDI Client structure: %d\n", int(status));
		exit(status);
	}
	/////////////////////////////////////////
	// create input
	/////////////////////////////////////////
	status = MIDIInputPortCreate(mMidiClient, CFSTR("input"), MidiReadProc, (void*) this, &mInPort);
	if (status)
	{	printf("Error trying to create MIDI Client structure: %d\n", int(status));
		exit(status);
	}
	mInSource = MIDIGetSource(mMidiInputDevice);
	MIDIPortConnectSource(mInPort, mInSource, NULL);
	/////////////////////////////////////////
	// create output
	/////////////////////////////////////////
	status = MIDIOutputPortCreate(mMidiClient, CFSTR("output"), &mOutPort);
	if(status)
	{	printf("Error trying to create MIDI output port: %d\n", int(status));
		exit(status);
	}
}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentInst::StopMidi()
{
	printf( "STOPMIDI\n" );
	MIDIPortDisconnectSource(mInPort, mInSource);
}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentInst::ControlMessage( int icontroller, float fV )
{
	mControlValues[icontroller] = fV;
}

///////////////////////////////////////////////////////////////////////////////

float AudioAnalysisComponentInst::GetController( int icontroller ) const
{
	orkmap<int,float>::const_iterator it=mControlValues.find(icontroller);
	if( it!=mControlValues.end() )
		return it->second;

	return 1.0f;
}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentInst::NoteOn( int iNOTE, float fV )
{
	miNote = iNOTE;
	mfVelocity = fV;
}

///////////////////////////////////////////////////////////////////////////////

void AudioAnalysisComponentInst::GetNote( int& outnote, float& outvel ) const
{
	outnote = miNote;
	outvel = mfVelocity;
}

AudioDeviceList::AudioDeviceList(bool inputs) :
	mInputs(inputs)
{
	BuildList();
}

AudioDeviceList::~AudioDeviceList()
{
}

///////////////////////////////////////////////////////////////////////////////

void AudioDeviceList::BuildList()
{
	mDevices.clear();

	UInt32 propsize;

	verify_noerr(AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &propsize, NULL));
	int nDevices = propsize / sizeof(AudioDeviceID);
	AudioDeviceID *devids = new AudioDeviceID[nDevices];
	verify_noerr(AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &propsize, devids));

	for (int i = 0; i < nDevices; ++i)
	{
		AudioDevice dev(devids[i], mInputs);
		if (dev.CountChannels() > 0)
		{
			Device d;
			d.mID = devids[i];
			dev.GetName(d.mName, sizeof(d.mName));
			mDevices.push_back(d);
		}
	}
	delete[] devids;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AudioDevice::Init(AudioDeviceID devid, bool isInput)
{
	mID = devid;
	mIsInput = isInput;
	if (mID == kAudioDeviceUnknown)
		return;
	UInt32 propsize;
	propsize = sizeof(UInt32);
	verify_noerr(AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertySafetyOffset, &propsize, &mSafetyOffset));
	propsize = sizeof(UInt32);
	verify_noerr(AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertyBufferFrameSize, &propsize, &mBufferSizeFrames));
	propsize = sizeof(AudioStreamBasicDescription);
	verify_noerr(AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertyStreamFormat, &propsize, &mFormat));

}

///////////////////////////////////////////////////////////////////////////////

void AudioDevice::SetBufferSize(UInt32 size)
{
	UInt32 propsize = sizeof(UInt32);
	verify_noerr(AudioDeviceSetProperty(mID, NULL, 0, mIsInput, kAudioDevicePropertyBufferFrameSize, propsize, &size));
	propsize = sizeof(UInt32);
	verify_noerr(AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertyBufferFrameSize, &propsize, &mBufferSizeFrames));
}

///////////////////////////////////////////////////////////////////////////////

int AudioDevice::CountChannels()
{
	OSStatus err;
	UInt32 propSize;
	int result = 0;

	err = AudioDeviceGetPropertyInfo(mID, 0, mIsInput, kAudioDevicePropertyStreamConfiguration, &propSize, NULL);
	if (err) return 0;

	AudioBufferList *buflist = (AudioBufferList *)malloc(propSize);
	err = AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertyStreamConfiguration, &propSize, buflist);
	if (!err)
	{
		for (UInt32 i = 0; i < buflist->mNumberBuffers; ++i)
		{
			result += buflist->mBuffers[i].mNumberChannels;
		}
	}
	free(buflist);
	return result;
}

char* AudioDevice::GetName(char *buf, UInt32 maxlen)
{
	verify_noerr(AudioDeviceGetProperty(mID, 0, mIsInput, kAudioDevicePropertyDeviceName, &maxlen, buf));
	return buf;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}}
//template const ork::ent::AudioAnalysiComponentData* ork::ent::EntData::GetTypedComponent<ork::ent::AudioAnalysiComponentData>() const;
//template ork::ent::AudioAnalysiComponentInst* ork::ent::Entity::GetTypedComponent<ork::ent::AudioAnalysiComponentInst>(bool);
#endif
