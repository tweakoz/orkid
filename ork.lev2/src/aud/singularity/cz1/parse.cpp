#include <stdio.h>
#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include <ork/file/file.h>
#include <stdint.h>
#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/dsp_ringmod.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/math/multicurve.h>

using namespace ork;

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
CzProgData::CzProgData() {
  _oscData[0] = std::make_shared<CzOscData>();
  _oscData[1] = std::make_shared<CzOscData>();
}
///////////////////////////////////////////
struct ratelevmodel {
  float _inpbias   = 0.0f;
  float _inpscale  = 0.0f;
  float _inpdiv    = 0.0f;
  float _rorbias   = 0.0f;
  float _basenumer = 0.0f;
  float _power     = 0.0f;
  float _scalar    = 0.0f;
  void wamodel() {
    _inpbias   = 1.09252f;
    _inpscale  = 90.9917f;
    _inpdiv    = 97.4786f;
    _rorbias   = 0.102644f;
    _basenumer = 1.04686f;
    _power     = 2.6284f;
    _scalar    = 0.635746f;
  }
  void low_pmodel() {
    _inpbias   = 0.583739f;
    _inpscale  = 87.4026f;
    _inpdiv    = 100.548f;
    _rorbias   = 0.0218978f;
    _basenumer = 1.32368f;
    _power     = 1.08842f;
    _scalar    = 0.0618756f;
  }
  void mid_pmodel() {
    _inpbias   = 0.612909f;
    _inpscale  = 89.747f;
    _inpdiv    = 100.471f;
    _rorbias   = 0.0959415f;
    _basenumer = 1.08779f;
    _power     = 3.32362f;
    _scalar    = 0.741f;
  }

  float transform(float value) {
    float slope    = _inpbias + (value * _inpscale) / _inpdiv;
    float ror      = env_slope2ror(slope, _rorbias);
    float computed = powf(_basenumer / ror, _power) * _scalar;
    return std::clamp(computed, 0.004f, 250.0f);
  }
};

