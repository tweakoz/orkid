#include "CoreAudioDevice.h"
#include "CoreAudioBuffer.h"
#include "ca_helpers/CARingBuffer.h"
#include "ca_helpers/CAStreamBasicDescription.h"
#include <libkern/OSAtomic.h>
#include <ork/kernel/opq.h>
#include <atomic>

#define tryerr(err, x)                                                                                                             \
  if (err == x)                                                                                                                    \
  return #x

inline const char* geterrcode(int err) {
  tryerr(err, kAudioUnitErr_InvalidProperty);
  tryerr(err, kAudioUnitErr_InvalidParameter);
  tryerr(err, kAudioUnitErr_InvalidElement);
  tryerr(err, kAudioUnitErr_NoConnection);
  tryerr(err, kAudioUnitErr_FailedInitialization);
  tryerr(err, kAudioUnitErr_TooManyFramesToProcess);
  tryerr(err, kAudioUnitErr_IllegalInstrument);
  tryerr(err, kAudioUnitErr_InstrumentTypeNotFound);
  tryerr(err, kAudioUnitErr_InvalidFile);
  tryerr(err, kAudioUnitErr_UnknownFileType);
  tryerr(err, kAudioUnitErr_FileNotSpecified);
  tryerr(err, kAudioUnitErr_FormatNotSupported);
  tryerr(err, kAudioUnitErr_Uninitialized);
  tryerr(err, kAudioUnitErr_InvalidScope);
  tryerr(err, kAudioUnitErr_PropertyNotWritable);
  tryerr(err, kAudioUnitErr_CannotDoInCurrentContext);
  tryerr(err, kAudioUnitErr_InvalidPropertyValue);
  tryerr(err, kAudioUnitErr_PropertyNotInUse);
  tryerr(err, kAudioUnitErr_Initialized);
  tryerr(err, kAudioUnitErr_InvalidOfflineRender);
  tryerr(err, kAudioUnitErr_Unauthorized);
  tryerr(err, kAUGraphErr_NodeNotFound);
  tryerr(err, kAUGraphErr_InvalidConnection);
  tryerr(err, kAUGraphErr_OutputNodeErr);
  tryerr(err, kAUGraphErr_CannotDoInCurrentContext);
  tryerr(err, kAUGraphErr_InvalidAudioUnit);
  return "unknown";
}

#define AuCheckErr(err)                                                                                                            \
  {                                                                                                                                \
    if (err) {                                                                                                                     \
      OSStatus error = static_cast<OSStatus>(err);                                                                                 \
      fprintf(stdout, "CAPlayThrough Error: code<%d:%s> ->  %s:  %d\n", int(error), geterrcode(int(error)), __FILE__, __LINE__);   \
      fflush(stdout);                                                                                                              \
      assert(false);                                                                                                               \
    }                                                                                                                              \
  }

namespace ork::lev2::ca {

bool IsUnitRunning(AudioUnit au);

typedef std::function<void(LayerFragment*)> input_channel_callback_t;

static const int KMAXCHANNELS = 64;

struct AuContext {
  AuContext();
  OSStatus Init(cadevice_impl_ptr_t indev, cadevice_impl_ptr_t outdev);
  OSStatus Start();
  OSStatus Stop();
  bool IsRunning();
  void Cleanup();

  StereoFragment* AllocOutBuffer(int numfr);
  void ReturnLayerFragment(LayerFragment*);

  void ReturnOutBuffer(StereoFragment*);
  LayerFragment* AllocLayerFragment(int inumch, int numfr);

  // High-level setup methods
  OSStatus setupGraph(cadevice_impl_ptr_t indev, cadevice_impl_ptr_t outdev);
  OSStatus setupGraphForInputOnly();
  OSStatus setupGraphForOutputOnly();
  OSStatus setupGraphForIO();

  // Device setup methods
  OSStatus setOutputDevice(cadevice_impl_ptr_t dev);
  OSStatus setInputDevice(cadevice_impl_ptr_t dev);

  // Common setup methods
  OSStatus createAUGraph();
  OSStatus createHALUnit(AudioUnit& unit, bool isInput);
  OSStatus configureHALUnit(AudioUnit unit, AudioDeviceID deviceID, bool isInput);

  OSStatus setupInputBuffers();
  OSStatus setupOutputBuffers();
  OSStatus callbackSetup();
  OSStatus enableInputs();
  OSStatus enableOutputs();

  void computeThruOffset();

  static OSStatus _inputProc(
      void* inRefCon,
      AudioUnitRenderActionFlags* ioActionFlags,
      const AudioTimeStamp* inTimeStamp,
      UInt32 inBusNumber,
      UInt32 inNumberFrames,
      AudioBufferList* ioData);

  static OSStatus _outputProc(
      void* inRefCon,
      AudioUnitRenderActionFlags* ioActionFlags,
      const AudioTimeStamp* inTimeStamp,
      UInt32 inBusNumber,
      UInt32 inNumberFrames,
      AudioBufferList* ioData);

  bool waitForOutputReady(int timeout_ms = 1000);
  int _numInputChannels           = 0;
  int _mumOutputBuffers           = 0;
  int _numOutputBuffersProcessed  = 0;
  int _inputFrameSize             = 0;
  int _outputFrameSize            = 0;
  bool output_started             = false;
  bool _keep_going                = true;
  std::atomic<bool> _output_ready = false;
  StereoFragment* _curMixOutGroup = nullptr;
  AudioBufferList* _inputBuffer   = nullptr;

  // Buffer sample info
  Float64 _firstInputTime      = -1.0;
  Float64 _firstOutputTime     = -1.0;
  Float64 _inToOutSampleOffset = 0.0;

  input_channel_callback_t _inputCallback;

  cadevice_impl_ptr_t _inputDev;
  cadevice_impl_ptr_t _outputDev;

  // AudioUnits and Graph
  AUGraph _graph = 0;
  AUNode _outputNode = 0;
  AudioUnit _outputUnit = 0;
  AudioUnit _inputUnit = 0;

  StereoFragmentPool _outputPool;
  LayerFragmentPool _inputPool;

  MpMcBoundedQueue<StereoFragment*, 4096> _outputQueue;
  MpMcBoundedQueue<LayerFragment*, 4096> _inputQueue;
};

} // namespace ork::lev2::ca
