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
struct VastObjectsDB;
struct FPARAM;
struct Wavetable;
struct KeyOnInfo;
struct IoMask;
struct EnvCtrlData;
///////////////////////////////////////////////////////////////////////////////
struct BlockModulationData;
struct DspKeyOnInfo;
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
///////////////////////////////////////////////////////////////////////////////
struct SynthData;
struct KrzSynthData;
struct Sf2TestSynthData;
struct Tx81zData;
struct CzData;
struct KrzTestData;
struct KrzKmTestData;
struct CzProgData;
///////////////////////////////////////////////////////////////////////////////
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
using envctrldata_ptr_t           = std::shared_ptr<EnvCtrlData>;
using kmpblockdata_ptr_t          = std::shared_ptr<KmpBlockData>;
///////////////////////////////////////////////////////////////////////////////
static const int kmaxenvperlayer       = 8;
static const int kmaxdspblocksperstage = 4;
static const int kmaxdspstagesperlayer = 8;
static const int kmaxctrlperblock      = 4;
static const int kmaxparmperblock      = 16;
///////////////////////////////////////////////////////////////////////////////
static const double pi    = 3.141592654;
static const double pi2   = 3.141592654 * 2.0;
static const double pid2  = 3.141592654 * 0.5;
static const double sqrt2 = sqrt(2.0);
inline float getSampleRate() {
  return 48000.0f;
}
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
