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
// typedef int16_t s16;
// typedef uint32_t u32;
typedef std::function<void()> void_lamda_t;
///////////////////////////////////////////////////////////////////////////////
struct programData;
struct LayerData;
struct KeyMap;
struct kmregion;
struct sample;
struct multisample;
struct synth;
struct layer;
struct outputBuffer;
struct RateLevelEnvData;
struct natenvseg;
struct VastObjectsDB;
struct DspBlockData;
struct FPARAM;
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
using keymap_ptr_t           = std::shared_ptr<KeyMap>;
using keymap_constptr_t      = std::shared_ptr<const KeyMap>;
using dspblkdata_ptr_t       = std::shared_ptr<DspBlockData>;
using lyrdata_ptr_t          = std::shared_ptr<LayerData>;
using lyrdata_constptr_t     = std::shared_ptr<const LayerData>;
using prgdata_ptr_t          = std::shared_ptr<programData>;
using prgdata_constptr_t     = std::shared_ptr<const programData>;
using syndata_ptr_t          = std::shared_ptr<SynthData>;
using krzsyndata_ptr_t       = std::shared_ptr<KrzSynthData>;
using sf2syndata_ptr_t       = std::shared_ptr<Sf2TestSynthData>;
using tx81zsyndata_ptr_t     = std::shared_ptr<Tx81zData>;
using czsyndata_ptr_t        = std::shared_ptr<CzData>;
using krztestsyndata_ptr_t   = std::shared_ptr<KrzTestData>;
using krzkmtestsyndata_ptr_t = std::shared_ptr<KrzKmTestData>;
using czprogdata_ptr_t       = std::shared_ptr<CzProgData>;
///////////////////////////////////////////////////////////////////////////////
static const int kmaxenvperlayer       = 8;
static const int kmaxdspblocksperlayer = 8;
static const int kmaxctrlperblock      = 4;
static const int kmaxctrlblocks        = 8;
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

} // namespace ork::audio::singularity
