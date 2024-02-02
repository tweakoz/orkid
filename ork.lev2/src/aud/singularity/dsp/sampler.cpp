////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// #include <audiofile.h>
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
  // addParam("pch")->usePitchEvaluator();
}

dspblk_ptr_t SAMPLER_DATA::createInstance() const { // override
  return std::make_shared<SAMPLER>(this);
}

///////////////////////////////////////////////////////////////////////////////

SAMPLER::SAMPLER(const DspBlockData* dbd)
    : DspBlock(dbd) {
  auto sampler_data = dynamic_cast<const SAMPLER_DATA*>(dbd);
  _spOsc            = new sampleOsc(sampler_data);
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

SampleData::SampleData()
    : _sampleBlock(nullptr)
    , _blk_start(0)
    , _blk_alt(0)
    , _blk_loopstart(0)
    , _blk_loopend(0)
    , _blk_end(0)
    , _loopPoint(0)
    , _subid(0)
    , _sampleRate(0.0f)
    , _linGain(1.0f)
    , _rootKey(0)
    , _highestPitch(0)
    , _loopMode(eLoopMode::NOTSET) {
}

///////////////////////////////////////////////////////////////////////////////
/*
void sample::load(const std::string& fname)
{
    printf( "loading sample<%s>\n", fname.c_str() );

    auto af_file = afOpenFile(fname.c_str(), "r", nullptr);


    _numFrames = afGetFrameCount(af_file, AF_DEFAULT_TRACK);
    float frameSize = afGetVirtualFrameSize(af_file, AF_DEFAULT_TRACK, 1);
    int channelCount = afGetVirtualChannels(af_file, AF_DEFAULT_TRACK);
    _sampleRate = afGetRate(af_file, AF_DEFAULT_TRACK);

    int sampleFormat, sampleWidth;

    afGetVirtualSampleFormat(af_file, AF_DEFAULT_TRACK, &sampleFormat,
        &sampleWidth);

    int numbytes = _numFrames*frameSize;

    printf( "frameCount<%d>\n", _numFrames );
    printf( "frameSize<%f>\n", frameSize );
    printf( "numbytes<%d>\n", numbytes );


    auto buffer = malloc(numbytes);
    int count = afReadFrames(af_file, AF_DEFAULT_TRACK, buffer, _numFrames);
    printf( "readcount<%d>\n", count );

    _data = (s16*) buffer;

    ////////////////////////////
    // get loop
    ////////////////////////////

    int numloopids = afGetLoopIDs(af_file, AF_DEFAULT_INST, NULL);
    printf( "numloopids<%d>\n", numloopids );

    auto loopids = new int[numloopids];
    afGetLoopIDs(af_file, AF_DEFAULT_INST, loopids);
    for (int i=0; i<numloopids; i++)
        printf( "loopid<%d> : %d\n", i, loopids[i] );

    int nummkrids = afGetMarkIDs(af_file, AF_DEFAULT_TRACK, NULL);
    printf( "nummkrids<%d>\n", nummkrids );
    auto mkrids = new int[nummkrids];
    afGetMarkIDs(af_file, AF_DEFAULT_TRACK, mkrids);
    for (int i=0; i<nummkrids; i++)
    {
        const char* mkrname = afGetMarkName(af_file, AF_DEFAULT_TRACK, mkrids[i]);
        auto mkrpos = afGetMarkPosition(af_file, AF_DEFAULT_TRACK, mkrids[i]);

        printf( "mkrid<%d> : %d : pos<%d> name<%s>\n", i, mkrids[i], int(mkrpos), mkrname );
    }

    int lpa_startMKR = afGetLoopStart(af_file, AF_DEFAULT_INST, 1);
    int lpa_endMKR = afGetLoopEnd(af_file, AF_DEFAULT_INST, 1);

    printf( "loopa_markers start<%d> end<%d>\n", lpa_startMKR,lpa_endMKR );

    auto lptrack1 = afGetLoopTrack(af_file, AF_DEFAULT_INST, 1);

    int lpstartfr = afGetLoopStartFrame(af_file, AF_DEFAULT_INST, 1);
    int lpendfr = afGetLoopEndFrame(af_file, AF_DEFAULT_INST, 1);

    _loopPoint = lpstartfr;

    printf( "loopa_frames start<%d> end<%d>\n", lpstartfr,lpendfr );

    afCloseFile(af_file);

}*/

///////////////////////////////////////////////////////////////////////////////

sampleOsc::sampleOsc(const SAMPLER_DATA* data)
    : _sampler_data(data)
    , _lyr(nullptr)
    , _active(false)
    , _pbindex(0.0f)
    , _pbindexNext(0.0f)
    , _pbincrem(0.0f)
    , _curratio(1.0f)
    , _loopMode(eLoopMode::NONE)
    , _loopCounter(0) {

  _natAmpEnv = std::make_shared<NatEnv>();
}

void sampleOsc::setSrRatio(float pbratio) {
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

void sampleOsc::keyOn(const KeyOnInfo& koi) {
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
    printf("sampleOsc no sample!\n");
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
      _pbFunc = &sampleOsc::playLoopBid;
      break;
    case eLoopMode::FWD:
      _pbFunc = &sampleOsc::playLoopFwd;
      break;
    case eLoopMode::NONE:
      _pbFunc = &sampleOsc::playNoLoop;
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
      _pbFunc = &sampleOsc::playLoopBid;
      break;
    case eLoopMode::FWD:
      _pbFunc = &sampleOsc::playLoopFwd;
      break;
    case eLoopMode::NONE:
      _pbFunc = &sampleOsc::playNoLoop;
      break;
    case eLoopMode::NOTSET:
      if (_pbFunc == nullptr)
        _pbFunc = &sampleOsc::playNoLoop;
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
  // printf("sampleBlock<%p>\n", (void*) sample->_sampleBlock);
  // printf("st<%d> en<%d>\n", sample->_blk_start, sample->_blk_end);
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

void sampleOsc::keyOff() {

  _released = true;
  // printf("osc<%p> beginRelease\n", (void*) this);

  if (_enableNatEnv)
    _natAmpEnv->keyOff();
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

  if (region) {
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

    //float SRratio = synth::instance()->sampleRate() / sampsr;
    float SRratio = 96000.0f / sampsr;
    int RKcents   = (RFOUND._sampleRoot) * 100;
    int delcents  = highestP - RKcents;
    int frqerc    = linear_freq_ratio_to_cents(SRratio);
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
    printf( "sampsr<%f> srrat<%f> rkc<%d> hp<%d> delc<%d> frqerc<%d> pitchadj<%d> pitchadjx<%d> bascents<%g>\n", sampsr, SRratio, RKcents, highestP,  delcents, frqerc, pitchadj_cents, pitchadjx_cents, RFOUND._baseCents );

    //sampsr<88100.000000> srrat<1.089671> rkc<6000> hp<20000> delc<14000> frqerc<148> pitchadj<0> pitchadjx<-13852> bascents<-9752>
    //pitcheval<0x14f11a8f8:pitch> _keyTrack<0> kr<-0> course<0.000000> fine<0> c1<0> c2<0> ko<-7> vt<0> totcents<0.000000> rat<1.000000>

    //sampsr<88100.000000> srrat<1.089671> rkc<6000> hp<20000> delc<14000> frqerc<148> pitchadj<0> pitchadjx<-13852> bascents<-9752>
    //pitcheval<0x14f11a8f8:pitch> _keyTrack<0> kr<-0> course<0.000000> fine<0> c1<0> c2<0> ko<-7> vt<0> totcents<0.000000> rat<1.000000>

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
    printf("no region found\n");
  }
  return RFOUND;
}

///////////////////////////////////////////////////////////////////////////////

void sampleOsc::updateFreqRatio() {
  int cents_at_root = (_regionsearch._sampleRoot * 100);
  int delta_cents   = _curcents - cents_at_root;
  _samppbnote       = _regionsearch._sampleRoot + (delta_cents / 100);
  _curSampSRratio   = cents_to_linear_freq_ratio(delta_cents);
}

///////////////////////////////////////////////////////////////////////////////

void sampleOsc::compute(int inumfr) {

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

float sampleOsc::playNoLoop() {
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
  float samp  = (sampB * fract + sampA * invfr) * kinv32k;

  ///////////////

  _pbindex = _pbindexNext;

  return samp;
}

///////////////////////////////////////////////////////////////////////////////

float sampleOsc::playLoopFwd() {
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

  ///////////////
  // linear
  auto sblk = sample->_sampleBlock;
  OrkAssert(sblk != nullptr);
  float sampA = float(sblk[iiA]);
  float sampB = float(sblk[iiB]);
  float samp  = (sampB * fract + sampA * invfr) * kinv32k;
  ///////////////
  // printf("iiA<%zd> iiB<%zd> sampA<%g> sampB<%g> samp<%g>\n", iiA, iiB, sampA, sampB, samp);
  ///////////////
  // cosine
  // float mu2 = (1.0f-cos(fract*pi))*0.5f;
  // float samp = (sampA*(1.0f-mu2)+sampB*mu2)*kinv32k;
  ///////////////

  ///////////////
  // cubic
  // int64_t iiC = iiB+1;
  // if( iiC > (_blk_loopend>>16) )
  //    iiC = (_blk_loopstart>>16);
  // int64_t iiD = iiC+1;
  // if( iiD > (_blk_loopend>>16) )
  //    iiD = (_blk_loopstart>>16);
  // float sampC = float(sblk[iiC] );
  // float sampD = float(sblk[iiD] );
  // float mu = fract;
  // float mu2 = mu*mu;
  // float a0 = sampD - sampC - sampA + sampB;
  // float a1 = sampA - sampB - a0;
  // float a2 = sampC - sampA;
  // float a3 = sampB;
  // float samp = (a0*mu*mu2+a1*mu2+a2*mu+a3)*kinv32k;
  ///////////////

  _pbindex = _pbindexNext;

  return samp;
}

float sampleOsc::playLoopBid() {
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
