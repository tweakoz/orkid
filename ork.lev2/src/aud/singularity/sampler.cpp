//#include <audiofile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/sampler.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

kmregion* KeyMap::getRegion(int note, int vel) const {
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

///////////////////////////////////////////////////////////////////////////////

void SAMPLER::initBlock(dspblkdata_ptr_t blockdata) {
  blockdata->_blocktype = "SAMPLER";
  blockdata->_paramd[0].usePitchEvaluator();
}

///////////////////////////////////////////////////////////////////////////////

SAMPLER::SAMPLER(const DspBlockData* dbd)
    : DspBlock(dbd) {
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
  float centoff = _param[0].eval();
  _fval[0]      = centoff;

  int inumframes = _layer->_dspwritecount;
  float* lbuf    = getOutBuf(dspbuf, 1) + _layer->_dspwritebase;
  float* ubuf    = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  // float lyrcents = _layer->_layerBasePitch;
  // float cin = (lyrcents+centoff)*0.01;
  // float frq = midi_note_to_frequency(cin);
  // float SR = _layer->_syn._sampleRate;
  // float pad = _dbd->_inputPad;

  //_filtp = 0.5*_filtp + 0.5*centoff;
  //_layer->_curPitchOffsetInCents = centoff;
  // printf( "centoff<%f>\n", centoff );
  _spOsc.compute(inumframes);

  for (int i = 0; i < inumframes; i++) {
    float outp = _spOsc._OUTPUT[i];
    lbuf[i]    = outp;
    ubuf[i]    = outp;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SAMPLER::doKeyOn(const DspKeyOnInfo& koi) // final
{
  _spOsc.keyOn(koi);
}

///////////////////////////////////////////////////////////////////////////////

void SAMPLER::doKeyOff() // final
{
  _spOsc.keyOff();
}

///////////////////////////////////////////////////////////////////////////////

sample::sample()
    : _sampleBlock(nullptr)
    , _loopPoint(0)
    , _subid(0)
    , _sampleRate(0.0f)
    , _rootKey(0)
    , _loopMode(eLoopMode::NOTSET)
    , _linGain(1.0f)
    , _highestPitch(0)
    , _blk_start(0)
    , _blk_alt(0)
    , _blk_loopstart(0)
    , _blk_loopend(0)
    , _blk_end(0) {
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

sampleOsc::sampleOsc()
    : _lyr(nullptr)
    , _sample(nullptr)
    , _active(false)
    , _pbindex(0.0f)
    , _pbindexNext(0.0f)
    , _pbincrem(0.0f)
    , _curratio(1.0f)
    , _sampleRoot(0)
    , _kmregion(nullptr)
    , _curcents(0)
    , _baseCents(0)
    , _preDSPGAIN(1.0f)
    , _loopMode(eLoopMode::NONE)
    , _loopCounter(0) {
}

void sampleOsc::setSrRatio(float pbratio) {
  _curratio = pbratio;
  if (_sample) {
    _playbackRate = _sample->_sampleRate * _curratio;
    _pbincrem     = (_dt * _playbackRate * 65536.0f);
  } else {
    _playbackRate = 0.0f;
    _pbincrem     = 0.0f;
  }
}

///////////////////////////////////////////////////////////////////////////////

void sampleOsc::keyOn(const DspKeyOnInfo& koi) {
  int note = koi._key;

  _lyr = koi._layer;
  assert(_lyr);

  findRegion(koi);

  float pbratio = this->_curSampSRratio;

  pbratio *= 0.5f;

  lyrdata_constptr_t ld = _lyr->_layerdata;

  if (nullptr == _sample) {
    printf("sampleOsc no sample!\n");
    return;
  }

  assert(_sample);

  _blk_start     = int64_t(_sample->_blk_start) << 16;
  _blk_alt       = int64_t(_sample->_blk_alt) << 16;
  _blk_loopstart = int64_t(_sample->_blk_loopstart) << 16;
  _blk_loopend   = int64_t(_sample->_blk_loopend) << 16;
  _blk_end       = int64_t(_sample->_blk_end) << 16;

  _pbindex     = _blk_start;
  _pbindexNext = _blk_start;

  _pbincrem = 0;
  _dt       = synth::instance()->_dt;

  _loopMode = _sample->_loopMode;

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
      assert(false);
      break;
  }
  switch (_kmregion->_loopModeOverride) {
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
      assert(false);
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

  // printf( "osc<%p> sroot<%d> SR<%d> ratio<%f> PBR<%d> looped<%d>\n", this, _sample->_rootKey, int(_sample->_sampleRate),
  // _curratio, int(_playbackRate), int(_isLooped) );
  printf("sample<%s>\n", _sample->_name.c_str());
  printf("sampleBlock<%p>\n", _sample->_sampleBlock);
  printf("st<%d> en<%d>\n", _sample->_blk_start, _sample->_blk_end);
  printf("lpst<%d> lpend<%d>\n", _sample->_blk_loopstart, _sample->_blk_loopend);
  _active = true;

  _forwarddir = true;

  _loopCounter = 0;
  _released    = false;

  _enableNatEnv = ld->_usenatenv;

  printf("_enableNatEnv<%d>\n", int(_enableNatEnv));

  if (_enableNatEnv) {
    OrkAssert(false);
    // probably should explicity create a NatEnv controller
    //  and bind it to AMP
    //_lyr->_AENV = _NATENV;
    _natAmpEnv.keyOn(koi, _kmregion->_sample);
  }
}

///////////////////////////////////////////////////////////////////////////////

void sampleOsc::keyOff() {

  _released = true;
  printf("osc<%p> beginRelease\n", this);

  if (_enableNatEnv)
    _natAmpEnv.keyOff();
}

///////////////////////////////////////////////////////////////////////////////

void sampleOsc::findRegion(const DspKeyOnInfo& koi) {
  auto ld  = _lyr->_layerdata;
  auto KMP = ld->_kmpBlock;

  auto PCHBLK = ld->_pchBlock;
  assert(PCHBLK);
  const auto& PCH = PCHBLK->_paramd[0];

  auto& HKF = _lyr->_HKF;

  int note = koi._key;

  _kmregion = nullptr;

  /////////////////////////////////////////////

  auto km = ld->_keymap;
  if (nullptr == km)
    return;

  ///////////////////////////////////////

  const int kNOTEC4 = 60;

  int timbreshift = KMP->_timbreShift;                // 0
  int kmtrans     = KMP->_transpose /*+timbreshift*/; // -20
  int kmkeytrack  = KMP->_keyTrack;                   // 100
  int pchkeytrack = PCH._keyTrack;                    // 0
  // expect 48-20+8 = 28+8 = 36*100 = 3600 total cents

  int kmpivot      = (kNOTEC4 + kmtrans);            // 48-20 = 28
  int kmdeltakey   = (note + kmtrans - kmpivot);     // 48-28 = 28
  int kmdeltacents = kmdeltakey * kmkeytrack;        // 8*0 = 0
  int kmfinalcents = (kmpivot * 100) + kmdeltacents; // 4800

  // printf( "kmtrans<%d>\n", kmtrans);
  // printf( "timbreshift<%d>\n", timbreshift);
  // printf( "kmpivot<%d>\n", kmpivot);
  // printf( "kmdeltakey<%d>\n", kmdeltakey);
  // printf( "kmdeltacents<%d>\n", kmdeltacents);

  _sampselnote = (kmfinalcents / 100) + timbreshift; // 4800/100=48

  int pchtrans      = PCH._coarse - timbreshift;        // 8
  int pchpivot      = (kNOTEC4 + pchtrans);             // 48-0 = 48
  int pchdeltakey   = (note + pchtrans - pchpivot);     // 48-48=0 //possible minus kmorigin?
  int pchdeltacents = pchdeltakey * pchkeytrack;        // 0*0=0
  int pchfinalcents = (pchtrans * 100) + pchdeltacents; // 0*100+0=0

  auto region = km->getRegion(_sampselnote, 64);

  // printf( "layer<%d> region<%p> kmkeytrack<%d> \n", _lyr->_ldindex, region, kmkeytrack );
  // printf( "note<%d> kmfinalcents<%d> pchfinalcents<%d>\n", note, kmfinalcents, pchfinalcents );
  if (region) {
    HKF._kmregion = region;

    _kmregion = region;

    ///////////////////////////////////////
    auto sample  = region->_sample;
    float sampsr = sample->_sampleRate;
    int highestP = sample->_highestPitch;
    _sampleRoot  = sample->_rootKey;
    _keydiff     = note - _sampleRoot;
    ///////////////////////////////////////

    _kmcents  = kmfinalcents + region->_tuning;
    _pchcents = pchfinalcents;

    ///////////////////////////////////////

    float SRratio = 96000.0f / sampsr;
    int RKcents   = (_sampleRoot)*100;
    int delcents  = highestP - RKcents;
    int frqerc    = linear_freq_ratio_to_cents(SRratio);
    int pitchadjx = (frqerc - delcents);  //+1200;
    int pitchadj  = sample->_pitchAdjust; //+1200;

    // if( SRratio<3.0f )
    //    pitchadj >>=1;
    // printf( "sampsr<%f> srrat<%f> rkc<%d> hp<%d> delc<%d> frqerc<%d> pitchadjx<%d>\n", sampsr, SRratio, RKcents, highestP,
    // delcents, frqerc, pitchadjx );

    _curpitchadj  = pitchadj;
    _curpitchadjx = pitchadjx;

    _baseCents = _kmcents + /*_pchcents*/ +pitchadjx - 1200;
    //_basecentsOSC = 6000;//(note-0)*100;//pitchadjx-1200;
    if (pitchadj) {
      _baseCents = _kmcents + /*_pchcents*/ +pitchadj;
      //_basecentsOSC = _pchcents+pitchadj;
    }
    _curcents = _baseCents;

    // printf( "kmfinalcents<%d>\n", kmfinalcents );
    // printf( "region->_tuning<%d>\n", region->_tuning );
    // printf( "_kmcents<%d>\n", _kmcents );
    // printf( "pitchadjx<%d>\n", pitchadjx );
    // printf( "_baseCents<%f>\n", _baseCents );

    updateFreqRatio();

    // printf( "_curcents<%d> curratio<%f>\n", _curcents, _curratio );

    // float outputPAD = decibel_to_linear_amp_ratio(F4._inputPad);
    // float ampCOARSE = decibel_to_linear_amp_ratio(F4._coarse);

    //_totalGain = sample->_linGain* region->_linGain; // * _layerGain * outputPAD * ampCOARSE;

    _preDSPGAIN = sample->_linGain * region->_linGain;

    ///////////////////////////////////////
    //  trigger sample playback oscillator
    ///////////////////////////////////////

    _sample = sample;
    //_spOsc.keyOn(_curSampSRratio);
  }
}

void sampleOsc::updateFreqRatio() {
  ///////////////////////////////////////

  int cents_at_root = (_sampleRoot * 100);
  int delta_cents   = _curcents - cents_at_root;

  // printf( "DESCENTS<%d>\n", _curcents );
  // printf( "SROOTCENTS<%d>\n", cents_at_root );
  // printf( "DELCENTS<%d>\n", delta_cents );

  _samppbnote = _sampleRoot + (delta_cents / 100);

  _curSampSRratio = cents_to_linear_freq_ratio(delta_cents);
}

///////////////////////////////////////////////////////////////////////////////

void sampleOsc::compute(int inumfr) {
  //_pchc1 = _pchControl1();
  //_pchc2 = _pchControl2();
  //_pchc1 = clip_float( _pchc1, -6400,6400 );
  //_pchc2 = clip_float( _pchc2, -6400,6400 );
  //_curPitchOffsetInCents = _pchc1+_pchc2;

  //_curPitchOffsetInCents = clip_float( _curPitchOffsetInCents, -6400,6400 );

  _curcents = _baseCents + _lyr->_curPitchOffsetInCents;
  _curcents = clip_float(_curcents, -0, 12700);

  // printf( "_baseCents<%f> offs<%f> _curcents<%d>\n",
  //		 _baseCents, _lyr->_curPitchOffsetInCents, _curcents );

  if (false == _active) {
    for (int i = 0; i < inumfr; i++) {
      _OUTPUT[i] = 0.0f;
    }
    return;
  }

  for (int i = 0; i < inumfr; i++) {

    updateFreqRatio();
    setSrRatio(_curSampSRratio);

    // _spOsc.setSrRatio(currat);

    // float lyrpocents = _lyr._curPitchOffsetInCents;

    // float ratio = cents_to_linear_freq_ratio(_keyoncents+lyrpocents);

    _playbackRate = _sample->_sampleRate * _curratio;

    _pbincrem = (_dt * _playbackRate * 65536.0f);

    /////////////////////////////

    float sampleval = _pbFunc ? (this->*_pbFunc)() : 0.0f;

    // float sampleval = std::invoke(this, _pbFunc);

    _OUTPUT[i] = sampleval;
    _NATENV[i] = _natAmpEnv.compute();

    //_lyr->_HAF_nenvseg = _natAmpEnv._curseg;
    // todo update HUD ui of segment change...

    // printf( "_NATENV<%f> sampleval<%f>\n\n", _NATENV[i], sampleval);
  }
}

///////////////////////////////////////////////////////////////////////////////

float sampleOsc::playNoLoop() {
  _pbindexNext = _pbindex + _pbincrem;

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
  auto sblk = _sample->_sampleBlock;

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
  auto sblk = _sample->_sampleBlock;
  assert(sblk != nullptr);
  float sampA = float(sblk[iiA]);
  float sampB = float(sblk[iiB]);
  float samp  = (sampB * fract + sampA * invfr) * kinv32k;
  ///////////////

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
  assert(false);
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

  assert(iiA<_numFrames);
  assert(iiB<_numFrames);
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
