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
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/dsp_ringmod.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/math/multicurve.h>
#include <ork/util/hexdump.inl>

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

struct wa_env_model {
  float transform(int value) {
    //float j = 0.25+float(value)/20.5f;
    //float T = 50.0f*powf(0.1,j);
    float j = float(value)/22.5f;
    float T = 100.0f*powf(0.1,j);
    return T;
  }
};
struct p_env_model {
  float transform(int value) {
    //float j = 0.25+float(value)/20.5f;
    //float T = 50.0f*powf(0.1,j);
    float j = float(value)/22.5f;
    float T = 100.0f*powf(0.1,j);
    return T;
  }
};

///////////////////////////////////////////
float decode_a_envlevel(int value) {
  int normed = (value * 99) / 127;
  if(normed<0) normed = 0;
  if(normed>99) normed = 99;
  float fn = float(normed) / 99.0f;
  float rval = powf(fn, 1);
  if(rval>1.0f) rval = 1.0f;
  if(rval<0.0f) rval = 0.0f;
  return rval;
}
///////////////////////////////////////////
float decode_a_envrate(int value, float delta) {
  int uservalue = 1 + (value * 99) / 109;
  if( uservalue < 0 ) uservalue = 0;
  if( uservalue > 99 ) uservalue = 99;
  wa_env_model model;
  float X = model.transform(uservalue);
  printf("val<%d> uservalue<%d> X<%g> delta<%g>\n", value, uservalue, X, delta);
  return X;
}
///////////////////////////////////////////
float decode_w_envlevel(int value) {
  int normed = (value * 99) / 127;
  if(normed<0) normed = 0;
  if(normed>99) normed = 99;
  float fn = float(normed) / 99.0f;
  float rval = powf(fn, 1);
  if(rval>1.0f) rval = 1.0f;
  if(rval<0.0f) rval = 0.0f;
  return rval;
}
///////////////////////////////////////////
float decode_w_envrate(int value, float delta) {
  int uservalue = 1 + ((value-8) * 99) / 119;
  if( uservalue < 0 ) uservalue = 0;
  if( uservalue > 99 ) uservalue = 99;
  wa_env_model model;
  float W = model.transform(uservalue);
  printf("val<%d> uservalue<%d> W<%g> delta<%g>\n", value, uservalue, W, delta);
  return W;
}
float decode_p_envrate(int value) {
  int normed = (value * 99) / 127;
  if(normed<0) normed = 0;
  if(normed>99) normed = 99;
  float fn = float(normed) / 99.0f;
  float rval = powf(fn, 1);
  if(rval>1.0f) rval = 1.0f;
  if(rval<0.0f) rval = 0.0f;
  return rval;
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

  cents = float(linv64) / 8.0;
  cents += 2 * float(linv66);

  return cents*2.0f;
} // namespace ork::audio::singularity

///////////////////////////////////////////////////////////////////////////////

