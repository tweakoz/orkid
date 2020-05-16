#include <stdio.h>
#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include <ork/file/file.h>
#include <stdint.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/dspblocks.h>

using namespace ork;
namespace ork::audio::singularity {

static const float openv_attacktimes[32] = {
    0,  .01, .013, .015, .017, .02, .023, .025, .03, .055, .06, .065, .07, .1, .2, .35,
    .5, .7,  1,    2,    3,    4,   5,    6,    7,   8,    9,   10,   15,  20, 25, 31,
};
static const float openv_dectimes[32] = {
    0,  .01, .02, .03, .04, .05, .06, .07, .08, .09, .1, .11, .12, .13, .14, .15,
    .2, .3,  .4,  .5,  .6,  .7,  .8,  .9,  1,   2,   3,  4,   5,   6,   7,   10,
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

static const float openv_reltimes[16] = {
    0,
    .1,
    .2,
    .4,
    .8,
    .85,
    .9,
    1,
    1.5,
    2,
    2.5,
    3,
    5,
    7,
    9,
    11,
};

static const float opfrq_ratios[64] = {0.5,   0.71,  0.78,  0.87,  1.00,  1.41,  1.57,  1.73,  2.0,   2.82,  3.0,   3.14,  3.46,
                                       4.0,   4.24,  4.71,  5.0,   5.19,  5.65,  6.0,   6.28,  6.92,  7.0,   7.07,  7.85,  8.0,
                                       8.48,  8.65,  9.0,   9.42,  9.89,  10.0,  10.38, 10.99, 11.0,  11.3,  12.0,  12.11, 12.56,
                                       12.72, 13.0,  13.84, 14.0,  14.1,  14.13, 15.0,  15.55, 15.57, 15.7,  16.96, 17.27, 17.30,
                                       18.37, 18.84, 19.03, 19.78, 20.41, 20.76, 21.20, 21.98, 22.49, 23.55, 24.22, 25.95};

static const int op_mitltab[20] = {
    127, 122, 118, 114, 110, 107, 104, 102, 100, 98, 96, 94, 92, 90, 88, 86, 85, 84, 82, 81,
};

void parse_tx81z(Tx81zData* outd, const file::Path& path) {

  ork::File syxfile(path, ork::EFM_READ);
  u8* data    = nullptr;
  size_t size = 0;
  syxfile.Load((void**)(&data), size);

  printf("tx81z syxfile<%s> loaded size<%d>\n", path.c_str(), int(size));

  auto zpmDB       = outd->_zpmDB;
  int programcount = 0;
  int prgbase      = 0;

  switch (size) {
    case 4104:
      assert(data[0] == 0xf0 and data[1] == 0x43);
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
      assert(false);
      break;
  }

  for (int iv = 0; iv < programcount; iv++) {
    auto prg                           = new ProgramData;
    zpmDB->_programs[outd->_lastprg++] = prg;
    prg->_role                         = "fm4";

    int index_vb = prgbase + (iv * 128);

    char namebuf[11];
    for (int n = 0; n < 10; n++)
      namebuf[n] = data[index_vb + 57 + n];
    namebuf[10] = 0;

    prg->_name = namebuf;

    auto fm4pd = new Fm4ProgData;

    auto ld            = prg->newLayer();
    ld->_keymap        = outd->_zpmKM;
    auto CB0           = new ControlBlockData;
    ld->_ctrlBlocks[0] = CB0;

    ld->_kmpBlock._keymap = outd->_zpmKM;

    auto stage0 = ld->appendStage();

    auto block0       = stage0->appendBlock();
    auto block1       = stage0->appendBlock();
    auto block2       = stage0->appendBlock();
    auto block3       = stage0->appendBlock();
    block0->_dspBlock = "FM4";
    // block0->_paramScheme = "FM4";
    block0->_extdata["FM4"].Set<Fm4ProgData*>(fm4pd);
    // block0->_mods._src1      = "OP0.Amp";
    // block1->_mods._src1      = "OP1.Amp";
    // block2->_mods._src1      = "OP2.Amp";
    // block3->_mods._src1      = "OP3.Amp";
    // block0->_mods._src1Depth = 1.0f;
    // block1->_mods._src1Depth = 1.0f;
    // block2->_mods._src1Depth = 1.0f;
    // block3->_mods._src1Depth = 1.0f;

    // ld->_fBlock[7]._dspBlock = "AMP";
    // ld->_fBlock[7]._paramScheme = "AMP";
    ld->_envCtrlData._useNatEnv = false;
    ld->_algdata->_krzAlgIndex  = 1;
    ld->_algdata->_name         = "ALG1";

    // for (int i = 0; i < 8; i++)
    // ld->_fBlock[i].initEvaluators();

    // printf( "////////////////////////////\n");
    printf("V<%d:%s>\n", iv, namebuf);

    ////////////
    // voice global
    ////////////

    int ALG = data[index_vb + 40] & 7;           // op amp 0..7
    int FBL = (data[index_vb + 40] & 0x38) >> 3; // fb level  0..7
    int SY  = (data[index_vb + 40] & 0x60) >> 6; // lfo sync (reset phase) bool

    fm4pd->_alg      = ALG;
    fm4pd->_feedback = FBL;
    fm4pd->_lfoSync  = SY;

    // printf( "ALG<%d> FBL<%d> SY<%d>\n", ALG, FBL, SY);

    int LFS = data[index_vb + 41]; // lfo speed 0..99
    int LFD = data[index_vb + 42]; // lfo depth 0..99
    int PMD = data[index_vb + 43]; // pch mod depth 0..99
    int AMD = data[index_vb + 44]; // amp mod depth 0..99

    fm4pd->_lfoSpeed = LFS;
    fm4pd->_lfoDepth = LFD;
    fm4pd->_pchDepth = PMD;
    fm4pd->_ampDepth = AMD;

    // printf( "LFS<%d> LFD<%d> PMD<%d> AMD<%d>\n", LFS, LFD, PMD, AMD);

    int _LAP = data[index_vb + 45];
    int LFW  = _LAP & 3;        // 0..3 lfo wave (sawup,squ,tri,shold)
    int AMS  = (_LAP >> 2) & 3; // lfo amp mod sensa 0..3
    int PMS  = (_LAP >> 5);     // lfo pch mod sensa 0..7

    fm4pd->_lfoWave  = LFW;
    fm4pd->_ampSensa = AMS;
    fm4pd->_pchSensa = PMS;

    // printf( "LFW<%d> AMS<%d> PMS<%d>\n", LFW, AMS, PMS);

    int TRPS = data[index_vb + 46] & 0x3f; // transpose 0..48 (middle-c)
    int PBR  = data[index_vb + 47] & 0x0f; // pitchbendrange 0..12

    fm4pd->_middleC        = TRPS;
    fm4pd->_pitchBendRange = PBR;

    // printf( "TRPS<%d> PBR<%d>\n", TRPS, PBR);

    bool CH = data[index_vb + 48] & 0x10;
    bool MO = data[index_vb + 48] & 0x08; // mono mode ?
    bool SU = data[index_vb + 48] & 0x04;
    bool PO = data[index_vb + 48] & 0x02; // porto mode ?
    bool PM = data[index_vb + 48] & 0x01;

    fm4pd->_mono     = MO;
    fm4pd->_portMode = PO;

    // printf( "CH<%d> MO<%d> SU<%d> PO<%d> PM<%d>\n", CH,MO,SU,PO,PM);

    int PORT = data[index_vb + 49] & 0x3f; // portotime 0..99
                                           // printf( "PORT<%d>\n", PORT);

    fm4pd->_portRate = PORT;

    ////////////
    // operator data
    ////////////
    for (int j = 0; j < 4; j++) {
      int index = index_vb + j * 10;

      const int kop[] = {3, 1, 2, 0};
      int op          = kop[j];
      auto& opd       = fm4pd->_ops[op];

      // ratio 0.5 .. 27.57

      opd._atkRate     = data[index + 0]; // EG 0..31
      opd._dec1Rate    = data[index + 1]; // EG 0..31
      opd._dec2Rate    = data[index + 2]; // EG 0..31
      opd._relRate     = data[index + 3]; // EG 0..15
      opd._dec1Lev     = data[index + 4]; // EG 0..15
      opd._levScaling  = data[index + 5]; // level scaling 0..99
      int _AEK         = data[index + 6];
      opd._opEnable    = (_AEK & 64) >> 6;              // AME - bool (enable op?)
      opd._egBiasSensa = (_AEK & 0x18) >> 3;            // eg bias sensitivity 0..7
      opd._kvSensa     = (_AEK & 0x07);                 // keyvel sensitivity 0..7
      opd._outLevel    = data[index + 7];               // out level 0..99
      opd._coarseFrq   = data[index + 8] & 0x3f;        // coarse frequency 0..63
      opd._ratScaling  = (data[index + 9] & 0x18) >> 3; // rate scaling 0..3
      opd._detune      = (data[index + 9] & 7);         // detune ? -3..3

      int _EFF          = data[index_vb + 73 + 2 * j] & 0x3f;
      opd._egShift      = (_EFF & 0x30) >> 4;      // eg shift (off,48,24,12)
      opd._fixedFrqMode = (_EFF & 0x08) >> 3;      // fixed mode ? bool
      opd._fixedRange   = (_EFF & 0x7);            // fixed range: 255,510,1k,2k,4k,8k,16k,32k
                                                   // fixed step:  1,  2,  4, 8, 16,32,64, 128
      int _OWF      = data[index_vb + 74 + 2 * j]; //&0x7f;
      opd._waveform = (_OWF & 0x70) >> 4;          // waveform 0..7
      opd._fineFrq  = (_OWF & 7);                  // fine frequency 0..15

      opd._F   = data[index + 8];
      opd._EFF = _EFF;
      opd._OWF = _OWF;

      ////////////////////////////

      if (opd._fixedFrqMode) {
        int fxrange   = opd._fixedRange;
        int index     = (opd._coarseFrq * 4); //|(fine&3);
        opd._frqFixed = float(8 << fxrange) + float(index << fxrange);
      } else {
        assert(opd._coarseFrq < 64);
        float r       = opfrq_ratios[opd._coarseFrq];
        opd._frqRatio = r;
      }

      ////////////////////////////
      // modulation Index
      ////////////////////////////

      int tlval = (opd._outLevel > 19) ? 99 - opd._outLevel : op_mitltab[opd._outLevel];

      // float MI = (4.0f*pi2) *  powf(2.0,(-tlval/8.0f));
      // opd._modIndex = (4.0f*512.0f) *  powf(2.0,(-tlval/8.0f));
      opd._modIndex = (4.0f * 1024.0f) * powf(2.0, (-tlval / 8.0f));

      ////////////////////////////

      auto AE      = CB0->addController<RateLevelEnvData>();
      AE->_name    = ork::FormatString("OP%d.Amp", j);
      AE->_ampenv  = true; //(j==0);
      AE->_envType = RlEnvType::ERLTYPE_DEFAULT;

      int atktime  = 31 - opd._atkRate;
      int dectime  = 31 - opd._dec1Rate;
      int dec2time = 31 - opd._dec2Rate;
      int declevl  = opd._dec1Lev;
      int reltime  = 15 - opd._relRate;

      printf("ATK<%d> DEC<%d> REL<%d>\n", atktime, dectime, reltime);
      assert(atktime < 32);
      assert(dectime < 32);
      assert(dec2time < 32);
      assert(declevl < 16);
      assert(reltime < 16);
      float attaktime = openv_attacktimes[atktime]; // float(j)*0.5;
      float decaytime = openv_dectimes[dectime];
      float deca2time = openv_dectimes[dec2time];
      float decaylevl = openv_declevels[declevl]; // float(j)*0.5;
      float relestime = openv_reltimes[reltime];  // float(j)*0.5;

      AE->_segments.push_back({attaktime, 1});         // atk1
      AE->_segments.push_back({decaytime, decaylevl}); // atk2
      AE->_segments.push_back({0, 0});                 // atk3
      if (opd._dec2Rate != 0)
        AE->_segments.push_back({deca2time, 0}); // dec
      else
        AE->_segments.push_back({0, 0});       // dec
      AE->_segments.push_back({relestime, 0}); // rel1
      AE->_segments.push_back({0, 0});         // rel2
      AE->_segments.push_back({0, 0});         // rel3

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
  _zpmDB              = new VastObjectsDB;
  _zpmKM              = new KeyMap;
  _zpmKM->_name       = "FM4";
  _zpmKM->_kmID       = 1;
  _zpmDB->_keymaps[1] = _zpmKM;
}

Tx81zData::~Tx81zData() {
}
void Tx81zData::loadBank(const file::Path& syxpath) {
  parse_tx81z(this, syxpath);
}
const ProgramData* Tx81zData::getProgram(int progID) const // final
{
  auto ObjDB = this->_zpmDB;
  return ObjDB->findProgram(progID);
}

} // namespace ork::audio::singularity