///////////////////////////////////////////
float decode_a_envlevel(int value) {
  int normed = (value * 99) / 127 + 1;
  switch (normed) {
    case 0:
      normed = 0;
      break;
    case 0x7f:
      normed = 99;
      break;
  }
  float fn = float(normed) / 99.0f;
  return powf(fn, 0.5);
}
///////////////////////////////////////////
float decode_a_envrate(int value, float delta) {
  int uservalue = (value * 99) / 119 + 1;
  switch (value) {
    case 0:
      uservalue = 0;
      break;
    case 0x7f:
      uservalue = 99;
      break;
  }
  ratelevmodel model;
  model.wamodel();
  return model.transform(uservalue) * fabs(delta);
}
///////////////////////////////////////////
float decode_w_envlevel(int value) {
  int normed = (value * 99) / 127 + 1;
  switch (normed) {
    case 0:
      normed = 0;
      break;
    case 0x7f:
      normed = 99;
      break;
  }
  float fn = float(normed) / 99.0f;
  return powf(fn, 1.0f);
}
///////////////////////////////////////////
float decode_w_envrate(int value, float delta) {
  int uservalue = (((value - 8) * 99) / 119) + 1;
  switch (value) {
    case 0:
      uservalue = 0;
      break;
    case 0x7f:
      uservalue = 99;
      break;
  }
  ratelevmodel model;
  model.wamodel();
  return model.transform(uservalue) * fabs(delta);
}
float decode_p_envrate(int value, float delta) {
  float uservalue = (value * 99) / 127 + 1;
  switch (value) {
    case 0:
      uservalue = 0;
      break;
    case 0x7f:
      uservalue = 99;
      break;
  }
  ratelevmodel lopmodel, midpmodel;
  lopmodel.low_pmodel();
  midpmodel.mid_pmodel();
  auto model = (uservalue <= 75) ? midpmodel : lopmodel;
  return model.transform(uservalue) * fabs(delta);
}
///////////////////////////////////////////////////////////////////////////////
float decode_p_envlevel(int value) {
  float cents = 0.0;
  ////////////////////
  // convert weird byte value to linear value
  ////////////////////
  int linv = value;
  if (linv > 68) {
    linv -= 4;
  }
  ////////////////////
  // convert linear value to cents
  ////////////////////

  // 40 == 5 semis
  // 48 == 6 semis
  // 56 == 7 semis
  // 64 == 8 semis

  // 65 == 10
  // 66 == 12
  // 72 == 24 semis
  // 78 == 36 semis
  // 84 == 48 semis
  // 90 == 60 semis
  // 96 == 72 semis
  // 99 == 84 semis
  int linv64 = std::clamp(linv, 0, 64);
  int linv66 = std::clamp(linv - 64, 0, 36);

  cents = 100.0 * float(linv64) / 8.0;
  cents += 200.0 * float(linv66);

  return cents;
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
czxprogdata_ptr_t parse_czprogramdata(CzData* outd, prgdata_ptr_t prgout, std::vector<u8> bytes) {

  bool is_cz1 = bytes.size() == 144;

  /* cz1 adds
    dco1 velocity (0..15)
    dco2 velocity (0..15)
    dcw1 velocity (0..15)
    dcw2 velocity (0..15)
    dca1 velocity (0..15)
    dca2 velocity (0..15)
    dca1 level (0..15)
    dca2 level (0..15)
  */

  if (1) {
    hexdumpbytes(bytes);
  }

  auto czprogdata      = std::make_shared<CzProgData>();
  u8 PFLAG             = bytes[0x00]; // octave/linesel
  czprogdata->_octave  = (PFLAG & 0x0c) >> 2;
  czprogdata->_lineSel = (PFLAG & 0x03);
  u8 PDS               = bytes[0x01];   // detune sign
  u8 PDETL             = bytes[0x02];   // detune fine
  u8 PDETH             = bytes[0x03];   // detune oct/note
  int detval           = (PDETH * 100)  // accumulate coarse
               + ((PDETL * 100) / 240); // accumulate fine
  czprogdata->_detuneCents = (PDS & 1) ? (detval) : +detval;

  switch (czprogdata->_lineSel) {
    case 0: // 1
      printf("linesel<1>\n");
      break;
    case 1: // 2
      printf("linesel<2>\n");
      break;
    case 2: // 1+1'
      printf("linesel<1+1'>\n");
      break;
    case 3: // 1+2'
      printf("linesel<1+2'>\n");
      break;
    default:
      assert(false);
  }

  u8 PVK = bytes[0x04]; // vibrato wave
  switch (PVK) {
    case 0x08:
      czprogdata->_vibratoWave = 1;
      break;
    case 0x04:
      czprogdata->_vibratoWave = 2;
      break;
    case 0x20:
      czprogdata->_vibratoWave = 3;
      break;
    case 0x02:
      czprogdata->_vibratoWave = 4;
      break;
    default:
      assert(false);
      break;
  }
  u8 PVDLD                  = bytes[0x05]; // vibrato delay (skip 2)
  czprogdata->_vibratoDelay = PVDLD;
  u8 PVSD                   = bytes[0x08]; // vibrato rate (skip 2)
  czprogdata->_vibratoRate  = PVSD;
  u8 PVDD                   = bytes[0x0b]; // vibrato depth (skip 2)
  czprogdata->_vibratoDepth = PVDD;

  int byteindex = 0x0e; // OSC START
  // (48+9) == 57 bytes per osc * 2 == 114 bytes
  // 114+0xe == 128
  // 0xe .. 0x46 osc 1
  // 0x25 == vel (oscbase+)
  // 0x47 .. 0x7f osc 2

  for (int o = 0; o < 2; o++) {
    auto OSC = czprogdata->_oscData[o];
    switch (czprogdata->_octave) {
      case 0:
        OSC->_octaveScale = 1.0;
        break;
      case 1:
        OSC->_octaveScale = 2.0;
        break;
      case 2:
        OSC->_octaveScale = 0.5;
        break;
      default:
        OrkAssert(false);
    }

    ///////////////////////////////////////////////////////////
    u8 MFW0 = bytes[byteindex++]; // dc01 wave / modulation
    u8 MFW1 = bytes[byteindex++]; // dc01 wave / modulation
    u8 MAMD = bytes[byteindex++]; // DCA key follow
    u8 MAMV = bytes[byteindex++];
    u8 MWMD = bytes[byteindex++]; // DCW key follow
    u8 MWMV = bytes[byteindex++];
    u8 PMAL = bytes[byteindex++]; // DCA1 end
    ///////////////////////////////////////////////////////////
    float prevlevel = 0;
    for (int i = 0; i < 8; i++) {                       // DCA env (16 bytes)
      u8 r                        = bytes[byteindex++]; // byte = (119*r/99)
      u8 l                        = bytes[byteindex++]; // byte = (127*l/99)
      u8 r7                       = r & 0x7f;
      u8 l7                       = l & 0x7f;
      OSC->_dcaEnv._decreasing[i] = (r & 0x80);

      float thislev  = decode_a_envlevel(l7);
      float deltalev = thislev - prevlevel;
      prevlevel      = thislev;

      if (l & 0x80)
        OSC->_dcaEnv._sustPoint = i;
      OSC->_dcaEnv._time[i]  = decode_a_envrate(r7, deltalev);
      OSC->_dcaEnv._level[i] = thislev;
    }
    ///////////////////////////////////////////////////////////
    u8 PMWL   = bytes[byteindex++]; // DCW1 end
    prevlevel = 0;
    for (int i = 0; i < 8; i++) {                       // DCW env (16 bytes)
      u8 r                        = bytes[byteindex++]; // byte = (119*r/99)+8
      u8 l                        = bytes[byteindex++]; // byte = (127*l/99)
      u8 r7                       = r & 0x7f;
      u8 l7                       = l & 0x7f;
      OSC->_dcwEnv._decreasing[i] = (r & 0x80);
      if (l & 0x80)
        OSC->_dcwEnv._sustPoint = i;

      float thislev  = decode_w_envlevel(l7);
      float deltalev = thislev - prevlevel;
      prevlevel      = thislev;

      OSC->_dcwEnv._time[i]  = decode_w_envrate(r7, deltalev);
      OSC->_dcwEnv._level[i] = thislev;
    }
    ///////////////////////////////////////////////////////////
    u8 PMPL   = bytes[byteindex++]; // DCO1 end
    prevlevel = 0;
    for (int i = 0; i < 8; i++) { // DCO (pitch) env (16 bytes)
      u8 r  = bytes[byteindex++]; // byte = (127*r/99)
      u8 l  = bytes[byteindex++]; // byte = (127*l/99)
      u8 r7 = r & 0x7f;
      u8 l7 = l & 0x7f;
      if (l & 0x80)
        OSC->_dcoEnv._sustPoint = i;

      OSC->_dcoEnv._decreasing[i] = (r & 0x80);

      float thislev  = decode_p_envlevel(l7);
      float deltalev = fabs(thislev - prevlevel) / 8400.0f;
      prevlevel      = thislev;

      OSC->_dcoEnv._level[i] = decode_p_envlevel(l7);
      float decoded_time     = decode_p_envrate(r7, deltalev);
      OSC->_dcoEnv._time[i]  = decoded_time;
    }
    ///////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////
    uint16_t MFW = (uint16_t(MFW0) << 8) | uint16_t(MFW1);
    printf("MFW0<x%2x> MFW1<x%2x>\n", MFW0, MFW1);
    OSC->_dcoBaseWaveA = int(MFW0 >> 5) & 0x7;
    OSC->_dcoBaseWaveB = int(MFW0 >> 2) & 0x7;
    OSC->_dcoWindow    = int(MFW0 << 2);
    OSC->_dcoWindow |= (int(MFW1 >> 6) & 0x3);

    if (o == 0) { // ignore linemod from line1{
      czprogdata->_lineMod = (MFW1 >> 2) & 0xf;
    }

    OSC->_dcaKeyFollow = MAMD & 0xf;
    OSC->_dcwKeyFollow = MWMD & 0xf;
    OSC->_dcaDepth     = 15 - (MAMD >> 4);
    OSC->_dcwDepth     = 15 - (MWMD >> 4);

    OSC->_dcaVelFollow    = PMAL >> 4;
    OSC->_dcwVelFollow    = PMWL >> 4;
    OSC->_dcaEnv._endStep = PMAL & 7;
    OSC->_dcwEnv._endStep = PMWL & 7;
    OSC->_dcoEnv._endStep = PMPL & 7;
  }
  assert(byteindex == 128);

  ////////////////////////////////////
  // todo create 2 instances of CzOsc
  //

  std::string name;
  if (is_cz1) {
    for (int i = 0; i < 16; i++) {
      name += char(bytes[128 + i]);
    }
    // remove leading and trailing spaces from patch name
    name = std::regex_replace(name, std::regex("^ +| +$|( ) +"), "$1");
  }
  prgout->_name = name;
  // czprogdata->dump();

  auto make_dco = [&](lyrdata_ptr_t layerdata, czxdata_ptr_t oscdata, int dcochannel) {
    auto dcoenvname = FormatString("DCOENV%d", dcochannel);
    auto dcaenvname = FormatString("DCAENV%d", dcochannel);
    auto dcwenvname = FormatString("DCWENV%d", dcochannel);
    /////////////////////////////////////////////////
    // Pitch Envelope
    /////////////////////////////////////////////////
    auto DCOENV             = layerdata->appendController<RateLevelEnvData>(dcoenvname);
    DCOENV->_ampenv         = false;
    const auto& srcdcoenv   = oscdata->_dcoEnv;
    DCOENV->_sustainSegment = srcdcoenv._sustPoint;
    for (int i = 0; i < 8; i++) {
      if (i <= srcdcoenv._endStep) {
        auto name = FormatString("seg%d", i);
        DCOENV->addSegment(name, srcdcoenv._time[i], srcdcoenv._level[i]);
      }
    }
    DCOENV->_releaseSegment = srcdcoenv._endStep;
    /////////////////////////////////////////////////
    // Wave(filter) Envelope
    /////////////////////////////////////////////////
    auto DCWENV             = layerdata->appendController<RateLevelEnvData>(dcwenvname);
    DCWENV->_ampenv         = false;
    const auto& srcdcwenv   = oscdata->_dcwEnv;
    DCWENV->_sustainSegment = srcdcwenv._sustPoint;
    for (int i = 0; i < 8; i++) {
      if (i <= srcdcwenv._endStep) {
        auto name = FormatString("seg%d", i);
        DCWENV->addSegment(name, srcdcwenv._time[i], srcdcwenv._level[i]);
      }
    }
    DCWENV->_releaseSegment = srcdcwenv._endStep;
    DCWENV->_envadjust      = [=](const EnvPoint& inp, //
                             int iseg,
                             const KeyOnInfo& KOI) -> EnvPoint { //
      EnvPoint outp = inp;
      int ikeydelta = KOI._key;
      float base    = 1.0 - (oscdata->_dcwKeyFollow * 0.001);
      float power   = pow(base, ikeydelta);
      // printf("DCW kf<%d> ikeydelta<%d> base<%0.3f> power<%0.3f>\n", oscdata->_dcwKeyFollow, ikeydelta, base, power);
      outp._level *= power;
      return outp;
    };
    /////////////////////////////////////////////////
    // Amplitude Envelope
    /////////////////////////////////////////////////
    auto DCAENV             = layerdata->appendController<RateLevelEnvData>(dcaenvname);
    DCAENV->_ampenv         = true;
    const auto& srcdcaenv   = oscdata->_dcaEnv;
    DCAENV->_sustainSegment = srcdcaenv._sustPoint;
    for (int i = 0; i < 8; i++) {
      if (i <= srcdcaenv._endStep) {
        auto name = FormatString("seg%d", i);
        DCAENV->addSegment(name, srcdcaenv._time[i], srcdcaenv._level[i]);
      }
      DCAENV->_releaseSegment = srcdcaenv._endStep;
    }
    DCAENV->_envadjust = [=](const EnvPoint& inp, //
                             int iseg,
                             const KeyOnInfo& KOI) -> EnvPoint { //
      EnvPoint outp = inp;
      int ikeydelta = KOI._key;
      float base    = 1.0 - (oscdata->_dcaKeyFollow * 0.001);
      float power   = pow(base, ikeydelta);
      // printf("DCA kf<%d> ikeydelta<%d> base<%0.3f> power<%0.3f>\n", oscdata->_dcaKeyFollow, ikeydelta, base, power);
      outp._time *= power;
      return outp;
    };
    //////////////////////////////////////
    // setup dsp graph
    //////////////////////////////////////
    auto dconame        = FormatString("dco%d", dcochannel);
    auto dcoampname     = FormatString("amp%d", dcochannel);
    auto dcostage       = layerdata->stageByName("DCO");
    auto ampstage       = layerdata->stageByName("AMP");
    auto dco            = dcostage->appendTypedBlock<CZX>(dconame, oscdata, dcochannel);
    auto amp            = ampstage->appendTypedBlock<AMP_MONOIO>(dcoampname);
    dco->_dspchannel[0] = dcochannel;
    amp->_dspchannel[0] = dcochannel;
    //////////////////////////////////////
    // setup modulators
    //////////////////////////////////////
    auto& pitch_mod      = dco->_paramd[0]._mods;
    pitch_mod._src1      = DCOENV;
    pitch_mod._src1Depth = 1.0f;
    /////////////////////////////////////////////////
    auto& modulation_index      = dco->_paramd[1]._mods;
    modulation_index._src1      = DCWENV;
    modulation_index._src1Depth = float(oscdata->_dcwDepth) / 15.0f;
    /////////////////////////////////////////////////
    auto& amp_param = amp->_paramd[0];
    amp_param.useDefaultEvaluator();
    amp_param._mods._src1      = DCAENV;
    amp_param._mods._src1Depth = float(oscdata->_dcaDepth) / 15.0f;
    /////////////////////////////////////////////////
    if (dcochannel == 1) { // add detune
      auto DETUNE             = layerdata->appendController<CustomControllerData>("DCO1DETUNE");
      pitch_mod._src2         = DETUNE;
      pitch_mod._src2MinDepth = 1.0;
      pitch_mod._src2MaxDepth = 1.0;
      DETUNE->_onkeyon        = [czprogdata](
                             CustomControllerInst* cci, //
                             const KeyOnInfo& KOI) {    //
        cci->_curval = czprogdata->_detuneCents;
        // printf("DETUNE<%g>\n", cci->_curval);
      };
    }
  };
  /////////////////////////////////////////////////
  auto layerdata           = prgout->newLayer();
  layerdata->_layerLinGain = 0.25;
  /////////////////////////////////////////////////
  // line select
  /////////////////////////////////////////////////
  switch (czprogdata->_lineSel) {
    case 0: // 1
      configureCz1Algorithm(layerdata, 1);
      make_dco(layerdata, czprogdata->_oscData[0], 0);
      break;
    case 1: // 2
      configureCz1Algorithm(layerdata, 1);
      make_dco(layerdata, czprogdata->_oscData[1], 0);
      break;
    case 2: // 1+1'
      configureCz1Algorithm(layerdata, 2);
      *(czprogdata->_oscData[1]) = *(czprogdata->_oscData[0]);
      make_dco(layerdata, czprogdata->_oscData[0], 0);
      make_dco(layerdata, czprogdata->_oscData[1], 1);
      break;
    case 3: // 1+2'
      configureCz1Algorithm(layerdata, 2);
      make_dco(layerdata, czprogdata->_oscData[0], 0);
      make_dco(layerdata, czprogdata->_oscData[1], 1);
      break;
    default:
      assert(false);
  }
  /////////////////////////////////////////////////
  // line modulation
  /////////////////////////////////////////////////
  int modulation_mode   = czprogdata->_lineMod >> 1;
  bool modulation_nomix = (czprogdata->_lineMod & 1);
  switch (modulation_mode) {
    case 0:   // none
    case 1: { // none
      if (czprogdata->numOscs() == 2) {
        auto modstage = layerdata->stageByName("MOD");
        auto mix      = modstage->appendTypedBlock<SUM2>("nomod");
      }
      break;
    }
    case 2: // ring 2
      OrkAssert(czprogdata->numOscs() == 2);
      OrkAssert(false);
      break;
    case 3: { // noise 1
      OrkAssert(czprogdata->numOscs() == 2);
      czprogdata->_oscData[1]->_noisemod = true;
      auto modstage                      = layerdata->stageByName("MOD");
      auto mix                           = modstage->appendTypedBlock<SUM2>("noisemod-1");
      break;
    }
    case 4:                           // ring 1
    case 5: {                         // ring 1
      if (czprogdata->numOscs() == 1) // we get some bad data with some banks
        return nullptr;
      auto modstage = layerdata->stageByName("MOD");
      if (modulation_nomix)
        modstage->appendTypedBlock<RingMod>("ringmod");
      else
        modstage->appendTypedBlock<RingModSumA>("ringmod-1");
      break;
    }
    case 6: // ring 3
      OrkAssert(czprogdata->numOscs() == 2);
      OrkAssert(false);
      break;
    case 7: { // noise 2
      OrkAssert(false);
      OrkAssert(czprogdata->numOscs() == 2);
      czprogdata->_oscData[1]->_noisemod = true;
      auto modstage                      = layerdata->stageByName("MOD");
      auto mix                           = modstage->appendTypedBlock<SUM2>("noisemod-2");
      break;
    }
  }
  /////////////////////////////////////////////////
  czprogdata->_name = name;
  czprogdata->dump();
  return czprogdata;
}
///////////////////////////////////////////////////////////////////////////////
int CzProgData::numOscs() const {
  return (_lineSel == 2 or _lineSel == 3) ? 2 : 1;
}
///////////////////////////////////////////////////////////////////////////////
void parse_czx(CzData* outd, const file::Path& path, const std::string& bnkname) {
  ork::File syxfile(path, ork::EFM_READ);
  u8* data    = nullptr;
  size_t size = 0;
  syxfile.Load((void**)(&data), size);

  printf("casio CZ syxfile<%s> loaded size<%d>\n", path.c_str(), int(size));

  int programcount = 0;
  int programincrm = (256 + 8);
  int prgbase      = 0;
  int bytesperprog = 128;
  bool sysexdecode = true;

  switch (size) {
    case 264: // CZ-101 single patch sysex dump
      // sysex format
      assert(data[0] == 0xf0 and data[1] == 0x44); // sysx header
      assert(data[263] == 0xf7);                   // sysx footer

      programcount = 1;
      prgbase      = 7;
      bytesperprog = 128;
      break;
    case 296: // CZ-1 single patch sysex dump
      // sysex format
      assert(data[0] == 0xf0 and data[1] == 0x44);
      assert(data[295] == 0xf7); // sysx footer
      programcount = 1;
      prgbase      = 7;
      bytesperprog = 144;
      break;
    case 4224: // CZ-101 16 patch sysex dump
      assert(data[0] == 0xf0 and data[1] == 0x44);
      programcount = 16;
      prgbase      = 7;
      break;
    case 4608: // CZ-1 32 patch binary blob format
      programcount = 32;
      prgbase      = 0;
      bytesperprog = 144;
      sysexdecode  = false;
      break;
    default:
      assert(false);
      break;
  }

  for (int iv = 0; iv < programcount; iv++) {
    printf("////////////////////////////\n");
    auto name        = FormatString("%s(%02d)", bnkname.c_str(), iv);
    int newprogramid = outd->_lastprg++;
    auto prgout      = std::make_shared<ProgramData>();
    prgout->_role    = "czx";
    ///////////////////////////
    // collect bytes for program
    ///////////////////////////
    std::vector<u8> bytes;
    if (sysexdecode) {
      int syxbase = prgbase + programincrm * iv;
      for (int i = 0; i < bytesperprog; i++) {
        int bidx = syxbase + (i * 2);
        u8 ln    = data[bidx + 0];
        u8 hn    = data[bidx + 1];
        u8 byte  = (hn << 4) | ln;
        bytes.push_back(byte);
      }
    } else {
      int base = (iv * bytesperprog);
      for (int i = 0; i < bytesperprog; i++)
        bytes.push_back(data[base + i]);
    }
    ///////////////////////////
    auto czpd = parse_czprogramdata(outd, prgout, bytes);
    if (czpd) {
      outd->_bankdata->addProgram(newprogramid, czpd->_name, prgout);
      prgout->_name = czpd->_name;
      printf("czprog<%s>\n", prgout->_name.c_str());
    }
    ///////////////////////////
  }
}
///////////////////////////////////////////////////////////////////////////////
void CzProgData::dump() const {

  std::string lineselmode;
  std::string oscmodulation;
  switch (_lineSel) {
    case 0:
      lineselmode = "1";
      break;
    case 1:
      lineselmode = "2";
      break;
    case 2:
      lineselmode = "1+1'";
      break;
    case 3:
      lineselmode = "1+2";
      break;
    default:
      OrkAssert(false);
  }
  switch (_lineMod >> 1) {
    case 0:
    case 1:
      oscmodulation = "none";
      break;
    case 2:
      oscmodulation = "ringmod2";
      break;
    case 3:
      oscmodulation = "noisemod1";
      break;
    case 4:
    case 5:
      oscmodulation = "ringmod1";
      break;
    case 6:
      oscmodulation = "ringmod3";
      break;
    case 7:
      oscmodulation = "noisemod2";
      break;
  }

  printf("CZPROG<%s>\n", _name.c_str());
  printf("  _octave<%d>\n", _octave);
  printf("  _lineSel<%s>\n", lineselmode.c_str());
  printf("  _lineMod<%s>\n", oscmodulation.c_str());
  printf("  _detuneCents<%d>\n", _detuneCents);
  printf("  _vibratoWave<%d>\n", _vibratoWave);
  printf("  _vibratoRate<%d>\n", _vibratoRate);
  printf("  _vibratoDepth<%d>\n", _vibratoDepth);
  for (int o = 0; o < 2; o++) {
    const auto OSC = _oscData[o];
    assert(OSC->_dcoBaseWaveA >= 0);
    assert(OSC->_dcoBaseWaveB >= 0);
    assert(OSC->_dcoBaseWaveA < 8);
    assert(OSC->_dcoBaseWaveB < 8);
    printf("  osc<%d>\n", o);
    printf("    _dcoBaseWaveA<%d>\n", OSC->_dcoBaseWaveA);
    printf("    _dcoBaseWaveB<%d>\n", OSC->_dcoBaseWaveB);
    auto dumpenv = [](const CzEnvelope& env) {
      printf("        _endStep<%d>\n", env._endStep);
      if (env._sustPoint >= 0)
        printf("        _sustPoint<%d>\n", env._sustPoint);
      printf("        r: ");
      for (int i = 0; i < 8; i++)
        printf("%0.3fs ", env._time[i]);
      printf("\n");
      printf("        l: ");
      for (int i = 0; i < 8; i++)
        printf("%g     ", env._level[i]);
      printf("\n");
    };
    printf("    _dcoEnv\n");
    dumpenv(OSC->_dcoEnv);
    printf("    _dcwEnv\n");
    dumpenv(OSC->_dcwEnv);
    printf("    _dcwKeyFollow<%d>\n", OSC->_dcwKeyFollow);
    printf("    _dcwVelFollow<%d>\n", OSC->_dcwVelFollow);
    printf("    _dcwDepth<%d>\n", OSC->_dcwDepth);
    printf("    _dcaEnv\n");
    dumpenv(OSC->_dcaEnv);
    printf("    _dcaKeyFollow<%d>\n", OSC->_dcaKeyFollow);
    printf("    _dcaVelFollow<%d>\n", OSC->_dcaVelFollow);
    printf("    _dcaDepth<%d>\n", OSC->_dcaDepth);
  }
}
///////////////////////////////////////////////////////////////////////////////
CzData::CzData()
    : SynthData()
    , _lastprg(0) {
}
///////////////////////////////////////////////////////////////////////////////
CzData::~CzData() {
}
///////////////////////////////////////////////////////////////////////////////
void CzData::appendBank(const file::Path& syxpath, const std::string& bnkname) {
  parse_czx(this, syxpath.c_str(), bnkname);
}
std::shared_ptr<CzData> CzData::load(const file::Path& syxpath, const std::string& bnkname) {
  auto czdata = std::make_shared<CzData>();
  czdata->appendBank(syxpath, bnkname);
  return czdata;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
