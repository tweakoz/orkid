#include <stdio.h>
#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include <ork/file/file.h>
#include <stdint.h>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/dspblocks.h>

using namespace ork;
namespace ork::audio::singularity {

///////////////////////////////////////////
struct ratelevmodel {
  float _inpbias   = 0.0f;
  float _inpscale  = 0.0f;
  float _inpdiv    = 0.0f;
  float _rorbias   = 0.0f;
  float _basenumer = 0.0f;
  float _power     = 0.0f;
  float _scalar    = 0.0f;
  void txmodel() {
    _inpbias   = 0.01f;
    _inpscale  = 89.9f;
    _inpdiv    = 1.0f;
    _rorbias   = 0.0f;
    _basenumer = 1.0f;
    _power     = 1.0f;
    _scalar    = 0.5f;
  }
  float transform(float value) {
    float slope    = _inpbias + (value * _inpscale) / _inpdiv;
    float ror      = env_slope2ror(slope, _rorbias);
    float computed = powf(_basenumer / ror, _power) * _scalar;
    return std::clamp(computed, 0.004f, 250.0f);
  }
};

static const float openv_declevels[16] = {
    0,
    .0625,
    .125,
    .25,
    .35,
    .4,
    .45,
    .5,
    .55,
    .6,
    .65,
    .7,
    .75,
    .8,
    .9,
    1,
};

////////////////////////////////////////////////////////////////////////////////
// Compact TX81z frequiency ratio decoding
// https://mgregory22.me/tx81z/freqratios.html
////////////////////////////////////////////////////////////////////////////////

static const std::vector<int> group_to_coarse = {
    0, 4, 8,  10, 13, 16, 19, 22, 25, 28, 31, 34, 36, 40, 42, 45, // #group
    1, 5, 9,  14, 18, 23, 26, 30, 35, 39, 43, 46, 49, 52, 55, 58, // #group
    2, 6, 11, 15, 20, 24, 29, 33, 38, 44, 48, 50, 53, 56, 59, 61, // #group
    3, 7, 12, 17, 21, 27, 32, 37, 41, 47, 51, 54, 57, 60, 62, 63  // #group
};

struct RatioCoef {
  float _a;
  float _b;
};

static const RatioCoef coeffs[4] = {
    {0.50, 0.0625},   // #group 0
    {0.71, 0.088105}, // # 0.50 + 0.21,  0.0625 + 0.025605 #group 1
    {0.78, 0.098145}, //# 0.71 + 0.07, 0.088105 + 0.01004 #group 2
    {0.87, 0.108105}, // # 0.78 + 0.09, 0.098145 + 0.00996 #group 3
};

float compute_ratio(int coarse, int fine) {
  if (coarse < 4 and fine >= 8)
    return 1.0f;
  int skip = (coarse >= 4) ? 8 : 0;
  auto iti = std::find(group_to_coarse.begin(), group_to_coarse.end(), coarse);
  OrkAssert(iti != group_to_coarse.end());
  int coarse_index = iti - group_to_coarse.begin();
  int group        = int(coarse_index / 16);
  int order        = (coarse_index - group * 16) * 16 - skip + fine;
  const auto& coef = coeffs[group];
  return coef._a + coef._b * order;
}

////////////////////////////////////////////////////////////////////////////////

void parse_tx81z(Tx81zData* outd, const file::Path& path) {

  ork::File syxfile(path, ork::EFM_READ);
  u8* data    = nullptr;
  size_t size = 0;
  syxfile.Load((void**)(&data), size);

  printf("tx81z syxfile<%s> loaded size<%d>\n", path.c_str(), int(size));

  auto zpmDB       = outd->_bankdata;
  int programcount = 0;
  int prgbase      = 0;
  int prgsiz       = 128;

  switch (size) {
    case 4104:
      OrkAssert(data[0] == 0xf0 and data[1] == 0x43);
      programcount = 32;
      prgbase      = 6;
      break;
    case 4096:
      programcount = 32;
      prgbase      = 0;
      break;
    case 8200:
      programcount = 64;
      prgbase      = 0;
      break;
    case 23948:
      programcount = 187;
      prgbase      = 0;
      break;
    default:
      OrkAssert(false);
      break;
  }

  for (int iv = 0; iv < programcount; iv++) {
    int progid               = outd->_lastprg++;
    auto prg                 = std::make_shared<ProgramData>();
    zpmDB->_programs[progid] = prg;
    prg->_role               = "fm4";

    ///////////////////////////
    // collect bytes for program
    ///////////////////////////
    std::vector<u8> bytes;
    {
      size_t bytesperprog = prgsiz;
      int base            = prgbase + (iv * bytesperprog);
      for (int i = 0; i < bytesperprog; i++)
        bytes.push_back(data[base + i]);
    }
    hexdumpbytes(bytes);

    ///////////////////////////

    char namebuf[11];
    for (int n = 0; n < 10; n++)
      namebuf[n] = bytes[57 + n];
    namebuf[10] = 0;

    auto name = std::regex_replace(std::string(namebuf), std::regex("^ +| +$|( ) +"), "$1");
    // remove leading and trailing spaces from patch name
    prg->_name = name;

    zpmDB->_programsByName[name] = prg;

    auto fm4pd     = std::make_shared<Fm4ProgData>();
    auto layerdata = prg->newLayer();

    printf("////////////////////////////\n");
    printf("V<%d:%s>\n", progid, name.c_str());

    ////////////
    // voice global
    ////////////

    configureTx81zAlgorithm(layerdata, fm4pd);
    auto ops_stage = layerdata->stageByName("OPS");
    auto ops_block = ops_stage->_blockdatas[0];

    int ALG = bytes[40] & 7;           // fm algoorithm 0..7
    int FBL = (bytes[40] & 0x38) >> 3; // fb level  0..7
    int SY  = (bytes[40] & 0x60) >> 6; // lfo sync (reset phase) bool

    fm4pd->_alg     = ALG;
    fm4pd->_lfoSync = SY;

    ///////////////////////////////
    // 2.0 == 4PI (7)
    // 1.0 == 2PI (6)
    // 1/2 == PI (5)
    // 1/4 == PI/2 (4)
    // 1/8 == PI/4 (3)
    // 1/16 == PI/8 (2)
    // 1/32 == PI/16 (1)
    auto& feedback_param = ops_block->param(8);
    // feedback_param._coarse = 0.0; //(FBL == 0) ? 0 : powf(2.0, FBL - 16);
    feedback_param._coarse = 0.3 * exp(log(2) * (double)(FBL - 7));
    ///////////////////////////////

    // printf( "ALG<%d> FBL<%d> SY<%d>\n", ALG, FBL, SY);

    int LFS = bytes[41]; // lfo speed 0..99
    int LFD = bytes[42]; // lfo depth 0..99
    int PMD = bytes[43]; // pch mod depth 0..99
    int AMD = bytes[44]; // amp mod depth 0..99

    fm4pd->_lfoSpeed = LFS;
    fm4pd->_lfoDepth = LFD;
    fm4pd->_pchDepth = PMD;
    fm4pd->_ampDepth = AMD;

    // printf( "LFS<%d> LFD<%d> PMD<%d> AMD<%d>\n", LFS, LFD, PMD, AMD);

    int _LAP = bytes[45];
    int LFW  = _LAP & 3;        // 0..3 lfo wave (sawup,squ,tri,shold)
    int AMS  = (_LAP >> 2) & 3; // lfo amp mod sensa 0..3
    int PMS  = (_LAP >> 5);     // lfo pch mod sensa 0..7

    fm4pd->_lfoWave  = LFW;
    fm4pd->_ampSensa = AMS;
    fm4pd->_pchSensa = PMS;

    // printf( "LFW<%d> AMS<%d> PMS<%d>\n", LFW, AMS, PMS);

    int TRPS = bytes[46] & 0x3f; // transpose 0..48 (middle-c)
    int PBR  = bytes[47] & 0x0f; // pitchbendrange 0..12

    int middleC            = TRPS;
    fm4pd->_pitchBendRange = PBR;

    printf("prog<%s> TRPS<%d> PBR<%d> ALG<%d> FBL<%d>\n", name.c_str(), TRPS, PBR, ALG, FBL);

    bool CH = bytes[48] & 0x10;
    bool MO = bytes[48] & 0x08; // mono mode ?
    bool SU = bytes[48] & 0x04;
    bool PO = bytes[48] & 0x02; // porto mode ?
    bool PM = bytes[48] & 0x01;

    fm4pd->_mono     = MO;
    fm4pd->_portMode = PO;

    // printf( "CH<%d> MO<%d> SU<%d> PO<%d> PM<%d>\n", CH,MO,SU,PO,PM);

    int PORT = bytes[49] & 0x3f; // portotime 0..99
                                 // printf( "PORT<%d>\n", PORT);

    fm4pd->_portRate = PORT;

    ////////////
    // per-operator data
    ////////////

    for (int opindex = 0; opindex < 4; opindex++) {
      const int src_op[] = {3, 1, 2, 0};
      // const int src_op[] = {0, 2, 1, 3};
      int op_base = src_op[opindex] * 10;
      // 4 2 3 1
      // 3 1 2 0
      // const int kop[]     = {0, 2, 1, 3};
      // const int kop[] = {0, 1, 2, 3};

      auto& opd         = fm4pd->_ops[opindex];
      auto& pitch_param = ops_block->param(0 + opindex);
      auto& amp_param   = ops_block->param(4 + opindex);

      // ratio 0.5 .. 27.57

      int atkRate      = bytes[op_base + 0]; // EG 0..31
      int dec1Rate     = bytes[op_base + 1]; // EG 0..31
      int dec2Rate     = bytes[op_base + 2]; // EG 0..31
      int relRate      = bytes[op_base + 3]; // EG 0..15
      int dec1Lev      = bytes[op_base + 4]; // EG 0..15
      int levScaling   = bytes[op_base + 5]; // level scaling 0..99
      int _AEK         = bytes[op_base + 6];
      bool opEnable    = !((_AEK & 64) >> 6);              // AME - bool (enable op?)
      int egBiasSensa  = (_AEK & 0x18) >> 3;               // eg bias sensitivity 0..7
      int keyvelsense  = (_AEK & 0x07);                    // keyvel sensitivity 0..7
      int outLevel     = bytes[op_base + 7] & 0x7f;        // out level 0..99
      int coarseFrq    = bytes[op_base + 8] & 0x3f;        // coarse frequency 0..63
      int ratScaling   = (bytes[op_base + 9] & 0x18) >> 3; // rate scaling 0..3
      uint8_t usdetune = (bytes[op_base + 9] & 7);         // detune ? -3..3
      int detune       = int(usdetune & 3);
      if (usdetune & 7)
        detune = -detune;

      int op_additional_base = 73 + src_op[opindex] * 2;

      int _EFF         = bytes[op_additional_base] & 0x3f;
      int egShift      = (_EFF & 0x30) >> 4;        // eg shift (off,48,24,12)
      bool fixdfrqmode = (_EFF & 0x08) >> 3;        // fixed mode ? bool
      int fixedRange   = (_EFF & 0x7);              // fixed range: 255,510,1k,2k,4k,8k,16k,32k
                                                    // fixed step:  1,  2,  4, 8, 16,32,64, 128
      int _OWF     = bytes[op_additional_base + 1]; //&0x7f;
      int waveform = (_OWF & 0x70) >> 4;            // waveform 0..7
      int fineFrq  = (_OWF & 0xf);                  // - 7;              // fine frequency 0..15

      // float fol = powf(float(outLevel) / 99.0f, 3.5);
      // outLevel  = std::clamp(int(fol * 99.0), 0, 99);

      printf("prog<%s> op<%d> waveform<%d> level<%d> enable<%d>\n", name.c_str(), opindex, waveform, outLevel, int(opEnable));

      if (opindex < 2) {
        // outLevel = 0;
      }

      opd._waveform = waveform;

      ////////////////////////////
      // translate tx operator frequency params
      //  to singularity style params
      //  so we can use singularity modulation
      ////////////////////////////

      if (fixdfrqmode) {
        float detcents = float(detune) * 2.6 / 3.0;
        float det_rat  = cents_to_linear_freq_ratio(detcents);
        int frqindex   = (coarseFrq << 2) | fineFrq;
        float fixedfrq = float(frqindex << (fixedRange)) * det_rat;

        // fixedfrq += fineFrq * finestep;
        // fixedfrq *= det_rat;

        pitch_param._coarse   = frequency_to_midi_note(fixedfrq);
        pitch_param._keyTrack = 0.0; // 0 cents/key

        printf(
            "prog<%s> op<%d> fixed range<%d> coarseFrq<%d> fineFrq<%d> index<%d> fixedfrq<%g> detune<%d> cents<%g>\n", //
            name.c_str(),
            opindex,
            fixedRange,
            coarseFrq,
            fineFrq,
            frqindex,
            fixedfrq,
            detune,
            pitch_param._coarse * 100.0);

      } else {
        OrkAssert(coarseFrq < 64);

        int keybase = 60 + (middleC - 24);

        float ratio           = compute_ratio(coarseFrq, fineFrq);
        float cents           = linear_freq_ratio_to_cents(ratio);
        pitch_param._coarse   = keybase + cents * 0.01; // middlec*ratio
        pitch_param._fine     = float(detune) * 5.6 / 3.0;
        pitch_param._keyTrack = 100.0; // 100 cents/key

        printf(
            "prog<%s> op<%d> ratio<%g> cents<%g>\n", //
            name.c_str(),
            opindex,
            ratio,
            cents);
      }

      ////////////////////////////////////////////////////////////////////////
      // Operator Amplitude Control
      ////////////////////////////////////////////////////////////////////////

      if (true) {

        auto envname       = ork::FormatString("op%d-env", opindex);
        auto opaname       = ork::FormatString("op%d-amp", opindex);
        auto ENVELOPE      = layerdata->appendController<RateLevelEnvData>(envname);
        auto OPAMP         = layerdata->appendController<CustomControllerData>(opaname);
        ENVELOPE->_ampenv  = true; //(opindex==0);
        ENVELOPE->_envType = RlEnvType::ERLTYPE_DEFAULT;

        float decaylevl = openv_declevels[dec1Lev];
        float ddec      = fabs(1.0f - decaylevl);
        ratelevmodel model;
        model.txmodel();

        ///////////////////////////////////////////////
        // convert exponential decay rate to decay time
        ///////////////////////////////////////////////

        auto expdecayrate2time = [](float decrate,           // decay rate: frac/sec
                                    float deslev) -> float { // desired linear level
          return logf(deslev) / logf(decrate);               // return: time to reach deslev
        };

        auto procrate = [](float inprate, float a, float b) -> float {
          if (inprate == 0.0f)
            return 1.0;
          else {
            float dt    = a * expf(b * inprate);
            float alpha = -logf(2.0f) / dt;
            return expf(alpha);
          }
        };

        ///////////////////////////////////////////////

        float atktime = 10.4423f * expf(-0.353767f * atkRate);
        float dc1time = expdecayrate2time(procrate(dec1Rate, 9.8f, -0.356f), 0.001f);
        float dc2time = expdecayrate2time(procrate(dec2Rate, 9.8f, -0.356f), 0.001f);
        float reltime = expdecayrate2time(procrate(relRate, 8.0f, -0.65f), 0.001f);
        if (reltime > 1.0f) {
          reltime = powf(reltime, 0.7f);
        }
        printf(
            "prog<%s> egShift<%d> levsca<%d> ATK<%g> DC1<%g> DC2<%g> REL<%g>\n", //
            name.c_str(),
            egShift,
            levScaling,
            atktime,
            dc1time,
            dc2time,
            reltime);

        // todo (should eqShift mapping be done post eg ?)
        auto levelshift = [egShift](float inp) -> float { //
          switch (egShift) {
            case 0:
              return inp;
              break;
            case 1: {
              float lo = decibel_to_linear_amp_ratio(-12);
              return lerp(lo, 1.0, inp);
              break;
            }
            case 2: {
              float lo = decibel_to_linear_amp_ratio(-24);
              return lerp(lo, 1.0, inp);
              break;
            }
            case 3: {
              float lo = decibel_to_linear_amp_ratio(-48);
              return lerp(lo, 1.0, inp);
              break;
            }
          }
          return inp;
        };

        ENVELOPE->_sustainSegment = 2;
        ENVELOPE->_releaseSegment = 3;
        // todo (attack-logshape - how interacts with egshift?)
        ENVELOPE->_segments.push_back({atktime, levelshift(1), 0.5});         // atk1
        ENVELOPE->_segments.push_back({dc1time, levelshift(decaylevl), 0.5}); // atk2
        ENVELOPE->_segments.push_back({dc2time, levelshift(0), 0.5});         // dec
        ENVELOPE->_segments.push_back({reltime, levelshift(0), 0.5});         // rel1

        //////////////////////////////////////////////////
        // rate scaling (envelope rate key follow)
        //  When RS is 0, the envelope will be the same
        //  time length for all notes. When RS is 3,
        //  high notes will have a shorter envelope
        // c1(24) .. g6(91) (91-24==67)
        //////////////////////////////////////////////////

        ENVELOPE->_envadjust = [=](const EnvPoint& inp, //
                                   int iseg,
                                   const KeyOnInfo& KOI) -> EnvPoint { //
          EnvPoint outp = inp;
          switch (iseg) {
            case 0: // attack
              break;
            case 1: // decay
              outp._time = powf(outp._time, 1.5f);
              break;
            case 2: // decay2
              outp._time = powf(outp._time, 1.5f);
              break;
            case 3: // release
              outp._time = powf(outp._time, 1.2f);
              break;
          }

          float unit_keyscale = float(KOI._key - 24) / 67.0f;
          unit_keyscale       = std::clamp(unit_keyscale, 0.0f, 1.0f);
          float power         = 1.0 / pow(1.4, unit_keyscale * 2.0);
          outp._time *= power;
          return outp;
        };

        //////////////////////////////////////////////////
        // level scaling (operator amplitude key follow)
        //  Level scaling operates on a curve from about c1.
        //  When LS is 0, the operator output level will be
        //  the same for all notes. When LS is 99 the opertaor
        //  level will have dropped to 0 by the time you get to G6
        // c1(24) .. g6(91) (91-24==67)
        //////////////////////////////////////////////////

        float fkeyvelsense = expf(float(-keyvelsense) * logf(2.0f));

        OPAMP->_oncompute = [name, //
                             outLevel,
                             levScaling,
                             pitch_param,
                             fkeyvelsense](CustomControllerInst* cci) { //
          const auto& koi = cci->_layer->_koi;

          //////////////////////////////////
          // map base operator level
          //////////////////////////////////

          float a       = logf(2.0f) * 0.1f;
          float b       = 90.0f * a;
          float baselev = expf(a * float(outLevel) - b);

          //////////////////////////////////
          // velocity scaling
          //////////////////////////////////

          float op_key   = pitch_param._coarse;
          float velocity = koi._vel;
          float velamp   = (fkeyvelsense + (1.0f - fkeyvelsense) * (velocity / 127.0f));

          //////////////////////////////////
          // key scaling
          //////////////////////////////////

          float unit_levscale = float(levScaling / 99.0f);
          unit_levscale       = std::clamp(unit_levscale, 0.0f, 1.0f);
          unit_levscale       = powf(unit_levscale, 2.0);
          float unit_keyscale = float(op_key - 24) / 67.0f;
          unit_keyscale       = std::clamp(unit_keyscale, 0.0f, 1.0f);
          float lindbscale    = unit_keyscale * unit_levscale;
          float keyamp        = decibel_to_linear_amp_ratio(lindbscale * -24.0f);

          //////////////////////////////////
          // final level
          //////////////////////////////////

          float final_amp = baselev * velamp * keyamp;

          //////////////////////////////////

          if (0)
            printf(
                "prog<%s> outLevel<%d> final_amp<%g> velamp<%g> unit_levscale<%g> unit_keyscale<%g> keyamp<%g>\n", //
                name.c_str(),
                outLevel,
                final_amp,
                velamp,
                unit_levscale,
                unit_keyscale,
                keyamp);

          /////////////////////////////////

          cci->_curval = final_amp;
        };

        ////////////////////////////////////////////////////////////////////////
        amp_param._coarse = 0.0f;
        if (true) { // opEnable) {
          auto funname               = ork::FormatString("op%d-fun", opindex);
          auto FUN                   = layerdata->appendController<FunData>(funname);
          FUN->_a                    = envname;
          FUN->_b                    = opaname;
          FUN->_op                   = "a*b";
          amp_param._mods._src1      = FUN;
          amp_param._mods._src1Depth = 1.0;
        }
      }
      // printf( "OP<%d>\n", op );
      // printf( "    AR<%d> D1R<%d> D2R<%d> RR<%d> D1L<%d>\n", AR,D1R,D2R,RR,D1L);
      // printf( "    AME<%d> EBS<%d> KVS<%d>\n", AME,EBS,KVS);
      // printf( "    OUT<%d> WAV<%d>\n", OUT, WAV);
      // printf( "    FCOA<%d> FFIN<%d> FIX<%d> FIXRG<%d>\n", FCOA, FFIN, FIX, FIXRG );
      // printf( "    RS<%d> DBT<%d>\n", RS, DBT);
    }
  }
} // namespace ork::audio::singularity

Tx81zData::Tx81zData()
    : SynthData()
    , _lastprg(0) {
}

Tx81zData::~Tx81zData() {
}
void Tx81zData::loadBank(const file::Path& syxpath) {
  parse_tx81z(this, syxpath);
}

} // namespace ork::audio::singularity