CZLAYERDATACTX configureCz1Algorithm(lyrdata_ptr_t layerdata, int numosc) {
  CZLAYERDATACTX lctx;
  lctx._layerdata = layerdata;
  auto algdout        = std::make_shared<AlgData>();
  layerdata->_algdata = algdout;
  algdout->_name      = layerdata->_name+ork::FormatString("osc%d", numosc);
  //////////////////////////////////////////
  auto stage_dco               = algdout->appendStage("DCO");
  auto stage_amp               = algdout->appendStage("AMP");
  dspstagedata_ptr_t stage_mod = (numosc == 2) //
                                 ? algdout->appendStage("MOD")
                                 : nullptr;
  //////////////////////////////////////////
  auto stage_stereo = algdout->appendStage("STMIX"); // todo : quadraphonic, 3d?
  stage_stereo->setNumIos(1, 2); // 1 in, 2 out
  //////////////////////////////////////////
  lctx._algdata = algdout;
  lctx._stage_dco = stage_dco;
  lctx._stage_amp = stage_amp;
  lctx._stage_mod = stage_mod;
  //////////////////////////////////////////
  int dcoios = stage_mod ? 2 : 1;
  stage_dco->setNumIos(0, dcoios);
  stage_amp->setNumIos(dcoios, dcoios);
  //////////////////////////////////////////
  // 2 DCO case..
  // ring, noise mod or mix stage
  //////////////////////////////////////////
  if (stage_mod)
    stage_mod->setNumIos(2, 1); // 2 ins, 1 out
  /////////////////////////////////////////////////
  // stereo mix out
  /////////////////////////////////////////////////
  auto stereoout         = stage_stereo->appendTypedBlock<MonoInStereoOut>("LCZX0.MonoInStereoOut");

  auto GAONCONST         = layerdata->appendController<ConstantControllerData>("LCZX0.STEREO-GAIN");
  auto gain_modulator    = stereoout->_paramd[0]->_mods;
  auto PANCONST          = layerdata->appendController<ConstantControllerData>("LCZX0.STEREO-PAN");
  auto PANCUSTOM         = layerdata->appendController<CustomControllerData>("LCZX0.STEREOPAN2");
  auto pan_modulator     = stereoout->_paramd[1]->_mods;

  GAONCONST->_constvalue   = 1.0f;
  gain_modulator->_src1  = GAONCONST;
  gain_modulator->_src1Scale = 1.0f;

  PANCONST->_constvalue   = 0.0f;
  PANCUSTOM->_oncompute   = [](CustomControllerInst* cci) { //
    cci->_value.x = 0.0f;
  };
  pan_modulator->_src1  = PANCONST;
  pan_modulator->_src1Scale = 1.0f;
  pan_modulator->_src2  = PANCUSTOM;
  pan_modulator->_src2MinDepth = 1.0f;
  pan_modulator->_src2MaxDepth = 1.0f;

  //////////////////////////////////////////
  return lctx;
}
///////////////////////////////////////////////////////////////////////////////
void make_dco(CZLAYERDATACTX czctx,
              czprogdata_ptr_t czprogdata,
              czxdata_ptr_t oscdata,
              int dcochannel) {
  auto layerdata = czctx._layerdata;
  /////////////////////////////////////////////////
  auto dcoenvname = layerdata->_name+"."+FormatString("DCOENV%d", dcochannel);
  auto dcaenvname = layerdata->_name+"."+FormatString("DCAENV%d", dcochannel);
  auto dcwenvname = layerdata->_name+"."+FormatString("DCWENV%d", dcochannel);
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
    float base    = 1.0 - (oscdata->_dcwKeyFollow * 0.003);
    float power   = pow(base, ikeydelta);
    // printf("DCW kf<%d> ikeydelta<%d> base<%0.3f> power<%0.3f>\n", oscdata->_dcwKeyFollow, ikeydelta, base, power);
    outp._time *= power;
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
      auto& seg = DCAENV->_segments.back();
      seg._raw_rate  = srcdcaenv._raw_rates[i];
      seg._raw_level = srcdcaenv._raw_levels[i];
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
    if(0){
      for(int i=0;i<8;i++){
        const auto& SEG = DCAENV->_segments[i];
        printf("DCAENV<%d> rawrate<%d> rawlev<%d> time<%g> level<%g>\n", i, SEG._raw_rate, SEG._raw_level, SEG._time, SEG._level);
      }
      printf("DCA kf<%d> ikeydelta<%d> base<%0.3f> power<%0.3f>\n", oscdata->_dcaKeyFollow, ikeydelta, base, power);
    }
    outp._time *= power;
    outp._level *= power;
    return outp;
  };
  //////////////////////////////////////
  // setup dsp graph
  //////////////////////////////////////
  auto dconame    = FormatString("dco%d", dcochannel);
  auto dcoampname = FormatString("amp%d", dcochannel);
  auto dcostage   = layerdata->stageByName("DCO");
  auto ampstage   = layerdata->stageByName("AMP");
  auto dco        = dcostage->appendTypedBlock<CZX>(dconame, oscdata, dcochannel);
  auto amp        = ampstage->appendTypedBlock<AMP_MONOIO>(dcoampname);
  dco->addDspChannel(dcochannel);
  amp->addDspChannel(dcochannel);
  //////////////////////////////////////
  // setup modulators
  //////////////////////////////////////
  auto pitch_mod        = dco->_paramd[0]->_mods;
  pitch_mod->_src1      = DCOENV;
  pitch_mod->_src1Scale = 1.0f;
  /////////////////////////////////////////////////
  auto modulation_index        = dco->_paramd[1]->_mods;
  modulation_index->_src1      = DCWENV;
  modulation_index->_src1Scale = float(oscdata->_dcwDepth) / 15.0f;
  /////////////////////////////////////////////////
  auto amp_param = amp->_paramd[0];
  amp_param->useDefaultEvaluator();
  amp_param->_mods->_src1      = DCAENV;
  amp_param->_mods->_src1Scale = float(oscdata->_dcaDepth) / 15.0f;
  /////////////////////////////////////////////////
  if (dcochannel == 1) { // add detune
    auto DETUNE              = layerdata->appendController<CustomControllerData>("DCO1DETUNE");
    pitch_mod->_src2         = DETUNE;
    pitch_mod->_src2MinDepth = 1.0;
    pitch_mod->_src2MaxDepth = 1.0;
    DETUNE->_onkeyon         = [czprogdata](
        CustomControllerInst* cci, //
        const KeyOnInfo& KOI) {    //
      cci->_value.x = czprogdata->_detuneCents;
      //printf("DETUNE<%g>\n", cci->_value.x);
    };
  }
};
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
      //printf("linesel<1>\n");
      break;
    case 1: // 2
      //printf("linesel<2>\n");
      break;
    case 2: // 1+1'
      //printf("linesel<1+1'>\n");
      break;
    case 3: // 1+2'
      //printf("linesel<1+2'>\n");
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
    int byteindex = 0x0e; // OSC START
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
      OSC->_dcaEnv._raw_rates[i] = r7;
      OSC->_dcaEnv._raw_levels[i] = l7;
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
      OSC->_dcwEnv._raw_rates[i] = r7;
      OSC->_dcwEnv._raw_levels[i] = l7;
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

      OSC->_dcoEnv._level[i] = decode_p_envlevel(l7);
      OSC->_dcoEnv._time[i]  = decode_p_envrate(r7);
    }
    //OrkAssert(byteindex == 128);

    ///////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////
    //uint16_t MFW = (uint16_t(MFW0) << 8);
     //| uint16_t(MFW1);
    printf("MFW0<0x%02x> MFW1<0x%02x>\n", MFW0, MFW1);
    //0xc2, 0x00
    // -> 0b1100---- 
    OSC->_dcoBaseWaveA = int(MFW0 >> 5) & 0x7;
    OSC->_dcoBaseWaveB = int(MFW0 >> 2) & 0x7;

    OSC->_dcoWindow    = (int(MFW1 >> 6) & 0x3);

    if (o == 0) { // ignore linemod from line1{
      czprogdata->_lineMod = (MFW1 >> 2) & 0xf;
    }

    OSC->_dcaKeyFollow = MAMD & 0xf;
    OSC->_dcwKeyFollow = MWMD & 0xf;

    printf( "MWMD<0x%02x> MAMD<0x%02x>\n", MWMD, MAMD );

    OSC->_dcaDepth     = 15 - (MAMD >> 4);
    OSC->_dcwDepth     = 15 - (MWMD >> 4);

    OSC->_dcaVelFollow    = PMAL >> 4;
    OSC->_dcwVelFollow    = PMWL >> 4;
    OSC->_dcaEnv._endStep = PMAL & 7;
    OSC->_dcwEnv._endStep = PMWL & 7;
    OSC->_dcoEnv._endStep = PMPL & 7;
  }

  ////////////////////////////////////
  // todo create 2 instances of CzOsc
  ////////////////////////////////////

  std::string name;
  if (is_cz1) {
    for (int i = 0; i < 16; i++) {
      name += char(bytes[128 + i]);
    }
    // remove leading and trailing spaces from patch name
    name = std::regex_replace(name, std::regex("^ +| +$|( ) +"), "$1");
  }
  else{
  }
  prgout->_name = name;
  czprogdata->_name = name;

  ////////////////////////////////////


  /////////////////////////////////////////////////
  auto layerdata           = prgout->newLayer();
  layerdata->_name = "LCZX0";
  layerdata->_layerLinGain = 0.25;
  /////////////////////////////////////////////////
  // line select
  /////////////////////////////////////////////////
  CZLAYERDATACTX lyrctx;
  switch (czprogdata->_lineSel) {
    case 0: // 1
      lyrctx = configureCz1Algorithm(layerdata, 1);
      make_dco(lyrctx, czprogdata, czprogdata->_oscData[0], 0);
      break;
    case 1: // 2
      lyrctx = configureCz1Algorithm(layerdata, 1);
      make_dco(lyrctx, czprogdata, czprogdata->_oscData[1], 0);
      break;
    case 2: // 1+1'
      lyrctx = configureCz1Algorithm(layerdata, 2);
      *(czprogdata->_oscData[1]) = *(czprogdata->_oscData[0]);
      make_dco(lyrctx, czprogdata, czprogdata->_oscData[0], 0);
      make_dco(lyrctx, czprogdata, czprogdata->_oscData[1], 1);
      break;
    case 3: // 1+2'
      lyrctx = configureCz1Algorithm(layerdata, 2);
      make_dco(lyrctx, czprogdata, czprogdata->_oscData[0], 0);
      make_dco(lyrctx, czprogdata, czprogdata->_oscData[1], 1);
      break;
    default:
      assert(false);
  }
  /////////////////////////////////////////////////
  // line modulation
  /////////////////////////////////////////////////
  int modulation_mode   = czprogdata->_lineMod >> 1;
  bool modulation_nomix = (czprogdata->_lineMod & 1);
  auto modstage = layerdata->stageByName("MOD");
  switch (modulation_mode) {
    case 0:   // none
    case 1: { // none
      if (czprogdata->numOscs() == 2) {
        auto mix      = modstage->appendTypedBlock<SUM2>("nomod");
        mix->addDspChannel(0);
        mix->addDspChannel(1);
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
      auto mix                           = modstage->appendTypedBlock<SUM2>("noisemod-1");
      break;
    }
    case 4:                           // ring 1
    case 5: {                         // ring 1
      if (czprogdata->numOscs() == 1) // we get some bad data with some banks
        return nullptr;
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
      OrkAssert(czprogdata->numOscs() == 2);
      czprogdata->_oscData[1]->_noisemod = true;
      auto modstage                      = layerdata->stageByName("MOD");
      auto mix                           = modstage->appendTypedBlock<SUM2>("noisemod-2");
      break;
    }
  }
  /////////////////////////////////////////////////
  //czprogdata->dump();
  return czprogdata;
}
///////////////////////////////////////////////////////////////////////////////
int CzProgData::numOscs() const {
  return (_lineSel == 2 or _lineSel == 3) ? 2 : 1;
}
///////////////////////////////////////////////////////////////////////////////
void parse_czx(CzData* outd, const file::Path& path, const std::string& bnkname) {
  ork::File syxfile(path, ork::EFM_READ);
  std::vector<u8> bytes;
  syxfile.Load(bytes);
  auto data = bytes.data();
  auto size = bytes.size();

  int programcount = 0;
  int programincrm = 0;
  int prgbase      = 0;
  int bytesperprog = 128;
  bool sysexdecode = true;

  std::string single_name = path.getName().c_str();
  switch (size) {
    case 264: { // CZ-101 single patch sysex dump
      printf("casio CZ (CZ101-single) syxfile<%s> name<%s> loaded size<%d>\n", path.c_str(), single_name.c_str(), int(size));
      // sysex format
      assert(data[0] == 0xf0 and data[1] == 0x44); // sysx header
      assert(data[263] == 0xf7);                   // sysx footer

      programcount = 1;
      prgbase      = 7;
      bytesperprog = 128;
      programincrm = (256 + 8);
      break;
    }
    case 296: // CZ-1 single patch sysex dump
      // sysex format
      printf("casio CZ (CZ1-single) syxfile<%s> name<%s> loaded size<%d>\n", path.c_str(), single_name.c_str(), int(size));
      assert(data[0] == 0xf0 and data[1] == 0x44);
      assert(data[295] == 0xf7); // sysx footer
      programcount = 1;
      prgbase      = 7;
      bytesperprog = 144;
      programincrm = (256 + 8);
      break;
    case 4224: // CZ-101 16 patch sysex dump
      printf("casio CZ (CZ101-16 patch) syxfile<%s> loaded size<%d>\n", path.c_str(), int(size));
      assert(data[0] == 0xf0 and data[1] == 0x44);
      programcount = 16;
      prgbase      = 7;
      break;
    case 4608: // CZ-1 32 patch binary blob format
      printf("casio CZ (CZ1-32 patch) syxfile<%s> loaded size<%d>\n", path.c_str(), int(size));
      programcount = 32;
      prgbase      = 0;
      bytesperprog = 144;
      sysexdecode  = false;
      programincrm = (256 + 8);
      break;
    case 8704: // CZ-1 32 patch binary blob format
      printf("casio CZ (CZ1-32 patch) syxfile<%s> loaded size<%d>\n", path.c_str(), int(size));
      programcount = 32;
      prgbase      = 0x200;
      bytesperprog = 128;
      programincrm = 128;
      sysexdecode  = true;
      break;
    default:
      assert(false);
      break;
  }

  for (int iv = 0; iv < programcount; iv++) {
    //printf("////////////////////////////\n");
    auto name        = FormatString("%s(%02d)", bnkname.c_str(), iv);
    int newprogramid = outd->_lastprg++;
    auto prgout      = std::make_shared<ProgramData>();
    prgout->_tags    = "czx";
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
    hexdumpbytes(bytes);
    ///////////////////////////
    auto czpd = parse_czprogramdata(outd, prgout, bytes);
    if (czpd) {
      outd->_bankdata->addProgram(newprogramid, czpd->_name, prgout);
      if(czpd->_name.length())
        prgout->_name = czpd->_name;
      else
        prgout->_name = single_name;
      //printf("czprog<%s>\n", prgout->_name.c_str());
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
    OSC->dump();
  }
}
void CzOscData::dump() const{
    OrkAssert(_dcoBaseWaveA >= 0);
    OrkAssert(_dcoBaseWaveB >= 0);
    OrkAssert(_dcoBaseWaveA < 8);
    OrkAssert(_dcoBaseWaveB < 8);
    printf("  osc<%p>\n", (void*) this);
    printf("    _dcoBaseWaveA<%d>\n", _dcoBaseWaveA);
    printf("    _dcoBaseWaveB<%d>\n", _dcoBaseWaveB);
    auto dumpenv = [](const CzEnvelope& env) {
      printf("        _endStep<%d>\n", env._endStep);
      if (env._sustPoint >= 0)
        printf("        _sustPoint<%d>\n", env._sustPoint);
      printf("        r: ");
      for (int i = 0; i < 8; i++){
        printf("[%d] %0.3fs ", env._raw_rates[i], env._time[i]);
      }
      printf("\n");
      printf("        l: ");
      for (int i = 0; i < 8; i++)
        printf("[%d] %g     ", env._raw_levels[i], env._level[i]);
      printf("\n");
    };
    printf("    _dcoEnv\n");
    dumpenv(_dcoEnv);
    printf("    _dcwEnv\n");
    dumpenv(_dcwEnv);
    printf("    _dcwKeyFollow<%d>\n", _dcwKeyFollow);
    printf("    _dcwVelFollow<%d>\n", _dcwVelFollow);
    printf("    _dcwDepth<%d>\n", _dcwDepth);
    printf("    _dcaEnv\n");
    dumpenv(_dcaEnv);
    printf("    _dcaKeyFollow<%d>\n", _dcaKeyFollow);
    printf("    _dcaVelFollow<%d>\n", _dcaVelFollow);
    printf("    _dcaDepth<%d>\n", _dcaDepth);}
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
