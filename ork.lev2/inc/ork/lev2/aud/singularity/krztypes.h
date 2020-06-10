////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

#include <ork/orktypes.h>
#include <ork/math/audiomath.h>

namespace ork::audio::singularity {
using namespace ork::audiomath;
///////////////////////////////////////////////////////////////////////////////
static constexpr int kmaxenvperlayer       = 8;
static constexpr int kmaxdspblocksperstage = 16; // vertical dimension of layer's dsp grid
static constexpr int kmaxdspstagesperlayer = 16; // horizontal dimension of layer's dsp grid
static constexpr int kmaxctrlperblock      = 16;
static constexpr int kmaxparmperblock      = 16;
static constexpr int kmaxlayerspersynth    = 512;
///////////////////////////////////////////////////////////////////////////////
static constexpr double pi   = 3.141592654;
static constexpr double pi2  = 3.141592654 * 2.0;
static constexpr double pid2 = 3.141592654 * 0.5;
static const double sqrt2    = sqrt(2.0);
///////////////////////////////////////////////////////////////////////////////
static constexpr int frames_per_dsppass     = 256;
static constexpr int frames_per_controlpass = 32;
inline constexpr float getSampleRate() {
  return 44100.0f;
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
constexpr float PI_ISR = pi2 * getInverseSampleRate();
///////////////////////////////////////////////////////////////////////////////
struct ProgramData;
struct LayerData;
struct KeyMap;
struct KmpBlockData;
struct kmregion;
struct sample;
struct multisample;
struct synth;
struct Layer;
struct outputBuffer;
struct RateLevelEnvData;
struct natenvseg;
struct SynthObjectsDB;
struct FPARAM;
struct Wavetable;
struct KeyOnInfo;
struct IoMask;
struct HudPanel;
struct BlockModulationData;
struct DspParamData;
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
///////////////////////////////////////////////////////////////////////////////
// scope / signal analyzer
///////////////////////////////////////////////////////////////////////////////
struct HudFrameAudio;
struct HudFrameControl;
struct HudViewport;
struct SignalScope;
struct ScopeSource;
struct ScopeSink;
///////////////////////////////////////////////////////////////////////////////
using outbus_ptr_t                = std::shared_ptr<OutputBus>;
using iomask_ptr_t                = std::shared_ptr<IoMask>;
using iomask_constptr_t           = std::shared_ptr<const IoMask>;
using algdata_ptr_t               = std::shared_ptr<AlgData>;
using algdata_constptr_t          = std::shared_ptr<const AlgData>;
using alg_ptr_t                   = std::shared_ptr<Alg>;
using keymap_ptr_t                = std::shared_ptr<KeyMap>;
using keymap_constptr_t           = std::shared_ptr<const KeyMap>;
using dspblk_ptr_t                = std::shared_ptr<DspBlock>;
using dspbuf_ptr_t                = std::shared_ptr<DspBuffer>;
using dspblkdata_ptr_t            = std::shared_ptr<DspBlockData>;
using dspblkdata_constptr_t       = std::shared_ptr<const DspBlockData>;
using dspstagedata_ptr_t          = std::shared_ptr<DspStageData>;
using dspstagedata_constptr_t     = std::shared_ptr<const DspStageData>;
using dspstage_ptr_t              = std::shared_ptr<DspStage>;
using dspstage_constptr_t         = std::shared_ptr<const DspStage>;
using lyrdata_ptr_t               = std::shared_ptr<LayerData>;
using lyrdata_constptr_t          = std::shared_ptr<const LayerData>;
using prgdata_ptr_t               = std::shared_ptr<ProgramData>;
using prgdata_constptr_t          = std::shared_ptr<const ProgramData>;
using syndata_ptr_t               = std::shared_ptr<SynthData>;
using bankdata_ptr_t              = std::shared_ptr<SynthObjectsDB>;
using bankdata_constptr_t         = std::shared_ptr<const SynthObjectsDB>;
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
using hudvp_ptr_t                 = std::shared_ptr<HudViewport>;
using hudpanel_ptr_t              = std::shared_ptr<HudPanel>;
using scopesource_ptr_t           = std::shared_ptr<ScopeSource>;
using scopesink_ptr_t             = std::shared_ptr<ScopeSink>;
using signalscope_ptr_t           = std::shared_ptr<SignalScope>;
///////////////////////////////////////////////////////////////////////////////
typedef std::function<float()> controller_t;
typedef std::function<float(float)> mapper_t;
typedef std::function<float(FPARAM& cec)> evalit_t;
///////////////////////////////////////////////////////////////////////////////
struct outputBuffer {
  outputBuffer();
  void resize(int inumframes);

  float* _leftBuffer;
  float* _rightBuffer;
  int _maxframes;
  int _numframes;
};
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
