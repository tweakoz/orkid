#include <ork/lev2/config.h>
#if defined(ENABLE_CORE_AUDIO)

#include "au.h"
#include "ca_helpers/CAHostTimeBase.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::ca {
static logchannel_ptr_t logchan_auio = logger()->configureChannel("AuIo", fvec3(1, 0.4, .6), true);

int gframesize                     = desired_framesize;

void setframesize(int fr) {
  gframesize = fr;
}
const int getframesize() {
  return gframesize;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::setupInputCallback() {
  //logchan_auio->log("setupInputCallback");
  OSStatus err = noErr;
  AURenderCallbackStruct input, output;

  input.inputProc       = _inputProc;
  input.inputProcRefCon = this;

  // Setup the input callback.
  err = AudioUnitSetProperty(
      _inputUnit,                                //
      kAudioOutputUnitProperty_SetInputCallback, //
      kAudioUnitScope_Global,                    //
      0,                                         //
      &input,
      sizeof(input));
  AuCheckErr(err);

  return err;
}

///////////////////////////////////////////////////////////////////////////////
// Allocate Audio Buffer List(s) to hold the data from input.
///////////////////////////////////////////////////////////////////////////////

void DumpStreamDesc(const char* name, const CAStreamBasicDescription& strd) {
  logchan_auio->log("StreamDesc<%s>", name);
  strd.Print();
  bool isinterl = strd.mFormatFlags & kAudioFormatFlagIsNonInterleaved;
  logchan_auio->log("StreamDesc<%s> mSampleRate:%d", name, (int)strd.mSampleRate);
  logchan_auio->log("StreamDesc<%s> interleaved<%d>", name, (int)isinterl);
  logchan_auio->log("StreamDesc<%s> mChannelsPerFrame:%d", name, (int)strd.mChannelsPerFrame);
  logchan_auio->log("StreamDesc<%s> mBytesPerFrame:%d", name, (int)strd.mBytesPerFrame);
  logchan_auio->log("StreamDesc<%s> mFramesPerPacket:%d", name, (int)strd.mFramesPerPacket);
}

///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::setupOutputBuffers() {
  //logchan_auio->log("SetupOutputBuffers");

  if (!_outputDev) {
    OrkAssert(false);
    return kAudioUnitErr_InvalidParameter; // no input device set
  }

  OSStatus err = noErr;
  CAStreamBasicDescription streamdesc_output;

  //////////////////////////////

  UInt32 bufferFrameSize = desired_framesize;
  UInt32 propertySize    = sizeof(streamdesc_output);

  err = AudioUnitSetProperty(
      _outputUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, &bufferFrameSize, propertySize);
  AuCheckErr(err);

  err = AudioUnitGetProperty(
      _outputUnit, kAudioDevicePropertyBufferFrameSize, kAudioUnitScope_Global, 0, (void*)&bufferFrameSize, &propertySize);
  AuCheckErr(err);

  _outputFrameSize = bufferFrameSize;

  logchan_auio->log("OUTBUFFERFRAMESIZE<%d>", int(_outputFrameSize));

  //////////////////////////////
  // Get the Stream Format (Output client side)
  //////////////////////////////

  propertySize = sizeof(streamdesc_output);
  err          = AudioUnitGetProperty(
      _outputUnit,                     // unitID
      kAudioUnitProperty_StreamFormat, // propID
      kAudioUnitScope_Output,          // scope
      0,                               // elementIDX
      &streamdesc_output,              // dest
      &propertySize);
  AuCheckErr(err);

  //DumpStreamDesc("OutputDevice", streamdesc_output);

  //////////////////////////////////////
  // Set the correct sample rate for the output device, but keep the channel count the same
  //////////////////////////////////////

  double rate = 0.0;

  AudioObjectPropertyAddress theAddress = {
      kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

  propertySize = sizeof(Float64);

  err = AudioObjectGetPropertyData(_outputDev->_info->_ID, &theAddress, 0, NULL, &propertySize, &rate);
  AuCheckErr(err);

  //////////////////////////////////////
  // Set the new audio stream formats for the rest of the AUs...
  //////////////////////////////////////

  streamdesc_output.mSampleRate = rate;

  err = AudioUnitSetProperty(
      _outputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &streamdesc_output, sizeof(streamdesc_output));
  AuCheckErr(err);

  return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::setupInputBuffers() {
  logchan_auio->log("SetupInputBuffers");

  if (!_inputDev) {
    OrkAssert(false);
    return kAudioUnitErr_InvalidParameter; // no input device set
  }

  OSStatus err = noErr;

  CAStreamBasicDescription streamdesc_appinp;
  CAStreamBasicDescription streamdesc_input;

  //////////////////////////////
  // Get the size of the IO buffer(s)
  //////////////////////////////

  UInt32 bufferFrameSize = desired_framesize;
  UInt32 propertySize    = sizeof(bufferFrameSize);

 // Note: Buffer frame size should be set AFTER the device is configured
 // We'll get the actual frame size from the device later

  logchan_auio->log("INPBUFFERFRAMESIZE<%d>", int(bufferFrameSize));

  _inputFrameSize = bufferFrameSize;
  setframesize(_inputFrameSize);

  //////////////////////////////
 // Get the Stream Format from the device
 // For HAL input units, we need to get the format from the device side (scope Input, element 1)
 // and apply it to the output side (scope Output, element 1)
  //////////////////////////////

  propertySize = sizeof(streamdesc_input);
  err          = AudioUnitGetProperty(
      _inputUnit,
      kAudioUnitProperty_StreamFormat,  // Property ID
      kAudioUnitScope_Input,            // Device side (scope)
      1,                                // Input element
      &streamdesc_input,
      &propertySize);

 if (err != noErr) {
   logchan_auio->log("Failed to get stream format from input unit (err=%d), trying device property", (int)err);
   // If we can't get the format from the input scope, try getting it from the device
   propertySize = sizeof(AudioStreamBasicDescription);
   err = AudioDeviceGetProperty(_inputDev->_info->_ID, 0, true,
                                kAudioDevicePropertyStreamFormat,
                                &propertySize, &streamdesc_input);
   AuCheckErr(err);
 }
  //////////////////////////////
  // Now set up the application side format
  //////////////////////////////

  // Start with the device format
  streamdesc_appinp = streamdesc_input;
  
  // Get the actual device info
  if (_inputDev && _inputDev->_info) {
    streamdesc_appinp = _inputDev->_info->_format;
  }
  //////////////////////////////////////
  // Set the format of all the AUs to the input/output devices channel count
  // For a simple case, you want to set this to the lower of count of the channels
  // in the input device vs output device
  //////////////////////////////////////

  DumpStreamDesc("InputDevice", streamdesc_input);
  DumpStreamDesc("ApplicationInput", streamdesc_appinp);

  // We must get the sample rate of the input device and set it to the stream format of AUHAL
  Float64 rate = 0;

  propertySize                          = sizeof(Float64);
  AudioObjectPropertyAddress theAddress = {
      kAudioDevicePropertyNominalSampleRate, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMaster};

  err = AudioObjectGetPropertyData(_inputDev->_info->_ID, &theAddress, 0, NULL, &propertySize, &rate);
  AuCheckErr(err);

  //////////////////////////////////////
  // Set the new formats to the AUs...
  //////////////////////////////////////

  streamdesc_appinp.mSampleRate = rate;

  // Get total channel count from device stream configuration
  _numInputChannels = _inputDev->_info->countChannels();

  // Set up format for INTERLEAVED float audio (more compatible with virtual devices)
  streamdesc_appinp.mFormatID = kAudioFormatLinearPCM;
  streamdesc_appinp.mChannelsPerFrame = _numInputChannels;
  streamdesc_appinp.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;  // interleaved (no NonInterleaved flag)
  streamdesc_appinp.mBytesPerFrame = sizeof(Float32) * _numInputChannels;  // all channels in one frame
  streamdesc_appinp.mBytesPerPacket = streamdesc_appinp.mBytesPerFrame;
  streamdesc_appinp.mBitsPerChannel = 32;
  streamdesc_appinp.mFramesPerPacket = 1;

  err = AudioUnitSetProperty(
      _inputUnit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Output, 1, &streamdesc_appinp, sizeof(streamdesc_appinp));
  AuCheckErr(err);

  //////////////////////////////////////
  // Allocate interleaved buffer (1 buffer with all channels)
  //////////////////////////////////////

  auto memsize = offsetof(AudioBufferList, mBuffers[0]) + sizeof(AudioBuffer);

  _inputBuffer                 = (AudioBufferList*)malloc(memsize);
  _inputBuffer->mNumberBuffers = 1;  // interleaved: single buffer

  UInt32 bufferSizeBytes = bufferFrameSize * sizeof(Float32) * _numInputChannels;

  auto& buffer           = _inputBuffer->mBuffers[0];
  buffer.mNumberChannels = _numInputChannels;  // interleaved: all channels in one buffer
  buffer.mDataByteSize   = bufferSizeBytes;
  buffer.mData           = malloc(bufferSizeBytes);
  return err;
}
///////////////////////////////////////////////////////////////////////////////
void AuContext::computeThruOffset() {
  // Handle cases where we might not have both devices
  if (!_inputDev || !_outputDev || !_inputDev->_info || !_outputDev->_info) {
    // If we only have output, use its latency
    if (_outputDev && _outputDev->_info) {
      _inToOutSampleOffset = SInt32(_outputDev->_info->_safetyOffset + _outputDev->_info->_bufferSizeFrames);
    }
    // If we only have input, use its latency
    else if (_inputDev && _inputDev->_info) {
      _inToOutSampleOffset = SInt32(_inputDev->_info->_safetyOffset + _inputDev->_info->_bufferSizeFrames);
    }
    // No devices available - use zero offset
    else {
      _inToOutSampleOffset = 0;
    }
  } else {
    // Both devices present - calculate full offset
    _inToOutSampleOffset = SInt32(
        _inputDev->_info->_safetyOffset + _inputDev->_info->_bufferSizeFrames + _outputDev->_info->_safetyOffset +
        _outputDev->_info->_bufferSizeFrames);
  }
}
///////////////////////////////////////////////////////////////////////////////
OSStatus AuContext::enableInputs() {

  logchan_auio->log("EnableInputs");

  if (!_inputDev) {
    OrkAssert(false);
    return kAudioUnitErr_InvalidParameter; // no input device set
  }

  OSStatus err = noErr;
  UInt32 enableIO;

  ///////////////
  // ENABLE IO (INPUT)
  // You must enable the Audio Unit (AUHAL) for input and disable output
  // BEFORE setting the AUHAL's current device.

  // Enable input on the AUHAL
  enableIO = 1;
  err      = AudioUnitSetProperty(
      _inputUnit,
      kAudioOutputUnitProperty_EnableIO,
      kAudioUnitScope_Input,
      1, // input element
      &enableIO,
      sizeof(enableIO));
  AuCheckErr(err);

  // disable Output on the AUHAL
  enableIO = 0;
  err      = AudioUnitSetProperty(
      _inputUnit,
      kAudioOutputUnitProperty_EnableIO,
      kAudioUnitScope_Output,
      0, // output element
      &enableIO,
      sizeof(enableIO));
  return err;
}
///////////////////////////////////////////////////////////////////////////////
OSStatus AuContext::enableOutputs() {
  logchan_auio->log("EnableOutputs");

  if (!_outputDev) {
    OrkAssert(false);
    return kAudioUnitErr_InvalidParameter; // no input device set
  }

  OSStatus err = noErr;
  return err; //
  UInt32 enableIO;

  ///////////////
  // ENABLE IO (INPUT)
  // You must enable the Audio Unit (AUHAL) for input and disable output
  // BEFORE setting the AUHAL's current device.

  // Enable input on the AUHAL
  enableIO = 0;
  err      = AudioUnitSetProperty(
      _outputUnit,
      kAudioOutputUnitProperty_EnableIO,
      kAudioUnitScope_Input,
      1, // input element
      &enableIO,
      sizeof(enableIO));
  AuCheckErr(err);

  // disable Output on the AUHAL
  enableIO = 0;
  err      = AudioUnitSetProperty(
      _outputUnit,
      kAudioOutputUnitProperty_EnableIO,
      kAudioUnitScope_Output,
      0, // output element
      &enableIO,
      sizeof(enableIO));
  return err;
}
///////////////////////////////////////////////////////////////////////////////
OSStatus AuContext::_inputProc(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* inTimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData) {


  OSStatus err = noErr;

  auto _this = (AuContext*)inRefCon;
  if (_this->_firstInputTime < 0.)
    _this->_firstInputTime = inTimeStamp->mSampleTime;

  assert(ioData == nullptr); // huh ..

  AudioBufferList* source_buffers = _this->_inputBuffer;

  /////////////////////////////
  // pull data from input
  /////////////////////////////

  err = AudioUnitRender(
      _this->_inputUnit,
      ioActionFlags,
      inTimeStamp,
      inBusNumber,
      inNumberFrames,  // # of frames requested
      source_buffers); // Audio Buffer List to hold data
  AuCheckErr(err);

  /////////////////////////////

  int inumbuf   = (source_buffers != 0) ? int(source_buffers->mNumberBuffers) : 0;
  int inumchans = (inumbuf > 0) ? int(source_buffers->mBuffers[0].mNumberChannels) : 0;
  // logchan_auio->log("inp numfr<%d> inumbuf<%d> inumchans<%d>", inNumberFrames, inumbuf, inumchans);

  // For interleaved format: inumbuf=1, inumchans=actual channel count
  auto inpbufgroup = _this->AllocLayerFragment(inumchans, inNumberFrames);

  static int framaccum = 0;
  OrkAssert(inumbuf == 1);  // interleaved: single buffer
  OrkAssert(inumchans >= 1 && inumchans <= 2);  // support mono or stereo

  /////////////////////////////
  // pull out input data (de-interleave)
  /////////////////////////////

  const auto& src_buffer = source_buffers->mBuffers[0];

  OrkAssert(src_buffer.mNumberChannels == inumchans);
  OrkAssert(src_buffer.mDataByteSize == inNumberFrames * sizeof(float) * inumchans);

  auto src_data = (const float*)src_buffer.mData;

  // De-interleave into separate channel buffers
  for (int ch = 0; ch < inumchans; ch++) {
    auto& dstbuf   = inpbufgroup->mChannels[ch];
    auto& dst_data = dstbuf.mSampleData;

    for (int j = 0; j < inNumberFrames; j++) {
      dst_data[j] = src_data[j * inumchans + ch];  // interleaved: ch0, ch1, ch0, ch1, ...
    }
  }

  auto& outL = inpbufgroup->mChannels[0].mSampleData;
  // auto& outR = inpbufgroup->mChannels[1].mSampleData;

  /////////////////////////////
  // if last channel, finish up group
  /////////////////////////////

  if (false) { // sine test

    double ftime = double(framaccum) / 44100.0;
    double fpanA = 0.5f + sinf(ftime * 2.0) * 0.5f;
    double fpanB = 0.5f + sinf(ftime * 1.0) * 0.5f;

    const double pi2 = 3.141592 * 2.0;

    static double ph = 0.0;
    for (int i = 0; i < inNumberFrames; i++) {

      double sineA = sinf(ph * pi2) * 0.3;
      double sineB = sinf(ph * pi2 * 1.3333) * 0.3;

      outL[i] = (fpanA * sineA) + (fpanB * sineB);
      // outR[i] = ((1.0 - fpanA) * sineA) + ((1.0 - fpanB) * sineB);

      ph += 0.01; // fmod(ph[i]+0.01,pi2);
    }
  }

  /////////////////////////////

  framaccum += inNumberFrames;

  _this->_inputQueue.push(inpbufgroup);
  ////////////////////////////////////////
  // write to passthru buffer
  ////////////////////////////////////////

  // if(!err) {
  //	err = This->mBuffer->Store( source_buffers,
  //		                        Float64(inNumberFrames),
  //		                        SInt64(inTimeStamp->mSampleTime) );
  // }

  return err;
}
///////////////////////////////////////////////////////////////////////////////
inline void ZeroBuffers(AudioBufferList* ioData) {
  for (UInt32 i = 0; i < ioData->mNumberBuffers; i++)
    memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
}
///////////////////////////////////////////////////////////////////////////////
OSStatus AuContext::_outputProc(
    void* inRefCon,
    AudioUnitRenderActionFlags* ioActionFlags,
    const AudioTimeStamp* TimeStamp,
    UInt32 inBusNumber,
    UInt32 inNumberFrames,
    AudioBufferList* ioData) {

  //printf("outputproc: begin\n");

  OSStatus err          = noErr;
  auto _this            = (AuContext*)inRefCon;
  _this->output_started = true;
  Float64 rate          = 0.0;
  AudioTimeStamp inTS, outTS;

  _this->_output_ready.store(true);

  // Handle output-only mode differently
  if (!_this->_inputDev) {
    // No input device - initialize output timing if needed
    if (_this->_firstOutputTime < 0.) {
      _this->_firstOutputTime = TimeStamp->mSampleTime;
    }

    // Skip directly to processing output queue
    // Don't return early - continue to queue processing below
  } else {
    // We have an input device - check if input has started
    if (_this->_firstInputTime < 0.) {
      // input device exists but hasn't run yet -> silence
      ZeroBuffers(ioData);
      if (_this->_curMixOutGroup) {
        _this->ReturnOutBuffer(_this->_curMixOutGroup);
        _this->_curMixOutGroup = nullptr;
      }
      printf("outputproc: end: A - waiting for input\n");
      return noErr;
    }
  }

  ////////////////////////////////////////
  // Timing synchronization (only needed when we have both devices)
  ////////////////////////////////////////

  // use the varispeed playback rate to offset small discrepancies in sample rate
  // first find the rate scalars of the input and output devices

  if (_this->_inputDev && _this->_inputDev->_info) {
    err = AudioDeviceGetCurrentTime(_this->_inputDev->_info->_ID, &inTS);
    // this callback may still be called a few times after the device has been stopped
    if (err) {
      ZeroBuffers(ioData);
      printf("outputproc: end: B - input device error\n");
      return noErr;
    }
  }

  if (_this->_outputDev && _this->_outputDev->_info) {
    err = AudioDeviceGetCurrentTime(_this->_outputDev->_info->_ID, &outTS);
    // AuCheckErr(err);
  }

  auto nanos       = AudioConvertHostTimeToNanos(outTS.mHostTime);
  static auto base = nanos;
  auto delta       = nanos - base;

  auto micros  = delta / 1000;
  auto millis  = micros / 1000;
  double ftime = double(millis) * 0.001;

  // get Delta between the devices and add it to the offset
  if (_this->_inputDev) {
    // Only do timing sync when we have input device
    if (_this->_firstOutputTime < 0.) {
      _this->_firstOutputTime = TimeStamp->mSampleTime;
      Float64 delta           = 0.0;
      if (_this->_firstInputTime >= 0.) {
        delta = (_this->_firstInputTime - _this->_firstOutputTime);
      }
      _this->computeThruOffset();
      // changed: 3865519 11/10/04
      if (delta < 0.0)
        _this->_inToOutSampleOffset -= delta;
      else
        _this->_inToOutSampleOffset = -delta + _this->_inToOutSampleOffset;
      ZeroBuffers(ioData);
      printf("outputproc: end: C - initializing timing\n");
      return noErr;
    }
  }

  auto startread       = SInt64(TimeStamp->mSampleTime - _this->_inToOutSampleOffset);
  static auto basefidx = startread;

  // logchan_auio->log( "outtime seconds<%g> fidx<%d>", ftime, int(startread-basefidx) );

  ////////////////////////////////////////
  // read from passthru buffer
  ////////////////////////////////////////

  int inumbuf = ioData->mNumberBuffers;
  OrkAssert(inumbuf == 1);

  int inumchan = ioData->mBuffers[0].mNumberChannels;
  OrkAssert(inumchan == 2);

  startread = std::max(0LL, startread);

  CARingBuffer::SampleTime endRead = startread + inNumberFrames;

  CARingBuffer::SampleTime startRead0 = startread;
  CARingBuffer::SampleTime endRead0   = endRead;

  ////////////////////////////////////////
  // mix
  ////////////////////////////////////////

  auto& outbufferI = ioData->mBuffers[0];
  auto as_floatI   = (float*)outbufferI.mData;

  ////////////////////////////////////////
  // clear output
  ////////////////////////////////////////

  for (int f = 0; f < inNumberFrames; f++) {
    as_floatI[(f << 1)]     = 0.0f;
    as_floatI[(f << 1) + 1] = 0.0f;
  }

  ////////////////////////////////////////
  // Process output queue
  ////////////////////////////////////////

  int inumframes_remaining = inNumberFrames;

  auto send = [&](StereoFragment* mixout) -> int {
    int num_sent = 0;

    const auto& inpbufferL         = mixout->mMixLeft;
    const auto& inpbufferR         = mixout->mMixRight;
    const float* inpdataL          = inpbufferL.mSampleData;
    const float* inpdataR          = inpbufferR.mSampleData;
    const int num_frames_in_mixout = mixout->mNumFrames - mixout->mNumUsed;
    int num_frames_to_send         = num_frames_in_mixout;

    if (num_frames_to_send > inumframes_remaining)
      num_frames_to_send = inumframes_remaining;

    int idst_base = (inNumberFrames - inumframes_remaining);
    int isrc_base = mixout->mNumUsed;

    // logchan_auio->log( "nfrem<%d> isrc_base<%d> idst_base<%d> "
    //	    "nfinmixout: %d nf2s: %d",
    //	    inumframes_remaining, isrc_base, idst_base,
    //	    num_frames_in_mixout, num_frames_to_send );

    if (0) // sine test
    {
      const double pi2     = 3.141592 * 2.0;
      static int framaccum = 0;
      double ftime         = double(framaccum) / 44100.0;
      double fpanA         = 0.5f + sinf(ftime * 2.0) * 0.5f;
      double fpanB         = 0.5f + sinf(ftime * 1.0) * 0.5f;

      static double ph = 0.0;
      for (int i = 0; i < num_frames_to_send; i++) {

        double sineA = sinf(ph * pi2) * 0.3;
        double sineB = sinf(ph * pi2 * 1.3333) * 0.3;

        as_floatI[i] = (fpanA * sineA) + (fpanB * sineB);
        as_floatI[i] = ((1.0 - fpanA) * sineA) + ((1.0 - fpanB) * sineB);

        ph += 0.01; // fmod(ph[i]+0.01,pi2);
      }
      framaccum++;
    } else
      for (int f = 0; f < num_frames_to_send; f++) {
        int didx = (f + idst_base) << 1;
        int sidx = f + isrc_base;

        as_floatI[didx + 0] = inpdataL[sidx];
        as_floatI[didx + 1] = inpdataR[sidx];
      }
    mixout->mNumUsed += num_frames_to_send;
    assert(mixout->mNumUsed <= mixout->mNumFrames);

    num_sent = num_frames_to_send;

    if (mixout->mNumUsed == mixout->mNumFrames) {
      _this->ReturnOutBuffer(mixout);
      _this->_curMixOutGroup = nullptr;
    }
   //printf("outputproc: end: D num_sent: %d\n", int(num_sent));

    return num_sent;
  };

  //////////////////////////////////////////////
  bool processed_any = false;
  while (inumframes_remaining > 0) {
    if (_this->_curMixOutGroup) {
      inumframes_remaining -= send(_this->_curMixOutGroup);
      processed_any = true;
    } else {
      if (_this->_outputQueue.try_pop(_this->_curMixOutGroup)) {
        _this->_curMixOutGroup->mNumUsed = 0;
        processed_any = true;
      } else {
       // No data available - break out to avoid spinning
       break;
      }

      if (_this->_curMixOutGroup) {
        inumframes_remaining -= send(_this->_curMixOutGroup);
        // logchan_auio->log( "outbuf<%d> inumch<%d> idbsiz<%d> data<%p>", i, inumch, idbsiz, data );
      }
    }
  }
  if (!processed_any && !_this->_inputDev) {
    static int empty_count = 0;
    if (++empty_count % 100 == 0) { // Log every 100th empty callback
      printf("outputproc: no data available (count=%d)\n", empty_count);
    }
  }
  return noErr;
}

bool AuContext::waitForOutputReady(int timeout_ms) {
 if (!_outputDev || !_outputUnit) {
   return true; // No output device, nothing to wait for
 }
 
 //logchan_auio->log("Waiting for output to be ready...");
 
 auto start_time = std::chrono::steady_clock::now();
 while (!_output_ready.load()) {
   auto current_time = std::chrono::steady_clock::now();
   auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
   if (elapsed_ms > timeout_ms) {
     logchan_auio->log("Timeout waiting for output ready");
     return false;
   }
   usleep(1000); // Sleep for 1ms
 }
 
 logchan_auio->log("Output is ready");
 return true;
}
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::ca
//

#endif // #if defined(ENABLE_CORE_AUDIO)
