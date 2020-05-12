
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/sampler.h>

using namespace rapidjson;

namespace ork::audio::singularity {

extern file::Path kbasepath;

const s16* getK2V3InternalSoundBlock() {
  static s16* gdata = nullptr;
  if (nullptr == gdata) {
    auto filename = kbasepath / "kurzweil" / "k2v3internalsamplerom.bin";
    printf("Loading Soundblock<%s>\n", filename.c_str());
    FILE* fin = fopen(filename.ToAbsolute().c_str(), "rb");
    if (fin == nullptr) {
      printf("You will need the K2000 ROM sampledata at <%s> to use this method!\n", filename.c_str());
      OrkAssert(false);
    }
    gdata = (s16*)malloc(8 << 20);
    fread(gdata, 8 << 20, 1, fin);
    fclose(fin);
  }
  return gdata;
}

///////////////////////////////////////////////////////////////////////////////
struct KrzAlgCfg {
  int _wp;
  int _w1;
  int _w2;
  int _w3;
  int _wa;
};

static KrzAlgCfg getAlgConfig(int algID) {
  switch (algID) {
    //               F 1 2 3 4
    case 1:
      return {1, 3, 0, 0, 1};
      break;
    case 2:
      return {1, 2, 0, 1, 1};
      break;
    case 3:
      return {1, 2, 0, 2, 0};
      break;
    case 4:
      return {1, 2, 0, 1, 1};
      break;
    case 5:
      return {1, 2, 0, 1, 1};
      break;
    case 6:
      return {1, 2, 0, 1, 1};
      break;
    case 7:
      return {1, 2, 0, 1, 1};
      break;
    case 8:
      return {1, 1, 1, 1, 1};
      break;
    case 9:
      return {1, 1, 1, 1, 1};
      break;
    case 10:
      return {1, 1, 1, 1, 1};
      break;

    //               F 1 2 3 4
    case 11:
      return {1, 1, 1, 1, 1};
      break;
    case 12:
      return {1, 1, 1, 1, 1};
      break;
    case 13:
      return {1, 1, 1, 1, 1};
      break;

    //               F 1 2 3 4
    case 14:
      return {1, 1, 1, 2, 0};
      break;
    case 15:
      return {1, 1, 1, 2, 0};
      break;

    case 16:
      return {1, 1, 2, 0, 1};
      break;
    case 17:
      return {1, 1, 2, 0, 1};
      break;
    case 18:
      return {1, 1, 2, 0, 1};
      break;
    case 19:
      return {1, 1, 2, 0, 1};
      break;

    //               F 1 2 3 4
    case 20:
      return {1, 1, 1, 1, 1};
      break;
    case 21:
      return {1, 1, 1, 1, 1};
      break;
    case 22:
      return {1, 1, 1, 1, 1};
      break;
    case 23:
      return {1, 1, 1, 1, 1};
      break;
    case 24:
      return {1, 1, 1, 1, 1};
      break;

    case 25:
      return {1, 1, 1, 2, 0};
      break;

    //               F 1 2 3 4
    case 26:
      return {0, 1, 1, 1, 1};
      break;
    case 27:
      return {0, 1, 1, 1, 1};
      break;
    case 28:
      return {0, 1, 1, 1, 1};
      break;
    case 29:
      return {0, 1, 1, 1, 1};
      break;
    case 30:
      return {0, 1, 1, 1, 1};
      break;

    case 31:
      return {0, 1, 1, 2, 0};
      break;
  }

  return KrzAlgCfg();
}
///////////////////////////////////////////////////////////////////////////////

DspParamData::DspParamData() {
  useDefaultEvaluator();
}

ork::svar16_t DspBlockData::getExtData(const std::string& name) const {
  auto it = _extdata.find(name);
  return (it == _extdata.end()) ? ork::svar16_t() : it->second;
}

///////////////////////////////////////////////////////////////////////////////

KeyMap* VastObjectsDB::parseKeymap(int kmid, const Value& jsonobj) {
  auto kmapout = new KeyMap;

  kmapout->_kmID = kmid;
  kmapout->_name = jsonobj["Keymap"].GetString();
  // printf( "Got Keymap name<%s>\n", kmapout->_name.c_str() );

  const auto& jsonrgns = jsonobj["regions"];
  assert(jsonrgns.IsArray());

  for (SizeType i = 0; i < jsonrgns.Size(); i++) // Uses SizeType instead of size_t
  {
    const auto& jsonrgn = jsonrgns[i];

    auto kmr         = new kmregion;
    kmr->_lokey      = jsonrgn["loKey"].GetInt() + 12;
    kmr->_hikey      = jsonrgn["hiKey"].GetInt() + 12;
    kmr->_lovel      = jsonrgn["loVel"].GetInt();
    kmr->_hivel      = jsonrgn["hiVel"].GetInt();
    kmr->_tuning     = jsonrgn["tuning"].GetInt();
    kmr->_volAdj     = jsonrgn["volAdj"].GetFloat();
    kmr->_multsampID = jsonrgn["multiSampleID"].GetInt();
    kmr->_sampID     = jsonrgn["subSampleID"].GetInt();

    // printf( "kmr->_msID<%d>\n", kmr->_multsampID );
    // printf( "kmr->_sampID<%d>\n", kmr->_sampID );
    kmr->_sampleName = jsonrgn["sampleName"].GetString();

    kmr->_linGain = decibel_to_linear_amp_ratio(kmr->_volAdj);

    kmapout->_regions.push_back(kmr);
  }
  return kmapout;
}

///////////////////////////////////////////////////////////////////////////////

sample* VastObjectsDB::parseSample(const Value& jsonobj, const multisample* parent) {
  auto sout           = new sample;
  sout->_sampleBlock  = getK2V3InternalSoundBlock();
  sout->_name         = jsonobj["subSampleName"].GetString();
  sout->_subid        = jsonobj["subSampleIndex"].GetInt();
  sout->_rootKey      = jsonobj["rootKey"].GetInt();
  sout->_highestPitch = jsonobj["highestPitch"].GetInt();

  sout->_blk_start     = jsonobj["uStart"].GetInt();
  sout->_blk_alt       = jsonobj["uAlt"].GetInt();
  sout->_blk_loopstart = jsonobj["uLoop"].GetInt();
  sout->_blk_loopend   = jsonobj["uEnd"].GetInt();
  sout->_blk_end       = jsonobj["uEnd"].GetInt();

  std::string pbmode = jsonobj["playbackMode"].GetString();
  bool isloop        = jsonobj["isLooped"].GetBool();

  if (pbmode == "Normal")
    sout->_loopMode = isloop ? eLoopMode::FWD : eLoopMode::NONE;
  else if (pbmode == "Reverse")
    sout->_loopMode = isloop ? eLoopMode::FWD : eLoopMode::NONE;
  else {
    printf("pbmode<%s>\n", pbmode.c_str());
    assert(false);
  }

  float sgain    = jsonobj["volAdjust"].GetFloat();
  sout->_linGain = decibel_to_linear_amp_ratio(sgain);

  // natenv
  const auto& jsonnatEnv = jsonobj["natEnv"];

  const auto& jsonneslopes = jsonnatEnv["segSlope (dB/sec)"];
  const auto& jsonnetimes  = jsonnatEnv["segTime (sec)"];
  assert(jsonneslopes.IsArray());
  assert(jsonnetimes.IsArray());
  assert(jsonneslopes.Size() == jsonnetimes.Size());
  int numsegs = jsonneslopes.Size();
  sout->_natenv.resize(numsegs);

  for (SizeType i = 0; i < numsegs; i++) // Uses SizeType instead of size_t
  {
    auto& dest  = sout->_natenv[i];
    dest._slope = jsonneslopes[i].GetFloat();
    dest._time  = jsonnetimes[i].GetFloat();
  }

  //

  int parid = parent->_objid;

  auto fname = ork::FormatString("samples/%03d_%s_%d.aiff", parid, parent->_name.c_str(), sout->_subid);

  // printf( "fname<%s>\n", fname.c_str() );

  // sout->load(fname);

  sout->_sampleRate = jsonobj["sampleRate"].GetFloat();

  return sout;
}

///////////////////////////////////////////////////////////////////////////////

multisample* VastObjectsDB::parseMultiSample(const Value& jsonobj) {
  auto msout    = new multisample;
  msout->_name  = jsonobj["MultiSample"].GetString();
  msout->_objid = jsonobj["objectID"].GetInt();
  // printf( "Got MultiSample name<%s>\n", msout->_name.c_str() );
  const auto& jsonsamps = jsonobj["samples"];
  assert(jsonsamps.IsArray());

  for (SizeType i = 0; i < jsonsamps.Size(); i++) // Uses SizeType instead of size_t
  {
    auto s                     = parseSample(jsonsamps[i], msout);
    msout->_samples[s->_subid] = s;
  }
  return msout;
}

///////////////////////////////////////////////////////////////////////////////

AsrData* VastObjectsDB::parseAsr(const rapidjson::Value& jo, const std::string& name) {
  auto aout      = new AsrData;
  aout->_trigger = jo["trigger"].GetString();
  aout->_mode    = jo["mode"].GetString();
  aout->_delay   = jo["delay"].GetFloat();
  aout->_attack  = jo["attack"].GetFloat();
  aout->_release = jo["release"].GetFloat();
  aout->_name    = name;

  return aout;
}

///////////////////////////////////////////////////////////////////////////////

LfoData* VastObjectsDB::parseLfo(const rapidjson::Value& jo, const std::string& name) {
  auto lout           = new LfoData;
  lout->_initialPhase = jo["phase"].GetFloat();
  lout->_shape        = jo["shape"].GetString();
  lout->_controller   = jo["rateCtl"].GetString();
  lout->_minRate      = jo["minRate(hz)"].GetFloat();
  lout->_maxRate      = jo["maxRate(hz)"].GetFloat();
  lout->_name         = name;

  return lout;
}

///////////////////////////////////////////////////////////////////////////////

FunData* VastObjectsDB::parseFun(const rapidjson::Value& jo, const std::string& name) {
  auto out   = new FunData;
  out->_a    = jo["a"].GetString();
  out->_b    = jo["b"].GetString();
  out->_op   = jo["op"].GetString();
  out->_name = name;
  return out;
}

int NoteFromString(const std::string& snote) {
  int note = -1;
  if (snote == std::string("C"))
    note = 0;
  if (snote == std::string("C#"))
    note = 1;
  if (snote == std::string("D"))
    note = 2;
  if (snote == std::string("D#"))
    note = 3;
  if (snote == std::string("E"))
    note = 4;
  if (snote == std::string("F"))
    note = 5;
  if (snote == std::string("F#"))
    note = 6;
  if (snote == std::string("G"))
    note = 7;
  if (snote == std::string("G#"))
    note = 8;
  if (snote == std::string("A"))
    note = 9;
  if (snote == std::string("A#"))
    note = 10;
  if (snote == std::string("B"))
    note = 11;
  return note;
}

///////////////////////////////////////////////////////////////////////////////

void VastObjectsDB::parseFBlock(const Value& fseg, DspParamData& fblk) {
  //////////////////////////////////

  if (fseg.HasMember("PARAM_SCHEME")) {
    auto scheme = fseg["PARAM_SCHEME"].GetString();
    if (scheme == "PCH")
      fblk.usePitchEvaluator();
    else if (scheme == "AMP")
      fblk.useAmplitudeEvaluator();
    else if (scheme == "FRQ")
      fblk.useFrequencyEvaluator();
    else if (scheme == "POS")
      fblk.useKrzPosEvaluator();
    else if (scheme == "EVN")
      fblk.useKrzEvnOddEvaluator();
    else if (scheme == "ODD")
      fblk.useKrzEvnOddEvaluator();
    else
      fblk.useDefaultEvaluator();
  }

  if (fseg.HasMember("KeyTrack"))
    fblk._keyTrack = fseg["KeyTrack"]["Value"].GetFloat();
  if (fseg.HasMember("VelTrack"))
    fblk._velTrack = fseg["VelTrack"]["Value"].GetFloat();
  float keytrack = fblk._keyTrack;
  float veltrack = fblk._velTrack;

  //////////////////////////////////

  if (fseg.HasMember("Coarse")) {
    auto& c = fseg["Coarse"];
    assert(c.HasMember("Value"));
    fblk._units = c["Unit"].GetString();
    auto& v     = c["Value"];
    switch (v.GetType()) {
      case kNumberType: {
        fblk._coarse = v.GetFloat();
        break;
      }
      case kStringType: // FRQ ?
      {
        auto toks    = ork::SplitString(v.GetString(), ' ');
        auto snote   = toks[0];
        auto soct    = toks[1];
        int inote    = NoteFromString(snote);
        int ioct     = atoi(soct.c_str()) + 1;
        int midinote = ioct * 12 + inote;
        float frq    = midi_note_to_frequency(midinote);
        fblk._coarse = frq;
        // printf( "v.GetString() %s\n", v.GetString() );
        // printf( "inote<%d> ioct<%d> midinote<%d> frq<%f>\n", inote, ioct, midinote, frq );
        // assert(false);
        assert(c["Unit"] == "nt");
        // note/cent evaluator
        // GenFRQ(fblk);
        break;
      }
      default:
        assert(false);
        break;
    }
  }
  if (fseg.HasMember("Fine") and fseg["Fine"].IsObject()) {
    fblk._fine = fseg["Fine"]["Value"].GetFloat();
    // printf( "fine<%f>\n", fblk._fine );
    // assert(false);
  }
  if (fseg.HasMember("FineHZ") and fseg["FineHZ"].IsNumber())
    fblk._fineHZ = fseg["FineHZ"].GetFloat();
  if (fseg.HasMember("Src1")) {
    auto& s1         = fseg["Src1"];
    fblk._mods._src1 = s1["Source"].GetString();
    if (s1.HasMember("Depth")) {
      auto& d               = s1["Depth"];
      fblk._mods._src1Depth = d["Value"].GetFloat();
    }
  }
  if (fseg.HasMember("Src2")) {
    auto& s          = fseg["Src2"];
    fblk._mods._src2 = s["Source"].GetString();
    if (s.HasMember("DepthControl"))
      fblk._mods._src2DepthCtrl = s["DepthControl"].GetString();
    if (s.HasMember("MinDepth"))
      fblk._mods._src2MinDepth = s["MinDepth"]["Value"].GetFloat();
    if (s.HasMember("MaxDepth"))
      fblk._mods._src2MaxDepth = s["MaxDepth"]["Value"].GetFloat();
  }
  if (fseg.HasMember("KeyStart")) {
    auto& i               = fseg["KeyStart"];
    fblk._keystartBipolar = i["Mode"].GetString() == std::string("Bipolar");
    int inote             = NoteFromString(i["Note"].GetString());
    int ioct              = i["Octave"].GetInt();
    fblk._keystartNote    = (ioct + 1) * 12 + inote;
  }

  //////////////////////////////////

  //////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

dspblkdata_ptr_t VastObjectsDB::parseDspBlock(const Value& dseg, lyrdata_ptr_t layd, bool force) {
  dspblkdata_ptr_t rval;
  if (dseg.HasMember("BLOCK_ALG ")) {
    rval            = std::make_shared<DspBlockData>();
    rval->_dspBlock = dseg["BLOCK_ALG "].GetString();
    printf("rval._dspBlock<%s>\n", rval->_dspBlock.c_str());
  } else if (force) {
    rval = std::make_shared<DspBlockData>();
  }
  if (rval && dseg.HasMember("Pad")) {
    rval->_inputPad = decibel_to_linear_amp_ratio(dseg["Pad"].GetFloat());
  }
  if (rval && dseg.HasMember("Var15")) {
    int var15 = dseg["Var15"]["Value"].GetInt();

    int pan = (var15 & 0xf0) >> 4;
    if (pan >= 9)
      pan = pan - 16;

    int panMode = (var15 & 0x0c) >> 2;

    switch (rval->_blockIndex) {
      case 3: // upper
        layd->_channelPans[1]     = pan;
        layd->_channelPanModes[1] = panMode;
        break;
      case 4: // lower
        layd->_channelPans[0]     = pan;
        layd->_channelPanModes[0] = panMode;
        break;
      default:
        break;
    }
  }
  if (rval && dseg.HasMember("Var14")) { // assert(false);

    int v14  = dseg["Var14"]["Value"].GetInt();
    int ggg  = v14 & 7;
    int gain = 30 - (ggg * 6);
    switch (rval->_blockIndex) {
      case 3: // upper
        layd->_channelGains[1] = gain;
        break;
      case 4: // lower
        layd->_channelGains[0] = gain;
        break;
      default:
        break;
    }
  }
  // parseFBlock(dseg,rval);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

dspblkdata_ptr_t VastObjectsDB::parsePchBlock(const Value& pseg, lyrdata_ptr_t layd) {
  auto dblk = parseDspBlock(pseg, layd, true);

  if (nullptr == dblk)
    return dblk;

  const auto& KMP = layd->_kmpBlock;
  if (KMP._pbMode == "Noise") {
    dblk->_dspBlock = "NOISE";
    dblk->_paramd[0].usePitchEvaluator();
  } else {
    SAMPLER::initBlock(dblk);
  }
  return dblk;
}

///////////////////////////////////////////////////////////////////////////////

void VastObjectsDB::parseKmpBlock(const Value& kmseg, KmpBlockData& kmblk) {
  kmblk._keyTrack    = kmseg["KeyTrack(ct)"].GetInt();
  kmblk._transpose   = kmseg["transposeTS(st)"].GetInt();
  kmblk._timbreShift = kmseg["timbreshift"].GetInt();
  kmblk._pbMode      = kmseg["pbmode"].GetString();
}

///////////////////////////////////////////////////////////////////////////////

lyrdata_ptr_t VastObjectsDB::parseLayer(const Value& jsonobj, ProgramData* pd) {
  const auto& name = pd->_name;
  printf("Got Prgram<%s> layer..\n", name.c_str());
  const auto& calvinSeg = jsonobj["CALVIN"];
  const auto& keymapSeg = calvinSeg["KEYMAP"];
  const auto& pitchSeg  = calvinSeg["PITCH"];
  const auto& miscSeg   = jsonobj["misc"];

  int kmid = keymapSeg["km1"].GetInt();
  // printf( "find KMID<%d>\n", kmid );
  auto it = _tempkeymaps.find(kmid);
  if (it == _tempkeymaps.end()) {
    it = _keymaps.find(kmid);
    assert(it != _keymaps.end());
  }

  auto rval     = pd->newLayer();
  auto km       = it->second;
  rval->_keymap = km;

  parseKmpBlock(keymapSeg, rval->_kmpBlock);

  rval->_loKey = jsonobj["loKey"].GetInt();
  rval->_hiKey = jsonobj["hiKey"].GetInt();
  rval->_loVel = jsonobj["loVel"].GetInt() * 18;
  rval->_hiVel = 1 + (jsonobj["hiVel"].GetInt() * 18);
  if (rval->_loVel == 0 && rval->_hiVel == 0)
    rval->_hiVel = 127;

  rval->_ignRels  = miscSeg["ignRels"].GetBool();
  rval->_atk1Hold = miscSeg["atkHold"].GetBool();
  rval->_atk3Hold = miscSeg["susHold"].GetBool();

  parseAlg(jsonobj, *(rval->_algdata));

  //////////////////////////////////////////////////////

  const auto& envcSeg = jsonobj["ENVCTRL"];
  parseEnvControl(envcSeg, rval->_envCtrlData);

  auto parseEnv = [&](const Value& envobj, const std::string& name) -> RateLevelEnvData* {
    auto rout                 = new RateLevelEnvData;
    RateLevelEnvData& destenv = *rout;
    rout->_name               = name;
    if (name == "AMPENV") {
      rout->_ampenv  = true;
      rout->_envType = RlEnvType::ERLTYPE_KRZAMPENV;
    } else {
      rout->_bipolar = true;
      rout->_envType = RlEnvType::ERLTYPE_KRZMODENV;
    }
    //////////////////////////////////////////
    const auto& jsonrates = envobj["rates"];
    assert(jsonrates.IsArray());
    int inumrates = jsonrates.Size();
    assert(inumrates == 7);
    std::vector<float> rates;
    for (SizeType i = 0; i < inumrates; i++) // Uses SizeType instead of size_t
      rates.push_back(jsonrates[i].GetFloat());
    //////////////////////////////////////////
    const auto& jsonlevels = envobj["levels"];
    assert(jsonlevels.IsArray());
    int inumlevels = jsonlevels.Size();
    assert(inumlevels == 7);
    std::vector<float> levels;
    for (SizeType i = 0; i < inumlevels; i++) // Uses SizeType instead of size_t
      levels.push_back(jsonlevels[i].GetFloat());
    //////////////////////////////////////////
    for (int i = 0; i < 7; i++) {
      EnvPoint ep;
      ep._rate  = rates[i];
      ep._level = levels[i];
      destenv._segments.push_back(ep);
    }
    //////////////////////////////////////////
    return rout;
  };
  //////////////////////////////////////////////////////
  auto CB0             = new ControlBlockData; // ENV
  auto CB1             = new ControlBlockData; // ASRLFO
  auto CB2             = new ControlBlockData; // FUN
  rval->_ctrlBlocks[0] = CB0;
  rval->_ctrlBlocks[1] = CB1;
  rval->_ctrlBlocks[2] = CB2;
  //////////////////////////////////////////////////////
  // kurzweil only has 3 envs per layer!
  //////////////////////////////////////////////////////
  if (jsonobj.HasMember("AMPENV")) {
    const auto& seg = jsonobj["AMPENV"];
    if (seg.IsObject())
      CB0->_cdata[0] = parseEnv(seg, "AMPENV");
  }
  if (jsonobj.HasMember("ENV2")) {
    const auto& seg = jsonobj["ENV2"];
    if (seg.IsObject())
      CB0->_cdata[1] = parseEnv(seg, "ENV2");
  }
  if (jsonobj.HasMember("ENV3")) {
    const auto& seg = jsonobj["ENV3"];
    if (seg.IsObject())
      CB0->_cdata[2] = parseEnv(seg, "ENV3");
  }
  //////////////////////////////////////////////////////
  if (jsonobj.HasMember("ASR1")) {
    const auto& seg = jsonobj["ASR1"];
    if (seg.IsObject())
      CB1->_cdata[0] = (const AsrData*)parseAsr(seg, "ASR1");
  }
  if (jsonobj.HasMember("ASR2")) {
    const auto& seg = jsonobj["ASR2"];
    if (seg.IsObject())
      CB1->_cdata[1] = (const AsrData*)parseAsr(seg, "ASR2");
  }
  //////////////////////////////////////////////////////
  if (jsonobj.HasMember("LFO1")) {
    const auto& lfo1seg = jsonobj["LFO1"];
    if (lfo1seg.IsObject())
      CB1->_cdata[2] = (const LfoData*)parseLfo(lfo1seg, "LFO1");
  }
  if (jsonobj.HasMember("LFO2")) {
    const auto& lfo2seg = jsonobj["LFO2"];
    if (lfo2seg.IsObject())
      CB1->_cdata[3] = (const LfoData*)parseLfo(lfo2seg, "LFO2");
  }
  //////////////////////////////////////////////////////
  if (jsonobj.HasMember("FUN1")) {
    const auto& seg = jsonobj["FUN1"];
    if (seg.IsObject())
      CB2->_cdata[0] = (const FunData*)parseFun(seg, "FUN1");
  }
  if (jsonobj.HasMember("FUN2")) {
    const auto& seg = jsonobj["FUN2"];
    if (seg.IsObject())
      CB2->_cdata[1] = (const FunData*)parseFun(seg, "FUN2");
  }
  if (jsonobj.HasMember("FUN3")) {
    const auto& seg = jsonobj["FUN3"];
    if (seg.IsObject())
      CB2->_cdata[2] = (const FunData*)parseFun(seg, "FUN3");
  }
  if (jsonobj.HasMember("FUN4")) {
    const auto& seg = jsonobj["FUN4"];
    if (seg.IsObject())
      CB2->_cdata[3] = (const FunData*)parseFun(seg, "FUN4");
  }
  //////////////////////////////////////////////////////

  if (rval->_algdata->_krzAlgIndex == 0)
    return rval;

  assert(rval->_algdata->_krzAlgIndex >= 1);
  assert(rval->_algdata->_krzAlgIndex <= 31);
  const KrzAlgCfg ACFG = getAlgConfig(rval->_algdata->_krzAlgIndex);

  auto blkname = [](int bid) -> const char* {
    switch (bid) {
      case 0:
        return "PITCH";
      case 1:
        return "F1";
      case 2:
        return "F2";
      case 3:
        return "F3";
      case 4:
        return "F4AMP";
      default:
        return "???";
    }
  };
  auto do_block = [&](int blkbase, int paramcount) -> dspblkdata_ptr_t {
    dspblkdata_ptr_t dspblock;

    auto blockn1 = blkname(blkbase + 0);
    auto blockn2 = blkname(blkbase + 1);
    auto blockn3 = blkname(blkbase + 2);

    printf("algd<%d> blkbase<%d> paramcount<%d> blockn1<%s>\n", rval->_algdata->_krzAlgIndex, blkbase, paramcount, blockn1);

    if (0 == blkbase) {
      dspblock             = parsePchBlock(pitchSeg, rval);
      dspblock->_numParams = 1;
      parseFBlock(pitchSeg, dspblock->_paramd[0]);
    } else if (jsonobj.HasMember(blockn1)) {
      dspblock = parseDspBlock(jsonobj[blockn1], rval);

      if (dspblock == nullptr)
        return nullptr;

      dspblock->_numParams = paramcount;

      switch (paramcount) {
        case 0:
          break;
        case 1: {
          if (jsonobj.HasMember(blockn1)) {
            parseFBlock(jsonobj[blockn1], dspblock->_paramd[0]);
          }
          break;
        }
        case 2: {
          if (jsonobj.HasMember(blockn1))
            parseFBlock(jsonobj[blockn1], dspblock->_paramd[0]);
          if (jsonobj.HasMember(blockn2))
            parseFBlock(jsonobj[blockn2], dspblock->_paramd[1]);
          break;
        }
        case 3: {
          if (jsonobj.HasMember(blockn1))
            parseFBlock(jsonobj[blockn1], dspblock->_paramd[0]);
          if (jsonobj.HasMember(blockn2))
            parseFBlock(jsonobj[blockn2], dspblock->_paramd[1]);
          if (jsonobj.HasMember(blockn3))
            parseFBlock(jsonobj[blockn3], dspblock->_paramd[2]);
          break;
        }
        default:
          assert(false);
      }
    }
    return dspblock;
  };
  int blockindex = 0;

  printf("ACFG._wp<%d>\n", ACFG._wp);
  printf("ACFG._w1<%d>\n", ACFG._w1);
  printf("ACFG._w2<%d>\n", ACFG._w2);
  printf("ACFG._w3<%d>\n", ACFG._w3);
  printf("ACFG._wa<%d>\n", ACFG._wa);

  int outindex = 0;
  OrkAssert(false);
  /* replace with dspstage method
  if (ACFG._wp)
    rval->_dspBlocks[outindex++] = do_block(blockindex, ACFG._wp);
  blockindex += ACFG._wp;
  if (ACFG._w1)
    rval->_dspBlocks[outindex++] = do_block(blockindex, ACFG._w1);
  blockindex += ACFG._w1;
  if (ACFG._w2)
    rval->_dspBlocks[outindex++] = do_block(blockindex, ACFG._w2);
  blockindex += ACFG._w2;
  if (ACFG._w3)
    rval->_dspBlocks[outindex++] = do_block(blockindex, ACFG._w3);
  blockindex += ACFG._w3;
  if (ACFG._wa)
    rval->_dspBlocks[outindex++] = do_block(blockindex, ACFG._wa);
  blockindex += ACFG._wa;

  if (pd->_name == "Click") {

    for (int i = 0; i < 5; i++) {
      auto blk = rval->_dspBlocks[i];
      if (blk)
        printf("dspblk<%d:%p:%s>\n", i, blk.get(), blk->_dspBlock.c_str());
    }
    // assert(false);
  }

  rval->_pchBlock = rval->_dspBlocks[0];
  */

  //////////////////////////////////////////////////////

  // printf( "got keymapID<%d:%p:%s>\n", kmid, km, km->_name.c_str() );
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void configureKrzAlgorithm(AlgData& algdout);

void VastObjectsDB::parseAlg(const rapidjson::Value& JO, AlgData& algd) {
  const auto& calvin = JO["CALVIN"];
  algd._krzAlgIndex  = calvin["ALG"].GetInt();
  algd._name         = ork::FormatString("ALG%d", algd._krzAlgIndex);
  if (algd._krzAlgIndex != 0)
    configureKrzAlgorithm(algd);
}

///////////////////////////////////////////////////////////////////////////////

void VastObjectsDB::parseEnvControl(const rapidjson::Value& seg, EnvCtrlData& ed) {
  auto aenvmode = seg["ampenv_mode"].GetString();
  ed._useNatEnv = (0 == strcmp(aenvmode, "Natural"));
  ed._atkAdjust = seg["AtkAdjust"].GetFloat();
  ed._decAdjust = seg["DecAdjust"].GetFloat();
  ed._relAdjust = seg["RelAdjust"].GetFloat();

  ed._decKeyTrack = seg["DecKeyTrack"].GetFloat();
  ed._relKeyTrack = seg["RelKeyTrack"].GetFloat();
}

///////////////////////////////////////////////////////////////////////////////

ProgramData* VastObjectsDB::parseProgram(const Value& jsonobj) {
  auto pdata       = new ProgramData;
  pdata->_role     = "Program";
  const auto& name = jsonobj["Program"].GetString();
  // printf( "Got Prgram name<%s>\n", name );
  pdata->_name = name;

  const auto& jsonlays = jsonobj["LAYERS"];
  assert(jsonlays.IsArray());

  for (SizeType i = 0; i < jsonlays.Size(); i++) // Uses SizeType instead of size_t
  {
    auto l = parseLayer(jsonlays[i], pdata);
  }

  return pdata;
}

///////////////////////////////////////////////////////////////////////////////

void VastObjectsDB::loadJson(const std::string& fname, int ibaseid) {
  auto realfname = kbasepath / "kurzweil" / (fname + ".json");
  printf("fname<%s>\n", realfname.c_str());
  FILE* fin = fopen(realfname.c_str(), "rt");
  assert(fin != nullptr);
  fseek(fin, 0, SEEK_END);
  int size = ftell(fin);
  printf("filesize<%d>\n", size);
  fseek(fin, 0, SEEK_SET);
  auto jsondata = (char*)malloc(size + 1);
  fread(jsondata, size, 1, fin);
  jsondata[size] = 0;
  fclose(fin);

  Document document;
  document.Parse(jsondata);
  free((void*)jsondata);

  auto objmap = this;

  assert(document.IsObject());
  assert(document.HasMember("KRZ"));

  const auto& root    = document["KRZ"];
  const auto& objects = root["objects"];
  assert(objects.IsArray());

  _tempprograms.clear();
  _tempkeymaps.clear();
  _tempmultisamples.clear();

  for (SizeType i = 0; i < objects.Size(); i++) // Uses SizeType instead of size_t
  {
    const auto& obj = objects[i];
    int objid       = obj["objectID"].GetInt();
    if (obj.HasMember("Keymap")) {
      auto km             = parseKeymap(objid, obj);
      _tempkeymaps[objid] = km;
    } else if (obj.HasMember("MultiSample")) {
      auto ms                  = parseMultiSample(obj);
      _tempmultisamples[objid] = ms;
    } else if (obj.HasMember("Program")) {
      auto p               = this->parseProgram(obj);
      _tempprograms[objid] = p;
      p->_role             = "k2000(" + fname + ")";
    } else {
      assert(false);
    }
  }

  /////////////////////////////////////////
  // fill

  for (auto it : _tempprograms) {
    int objid        = it.first;
    ProgramData* prg = it.second;
    auto it2         = objmap->_programs.find(objid);
    if (it2 != objmap->_programs.end()) {
      int nid = (objmap->_programs.rbegin()->first + 1);
      // nid = nid-(nid%
      printf("REMAP PROG<%d> -> <%d>\n", objid, nid);
      objid = nid;
    }
    objmap->_programs[objid] = prg;

    // TODO - remap duplicates (by name)
    objmap->_programsByName[prg->_name] = prg;
  }
  for (auto it : _tempkeymaps) {
    int objid               = it.first;
    KeyMap* km              = it.second;
    objmap->_keymaps[objid] = km;
    for (auto kr : km->_regions) {
      int msid  = kr->_multsampID;
      auto msit = _tempmultisamples.find(msid);
      if (msit == _tempmultisamples.end()) {
        msit = _multisamples.find(msid);
        assert(msit != _multisamples.end());
      }

      kr->_multiSample = msit->second;
      int sid          = kr->_sampID;
      auto& samplemap  = kr->_multiSample->_samples;
      auto sit         = samplemap.find(sid);
      if (sit == samplemap.end()) {
        // printf( "sample<%d> not found in multisample<%d>\n", sid, msid );
      } else {
        auto s      = sit->second;
        kr->_sample = s;
        // printf( "found sample<%d:%s> in multisample<%d>\n", sid, s->_name.c_str(), msid );
      }
    }
  }
  for (auto it : _tempmultisamples) {
    int objid                    = it.first;
    multisample* ms              = it.second;
    objmap->_multisamples[objid] = ms;
  }

  /////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

const ProgramData* VastObjectsDB::findProgram(int progID) const {
  ProgramData* pd = nullptr;
  auto it         = _programs.find(progID);
  if (it == _programs.end()) {
    return _programs.begin()->second;
  }
  assert(it != _programs.end());
  pd = it->second;
  return pd;
}

const ProgramData* VastObjectsDB::findProgramByName(const std::string named) const {
  ProgramData* pd = nullptr;
  auto it         = _programsByName.find(named);
  if (it == _programsByName.end()) {
    return _programsByName.begin()->second;
  }
  assert(it != _programsByName.end());
  pd = it->second;
  return pd;
}

///////////////////////////////////////////////////////////////////////////////

const KeyMap* VastObjectsDB::findKeymap(int kmID) const {
  KeyMap* kd = nullptr;
  auto it    = _keymaps.find(kmID);
  if (it != _keymaps.end())
    kd = it->second;
  return kd;
}

} // namespace ork::audio::singularity
