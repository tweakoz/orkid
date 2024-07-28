////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_filters.h>
#include <ork/lev2/aud/singularity/alg_nonlin.h>
#include <ork/lev2/aud/singularity/alg_eq.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/alg_pan.inl>
#include <ork/lev2/aud/singularity/sampler.h>

using namespace rapidjson;

namespace ork::audio::singularity {

struct SoundBlockData {

  SoundBlockData() {
    _romDATA       = (s16*)malloc(24 << 20);
    auto data_read = _romDATA;
    ///////////////////////////////////////////////////////////////////////////////////////////
    auto load_sound_block = [&](file::Path filename, size_t numbytes) {
      // printf("Loading Soundblock<%s>\n", filename.c_str());
      FILE* fin = fopen(filename.toAbsolute().c_str(), "rb");
      if (fin == nullptr) {
        printf("You will need the K2000 ROM sampledata at <%s> to use this method!\n", filename.c_str());
        OrkAssert(false);
      }
      fread(data_read, numbytes, 1, fin);
      data_read += (numbytes >> 1);
      fclose(fin);
    };
    ///////////////////////////////////////////////////////////////////////////////////////////
    load_sound_block(basePath() / "kurzweil" / "k2vx_samples_base.bin", 8 << 20);
    load_sound_block(basePath() / "kurzweil" / "k2vx_samples_ext1.bin", 8 << 20);
    load_sound_block(basePath() / "kurzweil" / "k2vx_samples_ext2.bin", 8 << 20);
  }
  s16* _romDATA = nullptr;
};

const s16* getK2V3InternalSoundBlock() {
  static auto romblocks = std::make_shared<SoundBlockData>();
  return romblocks->_romDATA;
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

keymap_ptr_t KrzBankDataParser::parseKeymap(int kmid, const Value& jsonobj) {
  auto kmapout = std::make_shared<KeyMapData>();

  kmapout->_kmID = kmid;
  kmapout->_name = jsonobj["Keymap"].GetString();
  // printf( "Got Keymap name<%s>\n", kmapout->_name.c_str() );

  const auto& jsonrgns = jsonobj["regions"];
  OrkAssert(jsonrgns.IsArray());

  for (SizeType i = 0; i < jsonrgns.Size(); i++) // Uses SizeType instead of size_t
  {
    const auto& jsonrgn = jsonrgns[i];

    auto kmr         = std::make_shared<KmRegionData>();
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

sample_ptr_t KrzBankDataParser::parseSample(const Value& jsonobj, multisample_constptr_t parent) {
  auto sout           = std::make_shared<SampleData>();

  sout->_sampleBlock  = _sampledata;

  sout->_name         = jsonobj["subSampleName"].GetString();
  sout->_subid        = jsonobj["subSampleIndex"].GetInt();
  sout->_rootKey      = jsonobj["rootKey"].GetInt();
  sout->_highestPitch = jsonobj["highestPitch"].GetInt();

  sout->_blk_start     = jsonobj["uStart"].GetInt();
  sout->_blk_alt       = jsonobj["uAlt"].GetInt();
  sout->_blk_loopstart = jsonobj["uLoop"].GetInt();
  sout->_blk_loopend   = jsonobj["uEnd"].GetInt();
  sout->_blk_end       = jsonobj["uEnd"].GetInt();

  if(0)printf( "sample<%s> start<%d> alt<%d> loopstart<%d> loopend<%d> end<%d>\n",
          sout->_name.c_str(),
          sout->_blk_start,
          sout->_blk_alt,
          sout->_blk_loopstart,
          sout->_blk_loopend,
          sout->_blk_end );

  int orchestral_base     = 16 << 20;
  int contemporary_base   = 20 << 20;
  int orchestral_offset   = orchestral_base - (4 << 20);
  int contemporary_offset = contemporary_base - (8 << 20);

  if (sout->_blk_start >= contemporary_base) {
    sout->_blk_start -= contemporary_offset;
    sout->_blk_alt -= contemporary_offset;
    sout->_blk_loopstart -= contemporary_offset;
    sout->_blk_loopend -= contemporary_offset;
    sout->_blk_end -= contemporary_offset;
  } else if (sout->_blk_start >= orchestral_base) {
    sout->_blk_start -= orchestral_offset;
    sout->_blk_alt -= orchestral_offset;
    sout->_blk_loopstart -= orchestral_offset;
    sout->_blk_loopend -= orchestral_offset;
    sout->_blk_end -= orchestral_offset;
  }

  std::string pbmode = jsonobj["playbackMode"].GetString();
  bool isloop        = jsonobj["isLooped"].GetBool();

  if (pbmode == "Normal")
    sout->_loopMode = isloop ? eLoopMode::FWD : eLoopMode::NONE;
  else if (pbmode == "Reverse")
    sout->_loopMode = isloop ? eLoopMode::FWD : eLoopMode::NONE;
  else if (pbmode == "Bidirectional")
    sout->_loopMode = isloop ? eLoopMode::BIDIR : eLoopMode::NONE;
  else {
    printf("pbmode<%s>\n", pbmode.c_str());
    // OrkAssert(false);
  }

  float sgain    = jsonobj["volAdjust"].GetFloat();
  sout->_linGain = decibel_to_linear_amp_ratio(sgain);

  // natenv
  const auto& jsonnatEnv = jsonobj["natEnv"];

  const auto& jsonneslopes = jsonnatEnv["segSlope (dB/sec)"];
  const auto& jsonnetimes  = jsonnatEnv["segTime (sec)"];
  OrkAssert(jsonneslopes.IsArray());
  OrkAssert(jsonnetimes.IsArray());
  OrkAssert(jsonneslopes.Size() == jsonnetimes.Size());
  int numsegs = jsonneslopes.Size();

  if (numsegs > 0) {

    auto natenv   = std::make_shared<NatEnvWrapperData>();
    natenv->_name = "AMPENV";
    natenv->_segments.resize(numsegs);

    for (SizeType i = 0; i < numsegs; i++) // Uses SizeType instead of size_t
    {
      auto& dest  = natenv->_segments[i];
      dest._slope = jsonneslopes[i].GetFloat();
      dest._time  = jsonnetimes[i].GetFloat();
    }

    sout->_naturalEnvelope = natenv;
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

multisample_ptr_t KrzBankDataParser::parseMultiSample(const Value& jsonobj) {
  auto msout    = std::make_shared<MultiSampleData>();
  msout->_name  = jsonobj["MultiSample"].GetString();
  msout->_objid = jsonobj["objectID"].GetInt();
  // printf( "Got MultiSample name<%s>\n", msout->_name.c_str() );
  const auto& jsonsamps = jsonobj["samples"];
  OrkAssert(jsonsamps.IsArray());

  for (SizeType i = 0; i < jsonsamps.Size(); i++) // Uses SizeType instead of size_t
  {
    auto s                          = parseSample(jsonsamps[i], msout);
    msout->_samples[s->_subid]      = s;
    msout->_samplesByName[s->_name] = s;
  }
  return msout;
}

///////////////////////////////////////////////////////////////////////////////

void KrzBankDataParser::parseAsr(
    const rapidjson::Value& jo, //
    controlblockdata_ptr_t cblock,
    const EnvCtrlData& ENVCTRL,
    const std::string& name) {
  auto aout      = cblock->addController<AsrData>(name);
  aout->_trigger = jo["trigger"].GetString();
  aout->_mode    = jo["mode"].GetString();
  aout->_delay   = jo["delay"].GetFloat();
  aout->_attack  = jo["attack"].GetFloat();
  aout->_release = jo["release"].GetFloat();

  aout->_envadjust = [=](const EnvPoint& inp, //
                         int iseg,
                         const KeyOnInfo& KOI) -> EnvPoint { //
    EnvPoint outp = inp;
    int ikey      = KOI._key;

    const auto& RKT = ENVCTRL._relKeyTrack;
    float atkAdjust = ENVCTRL._atkAdjust;
    float relAdjust = ENVCTRL._relAdjust;

    if (ikey > 60) {
      float flerp = float(ikey - 60) / float(127 - 60);
      relAdjust   = lerp(relAdjust, RKT, flerp);
    } else if (ikey < 60) {
      float flerp = float(59 - ikey) / 59.0f;
      relAdjust   = lerp(relAdjust, 1.0 / RKT, flerp);
    }

    switch (iseg) {
      case 0: // delay
      case 1: // atk
        outp._time *= 1.0 / atkAdjust;
        break;
      case 2: // hold
        break;
      case 3: // release
        outp._time *= 1.0 / relAdjust;
        break;
    }
    return outp;
  };
} // namespace ork::audio::singularity

///////////////////////////////////////////////////////////////////////////////

void KrzBankDataParser::parseLfo(const rapidjson::Value& jo, controlblockdata_ptr_t cblock, const std::string& name) {
  auto lout           = cblock->addController<LfoData>(name);
  lout->_initialPhase = jo["phase"].GetFloat();
  lout->_shape        = jo["shape"].GetString();
  lout->_controller   = jo["rateCtl"].GetString();
  lout->_minRate      = jo["minRate(hz)"].GetFloat()*0.5;
  lout->_maxRate      = jo["maxRate(hz)"].GetFloat()*0.5;
}

///////////////////////////////////////////////////////////////////////////////

void KrzBankDataParser::parseFun(
    const rapidjson::Value& jo,    //
    lyrdata_ptr_t layerdata,       //
    controlblockdata_ptr_t cblock, //
    const std::string& name) {
  auto out = cblock->addController<FunData>(name);
  out->_a  = jo["a"].GetString();
  out->_b  = jo["b"].GetString();
  out->_op = jo["op"].GetString();
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

void KrzBankDataParser::parseFBlock(
    const Value& fseg,       //
    lyrdata_ptr_t layerdata, //
    dspparam_ptr_t fblk) {   //

  //////////////////////////////////
  using namespace std::string_literals;

  auto mods = fblk->_mods;
  if (fseg.HasMember("PARAM_SCHEME")) {
  }

  //////////////////////////////////

  if (fseg.HasMember("Coarse")) {
    auto& c = fseg["Coarse"];
    OrkAssert(c.HasMember("Value"));
    fblk->_units = c["Unit"].GetString();
    auto& v      = c["Value"];
    switch (v.GetType()) {
      case kNumberType: {
        fblk->_coarse = v.GetFloat();
        break;
      }
      case kStringType: // FRQ ?
      {
        auto toks     = ork::SplitString(v.GetString(), ' ');
        auto snote    = toks[0];
        auto soct     = toks[1];
        int inote     = NoteFromString(snote);
        int ioct      = atoi(soct.c_str()) + 1;
        int midinote  = ioct * 12 + inote;
        float frq     = midi_note_to_frequency(midinote);
        fblk->_coarse = frq;
        // printf( "v.GetString() %s\n", v.GetString() );
        // printf( "inote<%d> ioct<%d> midinote<%d> frq<%f>\n", inote, ioct, midinote, frq );
        // assert(false);
        OrkAssert(c["Unit"] == "nt");
        // note/cent evaluator
        // GenFRQ(fblk);
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }
  if (fseg.HasMember("Fine") and fseg["Fine"].IsObject()) {
    fblk->_fine = fseg["Fine"]["Value"].GetFloat();
  }
  if (fseg.HasMember("FineHZ") and fseg["FineHZ"].IsNumber()) {
    fblk->_fineHZ = fseg["FineHZ"].GetFloat();
  }

  bool use_mods = false;
  if (fseg.HasMember("Src1")) {
    auto& s1              = fseg["Src1"];
    std::string ctrl_name = s1["Source"].GetString();
    auto SRC1             = layerdata->_ctrlBlock->controllerByName(ctrl_name);
    mods->_src1           = SRC1;
    if (s1.HasMember("Depth")) {
      auto& d          = s1["Depth"];
      mods->_src1Scale = d["Value"].GetFloat();
    }
    use_mods = true;
  }
  if (fseg.HasMember("Src2")) {
    auto& s2              = fseg["Src2"];
    std::string ctrl_name = s2["Source"].GetString();
    auto SRC2             = layerdata->_ctrlBlock->controllerByName(ctrl_name);
    mods->_src2           = SRC2;
    if (s2.HasMember("DepthControl")) {
      std::string ctrl_name = s2["DepthControl"].GetString();
      auto DEPTHCONTROL     = layerdata->_ctrlBlock->controllerByName(ctrl_name);
      mods->_src2DepthCtrl  = DEPTHCONTROL;
    }
    if (s2.HasMember("MinDepth")) {
      mods->_src2MinDepth = s2["MinDepth"]["Value"].GetFloat();
    }
    if (s2.HasMember("MaxDepth")) {
      mods->_src2MaxDepth = s2["MaxDepth"]["Value"].GetFloat();
    }
    use_mods = true;
  }
  if (fseg.HasMember("KeyTrack")) {
    fblk->_keyTrack      = fseg["KeyTrack"]["Value"].GetFloat();
    fblk->_keyTrackUnits = fseg["KeyTrack"]["Unit"].GetString();
  }
  if (fseg.HasMember("KeyStart")) {
    auto& i                = fseg["KeyStart"];
    fblk->_keystartBipolar = i["Mode"].GetString() == std::string("Bipolar");
    int inote              = NoteFromString(i["Note"].GetString());
    int ioct               = i["Octave"].GetInt();
    fblk->_keystartNote    = (ioct + 1) * 12 + inote;
  }
  if (fseg.HasMember("VelTrack")) {
    fblk->_velTrack      = fseg["VelTrack"]["Value"].GetFloat();
    fblk->_velTrackUnits = fseg["VelTrack"]["Unit"].GetString();
  }
  if (use_mods) {
    fblk->_mods = mods;
  }
  //////////////////////////////////

  //////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

dspblkdata_ptr_t KrzBankDataParser::parseDspBlock(const Value& dseg, dspstagedata_ptr_t stage, lyrdata_ptr_t layd) {
  dspblkdata_ptr_t rval;
  if (dseg.HasMember("BLOCK_ALG ")) {
    std::string blocktype = dseg["BLOCK_ALG "].GetString();
    // rval             = std::make_shared<DspBlockData>();
    // rval->_blocktype = dseg["BLOCK_ALG "].GetString();
    ///////////
    // alg_filters
    ///////////
    if (blocktype == "BANDPASS FILT") {
      rval = stage->appendTypedBlock<BANDPASS_FILT>(blocktype);
    } else if (blocktype == "BAND2") {
      rval = stage->appendTypedBlock<BAND2>(blocktype);
    } else if (blocktype == "NOTCH FILTER") {
      rval = stage->appendTypedBlock<NOTCH_FILT>(blocktype);
    } else if (blocktype == "NOTCH2") {
      rval = stage->appendTypedBlock<NOTCH2>(blocktype);
    } else if (blocktype == "DOUBLE NOTCH W/SEP") {
      rval = stage->appendTypedBlock<DOUBLE_NOTCH_W_SEP>(blocktype);
    } else if (blocktype == "TWIN PEAKS BANDPASS") {
      // TODO FIXME
      rval = stage->appendTypedBlock<DOUBLE_NOTCH_W_SEP>(blocktype);
    } else if (blocktype == "LOPAS2") {
      rval = stage->appendTypedBlock<LOPAS2>(blocktype);
    } else if (blocktype == "LP2RES") {
      rval = stage->appendTypedBlock<LP2RES>(blocktype);
    } else if (blocktype == "LPGATE") {
      rval = stage->appendTypedBlock<LPGATE>(blocktype);
    } else if (blocktype == "4POLE HIPASS W/SEP") {
      rval = stage->appendTypedBlock<FOURPOLE_HIPASS_W_SEP>(blocktype);
    } else if (blocktype == "LPCLIP") {
      rval = stage->appendTypedBlock<LPCLIP>(blocktype);
    } else if (blocktype == "LOPASS") {
      rval = stage->appendTypedBlock<LowPass>(blocktype);
    } else if (blocktype == "HIPASS") {
      rval = stage->appendTypedBlock<HighPass>(blocktype);
    } else if (blocktype == "HIPAS2") {
      // TODO FIXME
      rval = stage->appendTypedBlock<HighPass>(blocktype);
    } else if (blocktype == "ALPASS") {
      rval = stage->appendTypedBlock<AllPass>(blocktype);
    } else if (blocktype == "HIFREQ STIMULATOR") {
      rval = stage->appendTypedBlock<HighFreqStimulator>(blocktype);
    } else if (blocktype == "2POLE LOWPASS") {
      rval = stage->appendTypedBlock<TwoPoleLowPass>(blocktype);
    } else if (blocktype == "2POLE ALLPASS") {
      rval = stage->appendTypedBlock<TwoPoleAllPass>(blocktype);
    } else if (blocktype == "4POLE LOPASS W/SEP") {
      rval = stage->appendTypedBlock<FourPoleLowPassWithSep>(blocktype);
    }
    ///////////
    // alg_eqs
    ///////////
    else if (blocktype == "STEEP RESONANT BASS") {
      rval = stage->appendTypedBlock<STEEP_RESONANT_BASS>(blocktype);
    } else if (blocktype == "PARA BASS") {
      rval = stage->appendTypedBlock<PARABASS>(blocktype);
    } else if (blocktype == "PARA MID") {
      rval = stage->appendTypedBlock<PARAMID>(blocktype);
    } else if (blocktype == "PARA TREBLE") {
      rval = stage->appendTypedBlock<PARATREBLE>(blocktype);
    } else if (blocktype == "PARAMETRIC EQ") {
      rval = stage->appendTypedBlock<ParametricEq>(blocktype);
    }
    ///////////
    // alg_gain
    ///////////
    else if (blocktype == "AMP") {
      rval = stage->appendTypedBlock<AMP_ADAPTIVE>(blocktype);
    } else if (blocktype == "+ AMP") {
      rval = stage->appendTypedBlock<PLUSAMP>(blocktype);
    } else if (blocktype == "+ GAIN") {
      // TODO FIXME
      rval = stage->appendTypedBlock<PLUSAMP>(blocktype);
    } else if (blocktype == "x AMP") {
      rval = stage->appendTypedBlock<XAMP>(blocktype);
    } else if (blocktype == "AMPMOD") {
      // TODO FIXME
      rval = stage->appendTypedBlock<XAMP>(blocktype);
    } else if (blocktype == "AMP MOD OSC") {
      // TODO FIXME
      rval = stage->appendTypedBlock<AMP_MOD_OSC>(blocktype);
    } else if (blocktype == "GAIN") {
      rval = stage->appendTypedBlock<GAIN>(blocktype);
    } else if (blocktype == "x GAIN") {
      // TODO FIXME
      rval = stage->appendTypedBlock<XAMP>(blocktype);
    } else if (blocktype == "! AMP") {
      rval = stage->appendTypedBlock<BANGAMP>(blocktype);
    } else if (blocktype == "AMP U   AMP L") {
      rval = stage->appendTypedBlock<AMPU_AMPL>(blocktype);
    } else if (blocktype == "BAL     AMP") {
      rval = stage->appendTypedBlock<BAL_AMP>(blocktype);
    } else if (blocktype == "XFADE") {
      rval = stage->appendTypedBlock<XFADE>(blocktype);
    } else if (blocktype == "PANNER") {
      rval = stage->appendTypedBlock<PANNER>(blocktype);
    }
    ///////////
    // alg_nonlin
    ///////////
    else if (blocktype == "SHAPER") {
      rval = stage->appendTypedBlock<SHAPER>(blocktype);
    } else if (blocktype == "SHAPE2") {
      rval = stage->appendTypedBlock<SHAPE2>(blocktype);
    } else if (blocktype == "2PARAM SHAPER") {
      rval = stage->appendTypedBlock<TWOPARAM_SHAPER>(blocktype);
    } else if (blocktype == "SHAPER") {
      rval = stage->appendTypedBlock<SHAPER>(blocktype);
    } else if (blocktype == "WRAP") {
      rval = stage->appendTypedBlock<Wrap>(blocktype);
    } else if (blocktype == "DIST") {
      rval = stage->appendTypedBlock<Distortion>(blocktype);
    }
    ///////////
    else if (blocktype == "PITCH") {
      // instantiate sampler ?
      rval = stage->appendTypedBlock<PITCH>(blocktype);
    }
    ///////////
    // alg_oscil
    ///////////
    else if (blocktype == "SW+SHP") {
      rval = stage->appendTypedBlock<SWPLUSSHP>(blocktype);
    } else if (blocktype == "SAW+") {
      rval = stage->appendTypedBlock<SAWPLUS>(blocktype);
    } else if (blocktype == "SINE") {
      rval = stage->appendTypedBlock<SINE>(blocktype);
    } else if (blocktype == "LF SIN") {
      // TODO FIXME
      rval = stage->appendTypedBlock<SINE>(blocktype);
    } else if (blocktype == "SAW") {
      rval = stage->appendTypedBlock<SAW>(blocktype);
    } else if (blocktype == "SW+DST") {
      rval = stage->appendTypedBlock<SAW>(blocktype);
    } else if (blocktype == "SQUARE") {
      rval = stage->appendTypedBlock<SQUARE>(blocktype);
    } else if (blocktype == "SINE+") {
      rval = stage->appendTypedBlock<SINEPLUS>(blocktype);
    } else if (blocktype == "SHAPE MOD OSC") {
      rval = stage->appendTypedBlock<SHAPEMODOSC>(blocktype);
    } else if (blocktype == "+ SHAPEMOD OSC") {
      rval = stage->appendTypedBlock<PLUSSHAPEMODOSC>(blocktype);
    } else if (blocktype == "SYNC M") {
      rval = stage->appendTypedBlock<SYNCM>(blocktype);
    } else if (blocktype == "SYNC S") {
      rval = stage->appendTypedBlock<SYNCS>(blocktype);
    } else if (blocktype == "PWM") {
      rval = stage->appendTypedBlock<PWM>(blocktype);
    } else if (blocktype == "NOISE") {
      rval = stage->appendTypedBlock<NOISE>(blocktype);
    } else if (blocktype == "NOISE+") {
      // TODO FIXME
      rval = stage->appendTypedBlock<NOISE>(blocktype);
    }
    ///////////
    else if (blocktype == "NONE") {
    }
    ///////////
    // ??
    ///////////
    else {
      printf("unhandled DSPBLOCK <%s>\n", blocktype.c_str());
      OrkAssert(false);
    }
  } else {
    OrkAssert(false);
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
        layd->_channelPans[1]     = ((pan / 7.0f) - 3.5f) / 3.5f;
        layd->_channelPanModes[1] = panMode;
        break;
      case 4: // lower
        layd->_channelPans[0]     = ((pan / 7.0f) - 3.5f) / 3.5f;
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

void KrzBankDataParser::parseKmpBlock(const Value& kmseg, KmpBlockData& kmblk) {
  kmblk._keyTrack    = kmseg["KeyTrack(ct)"].GetInt();
  kmblk._transpose   = kmseg["transposeTS(st)"].GetInt();
  kmblk._timbreShift = kmseg["timbreshift"].GetInt();
  kmblk._pbMode      = kmseg["pbmode"].GetString();
}

///////////////////////////////////////////////////////////////////////////////

lyrdata_ptr_t KrzBankDataParser::parseLayer(const Value& jsonobj, prgdata_ptr_t pd) {
  const auto& name = pd->_name;
  // printf("Got Prgram<%s> layer..\n", name.c_str());
  const auto& calvinSeg = jsonobj["CALVIN"];
  const auto& keymapSeg = calvinSeg["KEYMAP"];
  const auto& pitchSeg  = calvinSeg["PITCH"];
  const auto& miscSeg   = jsonobj["misc"];

  int kmid = keymapSeg["km1"].GetInt();
  // printf( "find KMID<%d>\n", kmid );
  auto it = _tempkeymaps.find(kmid);
  if (it == _tempkeymaps.end()) {
    it = _objdb->_keymaps.find(kmid);
    OrkAssert(it != _objdb->_keymaps.end());
  }

  auto layerdata     = pd->newLayer();
  auto km            = it->second;
  layerdata->_keymap = km;

  parseKmpBlock(keymapSeg, *layerdata->_kmpBlock);

  layerdata->_headroom = jsonobj["headroom"].GetInt();
  layerdata->_panmode  = jsonobj["panmode"].GetInt();
  layerdata->_pan      = jsonobj["pan"].GetInt();

  layerdata->_loKey = jsonobj["loKey"].GetInt();
  layerdata->_hiKey = jsonobj["hiKey"].GetInt();
  layerdata->_loVel = jsonobj["loVel"].GetInt() * 18;
  layerdata->_hiVel = 1 + (jsonobj["hiVel"].GetInt() * 18);
  if (layerdata->_loVel == 0 && layerdata->_hiVel == 0)
    layerdata->_hiVel = 127;

  layerdata->_ignRels  = miscSeg["ignRels"].GetBool();
  layerdata->_atk1Hold = miscSeg["atkHold"].GetBool();
  layerdata->_atk3Hold = miscSeg["susHold"].GetBool();

  auto krzalgdat      = parseAlg(jsonobj);
  layerdata->_algdata = krzalgdat._algdata;

  //////////////////////////////////////////////////////

  EnvCtrlData ENVCTRL;
  const auto& envcSeg = jsonobj["ENVCTRL"];
  parseEnvControl(envcSeg, ENVCTRL);

  layerdata->_usenatenv = ENVCTRL._useNatEnv;

  auto parseEnv = [&](const Value& envobj, //
                      controlblockdata_ptr_t cblock,
                      const std::string& name) -> controllerdata_ptr_t {
    auto rout                 = cblock->addController<RateLevelEnvData>(name);
    RateLevelEnvData& destenv = *rout;
    if (name == "AMPENV") {
      rout->_ampenv  = true;
      rout->_envType = RlEnvType::ERLTYPE_KRZAMPENV;
    } else {
      rout->_bipolar = true;
      rout->_envType = RlEnvType::ERLTYPE_KRZMODENV;
    }
    //////////////////////////////////////////
    rout->_sustainSegment = 3;
    rout->_releaseSegment = 4;
    rout->_segmentNames.push_back("at1");
    rout->_segmentNames.push_back("at2");
    rout->_segmentNames.push_back("at3");
    rout->_segmentNames.push_back("dec");
    rout->_segmentNames.push_back("rl1");
    rout->_segmentNames.push_back("rl2");
    rout->_segmentNames.push_back("rl3");
    // kurzweil shenanigans
    // if( iseg>0 ) { // iseg==1 or iseg==2 or iseg==4 or iseg==5 ){
    // attack segss 2 and 3 only have effect
    // if their times are not 0
    //    printf( "segt0 iseg<%d>\n", iseg );
    //}
    //////////////////////////////////////////
    const auto& jsonrates = envobj["rates"];
    OrkAssert(jsonrates.IsArray());
    int inumrates = jsonrates.Size();
    OrkAssert(inumrates == 7);
    std::vector<float> times;
    for (SizeType i = 0; i < inumrates; i++) // Uses SizeType instead of size_t
      times.push_back(jsonrates[i].GetFloat());
    //////////////////////////////////////////
    const auto& jsonlevels = envobj["levels"];
    OrkAssert(jsonlevels.IsArray());
    int inumlevels = jsonlevels.Size();
    OrkAssert(inumlevels == 7);
    std::vector<float> levels;
    for (SizeType i = 0; i < inumlevels; i++) // Uses SizeType instead of size_t
      levels.push_back(jsonlevels[i].GetFloat());
    //////////////////////////////////////////
    for (int i = 0; i < 7; i++)
      destenv._segments.push_back(EnvPoint{times[i], levels[i]});
    //////////////////////////////////////////
    // Kurzweil Rate/Lev Env Adjust
    //////////////////////////////////////////
    destenv._envadjust = [=](const EnvPoint& inp, //
                             int iseg,
                             const KeyOnInfo& KOI) -> EnvPoint { //
      EnvPoint outp = inp;
      int ikey      = KOI._key;

      const auto& DKT = ENVCTRL._decKeyTrack;
      const auto& RKT = ENVCTRL._relKeyTrack;

      float atkAdjust = ENVCTRL._atkAdjust;
      float decAdjust = ENVCTRL._decAdjust;
      float relAdjust = ENVCTRL._relAdjust;

      if (ikey > 60) {
        float flerp = float(ikey - 60) / float(127 - 60);
        decAdjust   = lerp(decAdjust, DKT, flerp);
        relAdjust   = lerp(relAdjust, RKT, flerp);
      } else if (ikey < 60) {
        float flerp = float(59 - ikey) / 59.0f;
        decAdjust   = lerp(decAdjust, 1.0 / DKT, flerp);
        relAdjust   = lerp(relAdjust, 1.0 / RKT, flerp);
      }

      // atkAdjust = 80.0f;
      // decAdjust = 80.0f;
      // relAdjust = 80.0f;

      switch (iseg) {
        case 0:
        case 1:
        case 2: // atk
          outp._time *= 1.0 / atkAdjust;
          break;
        case 3: // decay
          outp._time *= 1.0 / decAdjust;
          break;
        case 4:
        case 5:
        case 6: // rel
          outp._time *= 1.0 / relAdjust;
          break;
      }
      return outp;
    };
    //////////////////////////////////////////
    return rout;
  };
  //////////////////////////////////////////////////////
  auto CB = layerdata->_ctrlBlock;
  natenvwrapperdata_ptr_t natenvwrapperdata;
  //////////////////////////////////////////////////////
  // kurzweil only has 3 envs per layer!
  //////////////////////////////////////////////////////
  if (jsonobj.HasMember("AMPENV")) {
    const auto& seg = jsonobj["AMPENV"];
    if (seg.IsObject())
      parseEnv(seg, CB, "AMPENV");
  } else if (layerdata->_usenatenv) {
    // AMPENV from NATENV
    natenvwrapperdata = CB->addController<NatEnvWrapperData>("AMPENV");
  }
  if (jsonobj.HasMember("ENV2")) {
    const auto& seg = jsonobj["ENV2"];
    if (seg.IsObject())
      parseEnv(seg, CB, "ENV2");
  }
  if (jsonobj.HasMember("ENV3")) {
    const auto& seg = jsonobj["ENV3"];
    if (seg.IsObject())
      parseEnv(seg, CB, "ENV3");
  }
  //////////////////////////////////////////////////////
  if (jsonobj.HasMember("ASR1")) {
    const auto& seg = jsonobj["ASR1"];
    if (seg.IsObject())
      parseAsr(seg, CB, ENVCTRL, "ASR1");
  }
  if (jsonobj.HasMember("ASR2")) {
    const auto& seg = jsonobj["ASR2"];
    if (seg.IsObject())
      parseAsr(seg, CB, ENVCTRL, "ASR2");
  }
  //////////////////////////////////////////////////////
  if (jsonobj.HasMember("LFO1")) {
    const auto& lfo1seg = jsonobj["LFO1"];
    if (lfo1seg.IsObject())
      parseLfo(lfo1seg, CB, "LFO1");
  }
  if (jsonobj.HasMember("LFO2")) {
    const auto& lfo2seg = jsonobj["LFO2"];
    if (lfo2seg.IsObject())
      parseLfo(lfo2seg, CB, "LFO2");
  }
  //////////////////////////////////////////////////////
  if (jsonobj.HasMember("FUN1")) {
    const auto& seg = jsonobj["FUN1"];
    if (seg.IsObject())
      parseFun(seg, layerdata, CB, "FUN1");
  }
  if (jsonobj.HasMember("FUN2")) {
    const auto& seg = jsonobj["FUN2"];
    if (seg.IsObject())
      parseFun(seg, layerdata, CB, "FUN2");
  }
  if (jsonobj.HasMember("FUN3")) {
    const auto& seg = jsonobj["FUN3"];
    if (seg.IsObject())
      parseFun(seg, layerdata, CB, "FUN3");
  }
  if (jsonobj.HasMember("FUN4")) {
    const auto& seg = jsonobj["FUN4"];
    if (seg.IsObject())
      parseFun(seg, layerdata, CB, "FUN4");
  }
  //////////////////////////////////////////////////////
  // resolve nested controllers
  //////////////////////////////////////////////////////
  auto named_controllers = CB->_controllers_by_name;
  for (auto item : named_controllers) {
    auto name = item.first;
    auto ctrl = item.second;
    if (auto as_fun = std::dynamic_pointer_cast<FunData>(ctrl)) {
      auto resolvedA = CB->controllerByName(as_fun->_a);
      auto resolvedB = CB->controllerByName(as_fun->_b);
    } else if (auto as_lfo = std::dynamic_pointer_cast<LfoData>(ctrl)) {
      auto resolved = CB->controllerByName(as_lfo->_controller);
    } else {
    }
  }
  //////////////////////////////////////////////////////

  if (krzalgdat._algindex == 0)
    return layerdata;

  bool USING_SAMPLE = krzalgdat._algindex < 26;
  USING_SAMPLE      = USING_SAMPLE and (kmid != 0);

  layerdata->_varmap->makeValueForKey<bool>("USING_SAMPLE", USING_SAMPLE);

  OrkAssert(krzalgdat._algindex >= 1);
  OrkAssert(krzalgdat._algindex <= 31);
  const KrzAlgCfg ACFG = getAlgConfig(krzalgdat._algindex);

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

  auto parse_dsp_block = [&](dspstagedata_ptr_t stage, int blkbase, int paramcount) -> dspblkdata_ptr_t {
    auto blockn1 = blkname(blkbase + 0);
    auto blockn2 = blkname(blkbase + 1);
    auto blockn3 = blkname(blkbase + 2);
    dspblkdata_ptr_t dspblock;
    
    static const std::string PITCH = "PITCH";
    // printf("algd<%d> blkbase<%d> paramcount<%d> blockn1<%s>\n", krzalgdat._algindex, blkbase, paramcount, blockn1);
    if (blockn1 != PITCH and blockn2 != PITCH) {
      dspblock = parseDspBlock(jsonobj[blockn1], stage, layerdata);
      if (dspblock) {
        if ((paramcount > 0) and (jsonobj.HasMember(blockn1))) {
          parseFBlock(jsonobj[blockn1], layerdata, dspblock->param(0));
        }
        if ((paramcount > 1) and (jsonobj.HasMember(blockn2))) {
          parseFBlock(jsonobj[blockn2], layerdata, dspblock->param(1));
        }
        if ((paramcount > 2) and (jsonobj.HasMember(blockn3))) {
          parseFBlock(jsonobj[blockn3], layerdata, dspblock->param(2));
        }
      }
    }

    return dspblock;
  };
  int blockindex = 0;

  /*printf("ACFG._wp<%d>\n", ACFG._wp);
  printf("ACFG._w1<%d>\n", ACFG._w1);
  printf("ACFG._w2<%d>\n", ACFG._w2);
  printf("ACFG._w3<%d>\n", ACFG._w3);
  printf("ACFG._wa<%d>\n", ACFG._wa);*/

  auto dspstage = layerdata->stageByName("DSP");
  auto ampstage = layerdata->stageByName("AMP");

  if (ACFG._wp) {
    // parse_dsp_block(dspstage, blockindex, ACFG._wp);
    // parseDspBlock(pitchSeg, dspstage, layerdata);
    OrkAssert(USING_SAMPLE);

    auto pitch   = dspstage->appendTypedBlock<PITCH>("PITCH");
    auto sampler = dspstage->appendTypedBlock<SAMPLER>("SAMPLER");
    parseFBlock(pitchSeg, layerdata, pitch->param(0));
    layerdata->_pchBlock = pitch;
    blockindex += ACFG._wp;
    // sampler->_natenvwrapperdata = natenvwrapperdata;

  } else {
    OrkAssert(not USING_SAMPLE);
  }
  if (ACFG._w1) {
    parse_dsp_block(dspstage, blockindex, ACFG._w1);
    blockindex += ACFG._w1;
  }
  if (ACFG._w2) {
    parse_dsp_block(dspstage, blockindex, ACFG._w2);
    blockindex += ACFG._w2;
  }
  if (ACFG._w3) {
    parse_dsp_block(dspstage, blockindex, ACFG._w3);
    blockindex += ACFG._w3;
  }
  if (ACFG._wa) {
    parse_dsp_block(ampstage, blockindex, ACFG._wa);
    blockindex += ACFG._wa;
    // if (krzalgdat._algindex >= 1 and krzalgdat._algindex <= 4) {
    //   auto amp = ampstage->appendTypedBlock<PANNER>("PANNER");
    // }
  }

  // if last stage is panner, then ioconfig numoutputs is 2

  auto last = dspstage->_blockdatas[dspstage->_numblocks - 1];
  if (auto as_panner = std::dynamic_pointer_cast<PANNER_DATA>(last)) {
    dspstage->_ioconfig->_outputs.push_back(1);
  }

  //////////////////////////////////////////////////////

  if (name == "Doomsday") {
    // OrkAssert(false);
  }
  // printf( "got keymapID<%d:%p:%s>\n", kmid, km, km->_name.c_str() );
  return layerdata;
}

///////////////////////////////////////////////////////////////////////////////

KrzAlgData KrzBankDataParser::parseAlg(const rapidjson::Value& JO) {
  const auto& calvin = JO["CALVIN"];
  int krzAlgIndex    = calvin["ALG"].GetInt();

  auto algd       = configureKrzAlgorithm(krzAlgIndex);
  KrzAlgData rval = {krzAlgIndex, algd};
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void KrzBankDataParser::parseEnvControl(const rapidjson::Value& seg, EnvCtrlData& ed) {
  auto aenvmode = seg["ampenv_mode"].GetString();
  ed._useNatEnv = (0 == strcmp(aenvmode, "Natural"));
  ed._atkAdjust = seg["AtkAdjust"].GetFloat();
  ed._decAdjust = seg["DecAdjust"].GetFloat();
  ed._relAdjust = seg["RelAdjust"].GetFloat();

  ed._decKeyTrack = seg["DecKeyTrack"].GetFloat();
  ed._relKeyTrack = seg["RelKeyTrack"].GetFloat();
}

///////////////////////////////////////////////////////////////////////////////

prgdata_ptr_t KrzBankDataParser::parseProgram(const Value& jsonobj) {
  auto pdata       = std::make_shared<ProgramData>();
  pdata->_tags     = "Program";
  const auto& name = jsonobj["Program"].GetString();
  // printf( "Got Prgram name<%s>\n", name );
  pdata->_name = name;

  const auto& jsonlays = jsonobj["LAYERS"];
  OrkAssert(jsonlays.IsArray());

  pdata->addHudInfo(FormatString("ProgramType: KRZ"));
  for (SizeType i = 0; i < jsonlays.Size(); i++) // Uses SizeType instead of size_t
  {
    auto l       = parseLayer(jsonlays[i], pdata);
    auto algdata = l->_algdata;
    pdata->addHudInfo(FormatString("LAYER: %d", i));
    pdata->addHudInfo(FormatString("  ALG: %s", algdata->_name.c_str()));
    int numstages = algdata->_numstages;
    for (int istage = 0; istage < numstages; istage++) {
      auto stage = l->stageByIndex(istage);
      pdata->addHudInfo(FormatString("  STAGE: %s", stage->_name.c_str()));
      int numblocks = stage->_namedblockdatas.size();
      for (int iblock = 0; iblock < numblocks; iblock++) {
        auto block = stage->_blockdatas[iblock];
        if (block) {
          pdata->addHudInfo(FormatString("    BLOCK: %s", block->_blocktype.c_str()));
          if (block->_blocktype == "SAMPLER") {
            auto sampler = std::dynamic_pointer_cast<SAMPLER_DATA>(block);
            auto kmp     = l->_kmpBlock;
            auto keymap  = l->_keymap;
            auto pstr    = FormatString("     KEYMAP: %s", keymap->_name.c_str());
            pdata->addHudInfo(pstr);
          }
          for (auto p : block->_paramd) {
            if (p) {
              auto pstr = FormatString(
                  "     PARAM name: %s units: %s evaluator: %s", p->_name.c_str(), p->_units.c_str(), p->_evaluatorid.c_str());
              pdata->addHudInfo(pstr);
            }
          }
        }
      }
    }
  }

  return pdata;
}

///////////////////////////////////////////////////////////////////////////////

bankdata_ptr_t KrzBankDataParser::loadKrzJsonFromFile(const std::string& fname, int ibaseid) {
  auto realfname = basePath() / "kurzweil" / (fname + ".json");
  // printf("fname<%s>\n", realfname.c_str());
  FILE* fin = fopen(realfname.c_str(), "rt");
  OrkAssert(fin != nullptr);
  fseek(fin, 0, SEEK_END);
  int size = ftell(fin);
  // printf("filesize<%d>\n", size);
  fseek(fin, 0, SEEK_SET);
  auto jsondata = (char*)malloc(size + 1);
  fread(jsondata, size, 1, fin);
  jsondata[size] = 0;
  fclose(fin);
  auto rval = loadKrzJsonFromString(jsondata, ibaseid);
  free((void*)jsondata);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bankdata_ptr_t KrzBankDataParser::loadKrzJsonFromString(const std::string& json_data, int ibaseid) {

  _remap_base = ibaseid;
  _objdb      = std::make_shared<BankData>();

  Document document;
  document.Parse(json_data.c_str());

  OrkAssert(document.IsObject());
  OrkAssert(document.HasMember("KRZ"));

  const auto& root    = document["KRZ"];
  const auto& objects = root["objects"];
  OrkAssert(objects.IsArray());

  _tempprograms.clear();
  _tempkeymaps.clear();
  _tempmultisamples.clear();

  ///////////////////////////////////

  std::set<int> used_ids;
  for (SizeType i = 0; i < objects.Size(); i++) { // Uses SizeType instead of size_t
    const auto& obj = objects[i];
    int objid       = obj["objectID"].GetInt();
    used_ids.insert(objid);
  }

  ///////////////////////////////////

  for (SizeType i = 0; i < objects.Size(); i++) { // Uses SizeType instead of size_t
    const auto& obj = objects[i];
    int objid       = obj["objectID"].GetInt();
    if (obj.HasMember("Keymap")) {
      auto km             = parseKeymap(objid, obj);
      _tempkeymaps[objid] = km;
    } else if (obj.HasMember("MultiSample")) {
      auto ms                  = parseMultiSample(obj);
      _tempmultisamples[objid] = ms;
    } else if (obj.HasMember("Program")) {
      auto p               = parseProgram(obj);
      _tempprograms[objid] = p;
      // p->_tags             = "k2000(" + fname + ")";
    } else {
      OrkAssert(false);
    }
  }

  /////////////////////////////////////////
  // fill
  /////////////////////////////////////////

  for (auto it : _tempprograms) {
    int objid         = it.first;
    prgdata_ptr_t prg = it.second;
    auto it2          = _objdb->_programs.find(objid);
    if (it2 != _objdb->_programs.end()) {
      int nid = (_objdb->_programs.rbegin()->first + 1);
      // nid = nid-(nid%
      printf("REMAP PROG<%d> -> <%d>\n", objid, nid);
      objid = nid;
    }
    _objdb->_programs[objid] = prg;

    // TODO - remap duplicates (by name)
    _objdb->_programsByName[prg->_name] = prg;
  }
  for (auto it : _tempkeymaps) {
    int objid                         = it.first;
    auto km                           = it.second;
    _objdb->_keymaps[objid]           = km;
    _objdb->_keymapsByName[km->_name] = km;
    for (auto kr : km->_regions) {
      int msid  = kr->_multsampID;
      auto msit = _tempmultisamples.find(msid);
      if (msit == _tempmultisamples.end()) {
        msit = _objdb->_multisamples.find(msid);
      }
      if (msit != _objdb->_multisamples.end()) {
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
  }
  for (auto it : _tempmultisamples) {
    int objid                                      = it.first;
    auto multsample                                = it.second;
    _objdb->_multisamples[objid]                   = multsample;
    _objdb->_multisamplesByName[multsample->_name] = multsample;
  }
  return _objdb;
  /////////////////////////////////////////
}

} // namespace ork::audio::singularity
