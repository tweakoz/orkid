////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include <ork/file/file.h>
#include <stdint.h>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>

using namespace ork;
namespace ork::audio::singularity {

constexpr float amp_table[100] = {
0.000254, 0.000254, 0.000254, 0.000254, 0.000254,  // 0
0.000254, 0.000254, 0.000254, 0.000254, 0.000254,  // 5
0.000254, 0.000254, 0.000339, 0.000403, 0.000473,  // 10
0.000562, 0.000624, 0.000653, 0.000804, 0.000881,
0.001084, 0.001189, 0.001288, 0.001429, 0.001603,  // 20
0.001758, 0.001862, 0.002018, 0.002265, 0.002427,
0.00263,  0.002917, 0.003199, 0.003508, 0.003846,  // 30
0.004217, 0.004519, 0.004898, 0.00537,  0.005957,
0.006383, 0.007161, 0.007674, 0.008318, 0.009016,  // 40
0.00955,  0.010351, 0.01122,  0.012303, 0.01349,
0.014791, 0.015849, 0.017179, 0.019055, 0.020654,  // 50
0.022909, 0.024831, 0.027227, 0.029174, 0.031989,
0.034674, 0.038019, 0.04217,  0.045709, 0.050119,  // 60
0.054325, 0.058884, 0.063826, 0.069984, 0.075858,
0.083176, 0.090157, 0.096605, 0.108393, 0.116145,  // 70
0.125893, 0.139637, 0.151356, 0.165959, 0.179887,
0.194984, 0.213796, 0.234423, 0.254097, 0.275423,  // 80
0.298538, 0.327341, 0.363078, 0.389045, 0.42658,
0.462381, 0.506991, 0.555904, 0.60256,  0.668344,  // 90
0.724436, 0.776247, 0.851138, 0.933254, 1 
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
  std::vector<u8> bytes;
  syxfile.Load(bytes);
  auto data = bytes.data();
  auto size = bytes.size();

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
    auto zpmprg              = std::make_shared<ProgramData>();
    zpmDB->_programs[progid] = zpmprg;
    zpmprg->_tags            = "fm4";

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
    //hexdumpbytes(bytes);

    ///////////////////////////

    char namebuf[11];
    for (int n = 0; n < 10; n++)
      namebuf[n] = bytes[57 + n];
    namebuf[10] = 0;

    auto name = std::regex_replace(std::string(namebuf), std::regex("^ +| +$|( ) +"), "$1");
    // remove leading and trailing spaces from patch name
    zpmprg->_name = name;

    zpmDB->_programsByName[name] = zpmprg;

    auto fm4pd     = std::make_shared<Tx81zProgData>();
    auto layerdata = zpmprg->newLayer();

    //printf("////////////////////////////\n");
    //printf("V<%d:%s>\n", progid, name.c_str());

    ////////////
    // voice global
    ////////////

    int ALG = bytes[40] & 7;           // fm algoorithm 0..7
    int FBL = (bytes[40] & 0x38) >> 3; // fb level  0..7
    int SY  = (bytes[40] & 0x60) >> 6; // lfo sync (reset phase) bool

    fm4pd->_alg     = ALG;
    fm4pd->_lfoSync = SY;
    fm4pd->_name    = name;

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

    //printf("prog<%s> TRPS<%d> PBR<%d> ALG<%d> FBL<%d>\n", name.c_str(), TRPS, PBR, ALG, FBL);

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

    ///////////////////////////////

    zpmprg->addHudInfo(FormatString("ProgramType: TX81Z"));
    zpmprg->addHudInfo(FormatString("TX-ALG: %d", ALG));
    zpmprg->addHudInfo(FormatString("FBL: %d", FBL));
    zpmprg->addHudInfo(FormatString("TRANSPOSE: %d", TRPS));

    ////////////
    // per-operator data
    ////////////

    configureTx81zAlgorithm(layerdata, fm4pd);
    auto ops_stage = layerdata->stageByName("OPS");
    auto pitch_block = ops_stage->appendTypedBlock<PITCH>("PITCH");
    auto pitch_param = pitch_block->paramByName("pitch");
    pitch_param->_debug = true;
    for (int opindex = 0; opindex < 4; opindex++) {
      const int src_op[]  = {3, 1, 2, 0};
      int op_base         = src_op[opindex] * 10;
      auto ops_block      = ops_stage->_blockdatas[3 - opindex];
      auto as_pmx         = dynamic_cast<PMXData*>(ops_block.get());
      auto& opd           = fm4pd->_ops[opindex];
      auto amp_param      = ops_block->param(0);
      auto feedback_param = ops_block->param(1);
      // feedback_param->_coarse = (FBL == 0) ? 0 : powf(2.0, FBL - 7);
      feedback_param->_coarse = 0.0f; // 0.3 * exp(log(2) * (double)(FBL - 7));
      ///////////////////////////////
      // 2.0 == 4PI (7)
      // 1.0 == 2PI (6)
      // 1/2 == PI (5)
      // 1/4 == PI/2 (4)
      // 1/8 == PI/4 (3)
      // 1/16 == PI/8 (2)
      // 1/32 == PI/16 (1)
      ///////////////////////////////

      // ratio 0.5 .. 27.57

      int atkRate      = bytes[op_base + 0]; // EG 0..31
      int dec1Rate     = bytes[op_base + 1]; // EG 0..31
      int dec2Rate     = bytes[op_base + 2]; // EG 0..31
      int relRate      = bytes[op_base + 3]; // EG 0..15
      int susLev       = bytes[op_base + 4]; // EG 0..15
      int levScaling   = bytes[op_base + 5]; // level scaling 0..99
      int _AEK         = bytes[op_base + 6];
      bool AME         = !((_AEK & 64) >> 6);              // AME - bool (amp modulation for op)
      int EBS          = (_AEK & 0x18) >> 3;               // eg bias sensitivity 0..7
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

      // printf("prog<%s> op<%d> waveform<%d> level<%d> ame<%d> ebs<%d>\n", name.c_str(), opindex, waveform, outLevel, int(AME),
      // EBS);

      opd._waveform = waveform;

      //////////////////////////////////
      // map base operator level
      //////////////////////////////////

      //float a       = logf(2.0f) * 0.08f;
      //float b       = 90.0f * a;
      //float baselev = expf(a * float(outLevel) - b); // / 1.86607;
      //baselev       = powf(baselev, 1.0f);
      if(outLevel>99)
        outLevel = 99;
      float baselev = amp_table[outLevel];
      /*printf(
          "prog<%s> op<%d> outLevel<%d> a<%g> b<%g> baselev<%g>\n", //
          name.c_str(),
          opindex,
          outLevel,
          a,
          b,
          baselev);*/

      ////////////////////////////
      // translate tx operator frequency params
      //  to singularity style params
      //  so we can use singularity modulation
      ////////////////////////////

      using keyprod_t = std::function<float(float inkey)>;

      keyprod_t keyprod = [](float inpkey) -> float { return 0.0f; };

      float detcents = float(detune) * 5.6 / 3.0;
      float det_rat  = cents_to_linear_freq_ratio(detcents);
      if (fixdfrqmode) {
        int frqindex   = (coarseFrq << 2) | fineFrq;
        float fixedfrq = float(frqindex << (fixedRange)) * det_rat;

        // fixedfrq += fineFrq * finestep;
        // fixedfrq *= det_rat;
        float coarse           = frequency_to_midi_note(fixedfrq);
        pitch_param->_coarse   = coarse;
        pitch_param->_keyTrack = 0.0; // 0 cents/key
        keyprod                = [coarse](float inpkey) { return coarse; };

        zpmprg->addHudInfo(FormatString(
            "OP%d waveform<%d> fixed-frequency<%g> outlev<%d> baselev<%g>", //
            opindex,
            waveform,
            fixedfrq,
            outLevel,
            baselev));

      } else {
        OrkAssert(coarseFrq < 64);

        int keybase = 60 + (middleC - 24);
        //int keybase = (middleC - 24);

        float ratio            = compute_ratio(coarseFrq, fineFrq) * det_rat;
        float cents            = linear_freq_ratio_to_cents(ratio);
        pitch_param->_coarse   = keybase + cents * 0.01; // middlec*ratio
        pitch_param->_fine     = float(detune) * 5.6 / 3.0;
        pitch_param->_keyTrack = 100.0;                           // 100 cents/key
        keyprod                = [cents, middleC](float inpkey) { //
          return inpkey + (cents * 0.01);
        };

        zpmprg->addHudInfo(FormatString(
            "OP%d waveform<%d> ratio<%g> outlev<%d> baselev<%g>", //
            opindex,
            waveform,
            ratio,
            outLevel,
            baselev));
      }

      ////////////////////////////////////////////////////////////////////////
      // Operator Amplitude Control
      ////////////////////////////////////////////////////////////////////////

      if (true) {
        auto envname  = ork::FormatString("op%d-env", opindex);
        auto opaname  = ork::FormatString("op%d-amp", opindex);
        //auto ENVELOPE = layerdata->appendController<YmEnvData>(envname);
        auto ENVELOPE = layerdata->appendController<TX81ZEnvData>(envname);
        auto OPAMP    = layerdata->appendController<CustomControllerData>(opaname);

        ///////////////////////////////////////////////

        ENVELOPE->_attackRate = atkRate;

        ENVELOPE->_decay1Rate  = dec1Rate; 
        ENVELOPE->_decay1Level = susLev;
        ENVELOPE->_decay2Rate  = dec2Rate; 
        ENVELOPE->_releaseRate = relRate;
        ENVELOPE->_egshift     = egShift;
        ENVELOPE->_rateScale   = ratScaling;
        ENVELOPE->_levScale   = levScaling;

        zpmprg->addHudInfo(FormatString(
            "OP%d atk<%d> dc1<%d> suslev<%d> dc2<%d> rel<%d> egShift<%d> levsca<%d>", //
            opindex,
            atkRate,
            dec1Rate,
            susLev,
            dec2Rate,
            relRate,
            egShift,
            levScaling));

        //////////////////////////////////////////////////
        // level scaling (operator amplitude key follow)
        //  Level scaling operates on a curve from about c1.
        //  When LS is 0, the operator output level will be
        //  the same for all notes. When LS is 99 the opertaor
        //  level will have dropped to 0 by the time you get to G6
        // c1(24) .. g6(91) (91-24==67)
        //////////////////////////////////////////////////

        float fkeyvelsense = expf(float(-keyvelsense) * logf(2.0f));

        //////////////////////////////////

        OPAMP->_oncompute = [name, //
                             opindex,
                             baselev,
                             levScaling,
                             keyprod,
                             as_pmx,
                             fkeyvelsense](CustomControllerInst* cci) { //
          const auto& koi = cci->_layer->_koi;

          //////////////////////////////////
          // velocity scaling
          //////////////////////////////////

          float velocity = koi._vel;
          float velamp   = fkeyvelsense //
                         + (1.0f - fkeyvelsense) * (velocity / 127.0f);
          // velamp         = powf(velamp, 0.5f);
          // printf("velamp<%g>\n", velamp);

          //////////////////////////////////
          // key scaling
          //////////////////////////////////

          float unit_levscale = float(levScaling / 99.0f);
          unit_levscale       = std::clamp(unit_levscale, 0.0f, 1.0f);

          float op_key         = keyprod(koi._key - 24);
          float number_octaves = float(op_key) / 12.0f;

          float dbfalloff_per_octave = -4.5f * (0.15 + unit_levscale);
          float db_falloff           = number_octaves * dbfalloff_per_octave;

          float keyamp = decibel_to_linear_amp_ratio(db_falloff);

          //////////////////////////////////
          // final level
          //////////////////////////////////

          float final_amp = baselev * velamp * keyamp;

          if (0)
            printf("uls<%g> numoct<%g> dbfalloff<%g> keyamp<%g> final_amp<%g>\n", unit_levscale, number_octaves, db_falloff, keyamp, final_amp);

          /////////////////////////////////

          cci->_value.x = final_amp;
        };

        ////////////////////////////////////////////////////////////////////////
        amp_param->_coarse           = 0.0f;
        auto funname                 = ork::FormatString("op%d-fun", opindex);
        auto FUN                     = layerdata->appendController<FunData>(funname);
        FUN->_a                      = envname;
        FUN->_b                      = opaname;
        FUN->_op                     = "a*b";
        amp_param->_mods->_src1      = FUN;
        amp_param->_mods->_src1Scale = 1.0;
      }
      // printf( "OP<%d>\n", op );
      // printf( "    AR<%d> D1R<%d> D2R<%d> RR<%d> D1L<%d>\n", AR,D1R,D2R,RR,D1L);
      // printf( "    AME<%d> EBS<%d> KVS<%d>\n", AME,EBS,KVS);
      // printf( "    OUT<%d> WAV<%d>\n", OUT, WAV);
      // printf( "    FCOA<%d> FFIN<%d> FIX<%d> FIXRG<%d>\n", FCOA, FFIN, FIX, FIXRG );
      // printf( "    RS<%d> DBT<%d>\n", RS, DBT);
      if (opindex == 0) {
        as_pmx->_txprogramdata = fm4pd;
        as_pmx->_pmoscdata     = opd;
        as_pmx->_opindex       = opindex;
      }
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
////////////////////////////////////////////////////////////////////////////////
void configureTx81zAlgorithm(
    lyrdata_ptr_t layerdata, //
    tx81zprgdata_ptr_t prgdata81z) {
  auto algdout        = std::make_shared<AlgData>();
  layerdata->_algdata = algdout;
  algdout->_name      = ork::FormatString("tx81z<%d>", prgdata81z->_alg);
  //////////////////////////////////////////
  auto stage_ops    = algdout->appendStage("OPS");
  auto stage_opmix  = algdout->appendStage("OPMIX");
  auto stage_stereo = algdout->appendStage("STMIX"); // todo : quadraphonic, 3d?
  //////////////////////////////////////////
  stage_stereo->setNumIos(1, 2); // 1 in, 2 out
  /////////////////////////////////////////////////
  // instantiate ops in reverse order
  //  because of order of operations (3,2,1,0)
  /////////////////////////////////////////////////
  auto op3 = stage_ops->appendTypedBlock<PMX>("op3");
  auto op2 = stage_ops->appendTypedBlock<PMX>("op2");
  auto op1 = stage_ops->appendTypedBlock<PMX>("op1");
  auto op0 = stage_ops->appendTypedBlock<PMX>("op0");
  /////////////////////////////////////////////////
  int opchanbase = 2;
  op0->addDspChannel(opchanbase + 0);
  op1->addDspChannel(opchanbase + 1);
  op2->addDspChannel(opchanbase + 2);
  op3->addDspChannel(opchanbase + 3);
  /////////////////////////////////////////////////
  auto opmix = stage_opmix->appendTypedBlock<PMXMix>("opmixer");
  opmix->addDspChannel(0);
  /////////////////////////////////////////////////
  float basemodindex = 32.5f;
  op0->_modIndex     = basemodindex;
  op1->_modIndex     = basemodindex;
  op2->_modIndex     = basemodindex;
  op3->_modIndex     = basemodindex;
  /////////////////////////////////////////////////
  switch (prgdata81z->_alg) {
    case 0:
      //   (3)->2->1->0
      stage_ops->setNumIos(1, 1);
      stage_opmix->setNumIos(1, 1);
      op1->_modulator = true;
      op2->_modulator = true;
      op3->_modulator = true;
      op0->addPmInput(opchanbase + 1);
      op1->addPmInput(opchanbase + 2);
      op2->addPmInput(opchanbase + 3);
      opmix->addInputChannel(opchanbase + 0);
      break;
    case 1:
      //   (3)
      // 2->1->0
      op1->_modIndex = basemodindex * 0.5f; // 2 inputs
      stage_ops->setNumIos(1, 1);
      stage_opmix->setNumIos(1, 1);
      op1->_modulator = true;
      op2->_modulator = true;
      op3->_modulator = true;
      op0->addPmInput(opchanbase + 1);
      op1->addPmInput(opchanbase + 2);
      op1->addPmInput(opchanbase + 3);
      opmix->addInputChannel(opchanbase + 0);
      break;
    case 2:
      //  2
      //  1 (3)
      //   0
      op0->_modIndex = basemodindex * 0.5f; // 2 inputs
      stage_ops->setNumIos(1, 1);
      stage_opmix->setNumIos(1, 1);
      op1->_modulator = true;
      op2->_modulator = true;
      op3->_modulator = true;
      op0->addPmInput(opchanbase + 1);
      op0->addPmInput(opchanbase + 3);
      op1->addPmInput(opchanbase + 2);
      opmix->addInputChannel(opchanbase + 0);
      break;
    case 3:
      // (3)
      //  1   2
      //    0
      op0->_modIndex = basemodindex * 0.5f; // 2 inputs
      op0->addPmInput(opchanbase + 1);
      op0->addPmInput(opchanbase + 2);
      op1->addPmInput(opchanbase + 3);
      stage_ops->setNumIos(1, 1);
      stage_opmix->setNumIos(1, 1);
      op1->_modulator = true;
      op2->_modulator = true;
      op3->_modulator = true;
      opmix->addInputChannel(opchanbase + 0);
      break;
    case 4:
      // 1 (3)
      // 0  2
      stage_ops->setNumIos(1, 2);
      stage_opmix->setNumIos(2, 1);
      op1->_modulator = true;
      op3->_modulator = true;
      op0->addPmInput(opchanbase + 1);
      op2->addPmInput(opchanbase + 3);
      opmix->addInputChannel(opchanbase + 0);
      opmix->addInputChannel(opchanbase + 2);
      break;
    case 5:
      //   (3)
      //   / \
      // 0  1  2
      stage_ops->setNumIos(1, 3);
      stage_opmix->setNumIos(3, 1);
      op3->_modulator = true;
      op0->addPmInput(opchanbase + 3);
      op1->addPmInput(opchanbase + 3);
      op2->addPmInput(opchanbase + 3);
      opmix->addInputChannel(opchanbase + 0);
      opmix->addInputChannel(opchanbase + 1);
      opmix->addInputChannel(opchanbase + 2);
      break;
    case 6:
      //      (3)
      // 0  1  2
      stage_ops->setNumIos(1, 3);
      stage_opmix->setNumIos(3, 1);
      op3->_modulator = true;
      op2->addPmInput(opchanbase + 3);
      opmix->addInputChannel(opchanbase + 0);
      opmix->addInputChannel(opchanbase + 1);
      opmix->addInputChannel(opchanbase + 2);
      break;
    case 7:
      //   0  1  2 (3)
      stage_ops->setNumIos(1, 4);
      stage_opmix->setNumIos(4, 1);
      opmix->addInputChannel(opchanbase + 0);
      opmix->addInputChannel(opchanbase + 1);
      opmix->addInputChannel(opchanbase + 2);
      opmix->addInputChannel(opchanbase + 3);
      break;
  }
  /////////////////////////////////////////////////
  // stereo mix out
  /////////////////////////////////////////////////
  auto stereoout         = stage_stereo->appendTypedBlock<MonoInStereoOut>("monoin-stereoout");
  auto STEREOC           = layerdata->appendController<ConstantControllerData>("STEREOMIX");
  auto stereo_mod        = stereoout->_paramd[0]->_mods;
  stereo_mod->_src1      = STEREOC;
  stereo_mod->_src1Scale = 1.0f;
  STEREOC->_constvalue   = 1.0f;
}
} // namespace ork::audio::singularity
