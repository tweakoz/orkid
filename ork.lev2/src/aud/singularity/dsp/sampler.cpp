////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <sndfile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::SAMPLER_DATA, "DspSampler");
ImplementReflectionX(ork::audio::singularity::KmRegionData, "SynKmRegionData");
ImplementReflectionX(ork::audio::singularity::KeyMapData, "SynKeyMapData");
ImplementReflectionX(ork::audio::singularity::SampleData, "SynSampleData");
ImplementReflectionX(ork::audio::singularity::MultiSampleData, "SynMultiSampleData");

namespace ork::audio::singularity {

void SAMPLER_DATA::describeX(class_t* clazz) {
}
void KmRegionData::describeX(class_t* clazz) {
}
void KeyMapData::describeX(class_t* clazz) {
}
void SampleData::describeX(class_t* clazz) {
}
void MultiSampleData::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

kmpblockdata_ptr_t KmpBlockData::clone() const {
  auto rval          = std::make_shared<KmpBlockData>();
  rval->_transpose   = _transpose;
  rval->_timbreShift = _timbreShift;
  rval->_keyTrack    = _keyTrack;
  rval->_velTrack    = _velTrack;
  if (_keymap) {
    rval->_keymap = _keymap->clone();
  }
  rval->_pbMode = _pbMode;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

kmregion_ptr_t KeyMapData::getRegion(int note, int vel) const {
  int k2vel = vel / 18; // map 0..127 -> 0..7

  for (auto r : _regions) {
    // printf( "testing region<%p> for note<%d>\n", r, note );
    // printf( "lokey<%d>\n", r->_lokey );
    // printf( "hikey<%d>\n", r->_hikey );
    // printf( "lovel<%d>\n", r->_lovel );
    // printf( "hivel<%d>\n", r->_hivel );
    if (note >= r->_lokey && note <= r->_hikey) {
      if (k2vel >= r->_lovel && k2vel <= r->_hivel) {
        return r;
      }
    }
  }

  return nullptr;
}

kmregion_ptr_t KmRegionData::clone() const {
  auto rval               = std::make_shared<KmRegionData>();
  rval->_lokey            = _lokey;
  rval->_hikey            = _hikey;
  rval->_lovel            = _lovel;
  rval->_hivel            = _hivel;
  rval->_tuning           = _tuning;
  rval->_linGain          = _linGain;
  rval->_loopModeOverride = _loopModeOverride;
  rval->_sample           = _sample;
  rval->_multiSample      = _multiSample;
  rval->_sampID           = _sampID;
  rval->_multsampID       = _multsampID;
  rval->_volAdj           = _volAdj;
  rval->_sampleName       = _sampleName;

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

keymap_ptr_t KeyMapData::clone() const {
  auto rval = std::make_shared<KeyMapData>();
  for (auto r : _regions) {
    auto rclone = r->clone();
    rval->_regions.push_back(rclone);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

SAMPLER_DATA::SAMPLER_DATA(std::string name)
    : DspBlockData(name) {
  _blocktype = "SAMPLER";
  _lowpassfrq = 14000.0f;
  // addParam("pch")->usePitchEvaluator();
}

dspblk_ptr_t SAMPLER_DATA::createInstance() const { // override
  return std::make_shared<SAMPLER>(this);
}

///////////////////////////////////////////////////////////////////////////////

SAMPLER::SAMPLER(const DspBlockData* dbd)
    : DspBlock(dbd) {
  auto sampler_data = dynamic_cast<const SAMPLER_DATA*>(dbd);
  _spOsc            = new SampleOscillator(sampler_data);
}

///////////////////////////////////////////////////////////////////////////////

void SAMPLER::doKeyOn(const KeyOnInfo& koi) { // final
  // NatEnvWrapperInst
  auto L = koi._layer;
  auto A = L->_alg;
  auto S = A->_stageblock._stages[0];
  auto C = L->_ctrlBlock;
  OrkAssert(C);
  NatEnvWrapperInst* nat = nullptr;
  for (auto ci : C->_cinst) {
    auto as_nat = dynamic_cast<NatEnvWrapperInst*>(ci);
    if (as_nat) {
      nat = as_nat;
      break;
    }
  }
  if (nat) {
    _spOsc->_natenvwrapperinst = nat;
  }
  _spOsc->keyOn(koi);
}

///////////////////////////////////////////////////////////////////////////////

void SAMPLER::doKeyOff() { // final
  _spOsc->keyOff();
}

///////////////////////////////////////////////////////////////////////////////
/* sampler/keymap specific pitch setup
if (auto PCHBLK = _layerdata->_pchBlock) {
  const int kNOTEC4 = 60;
  const auto& PCH   = PCHBLK->_paramd[0];
  const auto& KMP   = _layerdata->_kmpBlock;

  int timbreshift = KMP->_timbreShift;                // 0
  int kmtrans     = KMP->_transpose; //+timbreshift; // -20
  int kmkeytrack  = KMP->_keyTrack;                   // 100

  int kmpivot      = (kNOTEC4 + kmtrans);            // 48-20 = 28
  int kmdeltakey   = (_curnote + kmtrans - kmpivot); // 48-28 = 28
  int kmdeltacents = kmdeltakey * kmkeytrack;        // 8*0 = 0
  int kmfinalcents = (kmpivot * 100) + kmdeltacents; // 4800

  int pchtrans      = PCH._coarse;                      //-timbreshift; // 8
  int pchkeytrack   = PCH._keyTrack;                    // 0
  int pchpivot      = (kNOTEC4 + pchtrans);             // 48-0 = 48
  int pchdeltakey   = (_curnote + pchtrans - pchpivot); // 48-48=0 //possible minus kmorigin?
  int pchdeltacents = pchdeltakey * pchkeytrack;        // 0*0=0
  int pchfinalcents = (pchtrans * 100) + pchdeltacents; // 0*100+0=0

  int kmcents = kmfinalcents; //+region->_tuning;
  // printf( "_layerBasePitch<%d>\n", int(_layerBasePitch) );

  //_pchBlock = _layerdata->_pchBlock->create();

  if (_pchBlock) {
    float centoff          = _pchBlock->_param[0].eval();
    _curPitchOffsetInCents = int(centoff); // kmcents+pchfinalcents;
  }
  _layerBasePitch = kmcents + pchfinalcents + _curPitchOffsetInCents;

} else {
*/
void SAMPLER::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;

  _spOsc->compute(inumframes);

  for (int i = 0; i < inumframes; i++) {
    float outp = _spOsc->_OUTPUT[i];
    lbuf[i]    = outp;
    ubuf[i]    = outp;
  }
}

///////////////////////////////////////////////////////////////////////////////

SampleData::SampleData(){}

///////////////////////////////////////////////////////////////////////////////

void SampleData::loadFromAudioFile(const std::string& fname, bool normalize) {
    //printf("loading sample<%s>\n", fname.c_str());

    // Open the sound file
    SF_INFO sfinfo;
    SNDFILE *sf_file = sf_open(fname.c_str(), SFM_READ, &sfinfo);
    if (sf_file == nullptr) {
        printf("Error opening file\n");
        return; // Early return on error
    }

    // Initialize variables based on the sound file info
    _blk_start = 0;
    _blk_end = sfinfo.frames;
    int channelCount = sfinfo.channels;
    _sampleRate = sfinfo.samplerate;

    float highestPitch = _originalPitch * 48000.0f / _sampleRate;
    float highestPitchN = frequency_to_midi_note(highestPitch); // Assuming this function is defined elsewhere
    float highestPitchCents = static_cast<int>(highestPitchN * 100.0f) + 1.0f;
    _highestPitch = static_cast<int>(highestPitchCents);

    // Calculate the number of samples to read (frames * channels)
    int numSamples = static_cast<int>(_blk_end * channelCount);
    //printf("frameCount<%lld>\n", _blk_end);
    //printf("channelCount<%d>\n", channelCount);
    //printf("numSamples<%d>\n", numSamples);
    //printf("sampleRate<%f>\n", _sampleRate);
    _numChannels = channelCount;
    // Read the samples from the file

    std::vector<float> fbuf(numSamples); // Assuming float format for simplicity
    int readcount = sf_readf_float(sf_file, fbuf.data(), _blk_end);
    //printf("readCount<%d>\n", readcount);

    // No need to manually convert formats, libsndfile handles conversion

    if(normalize){
      // Normalization and bias correction
      float _min = *std::min_element(fbuf.begin(), fbuf.end());
      float _max = *std::max_element(fbuf.begin(), fbuf.end());
      //printf("_min<%g> _max<%g>\n", _min, _max);

      float frange = _max - _min;
      float fbias = (_max + _min) * 0.5f;
      for (auto& F : fbuf) {
          F -= fbias;
          F /= (frange * 0.5f);
      }
      //printf("frange<%f> fbias<%f>\n", frange, fbias);  
    }

    // Assuming WaveformData and _user are defined and initialized elsewhere
    auto& waveformOUT = _user.make<WaveformData>();
    waveformOUT._sampledata.resize(_blk_end * channelCount); // Assuming 16-bit output, correct size calculation needed
    for (int i = 0; i < _blk_end * channelCount; i++) {
        waveformOUT._sampledata[i] = static_cast<int16_t>(fbuf[i] * 32767.0f);
    }
    _sampleBlock = waveformOUT._sampledata.data();

    //printf("closing..\n");
    sf_close(sf_file);
}

///////////////////////////////////////////////////////////////////////////////

RegionSearch SAMPLER_DATA::findRegion(lyrdata_constptr_t ld, const KeyOnInfo& koi) const {

  auto KMP = ld->_kmpBlock;

  auto PCHBLK = ld->_pchBlock;
  OrkAssert(PCHBLK);
  const auto& PCH = PCHBLK->_paramd[0];

  int note = koi._key;

  /////////////////////////////////////////////

  auto km = ld->_keymap;
  if (nullptr == km) {
    RegionSearch not_found;
    printf("no keymap!\n");
    return not_found;
  }

  RegionSearch RFOUND;
  RFOUND._kmregion = nullptr;

  ///////////////////////////////////////

  const int kNOTEC4 = 60;

  int timbreshift = KMP->_timbreShift;                // 0
  int kmtrans     = KMP->_transpose /*+timbreshift*/; // -20
  int kmkeytrack  = KMP->_keyTrack;                   // 100
  int pchkeytrack = PCH->_keyTrack;                   // 0
  // expect 48-20+8 = 28+8 = 36*100 = 3600 total cents

  int kmpivot      = (kNOTEC4 + kmtrans);            // 48-20 = 28
  int kmdeltakey   = (note + kmtrans - kmpivot);     // 48-28 = 28
  int kmdeltacents = kmdeltakey * kmkeytrack;        // 8*0 = 0
  int kmfinalcents = (kmpivot * 100) + kmdeltacents; // 4800

  RFOUND._sampselnote = (kmfinalcents / 100) + timbreshift; // 4800/100=48

  int pchtrans      = PCH->_coarse - timbreshift;       // 8
  int pchpivot      = (kNOTEC4 + pchtrans);             // 48-0 = 48
  int pchdeltakey   = (note + pchtrans - pchpivot);     // 48-48=0 //possible minus kmorigin?
  int pchdeltacents = pchdeltakey * pchkeytrack;        // 0*0=0
  int pchfinalcents = (pchtrans * 100) + pchdeltacents; // 0*100+0=0

  auto region      = km->getRegion(RFOUND._sampselnote, 64);
  RFOUND._kmregion = region;

  if (region and region->_sample) {
    ///////////////////////////////////////
    auto sample        = region->_sample;
    float sampsr       = sample->_sampleRate;
    int highestP       = sample->_highestPitch;
    RFOUND._sampleRoot = sample->_rootKey;
    RFOUND._keydiff    = note - RFOUND._sampleRoot;
    ///////////////////////////////////////

    RFOUND._kmcents  = kmfinalcents + region->_tuning;
    RFOUND._pchcents = pchfinalcents;

    ///////////////////////////////////////

    // float SRratio = synth::instance()->sampleRate() / sampsr;
    float SRratio       = 96000.0f / sampsr;
    int RKcents         = (RFOUND._sampleRoot) * 100;
    int delcents        = highestP - RKcents;
    int frqerc          = linear_freq_ratio_to_cents(SRratio);
    int pitchadjx_cents = (frqerc - delcents);
    int pitchadj_cents  = sample->_pitchAdjust;

    // if( SRratio<3.0f )
    //    pitchadj >>=1;

    RFOUND._curpitchadj  = pitchadj_cents;
    RFOUND._curpitchadjx = pitchadjx_cents;

    RFOUND._baseCents = RFOUND._kmcents + pitchadjx_cents;
    RFOUND._baseCents -= 1200.0f;

    //_basecentsOSC = 6000;//(note-0)*100;//pitchadjx_cents-1200;
    if (pitchadj_cents) {
      RFOUND._baseCents = RFOUND._kmcents + pitchadjx_cents + pitchadj_cents;
      //_basecentsOSC = _pchcents+pitchadj ;
    }
    if(0)printf(
        "sampsr<%f> srrat<%f> rkc<%d> hp<%d> delc<%d> frqerc<%d> pitchadj<%d> pitchadjx<%d> bascents<%g>\n",
        sampsr,
        SRratio,
        RKcents,
        highestP,
        delcents,
        frqerc,
        pitchadj_cents,
        pitchadjx_cents,
        RFOUND._baseCents);

    // sampsr<88100.000000> srrat<1.089671> rkc<6000> hp<20000> delc<14000> frqerc<148> pitchadj<0> pitchadjx<-13852>
    // bascents<-9752> pitcheval<0x14f11a8f8:pitch> _keyTrack<0> kr<-0> course<0.000000> fine<0> c1<0> c2<0> ko<-7> vt<0>
    // totcents<0.000000> rat<1.000000>

    // sampsr<88100.000000> srrat<1.089671> rkc<6000> hp<20000> delc<14000> frqerc<148> pitchadj<0> pitchadjx<-13852>
    // bascents<-9752> pitcheval<0x14f11a8f8:pitch> _keyTrack<0> kr<-0> course<0.000000> fine<0> c1<0> c2<0> ko<-7> vt<0>
    // totcents<0.000000> rat<1.000000>

    // float outputPAD = decibel_to_linear_amp_ratio(F4._inputPad);
    // float ampCOARSE = decibel_to_linear_amp_ratio(F4._coarse);

    //_totalGain = sample->_linGain* region->_linGain; // * _layerGain * outputPAD * ampCOARSE;

    RFOUND._preDSPGAIN = sample->_linGain * region->_linGain * 4.0;

    ///////////////////////////////////////
    //  trigger sample playback oscillator
    ///////////////////////////////////////

    RFOUND._sample = sample;
    // printf("region found<%s> root<%d> keydiff<%d> cents<%f> preDSPGAIN<%f>\n", region->_sampleName.c_str(), RFOUND._sampleRoot,
    // RFOUND._keydiff, RFOUND._baseCents, RFOUND._preDSPGAIN); _spOsc->keyOn(_curSampSRratio);
  } else {
    //printf("no region found\n");
  }
  return RFOUND;
}

///////////////////////////////////////////////////////////////////////////////

SampleOscillator::SampleOscillator(const SAMPLER_DATA* data)
    : _sampler_data(data)
    , _lyr(nullptr)
    , _active(false)
    , _pbindex(0.0f)
    , _pbindexNext(0.0f)
    , _pbincrem(0.0f)
    , _curratio(1.0f)
    , _loopMode(eLoopMode::NONE)
    , _loopCounter(0)
    , _lpFilter(8) {

  _lpFilter2A.set(2000.0f);
  _lpFilter2B.set(2000.0f);
  for( int i=0; i<4; i++ ){
    _bq[i].Clear();
    _bq[i].SetLpf(data->_lowpassfrq);
  }

  _natAmpEnv = std::make_shared<NatEnv>();
}

void SampleOscillator::setSrRatio(float pbratio) {
  _curratio   = pbratio;
  auto sample = _regionsearch._sample;
  if (sample) {
    _playbackRate = sample->_sampleRate * _curratio;
    _pbincrem     = (_dt * _playbackRate * 65536.0f);
  } else {
    _playbackRate = 0.0f;
    _pbincrem     = 0.0f;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SampleOscillator::keyOn(const KeyOnInfo& koi) {
  int note = koi._key;

  _lyr = koi._layer;
  OrkAssert(_lyr);

  _regionsearch = _sampler_data->findRegion(_lyr->_layerdata, koi);
  _curcents     = _regionsearch._baseCents;
  updateFreqRatio();
  auto& HKF     = _lyr->_HKF;
  HKF._kmregion = _regionsearch._kmregion;

  float pbratio = this->_curSampSRratio;

  pbratio *= 0.5f;

  lyrdata_constptr_t ld = _lyr->_layerdata;

  auto sample = _regionsearch._sample;

  if (nullptr == sample) {
    //printf("SampleOscillator no sample!\n");
    return;
  }

  OrkAssert(sample);

  _blk_start     = int64_t(sample->_blk_start) << 16;
  _blk_alt       = int64_t(sample->_blk_alt) << 16;
  _blk_loopstart = int64_t(sample->_blk_loopstart) << 16;
  _blk_loopend   = int64_t(sample->_blk_loopend) << 16;
  _blk_end       = int64_t(sample->_blk_end) << 16;

  _pbindex     = _blk_start;
  _pbindexNext = _blk_start;

  _pbincrem = 0;
  _dt       = synth::instance()->_dt;

  _loopMode = sample->_loopMode;

  switch (_loopMode) {
    case eLoopMode::BIDIR:
      _pbFunc = &SampleOscillator::playLoopBid;
      break;
    case eLoopMode::FWD:
      _pbFunc = &SampleOscillator::playLoopFwd;
      break;
    case eLoopMode::NONE:
      _pbFunc = &SampleOscillator::playNoLoop;
      break;
    case eLoopMode::FROMKM:
      _pbFunc = nullptr;
      break;
    default:
      OrkAssert(false);
      break;
  }
  switch (_regionsearch._kmregion->_loopModeOverride) {
    case eLoopMode::BIDIR:
      _pbFunc = &SampleOscillator::playLoopBid;
      break;
    case eLoopMode::FWD:
      _pbFunc = &SampleOscillator::playLoopFwd;
      break;
    case eLoopMode::NONE:
      _pbFunc = &SampleOscillator::playNoLoop;
      break;
    case eLoopMode::NOTSET:
      if (_pbFunc == nullptr)
        _pbFunc = &SampleOscillator::playNoLoop;
      break;
    default:
      OrkAssert(false);
      break;
  }
  // printf( "LOOPMODE<%d>\n", int(_loopMode));
  // printf( "LOOPMODEOV<%d>\n", int(_kmregion->_loopModeOverride));

  _synsr = getSampleRate();

  /*04-05 Pitch at Highest Playback Rate: unsigned word (affected by
         change in Root Key Number and Pitch Adjust) -- value is
         some kind of computed highest pitch plus Pitch Adjust
         entered on Sample Editor MISC page: value in cents*/

  setSrRatio(pbratio);

  // printf( "osc<%p> sroot<%d> SR<%d> ratio<%f> PBR<%d> looped<%d>\n", this, sample->_rootKey, int(sample->sampleRate),
  // _curratio, int(_playbackRate), int(_isLooped) );
  // printf("sample<%s>\n", sample->_name.c_str());
  //printf("sampler::SAMPLEBLOCK<%p>\n", (void*) sample->_sampleBlock);
  //printf("sampler::SAMPLEBLOCK st<%d> en<%d>\n", sample->_blk_start, sample->_blk_end);
  // printf("lpst<%d> lpend<%d>\n", sample->_blk_loopstart, sample->_blk_loopend);
  _active = true;

  _forwarddir = true;

  _loopCounter = 0;
  _released    = false;

  _enableNatEnv = ld->_usenatenv;

  // printf("_enableNatEnv<%d>\n", int(_enableNatEnv));

  if (_enableNatEnv) {
    // probably should explicity create a NatEnv controller
    //  and bind it to AMP
    //_lyr->_AENV = _NATENV;
    _natAmpEnv->keyOn(koi, sample);
  }
}

///////////////////////////////////////////////////////////////////////////////

void SampleOscillator::keyOff() {

  _released = true;
  // printf("osc<%p> beginRelease\n", (void*) this);

  if (_enableNatEnv)
    _natAmpEnv->keyOff();
}

///////////////////////////////////////////////////////////////////////////////

void SampleOscillator::updateFreqRatio() {
  int cents_at_root = (_regionsearch._sampleRoot * 100);
  int delta_cents   = _curcents - cents_at_root;
  _samppbnote       = _regionsearch._sampleRoot + (delta_cents / 100);
  _curSampSRratio   = cents_to_linear_freq_ratio(delta_cents);
}

///////////////////////////////////////////////////////////////////////////////

void SampleOscillator::compute(int inumfr) {

  _curcents = _regionsearch._baseCents //
              + _lyr->_curPitchOffsetInCents;
  _curcents = clip_float(_curcents, -0, 12700);

  if (0)
    printf(
        "_baseCents<%f> offs<%f> _curcents<%d>\n", //
        _regionsearch._baseCents,                  //
        _lyr->_curPitchOffsetInCents,              //
        _curcents);

  if (false == _active) {
    for (int i = 0; i < inumfr; i++) {
      _OUTPUT[i] = 0.0f;
    }
    return;
  }

  auto sample = _regionsearch._sample;

  for (int i = 0; i < inumfr; i++) {

    updateFreqRatio();
    setSrRatio(_curSampSRratio);

    // _spOsc->setSrRatio(currat);

    // float lyrpocents = _lyr._curPitchOffsetInCents;

    // float ratio = cents_to_linear_freq_ratio(_keyoncents+lyrpocents);

    _playbackRate = sample->_sampleRate * _curratio;

    _pbincrem = (_dt * _playbackRate * 65536.0f);

    /////////////////////////////

    float sampleval = _pbFunc ? (this->*_pbFunc)() : 0.0f;

    if (_pbFunc) {
      // printf("sampleval<%g>\n", sampleval);
    } else {
      printf("sampleval no_pbFunc\n");
    }
    // float sampleval = std::invoke(this, _pbFunc);

    _OUTPUT[i] = sampleval;

    float natval = _natAmpEnv->compute();
    _NATENV[i]   = natval;

    if (_natenvwrapperinst) {
      _lyr->_ampenvgain = 0.0f; // natval;
      _OUTPUT[i] *= natval;
      _natenvwrapperinst->_value.x = natval;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

float SampleOscillator::playNoLoop() {
  _pbindexNext = _pbindex + _pbincrem;

  auto sample = _regionsearch._sample;

  ///////////////

  float fract = float(_pbindex & 0xffff) * kinv64k;
  float invfr = 1.0f - fract;

  ///////////////

  int64_t iiA = (_pbindex >> 16);
  if (iiA > (_blk_end >> 16))
    iiA = (_blk_end >> 16);

  int64_t iiB = iiA + 1;
  if (iiB > (_blk_end >> 16))
    iiB = (_blk_end >> 16);

  ///////////////
  auto sblk = sample->_sampleBlock;

  float sampA = float(sblk[iiA]);
  float sampB = float(sblk[iiB]);
  float sampA_filtered = _lpFilter2A.compute(sampA);
  float sampB_filtered = _lpFilter2B.compute(sampB);
  float samp  = (sampB * fract + sampA * invfr) * kinv32k;
  //printf("fract<%g> sampA<%g> sampB<%g> samp<%g>\n", fract, sampA_filtered, sampB_filtered, samp);
  ///////////////

  _pbindex = _pbindexNext;

  return _bq[3].compute(
          _bq[2].compute(
              _bq[1].compute(
                  _bq[0].compute(samp))));
}

///////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////

float SampleOscillator::playLoopFwd() {
  _pbindexNext = _pbindex + _pbincrem;
  auto sample  = _regionsearch._sample;

  bool did_loop = false;

  if ((_pbindexNext >> 16) > (_blk_loopend >> 16)) {
    // printf( "reached _blk_loopend<%d>\n", int(_blk_loopend>>16));

    int64_t over = (_pbindexNext - _blk_loopend) - (1 << 16);
    _pbindexNext = _blk_loopstart + over;
    did_loop     = true;
    _loopCounter++;
  }

  ///////////////

  float fract = float(_pbindex & 0xffff) * kinv64k;
  float invfr = 1.0f - fract;

  ///////////////

  int64_t iiA = (_pbindex >> 16);

  int64_t iiB = iiA + 1;
  if (iiB > (_blk_loopend >> 16))
    iiB = (_blk_loopstart >> 16);

  // printf( "iia<%d> lpstart<%d> lpend<%d>\n", iiA,int(_blk_loop>>16),int(_blk_end>>16));

  float samp = 0.0f;

  auto sblk = sample->_sampleBlock;
  OrkAssert(sblk != nullptr);
  float sampA = float(sblk[iiA]);
  float sampB = float(sblk[iiB]);
  float sampA_filtered = _lpFilter.process(sampA);
  float sampB_filtered = _lpFilter.process(sampB);

  switch (sample->_interpMethod) {
    case 0: {
      ///////////////
      // linear
      ///////////////
      samp = (sampB * fract + sampA * invfr) * kinv32k;
      break;
    }
    case 1: {
      ///////////////
      // cosine
      ///////////////
      float mu2 = (1.0f - cos(fract * pi)) * 0.5f;
      samp      = (sampA * (1.0f - mu2) + sampB * mu2) * kinv32k;
      break;
    }
    case 2: {
      ///////////////
      // cubic
      ///////////////
      int64_t iiC = iiB + 1;
      if (iiC > (_blk_loopend >> 16))
        iiC = (_blk_loopstart >> 16);
      int64_t iiD = iiC + 1;
      if (iiD > (_blk_loopend >> 16))
        iiD = (_blk_loopstart >> 16);
      //float sampC = float(sblk[iiC]);
      //float sampD = float(sblk[iiD]);
      float sampC_filtered = _lpFilter.process(float(sblk[iiC]));
      float sampD_filtered = _lpFilter.process(float(sblk[iiD]));
      float mu    = fract;
      float mu2   = mu * mu;
      float a0    = sampD_filtered - sampC_filtered - sampA_filtered + sampB_filtered;
      float a1    = sampA_filtered - sampB_filtered - a0;
      float a2    = sampC_filtered - sampA_filtered;
      float a3    = sampB_filtered;
      samp        = (a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3) * kinv32k;
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
  ///////////////
  // printf("iiA<%zd> iiB<%zd> sampA<%g> sampB<%g> samp<%g>\n", iiA, iiB, sampA, sampB, samp);

  ///////////////

  _pbindex = _pbindexNext;

  return _bq[3].compute(
          _bq[2].compute(
              _bq[1].compute(
                  _bq[0].compute(samp))));
}

float SampleOscillator::playLoopBid() {
  OrkAssert(false);
  return 0.0f;
  /*
  _pbindexNext = _forwarddir
               ? _pbindex + _pbincrem
               : _pbindex - _pbincrem;


  bool did_loop = false;

  if( _forwarddir && int(_pbindexNext) > (_numFrames-1) )
  {
      _pbindexNext = _pbindexNext - _pbincrem;
      printf( "LoopedBIDIR (F)->(B) : _loopPoint<%d> _numFrames<%d> _pbindexNext<%f>\n", (int)_loopPoint, _numFrames, _pbindexNext
  );

      did_loop = true;

      _forwarddir = false;

  }
  else if( (!_forwarddir) && int(_pbindexNext) < _loopPoint )
  {
      _pbindexNext = _pbindexNext + _pbincrem;
      printf( "LoopedBIDIR (B)->(F) : _loopPoint<%d> _numFrames<%d> _pbindexNext<%f>\n", (int)_loopPoint, _numFrames, _pbindexNext
  );

      did_loop = true;

      _forwarddir = true;
  }

  ///////////////

  double fract = double(int64_t(_pbindex*65536.0)&0xffff)/65536.0;
  double whole = _pbindex-fract;

  int iiA = int(whole);
  if( iiA >= (_numFrames-1) )
  {
      if( _isLooped )
          iiA = _numFrames-2;
  }

  int iiB = int(whole+1.0);
  if( iiB >= _numFrames )
  {
      iiB = _loopPoint;
      printf( "yo\n");
  }

  OrkAssert(iiA<_numFrames);
  OrkAssert(iiB<_numFrames);
  float sampA = float(_sampleData[iiA] );
  float sampB = float(_sampleData[iiB] );

  //if( false == _forwarddir )
  //	std::swap(sampA,sampB);

  float samp = (sampB*fract+sampA*(1.0-fract))/32768.0;

  if( did_loop )
      printf( "iiA<%d> sampA<%f> iiB<%d> sampB<%f>\n", iiA, sampA, iiB, sampB );
  ///////////////

  _pbindex = _pbindexNext;

  return samp;*/
}

} // namespace ork::audio::singularity
