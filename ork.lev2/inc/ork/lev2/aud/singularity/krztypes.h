////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
///////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <map>
#include <set>
#include <string>
#include <queue>
#include <functional>
#include <memory>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/kernel/svariant.h>
#include <ork/kernel/concurrent_queue.h>
#include <ork/math/audiomath.h>

namespace ork::audio::singularity {
using namespace ork::audiomath;
///////////////////////////////////////////////////////////////////////////////
static constexpr int kmaxenvperlayer       = 8;
static constexpr int kmaxdspblocksperstage = 16; // vertical dimension of layer's dsp grid
static constexpr int kmaxdspstagesperlayer = 16; // horizontal dimension of layer's dsp grid
static constexpr int kmaxctrlperblock      = 32;
static constexpr int kmaxparmperblock      = 32;
static constexpr int kmaxlayerspersynth    = 512;
///////////////////////////////////////////////////////////////////////////////
static constexpr double pi      = 3.141592654;
static constexpr double pi2     = 3.141592654 * 2.0;
static constexpr double pid2    = 3.141592654 * 0.5;
static const double sqrt2       = sqrt(2.0);
static constexpr double kinv256 = 1.0 / double(1 << 8);
static constexpr double kinv4k  = 1.0 / double(1 << 12);
static constexpr double kinv32k = 1.0 / double(1 << 15);
static constexpr double kinv64k = 1.0 / double(1 << 16);
static constexpr double kinv24m = 1.0 / double(1 << 24);
static constexpr double kinv4g  = 1.0 / double(1L << 32);
///////////////////////////////////////////////////////////////////////////////
static constexpr int frames_per_controlpass = 32;
static constexpr float kfpc                 = 1.0f / float(frames_per_controlpass);
inline constexpr float getSampleRate() {
  return 48000.0f;
}
inline constexpr float getInverseSampleRate() {
  return 1.0f / getSampleRate();
}
inline constexpr float controlRate() {
  return getSampleRate() / float(frames_per_controlpass);
}
inline constexpr float controlPeriod() {
  return float(frames_per_controlpass) / getSampleRate();
}
constexpr float PI2XISR = pi2 * getInverseSampleRate();
///////////////////////////////////////////////////////////////////////////////
struct ProgramData;
struct LayerData;
struct KeyMapData;
struct KmpBlockData;
struct KmRegionData;
struct SampleData;
struct MultiSampleData;
struct synth;
struct Layer;
struct outputBuffer;
struct RateLevelEnvData;
struct natenvseg;
struct NatEnv;
struct BankData;
struct DspParam;
struct Wavetable;
struct KeyOnInfo;
struct IoConfig;
struct HudPanel;
struct BlockModulationData;
struct DspParamData;
struct DspParam;
struct DspBlockData;
struct DspStageData;
struct AlgData;
struct DspBlock;
struct DspBuffer;
struct Alg;
struct DspStage;
struct OscillatorSyncTrack;
struct ScopeSyncTrack;
struct ControllerData;
struct ControllerInst;
struct ControlBlockData;
struct SynthData;
struct KrzSynthData;
struct Sf2TestSynthData;
struct Tx81zData;
struct CzData;
struct KrzTestData;
struct KrzKmTestData;
struct CzProgData;
struct OutputBus;
struct KeyOnModifiers;
struct NatEnvWrapperData;
struct DelayContext;
struct SAMPLER_DATA;
struct programInst;
///////////////////////////////////////////////////////////////////////////////
// sequencer
///////////////////////////////////////////////////////////////////////////////
struct Event;
struct Sequencer;
struct Sequence;
struct Track;
struct Clip;
struct TimeStamp;
struct TimeBase;
struct EventClip;
struct FourOnFloorClip;
struct SequencePlayback;
struct TrackPlayback;
struct ClipPlayback;
struct EventIterator;
///////////////////////////////////////////////////////////////////////////////
// scope / signal analyzer
///////////////////////////////////////////////////////////////////////////////
struct HudFrameAudio;
struct HudFrameControl;
struct HudLayoutGroup;
struct SignalScope;
struct ScopeSource;
struct ScopeSink;
///////////////////////////////////////////////////////////////////////////////
using outbus_ptr_t                = std::shared_ptr<OutputBus>;
using ioconfig_ptr_t              = std::shared_ptr<IoConfig>;
using ioconfig_constptr_t         = std::shared_ptr<const IoConfig>;
using algdata_ptr_t               = std::shared_ptr<AlgData>;
using algdata_constptr_t          = std::shared_ptr<const AlgData>;
using alg_ptr_t                   = std::shared_ptr<Alg>;
using keymap_ptr_t                = std::shared_ptr<KeyMapData>;
using keymap_constptr_t           = std::shared_ptr<const KeyMapData>;
using dspblk_ptr_t                = std::shared_ptr<DspBlock>;
using dspbuf_ptr_t                = std::shared_ptr<DspBuffer>;
using dspblkdata_ptr_t            = std::shared_ptr<DspBlockData>;
using dspblkdata_constptr_t       = std::shared_ptr<const DspBlockData>;
using dspstagedata_ptr_t          = std::shared_ptr<DspStageData>;
using dspstagedata_constptr_t     = std::shared_ptr<const DspStageData>;
using dspstage_ptr_t              = std::shared_ptr<DspStage>;
using dspstage_constptr_t         = std::shared_ptr<const DspStage>;
using layer_ptr_t                 = std::shared_ptr<Layer>;
using lyrdata_ptr_t               = std::shared_ptr<LayerData>;
using lyrdata_constptr_t          = std::shared_ptr<const LayerData>;
using prgdata_ptr_t               = std::shared_ptr<ProgramData>;
using prgdata_constptr_t          = std::shared_ptr<const ProgramData>;
using syndata_ptr_t               = std::shared_ptr<SynthData>;
using bankdata_ptr_t              = std::shared_ptr<BankData>;
using bankdata_constptr_t         = std::shared_ptr<const BankData>;
using krzsyndata_ptr_t            = std::shared_ptr<KrzSynthData>;
using sf2syndata_ptr_t            = std::shared_ptr<Sf2TestSynthData>;
using tx81zsyndata_ptr_t          = std::shared_ptr<Tx81zData>;
using czsyndata_ptr_t             = std::shared_ptr<CzData>;
using krztestsyndata_ptr_t        = std::shared_ptr<KrzTestData>;
using krzkmtestsyndata_ptr_t      = std::shared_ptr<KrzKmTestData>;
using czprogdata_ptr_t            = std::shared_ptr<CzProgData>;
using oschardsynctrack_ptr_t      = std::shared_ptr<OscillatorSyncTrack>;
using scopesynctrack_ptr_t        = std::shared_ptr<ScopeSyncTrack>;
using controllerdata_ptr_t        = std::shared_ptr<ControllerData>;
using controllerdata_constptr_t   = std::shared_ptr<const ControllerData>;
using controlblockdata_ptr_t      = std::shared_ptr<ControlBlockData>;
using controlblockdata_constptr_t = std::shared_ptr<const ControlBlockData>;
using kmpblockdata_ptr_t          = std::shared_ptr<KmpBlockData>;
using hudframeaud_ptr_t           = std::shared_ptr<HudFrameAudio>;
using hudframectrl_ptr_t          = std::shared_ptr<HudFrameControl>;
using hudvp_ptr_t                 = std::shared_ptr<HudLayoutGroup>;
using hudpanel_ptr_t              = std::shared_ptr<HudPanel>;
using scopesource_ptr_t           = std::shared_ptr<ScopeSource>;
using scopesink_ptr_t             = std::shared_ptr<ScopeSink>;
using signalscope_ptr_t           = std::shared_ptr<SignalScope>;
using fxpresetmap_t               = std::vector<lyrdata_ptr_t>;
using keyonmod_ptr_t = std::shared_ptr<KeyOnModifiers>;
using natenv_ptr_t = std::shared_ptr<NatEnv>;
using dspparam_ptr_t         = std::shared_ptr<DspParamData>;
using dspparam_constptr_t    = std::shared_ptr<const DspParamData>;
using dspparammod_ptr_t      = std::shared_ptr<BlockModulationData>;
using dspparammod_constptr_t = std::shared_ptr<const BlockModulationData>;
using natenvwrapperdata_ptr_t = std::shared_ptr<NatEnvWrapperData>;
using delaycontext_ptr_t = std::shared_ptr<DelayContext>;
using sample_ptr_t = std::shared_ptr<SampleData>;
using sample_constptr_t = std::shared_ptr<const SampleData>;
using multisample_ptr_t = std::shared_ptr<MultiSampleData>;
using multisample_constptr_t = std::shared_ptr<const MultiSampleData>;
using kmregion_ptr_t = std::shared_ptr<KmRegionData>;
using kmregion_constptr_t = std::shared_ptr<const KmRegionData>;
using samplerdata_ptr_t = std::shared_ptr<SAMPLER_DATA>;
using programinst_ptr_t = programInst*;
///////////////////////////////////////////////////////////////////////////////
using sequencer_ptr_t = std::shared_ptr<Sequencer>;
using sequence_ptr_t = std::shared_ptr<Sequence>;
using track_ptr_t = std::shared_ptr<Track>;
using clip_ptr_t = std::shared_ptr<Clip>;
using event_ptr_t = std::shared_ptr<Event>;
using timestamp_ptr_t = std::shared_ptr<TimeStamp>;
using timebase_ptr_t = std::shared_ptr<TimeBase>;
using sequenceplayback_ptr_t = std::shared_ptr<SequencePlayback>;
using clipplayback_ptr_t = std::shared_ptr<ClipPlayback>;
using trackplayback_ptr_t = std::shared_ptr<TrackPlayback>;
using layer_vect_t = std::vector<layer_ptr_t>;
///////////////////////////////////////////////////////////////////////////////
typedef std::function<float()> controller_t;
typedef std::function<float(float)> mapper_t;
typedef std::function<float(DspParam& cec)> evalit_t;
///////////////////////////////////////////////////////////////////////////////
struct outputBuffer {
  outputBuffer();
  void resize(int inumframes);

  float* _leftBuffer;
  float* _rightBuffer;
  int _maxframes;
  int _numframes;
};
inline void validateSample(float inp) {
  OrkAssert(not isnan(inp));
  OrkAssert(isfinite(inp));
}
///////////////////////////////////////////////////////////////////////////////
struct HudEvent{
  uint32_t _eventID;
  svar64_t _eventData;
};

using hudevent_ptr_t = std::shared_ptr<HudEvent>;

struct HudEventSink {
  using eventcallback_t = std::function<void(hudevent_ptr_t)>;
  eventcallback_t _onEvent = [](hudevent_ptr_t) {};
};
using hudeventsink_ptr_t = std::shared_ptr<HudEventSink>;
using hudeventsink_list = std::vector<hudeventsink_ptr_t>;

struct HudEventRouter {
  void registerSinkForHudEvent(uint32_t eventID, hudeventsink_ptr_t sink);
  void routeEvent(hudevent_ptr_t hev);
  void processEvents();
  std::map<uint32_t, hudeventsink_list> _routing_map;
  ork::MpMcBoundedQueue<hudevent_ptr_t,1024> _hudevents;
};

using hudeventrouter_ptr_t = std::shared_ptr<HudEventRouter>;
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
