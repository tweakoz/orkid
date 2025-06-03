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

///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::Init(cadevice_impl_ptr_t indev, cadevice_impl_ptr_t outdev) {

  logchan_audunit->log("AuContext::Init() indev<%d> outdev<%d>", indev ? indev->_info->_ID : -1, outdev->_info->_ID);
  OSStatus err = noErr;
  if(indev){
    // Setup AUHAL for an input device
    err = SetupAUHAL(indev->_info->_ID);
    AuCheckErr(err);
    SetInputDevice(indev);
  }
  if(outdev){
    SetOutputDevice(outdev);
  }

  // Setup Graph containing Default Output Unit
  err = SetupGraph(indev, outdev);
  AuCheckErr(err);

  err = AUGraphInitialize(_graph);
  AuCheckErr(err);

  // Add latency between the two devices
  ComputeThruOffset();

  if(indev){
    err = SetupInputBuffers();
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
  // Start pulling for audio data
  if(_inputDev){
    err = AudioOutputUnitStart(_inputUnit);
    AuCheckErr(err);
  }

  err = AUGraphStart(_graph);
  AuCheckErr(err);

  _firstInputTime  = -1;
  _firstOutputTime = -1;

  return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::Stop() {

  logchan_audunit->log("AuContext::Stop");
  if (false == IsRunning())
    return noErr;

  OSStatus err = noErr;
  if(_inputDev) {
    err          = AudioOutputUnitStop(_inputUnit);
    AuCheckErr(err);
  }

  err = AUGraphStop(_graph);
  AuCheckErr(err);

  _firstInputTime  = -1;
  _firstOutputTime = -1;

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

  bool hal_running = true; 
  if(_inputDev) {
    hal_running = IsUnitRunning(_inputUnit);
  }

  Boolean graph_running = false;

  if (_graph) {
    auto err = AUGraphIsRunning(_graph, &graph_running);
    AuCheckErr(err);
  }

  return (hal_running || graph_running);
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

OSStatus AuContext::SetupGraph(cadevice_impl_ptr_t indev, cadevice_impl_ptr_t outdev) {
  logchan_audunit->log("AuContext::SetupGraph() indev<%d> outdev<%d>", indev ? indev->_info->_ID : -1, outdev ? outdev->_info->_ID : -1);
  OSStatus err = noErr;
  AURenderCallbackStruct output;

  // Make a New Graph
  err = NewAUGraph(&_graph);
  AuCheckErr(err);

  // Open the Graph, AudioUnits are opened but not initialized
  err = AUGraphOpen(_graph);
  AuCheckErr(err);

  err = MakeGraph();
  AuCheckErr(err);

  // Tell the output unit not to reset timestamps
  // Otherwise sample rate changes will cause sync los
  if(outdev) {
    UInt32 startAtZero = 0;
    err                = AudioUnitSetProperty(
        _outputUnit, kAudioOutputUnitProperty_StartTimestampsAtZero, kAudioUnitScope_Global, 0, &startAtZero, sizeof(startAtZero));
    AuCheckErr(err);

    output.inputProc       = _outputProc;
    output.inputProcRefCon = this;

    SetupOutputBuffers();

    err = AudioUnitSetProperty(_outputUnit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &output, sizeof(output));
    AuCheckErr(err);
  }

  return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::MakeGraph() { //
  logchan_audunit->log("AuContext::MakeGraph");
  OSStatus err = noErr;
  AudioComponentDescription outDesc;

  outDesc.componentType         = kAudioUnitType_Output;
  outDesc.componentSubType      = kAudioUnitSubType_DefaultOutput;
  outDesc.componentManufacturer = kAudioUnitManufacturer_Apple;
  outDesc.componentFlags        = 0;
  outDesc.componentFlagsMask    = 0;

  //////////////////////////
  /// MAKE NODES
  //////////////////////////

  err = AUGraphAddNode(_graph, &outDesc, &_outputNode);
  AuCheckErr(err);

  err = AUGraphNodeInfo(_graph, _outputNode, NULL, &_outputUnit);
  AuCheckErr(err);

  return err;
}

///////////////////////////////////////////////////////////////////////////////

OSStatus AuContext::SetupAUHAL(AudioDeviceID in) { //

  logchan_audunit->log("AuContext::SetupAUHAL");
  OSStatus err = noErr;

  AudioComponent comp;
  AudioComponentDescription desc;

  // There are several different types of Audio Units.
  // Some audio units serve as Outputs, Mixers, or DSP
  // units. See AUComponent.h for listing
  desc.componentType = kAudioUnitType_Output;

  // Every Component has a subType, which will give a clearer picture
  // of what this components function will be.
  desc.componentSubType = kAudioUnitSubType_HALOutput;

  // all Audio Units in AUComponent.h must use
  //"kAudioUnitManufacturer_Apple" as the Manufacturer
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  desc.componentFlags        = 0;
  desc.componentFlagsMask    = 0;

  // Finds a component that meets the desc spec's
  comp = AudioComponentFindNext(NULL, &desc);
  if (comp == NULL)
    exit(-1);

  // gains access to the services provided by the component
  err = AudioComponentInstanceNew(comp, &_inputUnit);
  AuCheckErr(err);

  // AUHAL needs to be initialized before anything is done to it
  err = AudioUnitInitialize(_inputUnit);
  AuCheckErr(err);

  err = EnableInputs();
  AuCheckErr(err);

  // err= SetInputDeviceAsCurrent(in);
  // AuCheckErr(err);

  err = CallbackSetup();
  AuCheckErr(err);

  // Don't setup buffers until you know what the
  // input and output device audio streams look like.

  err = AudioUnitInitialize(_inputUnit);
  AuCheckErr(err);

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
