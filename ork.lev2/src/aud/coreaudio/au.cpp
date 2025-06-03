#include <ork/lev2/config.h>
#if defined(ENABLE_CORE_AUDIO)

#include "au.h"
#include "CoreAudioBuffer.hpp"
#include <ork/util/logger.h>

namespace ork::lev2::ca {
static logchannel_ptr_t logchan_audunit = logger()->createChannel("AuContext", fvec3(1, 0.3, .6), true);

///////////////////////////////////////////////////////////////////////////////

AuContext::AuContext() //
    : _outputPool(32)  //
    , _inputPool(32) { //

  _inputCallback = [](LayerFragment* data) { //
                                             // printf( "got buffer for channel<%d>\n", abd.mChannelID );
  };
}

///////////////////////////////////////////////////////////////////////////////
/*
OSStatus AuContext::SetInputDevice(cadevice_impl_ptr_t dev) {
  _inputDev = dev;
  logchan_audunit->log("SetInputDevice<%d>", dev->_info->_ID);
  auto err = AudioUnitSetProperty(
      _inputUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &dev->_info->_ID, sizeof(dev->_info->_ID));
  AuCheckErr(err);

  return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::SetOutputDevice(cadevice_impl_ptr_t dev) {
  _outputDev = dev;
  logchan_audunit->log("SetOutputDevice<%d>", dev->_info->_ID);
  auto err = AudioUnitSetProperty(
      _outputUnit, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &dev->_info->_ID, sizeof(dev->_info->_ID));

  // AuCheckErr(err);

  return err;
}
*/
///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::Init(cadevice_impl_ptr_t indev, cadevice_impl_ptr_t outdev) {

  logchan_audunit->log(
      "AuContext::Init() indev<%d> outdev<%d>", //
      indev ? indev->_info->_ID : -1,           //
      outdev ? outdev->_info->_ID : -1);

  _inputDev  = indev;
  _outputDev = outdev;

  OSStatus err = setupGraph(indev, outdev);
  AuCheckErr(err);

  // Initialize graph if we created one
  if (_graph) {
    err = AUGraphInitialize(_graph);
    AuCheckErr(err);
  } else {
    // For input-only or output-only, units are already initialized in setup methods
    logchan_audunit->log("No graph to initialize (input-only or output-only mode)");
  }

  // Add latency between the two devices
  computeThruOffset();

  if (indev) {
    err = setupInputBuffers();
    AuCheckErr(err);
  }
  if (outdev) {
    err = setupOutputBuffers();
    AuCheckErr(err);
  }

