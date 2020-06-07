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
    _scalar    = 1.0f;
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
    auto prg                           = std::make_shared<ProgramData>();
    zpmDB->_programs[outd->_lastprg++] = prg;
    prg->_role                         = "fm4";

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

    prg->_name = namebuf;

    auto fm4pd     = std::make_shared<Fm4ProgData>();
    auto layerdata = prg->newLayer();

    printf("////////////////////////////\n");
    printf("V<%d:%s>\n", iv, namebuf);

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
    auto& feedback_param   = ops_block->param(8);
    feedback_param._coarse = (FBL == 0) ? 0 : powf(2.0, FBL - 7);
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

    printf("TRPS<%d> PBR<%d>\n", TRPS, PBR);

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
      int op_base = opindex * 10;

      auto& pitch_param = ops_block->param(0 + opindex);
      auto& amp_param   = ops_block->param(4 + opindex);

      const int kop[] = {3, 1, 2, 0};
      int op          = kop[opindex];
      auto& opd       = fm4pd->_ops[op];

      // ratio 0.5 .. 27.57

      int atkRate     = bytes[op_base + 0]; // EG 0..31
      int dec1Rate    = bytes[op_base + 1]; // EG 0..31
      int dec2Rate    = bytes[op_base + 2]; // EG 0..31
      int relRate     = bytes[op_base + 3]; // EG 0..15
      int dec1Lev     = bytes[op_base + 4]; // EG 0..15
      int levScaling  = bytes[op_base + 5]; // level scaling 0..99
      int _AEK        = bytes[op_base + 6];
      bool opEnable   = !((_AEK & 64) >> 6);              // AME - bool (enable op?)
      int egBiasSensa = (_AEK & 0x18) >> 3;               // eg bias sensitivity 0..7
      int kvSensa     = (_AEK & 0x07);                    // keyvel sensitivity 0..7
      int outLevel    = bytes[op_base + 7];               // out level 0..99
      int coarseFrq   = bytes[op_base + 8] & 0x3f;        // coarse frequency 0..63
      int ratScaling  = (bytes[op_base + 9] & 0x18) >> 3; // rate scaling 0..3
      int detune      = (bytes[op_base + 9] & 7) - 3;     // detune ? -3..3

      int _EFF         = bytes[73 + 2 * opindex] & 0x3f;
      int egShift      = (_EFF & 0x30) >> 4;   // eg shift (off,48,24,12)
      bool fixdfrqmode = (_EFF & 0x08) >> 3;   // fixed mode ? bool
      int fixedRange   = (_EFF & 0x7);         // fixed range: 255,510,1k,2k,4k,8k,16k,32k
                                               // fixed step:  1,  2,  4, 8, 16,32,64, 128
      int _OWF      = bytes[74 + 2 * opindex]; //&0x7f;
      opd._waveform = (_OWF & 0x70) >> 4;      // waveform 0..7
      int fineFrq   = (_OWF & 0xf);            // fine frequency 0..15

      // opd._F   = bytes[op_base + 8];
      // opd._EFF = _EFF;
      // opd._OWF = _OWF;

      ////////////////////////////
      // translate tx operator frequency params
      //  to singularity style params
      //  so we can use singularity modulation
      ////////////////////////////

      if (fixdfrqmode) {
        int index      = (coarseFrq << 2);      //|(fine&3);
        float fixedfrq = float(8 << fixedRange) //
                         + float(index << fixedRange);
        pitch_param._coarse   = frequency_to_midi_note(fixedfrq) * 100.0;
        pitch_param._keyTrack = 0.0; // 0 cents/key

      } else {
        OrkAssert(coarseFrq < 64);

        int keybase = 60 + (middleC - 24);

        float ratio           = compute_ratio(coarseFrq, fineFrq);
        float cents           = linear_freq_ratio_to_cents(ratio);
        pitch_param._coarse   = keybase + cents * 0.01; // middlec*ratio
        pitch_param._fine     = float(detune) * 2.6 / 3.0;
        pitch_param._keyTrack = 100.0; // 100 cents/key
      }

      ////////////////////////////
      // modulation Index
      ////////////////////////////
      constexpr int op_mitltab[20] = {
          127, 122, 118, 114, 110, 107, 104, 102, //
          100, 98,  96,  94,  92,  90,  88,  86,  //
          85,  84,  82,  81,
      };

      int tlval = (outLevel > 19) //
                      ? 99 - outLevel
                      : op_mitltab[outLevel];

      // float MI = (4.0f*pi2) *  powf(2.0,(-tlval/8.0f));
      // opd._modIndex = (4.0f*512.0f) *  powf(2.0,(-tlval/8.0f));
      opd._modIndex = 2.0f * powf(2.0, (-tlval / 8.0f));

      ////////////////////////////////////////////////////////////////////////
      // Operator Amplitude Control
      ////////////////////////////////////////////////////////////////////////

      printf("opEnable<%d:%d>\n", opindex, int(opEnable));

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
        float atktime = model.transform(atkRate / 31.0);
        float dc1time = model.transform(dec1Rate / 31.0) * ddec;
        float dc2time = model.transform(dec2Rate / 31.0);
        float reltime = model.transform(relRate / 15.0);

        printf("egShift<%d> levsca<%d> ATK<%g> DC1<%g> DC2<%g> REL<%g>\n", egShift, levScaling, atktime, dc1time, dc2time, reltime);

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
        ENVELOPE->_segments.push_back({dc1time, levelshift(decaylevl), 1.0}); // atk2
        ENVELOPE->_segments.push_back({dc2time, levelshift(0), 1.0});         // dec
        ENVELOPE->_segments.push_back({reltime, levelshift(0), 1.0});         // rel1

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
          EnvPoint outp       = inp;
          float unit_keyscale = float(KOI._key - 24) / 67.0f;
          unit_keyscale       = std::clamp(unit_keyscale, 0.0f, 1.0f);
          float power         = 1.0 / pow(2.0, unit_keyscale * 2.0);
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

        OPAMP->_oncompute = [outLevel, levScaling](CustomControllerInst* cci) { //
          const auto& koi = cci->_layer->_koi;

          float inplevel = float(outLevel / 99.0f);
          inplevel       = std::clamp(inplevel, 0.0f, 1.0f);
          inplevel       = powf(inplevel, 2.0);

          float unit_levscale = float(levScaling / 99.0f);
          unit_levscale       = std::clamp(unit_levscale, 0.0f, 1.0f);
          unit_levscale       = powf(unit_levscale, 2.0);
          float unit_keyscale = float(koi._key - 24) / 67.0f;
          unit_keyscale       = std::clamp(unit_keyscale, 0.0f, 1.0f);
          float lindbscale    = unit_keyscale * unit_levscale;
          float atten         = decibel_to_linear_amp_ratio(lindbscale * -12.0f);

          if (0)
            printf(
                "inplevel<%g> unit_levscale<%g> unit_keyscale<%g> atten<%g>\n", //
                inplevel,
                unit_levscale,
                unit_keyscale,
                atten);

          /////////////////////////////////

          cci->_curval = inplevel * atten;
        };

        ////////////////////////////////////////////////////////////////////////
        amp_param._coarse = 0.0f;
        if (opEnable) {
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
}

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
