#include <stdio.h>
#include <ork/orktypes.h>
#include <ork/math/audiomath.h>
#include <ork/file/file.h>
#include <stdint.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/cz101.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/math/multicurve.h>

using namespace ork;

namespace ork::audio::singularity {
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
void parse_czprogramdata(CzData* outd, programData* prgout, std::vector<u8> bytes) {

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

  auto czdata      = std::make_shared<CzProgData>();
  u8 PFLAG         = bytes[0x00]; // octave/linesel
  czdata->_octave  = (PFLAG & 0x0c) >> 2;
  czdata->_lineSel = (PFLAG & 0x03);
  u8 PDS           = bytes[0x01];       // detune sign
  u8 PDETL         = bytes[0x02];       // detune fine
  u8 PDETH         = bytes[0x03];       // detune oct/note
  int detval       = (PDETH * 100)      // accumulate coarse
               + ((PDETL * 100) / 250); // accumulate fine
  czdata->_detuneCents = PDS ? (-detval) : detval;
  printf("PDETL<%d>\n", PDETL);

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
  auto decodekeyfollow = [](u8 b1, u8 b2) -> int { //
    return int(b1 & 0xf);
    //(int(b1) << 8 | int(b2));
  };

  int byteindex = 0x0e; // OSC START
  // (48+9) == 57 bytes per osc * 2 == 114 bytes
  // 114+0xe == 128
  // 0xe .. 0x46 osc 1
  // 0x25 == vel (oscbase+)
  // 0x47 .. 0x7f osc 2
  for (int o = 0; o < 2; o++) {
    auto& OSC = czdata->_oscData[o];
    ///////////////////////////////////////////////////////////
    u8 MFW0 = bytes[byteindex++]; // dc01 wave / modulation
    u8 MFW1 = bytes[byteindex++]; // dc01 wave / modulation
    u8 MAMD = bytes[byteindex++]; // DCA key follow
    u8 MAMV = bytes[byteindex++];
    u8 MWMD = bytes[byteindex++]; // DCW key follow
    u8 MWMV = bytes[byteindex++];
    ///////////////////////////////////////////////////////////
    u8 PMAL = bytes[byteindex++];                      // DCA1 end
    for (int i = 0; i < 8; i++) {                      // DCA env (16 bytes)
      u8 r                       = bytes[byteindex++]; // byte = (119*r/99)
      u8 l                       = bytes[byteindex++]; // byte = (127*l/99)
      u8 r7                      = r & 0x7f;
      u8 l7                      = l & 0x7f;
      OSC._dcaEnv._decreasing[i] = (r & 0x80);
      OSC._dcaEnv._time[i]       = decode_wa_envrate((r7 * 99) / 127);
      OSC._dcaEnv._level[i]      = (l7 * 99) / 127;
    }
    ///////////////////////////////////////////////////////////
    u8 PMWL = bytes[byteindex++];                      // DCW1 end
    for (int i = 0; i < 8; i++) {                      // DCW env (16 bytes)
      u8 r                       = bytes[byteindex++]; // byte = (119*r/99)+8
      u8 l                       = bytes[byteindex++]; // byte = (127*l/99)
      u8 r7                      = r & 0x7f;
      u8 l7                      = l & 0x7f;
      OSC._dcwEnv._decreasing[i] = (r & 0x80);
      if (l & 0x80)
        OSC._dcwEnv._sustPoint = i;
      OSC._dcwEnv._time[i]  = decode_wa_envrate(((r7 - 8) * 99) / 119);
      OSC._dcwEnv._level[i] = (l7 * 99) / 127;
    }
    ///////////////////////////////////////////////////////////
    u8 PMPL = bytes[byteindex++]; // DCO1 end
    for (int i = 0; i < 8; i++) { // DCO (pitch) env (16 bytes)
      u8 r  = bytes[byteindex++]; // byte = (127*r/99)
      u8 l  = bytes[byteindex++]; // byte = (127*l/99)
      u8 r7 = r & 0x7f;
      u8 l7 = l & 0x7f;

      OSC._dcoEnv._decreasing[i] = (r & 0x80);
      OSC._dcoEnv._time[i]       = decode_p_envrate((r7 * 99) / 127);
      OSC._dcoEnv._level[i]      = (l7 * 99) / 127;
    }
    ///////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////
    printf("MFW0<x%2x> MFW1<x%2x>\n", MFW0, MFW1);
    OSC._dcoWaveA = decodewave((MFW0 >> 5) & 0x7, (MFW1 >> 6) & 0x3);
    OSC._dcoWaveB = decodewave((MFW0 >> 2) & 0x7, (MFW1 >> 6) & 0x3);
    if (o == 0) // ignore linemod from line1
      czdata->_lineMod = (MFW1 >> 3) & 0x7;

    OSC._dcaKeyFollow = MAMD & 0xf;
    OSC._dcwKeyFollow = MWMD & 0xf;
    OSC._dcaDepth     = 15 - (MAMD >> 4);
    OSC._dcwDepth     = 15;

    OSC._dcaVelFollow    = PMAL >> 4;
    OSC._dcwVelFollow    = PMWL >> 4;
    OSC._dcaEnv._endStep = PMAL & 7;
    OSC._dcwEnv._endStep = PMWL & 7;
    OSC._dcoEnv._endStep = PMPL & 7;
  }
  assert(byteindex == 128);

  std::string name;
  if (is_cz1) {
    for (int i = 0; i < 16; i++) {
      name += char(bytes[128 + i]);
    }
    // remove leading and trailing spaces from patch name
    name = std::regex_replace(name, std::regex("^ +| +$|( ) +"), "$1");
  }
  // czdata->dump();
  auto ld     = prgout->newLayer();
  ld->_keymap = outd->_zpmKM;

  auto CB0           = new controlBlockData;
  ld->_ctrlBlocks[0] = CB0;

  auto AE        = new RateLevelEnvData;
  CB0->_cdata[0] = AE;

  AE->_name   = "AMPENV";
  AE->_ampenv = true;
  AE->_segments.push_back({0, 1}); // atk1
  AE->_segments.push_back({0, 0}); // atk2
  AE->_segments.push_back({0, 0}); // atk3
  AE->_segments.push_back({0, 0}); // dec
  AE->_segments.push_back({2, 0}); // rel1
  AE->_segments.push_back({0, 0}); // rel2
  AE->_segments.push_back({0, 0}); // rel3
  ld->_kmpBlock._keymap = outd->_zpmKM;

  auto osc = ld->appendDspBlock();
  auto amp = ld->appendDspBlock();
  CZX::initBlock(osc, czdata);
  AMP::initBlock(amp);
  ld->_envCtrlData._useNatEnv = false;
  ld->_algData._algID         = 1;
  ld->_algData._name          = "ALG1";
  czdata->_name               = name;

  czdata->dump();
}
///////////////////////////////////////////////////////////////////////////////
void parse_cz101(CzData* outd, const file::Path& path, const std::string& bnkname) {
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
    auto prgout                    = new programData;
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
    const auto& OSC = _oscData[o];
    assert(OSC._dcoWaveA >= 0);
    assert(OSC._dcoWaveB >= 0);
    assert(OSC._dcoWaveA < 8);
    assert(OSC._dcoWaveB < 8);
    printf("  osc<%d>\n", o);
    printf("    _dcoWaveA<%d>\n", OSC._dcoWaveA);
    printf("    _dcoWaveB<%d>\n", OSC._dcoWaveB);
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
        printf("%02d     ", env._level[i]);
      printf("\n");
    };
    printf("    _dcoEnv\n");
    dumpenv(OSC._dcoEnv);
    printf("    _dcwEnv\n");
    dumpenv(OSC._dcwEnv);
    printf("    _dcwKeyFollow<%d>\n", OSC._dcwKeyFollow);
    printf("    _dcwVelFollow<%d>\n", OSC._dcwVelFollow);
    printf("    _dcwDepth<%d>\n", OSC._dcwDepth);
    printf("    _dcaEnv\n");
    dumpenv(OSC._dcaEnv);
    printf("    _dcaKeyFollow<%d>\n", OSC._dcaKeyFollow);
    printf("    _dcaVelFollow<%d>\n", OSC._dcaVelFollow);
    printf("    _dcaDepth<%d>\n", OSC._dcaDepth);
  }
}
///////////////////////////////////////////////////////////////////////////////
CzData::CzData(synth* syn)
    : SynthData(syn)
    , _lastprg(0) {
  _zpmDB              = new VastObjectsDB;
  _zpmKM              = new KeyMap;
  _zpmKM->_name       = "CZX";
  _zpmKM->_kmID       = 1;
  _zpmDB->_keymaps[1] = _zpmKM;
}
///////////////////////////////////////////////////////////////////////////////
CzData::~CzData() {
}
///////////////////////////////////////////////////////////////////////////////
void CzData::loadBank(const file::Path& syxpath, const std::string& bnkname) {
  parse_cz101(this, syxpath.c_str(), bnkname);
}
///////////////////////////////////////////////////////////////////////////////
const programData* CzData::getProgram(int progID) const // final
{
  auto ObjDB = this->_zpmDB;
  return ObjDB->findProgram(progID);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
