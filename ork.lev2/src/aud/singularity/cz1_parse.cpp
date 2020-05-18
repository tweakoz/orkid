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
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/math/multicurve.h>

using namespace ork;

namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////
CzProgData::CzProgData() {
  _oscData[0] = std::make_shared<CzOscData>();
  _oscData[1] = std::make_shared<CzOscData>();
}
///////////////////////////////////////////////////////////////////////////////
auto genwacurve = []() -> MultiCurve1D {
  MultiCurve1D curve;
  auto& s = curve.mSegmentTypes;

  curve.SetPoint(0, 0.0, 105.0);
  curve.SetPoint(1, 1.0, 0.004);

  s.resize(12);
  for (int i = 0; i < 12; i++)
    s[i] = EMCST_LINEAR;

  auto& v = curve.mVertices;
  v.AddSorted(0.05, 0.0);
  v.AddSorted(0.1, 35.0);
  v.AddSorted(0.2, 13.0);
  v.AddSorted(0.3, 4.33);
  v.AddSorted(0.4, 1.63);
  v.AddSorted(0.5, 0.54);
  v.AddSorted(0.6, 0.19);
  v.AddSorted(0.7, 0.066);
  v.AddSorted(0.8, 0.025);
  v.AddSorted(0.9, 0.008);
  return curve;
};
auto genpcurve = []() -> MultiCurve1D {
  MultiCurve1D curve;
  auto& s = curve.mSegmentTypes;
  curve.SetPoint(0, 0.0, 235.0);
  curve.SetPoint(1, 1.0, 0.004);

  s.resize(12);
  for (int i = 0; i < 12; i++)
    s[i] = EMCST_LINEAR;

  auto& v = curve.mVertices;
  v.AddSorted(0.05, 134.0);
  v.AddSorted(0.1, 70.0);
  v.AddSorted(0.2, 26.0);
  v.AddSorted(0.3, 8.5);
  v.AddSorted(0.4, 2.68);
  v.AddSorted(0.5, 0.92);
  v.AddSorted(0.6, 0.29);
  v.AddSorted(0.7, 0.097);
  v.AddSorted(0.8, 0.032);
  v.AddSorted(0.9, 0.010);

  return curve;
};
///////////////////////////////////////////////////////////////////////////////
float decode_wa_envrate(int value) {
  static auto curve = genwacurve();
  return curve.Sample(float(value) * 0.01f);
}
float decode_p_envrate(int value) {
  static auto curve = genpcurve();
  return curve.Sample(float(value) * 0.01f);
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
void parse_czprogramdata(CzData* outd, ProgramData* prgout, std::vector<u8> bytes) {

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
    int numlines = bytes.size() / 16;
    for (int r = 0; r < numlines; r++) {
      int bidx = (r * 16);
      printf("0x%02x: ", bidx);

      for (int c = 0; c < 16; c++) {
        u8 byte = bytes[bidx + c];
        printf("%02x ", byte);
      }

      printf(" |");
      for (int c = 0; c < 16; c++) {
        char byte = (char)bytes[bidx + c];
        if (false == isprint(byte))
          byte = '.';
        printf("%c", byte);
      }
      printf("|\n");
    }
  }

  auto czdata     = std::make_shared<CzProgData>();
  u8 PFLAG        = bytes[0x00]; // octave/linesel
  czdata->_octave = (PFLAG & 0x0c) >> 2;
  int lineSel     = (PFLAG & 0x03);
  u8 PDS          = bytes[0x01];        // detune sign
  u8 PDETL        = bytes[0x02];        // detune fine
  u8 PDETH        = bytes[0x03];        // detune oct/note
  int detval      = (PDETH * 100)       // accumulate coarse
               + ((PDETL * 100) / 240); // accumulate fine
  czdata->_detuneCents = (PDS & 1) ? (detval) : +detval;

  switch (lineSel) {
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
      czdata->_vibratoWave = 1;
      break;
    case 0x04:
      czdata->_vibratoWave = 2;
      break;
    case 0x20:
      czdata->_vibratoWave = 3;
      break;
    case 0x02:
      czdata->_vibratoWave = 4;
      break;
    default:
      assert(false);
      break;
  }
  u8 PVDLD              = bytes[0x05]; // vibrato delay (skip 2)
  czdata->_vibratoDelay = PVDLD;
  u8 PVSD               = bytes[0x08]; // vibrato rate (skip 2)
  czdata->_vibratoRate  = PVSD;
  u8 PVDD               = bytes[0x0b]; // vibrato depth (skip 2)
  czdata->_vibratoDepth = PVDD;

  auto decodewave = [](int c0, int c1) -> int {
    switch (c0) {
      case 0:
      case 1:
      case 2:
        return c0;
        break;
      case 4:
        return 3;
        break;
      case 5:
        return 4;
        break;
      case 6:
        switch (c1) {
          case 1:
            return 5;
            break;
          case 2:
            return 6;
            break;
          case 3:
            return 7;
            break;
        }
        break;
      default:
        assert(false);
    }
    return -1;
  };

  int byteindex = 0x0e; // OSC START
  // (48+9) == 57 bytes per osc * 2 == 114 bytes
  // 114+0xe == 128
  // 0xe .. 0x46 osc 1
  // 0x25 == vel (oscbase+)
  // 0x47 .. 0x7f osc 2

  for (int o = 0; o < 2; o++) {
    auto OSC         = czdata->_oscData[o];
    OSC->_dspchannel = o;
    switch (czdata->_octave) {
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
    ///////////////////////////////////////////////////////////
    u8 PMAL = bytes[byteindex++];                       // DCA1 end
    for (int i = 0; i < 8; i++) {                       // DCA env (16 bytes)
      u8 r                        = bytes[byteindex++]; // byte = (119*r/99)
      u8 l                        = bytes[byteindex++]; // byte = (127*l/99)
      u8 r7                       = r & 0x7f;
      u8 l7                       = l & 0x7f;
      OSC->_dcaEnv._decreasing[i] = (r & 0x80);
      if (l & 0x80)
        OSC->_dcaEnv._sustPoint = i;
      OSC->_dcaEnv._time[i]  = decode_wa_envrate((r7 * 99) / 119);
      OSC->_dcaEnv._level[i] = (l7 * 99) / 127;
    }
    ///////////////////////////////////////////////////////////
    u8 PMWL = bytes[byteindex++];                       // DCW1 end
    for (int i = 0; i < 8; i++) {                       // DCW env (16 bytes)
      u8 r                        = bytes[byteindex++]; // byte = (119*r/99)+8
      u8 l                        = bytes[byteindex++]; // byte = (127*l/99)
      u8 r7                       = r & 0x7f;
      u8 l7                       = l & 0x7f;
      OSC->_dcwEnv._decreasing[i] = (r & 0x80);
      if (l & 0x80)
        OSC->_dcwEnv._sustPoint = i;
      OSC->_dcwEnv._time[i]  = decode_wa_envrate(((r7 - 8) * 99) / 119);
      OSC->_dcwEnv._level[i] = (l7 * 99) / 127;
    }
    ///////////////////////////////////////////////////////////
    u8 PMPL = bytes[byteindex++]; // DCO1 end
    for (int i = 0; i < 8; i++) { // DCO (pitch) env (16 bytes)
      u8 r  = bytes[byteindex++]; // byte = (127*r/99)
      u8 l  = bytes[byteindex++]; // byte = (127*l/99)
      u8 r7 = r & 0x7f;
      u8 l7 = l & 0x7f;
      if (l & 0x80)
        OSC->_dcoEnv._sustPoint = i;

      OSC->_dcoEnv._decreasing[i] = (r & 0x80);
      OSC->_dcoEnv._time[i]       = decode_p_envrate(1 + (r7 * 99) / 127);
      OSC->_dcoEnv._level[i]      = decode_p_envlevel(l7);
    }
    ///////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////
    printf("MFW0<x%2x> MFW1<x%2x>\n", MFW0, MFW1);
    OSC->_dcoWaveA = decodewave((MFW0 >> 5) & 0x7, (MFW1 >> 6) & 0x3);
    OSC->_dcoWaveB = decodewave((MFW0 >> 2) & 0x7, (MFW1 >> 6) & 0x3);

    if (o == 0) { // ignore linemod from line1{
      czdata->_lineMod = (MFW1 >> 3) & 0x7;
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
  // czdata->dump();

  auto make_dco = [&](lyrdata_ptr_t layerdata, czxdata_ptr_t oscdata, int dcoindex) {
    /////////////////////////////////////////////////
    auto DCOENV           = layerdata->appendController<RateLevelEnvData>("DCOENV");
    DCOENV->_ampenv       = false;
    const auto& srcdcoenv = oscdata->_dcoEnv;
    DCOENV->_sustainPoint = (srcdcoenv._sustPoint == 0) //
                                ? 7
                                : srcdcoenv._sustPoint - 1;
    for (int i = 0; i < 8; i++) {
      bool end   = false; // i >= srcdcoenv._endStep;
      auto point = end ?  //
                       EnvPoint{0, 0}
                       : EnvPoint{//
                                  srcdcoenv._time[i],
                                  srcdcoenv._level[i]};
      DCOENV->_segments.push_back(point);
    }
    /////////////////////////////////////////////////
    auto DCWENV           = layerdata->appendController<RateLevelEnvData>("DCWENV");
    DCWENV->_ampenv       = false;
    const auto& srcdcwenv = oscdata->_dcwEnv;
    DCWENV->_sustainPoint = (srcdcwenv._sustPoint == 0) //
                                ? 7
                                : srcdcwenv._sustPoint - 1;
    for (int i = 0; i < 8; i++) {
      bool end   = false; // i >= srcdcwenv._endStep;
      auto point = end ?  //
                       EnvPoint{0, 0}
                       : EnvPoint{//
                                  srcdcwenv._time[i],
                                  srcdcwenv._level[i] / 100.0f};
      DCWENV->_segments.push_back(point);
    }
    DCWENV->_envadjust = [=](const EnvPoint& inp, //
                             int iseg,
                             const KeyOnInfo& KOI) -> EnvPoint { //
      EnvPoint outp = inp;
      int ikeydelta = KOI._key;
      float base    = 1.0 - (oscdata->_dcwKeyFollow * 0.005);
      float power   = pow(base, ikeydelta);
      // printf("DCW kf<%d> ikeydelta<%d> base<%0.3f> power<%0.3f>\n", oscdata->_dcwKeyFollow, ikeydelta, base, power);
      outp._level *= power;
      return outp;
    };

    /////////////////////////////////////////////////
    auto DCAENV           = layerdata->appendController<RateLevelEnvData>("DCAENV");
    DCAENV->_ampenv       = true;
    const auto& srcdcaenv = oscdata->_dcaEnv;
    DCAENV->_sustainPoint = (srcdcaenv._sustPoint == 0) //
                                ? 7
                                : srcdcaenv._sustPoint - 1;
    for (int i = 0; i < 8; i++) {
      bool end   = false; // i >= srcdcaenv._endStep;
      auto point = end ?  //
                       EnvPoint{0, 0}
                       : EnvPoint{//
                                  srcdcaenv._time[i],
                                  srcdcaenv._level[i] / 100.0f};
      DCAENV->_segments.push_back(point);
    }
    DCAENV->_envadjust = [=](const EnvPoint& inp, //
                             int iseg,
                             const KeyOnInfo& KOI) -> EnvPoint { //
      EnvPoint outp = inp;
      int ikeydelta = KOI._key;
      float base    = 1.0 - (oscdata->_dcaKeyFollow * 0.005);
      float power   = pow(base, ikeydelta);
      // printf("DCA kf<%d> ikeydelta<%d> base<%0.3f> power<%0.3f>\n", oscdata->_dcaKeyFollow, ikeydelta, base, power);
      outp._time *= power;
      return outp;
    };
    /////////////////////////////////////////////////
    auto osc = layerdata->stage(0)->appendBlock();
    auto amp = layerdata->stage(2)->appendBlock();
    CZX::initBlock(osc, oscdata);
    AMP::initBlock(amp);
    /////////////////////////////////////////////////
    auto& pitch_mod      = osc->_paramd[0]._mods;
    pitch_mod._src1      = DCOENV;
    pitch_mod._src1Depth = 1.0f;
    /////////////////////////////////////////////////
    auto& modulation_index      = osc->_paramd[1]._mods;
    modulation_index._src1      = DCWENV;
    modulation_index._src1Depth = float(oscdata->_dcwDepth) / 15.0f;
    /////////////////////////////////////////////////
    auto& amp_param = amp->addParam();
    amp_param.useDefaultEvaluator();
    amp_param._mods._src1      = DCAENV;
    amp_param._mods._src1Depth = float(oscdata->_dcaDepth) / 15.0f;
    /////////////////////////////////////////////////
    if (dcoindex == 1) { // add detune
      auto CZPITCH            = layerdata->appendController<CustomControllerData>("CZPITCH");
      pitch_mod._src2         = CZPITCH;
      pitch_mod._src2MinDepth = 1.0;
      pitch_mod._src2MaxDepth = 1.0;
      CZPITCH->_onkeyon       = [czdata](
                              CustomControllerInst* cci, //
                              const KeyOnInfo& KOI) {    //
        cci->_curval = czdata->_detuneCents;
        printf("DETUNE<%g>\n", cci->_curval);
      };
    }
  };
  /////////////////////////////////////////////////
  auto layerdata      = prgout->newLayer();
  layerdata->_algdata = configureCz1Algorithm();
  switch (lineSel) {
    case 0: // 1
      make_dco(layerdata, czdata->_oscData[0], 0);
      break;
    case 1: // 2
      make_dco(layerdata, czdata->_oscData[1], 0);
      break;
    case 2: // 1+1'
      // TODO : ring, noisemod
      make_dco(layerdata, czdata->_oscData[0], 0);
      make_dco(layerdata, czdata->_oscData[0], 1);
      break;
    case 3: // 1+2'
      // TODO : ring, noisemod
      make_dco(layerdata, czdata->_oscData[0], 0);
      make_dco(layerdata, czdata->_oscData[1], 1);
      break;
    default:
      assert(false);
  }
  switch (czdata->_lineMod) {
    case 0: { // none
      auto mix = layerdata->stage(1)->appendBlock();
      SUM2::initBlock(mix);
      break;
    }
    case 4: { // ring
      OrkAssert(lineSel == 2 or lineSel == 3);
      auto ring = layerdata->stage(1)->appendBlock();
      MUL2::initBlock(ring);
      break;
    }
    case 3: // noise
      OrkAssert(lineSel == 2 or lineSel == 3);
      czdata->_oscData[1]->_noisemod = true;
      auto mix                       = layerdata->stage(1)->appendBlock();
      SUM2::initBlock(mix);
      break;
  }
  czdata->_name = name;

  czdata->dump();
} // namespace ork::audio::singularity
///////////////////////////////////////////////////////////////////////////////
void parse_czx(CzData* outd, const file::Path& path, const std::string& bnkname) {
  ork::File syxfile(path, ork::EFM_READ);
  u8* data    = nullptr;
  size_t size = 0;
  syxfile.Load((void**)(&data), size);

  printf("casio CZ syxfile<%s> loaded size<%d>\n", path.c_str(), int(size));

  auto zpmDB       = outd->_zpmDB;
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
    int newprogramid               = outd->_lastprg++;
    auto prgout                    = new ProgramData;
    zpmDB->_programs[newprogramid] = prgout;
    prgout->_role                  = "czx";
    prgout->_name                  = FormatString("%s(%02d)", bnkname.c_str(), iv);
    printf("czprog<%s>\n", prgout->_name.c_str());
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
    parse_czprogramdata(outd, prgout, bytes);
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
  switch (_lineMod) {
    case 0:
      oscmodulation = "none";
      break;
    case 4:
      oscmodulation = "ring";
      break;
    case 3:
      oscmodulation = "noise";
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
    assert(OSC->_dcoWaveA >= 0);
    assert(OSC->_dcoWaveB >= 0);
    assert(OSC->_dcoWaveA < 8);
    assert(OSC->_dcoWaveB < 8);
    printf("  osc<%d>\n", o);
    printf("    _dcoWaveA<%d>\n", OSC->_dcoWaveA);
    printf("    _dcoWaveB<%d>\n", OSC->_dcoWaveB);
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
  _zpmDB = new SynthObjectsDB;
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
const ProgramData* CzData::getProgram(int progID) const // final
{
  auto ObjDB = this->_zpmDB;
  return ObjDB->findProgram(progID);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