  return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::Start() {

  logchan_audunit->log("AuContext::Start");
  if (IsRunning())
    return noErr;

  OSStatus err = noErr;

  _firstInputTime  = -1;
  _firstOutputTime = -1;

  // Start pulling for audio data
  // Start input if we have an input device
  if (_inputDev && _inputUnit) {
    err = AudioOutputUnitStart(_inputUnit);
    AuCheckErr(err);
  }

  // Handle output based on configuration
  if (_outputDev && _outputUnit) {
    if (_graph) {
      // If we have a graph (I/O mode), start the graph
      err = AUGraphStart(_graph);
      AuCheckErr(err);
    } else {
      // Output-only mode - start the HAL output unit directly
      logchan_audunit->log("Starting HAL output unit directly");
      err = AudioOutputUnitStart(_outputUnit);
      AuCheckErr(err);
    }
  }

  return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::Stop() {

  logchan_audunit->log("AuContext::Stop");
  if (false == IsRunning())
    return noErr;

  OSStatus err = noErr;
  if (_inputDev) {
    err = AudioOutputUnitStop(_inputUnit);
    AuCheckErr(err);
  }
  if (_outputDev) {
    err = AudioOutputUnitStop(_outputUnit);
    AuCheckErr(err);
    OrkAssertI(false, "TODO: force quit till we can get graceful shutdown working");
  }

  err = AUGraphStop(_graph);
  AuCheckErr(err);

  _firstInputTime  = -1;
  _firstOutputTime = -1;

  _keep_going = false;
  return err;
}

///////////////////////////////////////////////////////////////////////////////

bool IsUnitRunning(AudioUnit aunit) {
  if (!aunit)
    return false;

  UInt32 au_is_running = 0;
  UInt32 size          = sizeof(au_is_running);
  auto err             = AudioUnitGetProperty(
      aunit,
      kAudioOutputUnitProperty_IsRunning,
      kAudioUnitScope_Global,
      0, // input element
      &au_is_running,
      &size);
  AuCheckErr(err);
  return bool(au_is_running);
}

///////////////////////////////////////////////////////////////////////////////

bool AuContext::IsRunning() {

  bool input_running  = false;
  bool output_running = false;
  bool graph_running  = false;
  if (_inputDev) {
    input_running = IsUnitRunning(_inputUnit);
  }
  if (_outputDev) {
    // Output-only mode - check HAL unit
    output_running = IsUnitRunning(_outputUnit);
  }

  if (_graph) {
    Boolean is_running = false;
    auto err           = AUGraphIsRunning(_graph, &is_running);
    graph_running      = is_running;
    AuCheckErr(err);
  }

  return (input_running || output_running || graph_running);
  ;
}

///////////////////////////////////////////////////////////////////////////////

void AuContext::Cleanup() {
  logchan_audunit->log("AuContext::Cleanup");
  Stop();

  // delete mBuffer;
  // mBuffer = 0;
  if (_inputBuffer) {
    for (UInt32 i = 0; i < _inputBuffer->mNumberBuffers; i++)
      free(_inputBuffer->mBuffers[i].mData);
    free(_inputBuffer);
    _inputBuffer = 0;
  }

  AudioUnitUninitialize(_inputUnit);
  AUGraphClose(_graph);
  DisposeAUGraph(_graph);
  AudioComponentInstanceDispose(_inputUnit);
}

///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::createAUGraph() {
  OSStatus err = noErr;

  // Make a New Graph
  err = NewAUGraph(&_graph);
  AuCheckErr(err);

  // Open the Graph, AudioUnits are opened but not initialized
  err = AUGraphOpen(_graph);
  AuCheckErr(err);

  return err;
}
///////////////////////////////////////////////////////////////////////////////
OSStatus AuContext::createHALUnit(AudioUnit& unit, bool isInput) {
  OSStatus err = noErr;
  AudioComponent comp;
  AudioComponentDescription desc;

  desc.componentType         = kAudioUnitType_Output;
  desc.componentSubType      = kAudioUnitSubType_HALOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags        = 0;
  desc.componentFlagsMask    = 0;

  comp = AudioComponentFindNext(NULL, &desc);
  if (comp == NULL) {
    logchan_audunit->log("Failed to find HAL component");
    return -1;
  }

  err = AudioComponentInstanceNew(comp, &unit);
  AuCheckErr(err);

  return err;
}
///////////////////////////////////////////////////////////////////////////////
OSStatus AuContext::configureHALUnit(AudioUnit unit, AudioDeviceID deviceID, bool isInput) {
  OSStatus err = noErr;

  // Enable/disable IO based on whether this is input or output
  UInt32 enableIO = 1;
  if (isInput) {
    // Enable input
    err = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &enableIO, sizeof(enableIO));
    AuCheckErr(err);

    // Disable output
    enableIO = 0;
    err = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &enableIO, sizeof(enableIO));
    AuCheckErr(err);
  } else {
    // Enable output
    err = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Output, 0, &enableIO, sizeof(enableIO));
    AuCheckErr(err);

    // Disable input
    enableIO = 0;
    err      = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_EnableIO, kAudioUnitScope_Input, 1, &enableIO, sizeof(enableIO));
    AuCheckErr(err);
  }
 
    // Set the device AFTER enabling/disabling IO
  err = AudioUnitSetProperty(unit, kAudioOutputUnitProperty_CurrentDevice, 
                             kAudioUnitScope_Global, 0, &deviceID, sizeof(deviceID));
  AuCheckErr(err);
  

 // Set the buffer frame size
 UInt32 bufferFrameSize = desired_framesize;
 UInt32 propertySize = sizeof(UInt32);
 
 // Try to set the preferred buffer size
 err = AudioUnitSetProperty(unit, 
                            kAudioDevicePropertyBufferFrameSize, 
                            kAudioUnitScope_Global, 
                            0, 
                            &bufferFrameSize, 
                            propertySize);
 // It's OK if this fails - we'll use whatever the device prefers
 // Don't check the error here
 
  return err;
}
///////////////////////////////////////////////////////////////////////////////
OSStatus AuContext::setupGraphForInputOnly() {
  logchan_audunit->log("setupGraphForInputOnly");
  OSStatus err = noErr;

  // Create HAL unit for input
  err = createHALUnit(_inputUnit, true);
  AuCheckErr(err);

  // Configure for input
  err = configureHALUnit(_inputUnit, _inputDev->_info->_ID, true);
  AuCheckErr(err);

  // Setup input callback
  err = setupInputCallback();
  AuCheckErr(err);

  // Initialize the input unit
  err = AudioUnitInitialize(_inputUnit);
  AuCheckErr(err);

  return err;
}
///////////////////////////////////////////////////////////////////////////////
OSStatus AuContext::setupGraphForOutputOnly() {
  logchan_audunit->log("setupGraphForOutputOnly");
  OSStatus err = noErr;

  // Create HAL unit for output
  err = createHALUnit(_outputUnit, false);
  AuCheckErr(err);

  // Configure for output
  err = configureHALUnit(_outputUnit, _outputDev->_info->_ID, false);
  AuCheckErr(err);

  // Setup output callback
  AURenderCallbackStruct output;
  output.inputProc       = _outputProc;
  output.inputProcRefCon = this;

  err = AudioUnitSetProperty(_outputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &output, sizeof(output));
  AuCheckErr(err);

  // Tell the output unit not to reset timestamps
  UInt32 startAtZero = 0;
  err                = AudioUnitSetProperty(
      _outputUnit, kAudioOutputUnitProperty_StartTimestampsAtZero, kAudioUnitScope_Global, 0, &startAtZero, sizeof(startAtZero));
  AuCheckErr(err);

  // Initialize the output unit
  err = AudioUnitInitialize(_outputUnit);
  AuCheckErr(err);

  return err;
}
///////////////////////////////////////////////////////////////////////////////
OSStatus AuContext::setupGraphForIO() {
  logchan_audunit->log("setupGraphForIO");
  OSStatus err = noErr;

  // Setup input HAL unit
  err = createHALUnit(_inputUnit, true);
  AuCheckErr(err);

  err = configureHALUnit(_inputUnit, _inputDev->_info->_ID, true);
  AuCheckErr(err);

  err = setupInputCallback();
  AuCheckErr(err);

  err = AudioUnitInitialize(_inputUnit);
  AuCheckErr(err);

  // Create AUGraph for output
  err = createAUGraph();
  AuCheckErr(err);

  // Add output node to graph
  AudioComponentDescription outDesc;
  outDesc.componentType         = kAudioUnitType_Output;
  outDesc.componentSubType      = kAudioUnitSubType_DefaultOutput;
  outDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
  outDesc.componentFlags        = 0;
  outDesc.componentFlagsMask    = 0;

  err = AUGraphAddNode(_graph, &outDesc, &_outputNode);
  AuCheckErr(err);

  err = AUGraphNodeInfo(_graph, _outputNode, NULL, &_outputUnit);
  AuCheckErr(err);

  // Setup output callback
  AURenderCallbackStruct output;
  output.inputProc       = _outputProc;
  output.inputProcRefCon = this;

  err = AudioUnitSetProperty(_outputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &output, sizeof(output));
  AuCheckErr(err);

  // Set output device
  err = AudioUnitSetProperty(
      _outputUnit,
      kAudioOutputUnitProperty_CurrentDevice,
      kAudioUnitScope_Global,
      0,
      &_outputDev->_info->_ID,
      sizeof(_outputDev->_info->_ID));
  AuCheckErr(err);

  return err;
}
///////////////////////////////////////////////////////////////////////////////
OSStatus AuContext::setupGraph(cadevice_impl_ptr_t indev, cadevice_impl_ptr_t outdev) {
  logchan_audunit->log("setupGraph() indev<%d> outdev<%d>", indev ? indev->_info->_ID : -1, outdev ? outdev->_info->_ID : -1);
  OSStatus err = noErr;

  if (indev && outdev) {
    err = setupGraphForIO();
  } else if (indev && !outdev) {
    err = setupGraphForInputOnly();
  } else if (!indev && outdev) {
    err = setupGraphForOutputOnly();
  } else {
    logchan_audunit->log("Error: No devices specified");
    return kAudioUnitErr_InvalidParameter;
  }

  return err;
}

///////////////////////////////////////////////////////////////////////////////

StereoFragment* AuContext::AllocOutBuffer(int numfr) {
  auto buf = _outputPool.AllocObject();
  buf->Init(numfr);
  return buf;
}

///////////////////////////////////////////////////////////////////////////////

void AuContext::ReturnOutBuffer(StereoFragment* data) {
  _outputPool.ReturnObject(data);
}

///////////////////////////////////////////////////////////////////////////////

LayerFragment* AuContext::AllocLayerFragment(int inumch, int numfr) {
  auto buf = _inputPool.AllocObject();
  buf->Init(inumch, numfr);
  return buf;
}

///////////////////////////////////////////////////////////////////////////////

void AuContext::ReturnLayerFragment(LayerFragment* data) {
  _inputPool.ReturnObject(data);
}

} // namespace ork::lev2::ca

#endif // #if defined(ENABLE_CORE_AUDIO)
