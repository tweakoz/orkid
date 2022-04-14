////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
#include "CARingBuffer.h"
#include "CAStreamBasicDescription.h"
#include <libkern/OSAtomic.h>

inline void checkErr( OSStatus err )
{
	if(err)
	{
		OSStatus error = static_cast<OSStatus>(err);
		fprintf(stdout, "CAPlayThrough Error: %d ->  %s:  %d\n"
				, int(error)
				, __FILE__
				, __LINE__
		);
		fflush(stdout);
		OrkAssert(false);
	}
}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {

class CAPlayThrough
{
public:
	CAPlayThrough(AudioDeviceID input,AudioAnalysisComponentInst*pAACI);//, AudioDeviceID output);
	~CAPlayThrough();

	OSStatus	Init(AudioDeviceID input);
	void		Cleanup();
	OSStatus	Start();
	OSStatus	Stop();
	Boolean		IsRunning();
	OSStatus	SetInputDeviceAsCurrent(AudioDeviceID in);

	AudioDeviceID getInputDeviceID()	{ return mInputDevice.mID;	}

private:
	OSStatus SetupGraph();
	OSStatus MakeGraph();

	OSStatus SetupAUHAL(AudioDeviceID in);
	OSStatus EnableIO();
	OSStatus CallbackSetup();
	OSStatus SetupBuffers();

	void ComputeThruOffset();

	static OSStatus InputProc(void *inRefCon,
							  AudioUnitRenderActionFlags *ioActionFlags,
							  const AudioTimeStamp *inTimeStamp,
							  UInt32				inBusNumber,
							  UInt32				inNumberFrames,
							  AudioBufferList *		ioData);

	static OSStatus OutputProc(void *inRefCon,
							   AudioUnitRenderActionFlags *ioActionFlags,
							   const AudioTimeStamp *inTimeStamp,
							   UInt32				inBusNumber,
							   UInt32				inNumberFrames,
							   AudioBufferList *	ioData);

	AudioUnit mInputUnit;
	AudioBufferList *mInputBuffer;
	AudioDevice mInputDevice;

	//AudioUnits and Graph
	AUGraph mGraph;

	//Buffer sample info
	Float64 mFirstInputTime;
	Float64 mFirstOutputTime;

	AudioAnalysisComponentInst* mpAACI;
};

///////////////////////////////////////////////////////////////////////////////

CAPlayThrough::CAPlayThrough(AudioDeviceID input,AudioAnalysisComponentInst*pAACI)
	: mFirstInputTime(-1)
	, mInputBuffer(0)
	, mpAACI(pAACI)
{
	OSStatus err = noErr;
	err =Init(input);
    if(err) {
		fprintf(stderr,"CAPlayThrough ERROR: Cannot Init CAPlayThrough");
		exit(1);
	}
}

///////////////////////////////////////////////////////////////////////////////

CAPlayThrough::~CAPlayThrough()
{
	Cleanup();
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThrough::Init(AudioDeviceID input)
{
    OSStatus err = noErr;
	//Setup AUHAL for an input device
	err = SetupAUHAL(input);
	checkErr(err);

	//Setup Graph containing Varispeed Unit & Default Output Unit
	err = SetupGraph();
	checkErr(err);

	err = SetupBuffers();
	checkErr(err);

	// the varispeed unit should only be conected after the input and output formats have been set
	//err = AUGraphConnectNodeInput(mGraph, mVarispeedNode, 0, mOutputNode, 0);
	//checkErr(err);

	err = AUGraphInitialize(mGraph);
	checkErr(err);

	//Add latency between the two devices
	ComputeThruOffset();

	return err;
}

///////////////////////////////////////////////////////////////////////////////

void CAPlayThrough::Cleanup()
{
	Stop();

	if(mInputBuffer)
	{
		for(UInt32 i = 0; i<mInputBuffer->mNumberBuffers; i++)
			free(mInputBuffer->mBuffers[i].mData);
		free(mInputBuffer);
		mInputBuffer = 0;
	}

	AudioUnitUninitialize(mInputUnit);
	AUGraphClose(mGraph);
	DisposeAUGraph(mGraph);
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThrough::Start()
{
	OSStatus err = noErr;
	if(!IsRunning()){
		//Start pulling for audio data
		err = AudioOutputUnitStart(mInputUnit);
		checkErr(err);

		err = AUGraphStart(mGraph);
		checkErr(err);

		//reset sample times
		mFirstInputTime = -1;
		mFirstOutputTime = -1;
	}
	printf( "CAPlayThrough::Start() ret<%d>\n", int(err) );
	return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThrough::Stop()
{
	OSStatus err = noErr;
	if(IsRunning()){
		//Stop the AUHAL
		err = AudioOutputUnitStop(mInputUnit);
		err = AUGraphStop(mGraph);
		mFirstInputTime = -1;
		mFirstOutputTime = -1;
	}
	printf( "CAPlayThrough::Stop() ret<%d>\n", int(err) );
	return err;
}

///////////////////////////////////////////////////////////////////////////////

Boolean CAPlayThrough::IsRunning()
{
	OSStatus err = noErr;
	UInt32 auhalRunning = 0, size = 0;
	Boolean graphRunning;
	size = sizeof(auhalRunning);
	if(mInputUnit)
	{
		err = AudioUnitGetProperty(mInputUnit,
								kAudioOutputUnitProperty_IsRunning,
								kAudioUnitScope_Global,
								0, // input element
								&auhalRunning,
								&size);
	}

	if(mGraph)
		err = AUGraphIsRunning(mGraph,&graphRunning);

	return (auhalRunning || graphRunning);
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThrough::SetInputDeviceAsCurrent(AudioDeviceID in)
{
    UInt32 size = sizeof(AudioDeviceID);
    OSStatus err = noErr;

	if(in == kAudioDeviceUnknown) //get the default input device if device is unknown
	{
		err = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultInputDevice,
									   &size,
									   &in);
		checkErr(err);
	}

	mInputDevice.Init(in, true);

	//Set the Current Device to the AUHAL.
	//this should be done only after IO has been enabled on the AUHAL.
    err = AudioUnitSetProperty(mInputUnit,
							  kAudioOutputUnitProperty_CurrentDevice,
							  kAudioUnitScope_Global,
							  0,
							  &mInputDevice.mID,
							  sizeof(mInputDevice.mID));
	checkErr(err);
	return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThrough::SetupGraph()//AudioDeviceID out)
{
	OSStatus err = noErr;
	AURenderCallbackStruct output;

	//Make a New Graph
    err = NewAUGraph(&mGraph);
	checkErr(err);

	//Open the Graph, AudioUnits are opened but not initialized
    err = AUGraphOpen(mGraph);
	checkErr(err);

	err = MakeGraph();
	checkErr(err);

	return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThrough::MakeGraph()
{
	OSStatus err = noErr;
	AudioComponentDescription varispeedDesc,outDesc;
	return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThrough::SetupAUHAL(AudioDeviceID in)
{
	OSStatus err = noErr;

	Component comp;
	ComponentDescription desc;

	//There are several different types of Audio Units.
	//Some audio units serve as Outputs, Mixers, or DSP
	//units. See AUComponent.h for listing
	desc.componentType = kAudioUnitType_Output;

	//Every Component has a subType, which will give a clearer picture
	//of what this components function will be.
	desc.componentSubType = kAudioUnitSubType_HALOutput;

	//all Audio Units in AUComponent.h must use
	//"kAudioUnitManufacturer_Apple" as the Manufacturer
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	//Finds a component that meets the desc spec's
	comp = FindNextComponent(NULL, &desc);
	if (comp == NULL) exit (-1);

	//gains access to the services provided by the component
	OpenAComponent(comp, &mInputUnit);

	//AUHAL needs to be initialized before anything is done to it
	err = AudioUnitInitialize(mInputUnit);
	checkErr(err);

	err = EnableIO();
	checkErr(err);

	err= SetInputDeviceAsCurrent(in);
	checkErr(err);

	err = CallbackSetup();
	checkErr(err);

	//Don't setup buffers until you know what the
	//input and output device audio streams look like.

	err = AudioUnitInitialize(mInputUnit);

	return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThrough::EnableIO()
{
	OSStatus err = noErr;
	UInt32 enableIO;

	//Enable input
	enableIO = 1;
	err =  AudioUnitSetProperty(mInputUnit,
								kAudioOutputUnitProperty_EnableIO,
								kAudioUnitScope_Input,
								1, // input element
								&enableIO,
								sizeof(enableIO));
	checkErr(err);
	printf( "CAPlayThrough::EnableIO() inputerr<%d>\n", int(err) );

	//disable Output on the AUHAL
	enableIO = 0;
	err = AudioUnitSetProperty(mInputUnit,
							  kAudioOutputUnitProperty_EnableIO,
							  kAudioUnitScope_Output,
							  0,   //output element
							  &enableIO,
							  sizeof(enableIO));
	printf( "CAPlayThrough::EnableIO() outputerr<%d>\n", int(err) );
	return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThrough::CallbackSetup()
{
	OSStatus err = noErr;
    AURenderCallbackStruct input;

    input.inputProc = InputProc;
    input.inputProcRefCon = this;

	//Setup the input callback.
	err = AudioUnitSetProperty(mInputUnit,
							  kAudioOutputUnitProperty_SetInputCallback,
							  kAudioUnitScope_Global,
							  0,
							  &input,
							  sizeof(input));
	checkErr(err);

	printf( "CAPlayThrough::CallbackSetup() ret<%d>\n", int(err) );
	return err;
}

///////////////////////////////////////////////////////////////////////////////

//Allocate Audio Buffer List(s) to hold the data from input.
OSStatus CAPlayThrough::SetupBuffers()
{
	OSStatus err = noErr;
	UInt32 bufferSizeFrames,bufferSizeBytes,propsize;

	CAStreamBasicDescription asbd,asbd_dev1_in,asbd_dev2_out;
	Float64 rate=0;

	//Get the size of the IO buffer(s)
	UInt32 propertySize = sizeof(bufferSizeFrames);
	err = AudioUnitGetProperty(mInputUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &bufferSizeFrames, &propertySize);
	bufferSizeBytes = bufferSizeFrames * sizeof(Float32);

	//Get the Stream Format (Output client side)
	propertySize = sizeof(asbd_dev1_in);
	err = AudioUnitGetProperty(mInputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 1, &asbd_dev1_in, &propertySize);
	printf("=====Input DEVICE stream format\n" );
	asbd_dev1_in.Print();

	//Get the Stream Format (client side)
	propertySize = sizeof(asbd);
	err = AudioUnitGetProperty(mInputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &asbd, &propertySize);
	printf("=====current Input (Client) stream format\n");
	asbd.Print();

	//////////////////////////////////////
	//Set the format of all the AUs to the input/output devices channel count
	//For a simple case, you want to set this to the lower of count of the channels
	//in the input device vs output device
	//////////////////////////////////////
	asbd.mChannelsPerFrame =asbd_dev1_in.mChannelsPerFrame; // < asbd_dev2_out.mChannelsPerFrame) ?asbd_dev1_in.mChannelsPerFrame :asbd_dev2_out.mChannelsPerFrame) ;
	printf("Info: Input Device channel count=%d\n", int(asbd_dev1_in.mChannelsPerFrame) );
	printf("Info: CAPlayThrough will use %d channels\n", int(asbd.mChannelsPerFrame) );

	// We must get the sample rate of the input device and set it to the stream format of AUHAL
	propertySize = sizeof(Float64);
	AudioDeviceGetProperty(mInputDevice.mID, 0, 1, kAudioDevicePropertyNominalSampleRate, &propertySize, &rate);
	asbd.mSampleRate =rate;
	propertySize = sizeof(asbd);

	//Set the new formats to the AUs...
	err = AudioUnitSetProperty(mInputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &asbd, propertySize);
	checkErr(err);
	//err = AudioUnitSetProperty(mVarispeedUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &asbd, propertySize);
	//checkErr(err);

	//calculate number of buffers from channels
	propsize = offsetof(AudioBufferList, mBuffers[0]) + (sizeof(AudioBuffer) *asbd.mChannelsPerFrame);

	//malloc buffer lists
	mInputBuffer = (AudioBufferList *)malloc(propsize);
	mInputBuffer->mNumberBuffers = asbd.mChannelsPerFrame;

	//pre-malloc buffers for AudioBufferLists
	for(UInt32 i =0; i< mInputBuffer->mNumberBuffers ; i++) {
		mInputBuffer->mBuffers[i].mNumberChannels = 1;
		mInputBuffer->mBuffers[i].mDataByteSize = bufferSizeBytes;
		mInputBuffer->mBuffers[i].mData = malloc(bufferSizeBytes);
	}

    return err;
}

///////////////////////////////////////////////////////////////////////////////

void	CAPlayThrough::ComputeThruOffset()
{
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThrough::InputProc(void *inRefCon,
									AudioUnitRenderActionFlags *ioActionFlags,
									const AudioTimeStamp *inTimeStamp,
									UInt32 inBusNumber,
									UInt32 inNumberFrames,
									AudioBufferList * ioData)
{
    OSStatus err = noErr;

	CAPlayThrough *This = (CAPlayThrough *)inRefCon;

	AudMultiBuffer& MultiBuffer = This->mpAACI->GetMultiBuffer();
	AudBufSet& audBufSet = MultiBuffer.GetWriteBuffer();

	if (This->mFirstInputTime < 0.)
		This->mFirstInputTime = inTimeStamp->mSampleTime;

	AudioBufferList* buffers = This->mInputBuffer;
	//Get the new audio data
	err = AudioUnitRender(This->mInputUnit,
						 ioActionFlags,
						 inTimeStamp,
						 inBusNumber,
						 inNumberFrames,
						 buffers );
	checkErr(err);

	int inumbuf = (buffers!=0) ? int(buffers->mNumberBuffers) : 0;
	int ichansperframe = inumbuf;

	audBufSet.miNumItems = buffers->mNumberBuffers;
    for(int i =0; i< buffers->mNumberBuffers ; i++)
	{
		AudBufItem& bufItem = audBufSet.mItems[i];

		bufItem.miNumFrames = inNumberFrames;

		int inumchan = buffers->mBuffers[i].mNumberChannels;
		int idatasizebytes = buffers->mBuffers[i].mDataByteSize;
		void* pdata = buffers->mBuffers[i].mData;
		float* pdataf32 = (float*) pdata;

		float fmin = 10000000.0f;
		float fmax = -10000000.0f;

		for( int ifr=0; ifr<inNumberFrames; ifr++ )
		{
			float fv = pdataf32[ifr];

			bufItem.mData[ifr] = fv;

			if( fv > fmax ) fmax = fv;
			if( fv < fmin ) fmin = fv;
		}
		float fabsmin = fabs(fmin);
		float fabsmax = fabs(fmax);
		float fPWR = (fabsmax>fabsmin) ? fabsmax : fabsmin;

		printf( "buf<%d> inumchan<%d> idbsize<%d> pdata<%p> fPWR<%f>\n", i, inumchan, idatasizebytes, pdata, fPWR );
	}

    printf( "RECEIVED AUDIODATA ts<%d> inumbuf<%d> inumframes<%d>\n", int(inTimeStamp->mSampleTime), inumbuf, int(inNumberFrames) );

	return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThrough::OutputProc(void *inRefCon,
									 AudioUnitRenderActionFlags *ioActionFlags,
									 const AudioTimeStamp *TimeStamp,
									 UInt32 inBusNumber,
									 UInt32 inNumberFrames,
									 AudioBufferList * ioData)
{
    OSStatus err = noErr;
	return noErr;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus CAPlayThroughHost::StreamListener( AudioStreamID         inStream,
											UInt32                  inChannel,
											AudioDevicePropertyID   inPropertyID,
											void*                   inClientData)
{
	CAPlayThroughHost *This = (CAPlayThroughHost *)inClientData;
	This->ResetPlayThrough();
	return noErr;
}

///////////////////////////////////////////////////////////////////////////////

CAPlayThroughHost::CAPlayThroughHost(AudioDeviceID input,AudioAnalysisComponentInst*pAACI)
	: mPlayThrough(NULL)
	, mpAACI(pAACI)
{
	CreatePlayThrough(input);
}

///////////////////////////////////////////////////////////////////////////////

CAPlayThroughHost::~CAPlayThroughHost()
{
	DeletePlayThrough();
}

///////////////////////////////////////////////////////////////////////////////

void CAPlayThroughHost::CreatePlayThrough(AudioDeviceID input)
{
	mPlayThrough = new CAPlayThrough(input,mpAACI);
	addDeviceListeners(input);
}

///////////////////////////////////////////////////////////////////////////////

void CAPlayThroughHost::DeletePlayThrough()
{
	if(mPlayThrough)
	{
		mPlayThrough->Stop();
		RemoveDeviceListeners(mPlayThrough->getInputDeviceID());
		delete mPlayThrough;
		mPlayThrough = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

void CAPlayThroughHost::ResetPlayThrough ()
{

	AudioDeviceID input = mPlayThrough->getInputDeviceID();
	DeletePlayThrough();
	CreatePlayThrough(input); //, output);
	mPlayThrough->Start();
}

///////////////////////////////////////////////////////////////////////////////

bool CAPlayThroughHost::PlayThroughExists()
{
	return (mPlayThrough != NULL) ? true : false;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus	CAPlayThroughHost::Start()
{
	printf( "CAPlayThroughHost::Start()\n" );
	if (mPlayThrough) return mPlayThrough->Start();
	return noErr;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus	CAPlayThroughHost::Stop()
{
	if (mPlayThrough) return mPlayThrough->Stop();
	return noErr;
}

///////////////////////////////////////////////////////////////////////////////

Boolean		CAPlayThroughHost::IsRunning()
{
	if (mPlayThrough) return mPlayThrough->IsRunning();
	return noErr;
}

///////////////////////////////////////////////////////////////////////////////

void CAPlayThroughHost::addDeviceListeners(AudioDeviceID input)
{
	// StreamListener is called whenever the sample rate changes (as well as other format characteristics of the device)
	UInt32 propSize;
	OSStatus err = AudioDeviceGetPropertyInfo(input, 0, true, kAudioDevicePropertyStreams, &propSize, NULL);
	if(!err)
	{
		AudioStreamID *streams = (AudioStreamID*)malloc(propSize);
		err = AudioDeviceGetProperty(input, 0, true, kAudioDevicePropertyStreams, &propSize, streams);

		if(!err)
		{
			UInt32 numStreams = propSize / sizeof(AudioStreamID);
			for(UInt32 i=0; i < numStreams; i++)
			{
				UInt32 isInput;
				propSize = sizeof(UInt32);
				err = AudioStreamGetProperty(streams[i], 0, kAudioStreamPropertyDirection, &propSize, &isInput);
				if(!err && isInput)
					err = AudioStreamaddPropertyListener(streams[i], 0, kAudioStreamPropertyPhysicalFormat, StreamListener, this);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void CAPlayThroughHost::RemoveDeviceListeners(AudioDeviceID input)
{
	UInt32 propSize;
	OSStatus err = AudioDeviceGetPropertyInfo(input, 0, true, kAudioDevicePropertyStreams, &propSize, NULL);
	if(!err)
	{
		AudioStreamID *streams = (AudioStreamID*)malloc(propSize);
		err = AudioDeviceGetProperty(input, 0, true, kAudioDevicePropertyStreams, &propSize, streams);
		if(!err)
		{
			UInt32 numStreams = propSize / sizeof(AudioStreamID);
			for(UInt32 i=0; i < numStreams; i++)
			{
				UInt32 isInput;
				propSize = sizeof(UInt32);
				err = AudioStreamGetProperty(streams[i], 0, kAudioStreamPropertyDirection, &propSize, &isInput);
				if(!err && isInput)
					err = AudioStreamRemovePropertyListener(streams[i], 0, kAudioStreamPropertyPhysicalFormat, StreamListener);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AudBufSet& AudMultiBuffer::GetWriteBuffer()
{
	int32_t widx = OSAtomicIncrement32( & miWriteIndex ) % kmaxmbuf;
	miReadIndex = (widx-1);
	if( miReadIndex<0 ) miReadIndex += kmaxmbuf;

	printf( "AudMultiBuffer::GetWriteBuffer() widx<%d>\n", widx );

	return mMultiBuf[widx];
}
void AudMultiBuffer::GetReadBuffer( AudBufSet& output )
{
	const AudBufSet& rbuf = mMultiBuf[miReadIndex];
	output = rbuf;
}

}}
#endif
