////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "krzio.h"
#include <fstream>

using namespace rapidjson;
namespace ork::audio::singularity::krzio {

///////////////////////////////////////////////////////////////////////////////

void filescanner::parseCalvin(const datablock& db, datablock::iterator& it) {
  // printf( "////////////////////////////// parseCalvin\n" );

  assert(_curLayer);
  auto calvin     = _curLayer->_calvin;
  auto& KMP       = calvin->_kmpage;
  fparam& pitchFP = calvin->_pitchFP;

  ///////////////////////////////////////////////////////////////////////////////

  auto subtag               = db.GetTypedData<u8>(it);  // 01
  KMP._transposeTimbreShift = db.GetTypedData<u8>(it);  // 02
  KMP._detune               = db.GetTypedData<u8>(it);  // 03
  KMP._keyTrack             = db.GetTypedData<u8>(it);  // 04
  KMP._velTrack             = db.GetTypedData<u8>(it);  // 05
  KMP._tControl             = db.GetTypedData<u8>(it);  // 06
  KMP._tRange               = db.GetTypedData<u8>(it);  // 07
  KMP._kmid2                = db.GetTypedData<u16>(it); // 08..09
  KMP._sampleRoot           = db.GetTypedData<u8>(it);  // 0A
  KMP._sampleSkip           = db.GetTypedData<u8>(it);  // 0B
  KMP._kmid1                = db.GetTypedData<u16>(it); // 0C..0D
  KMP._sampleBlockRoot      = db.GetTypedData<u8>(it);  // 0E
  KMP._altControl           = db.GetTypedData<u8>(it);  // 0F
  KMP._timbreShift          = db.GetTypedData<u8>(it);  // 10
  auto reserved             = db.GetTypedData<u8>(it);  // 11

  pitchFP._inputCourse   = (int)db.GetTypedData<u8>(it); // 12
  pitchFP._inputFine     = (int)db.GetTypedData<u8>(it); // 13
  pitchFP._inputKeyTrack = (int)db.GetTypedData<u8>(it); // 14
  pitchFP._inputVelTrack = (int)db.GetTypedData<u8>(it); // 15
  pitchFP._inputSrc1     = (int)db.GetTypedData<u8>(it); // 16
  pitchFP._inputDepth    = (int)db.GetTypedData<u8>(it); // 17
  pitchFP._inputDptCtl   = (int)db.GetTypedData<u8>(it); // 18
  pitchFP._inputMinDepth = (int)db.GetTypedData<u8>(it); // 19
  pitchFP._inputMaxDepth = (int)db.GetTypedData<u8>(it); // 1A
  pitchFP._inputSrc2     = (int)db.GetTypedData<u8>(it); // 1B
  u8 CCR                 = db.GetTypedData<u8>(it);      // 1C
  KMP._playbackMode      = db.GetTypedData<u8>(it);      // 1D
  calvin->_algorithm     = db.GetTypedData<u8>(it);      // 1E
  pitchFP._input15       = (int)db.GetTypedData<u8>(it); // 1F
  // printf( "  LAYER::ALG<%d>\n", ALG );
  // printf( "  KEYMAP::Keymap1<%d> Keymap2<%d>\n", KEYMAP.mKeymap1, KEYMAP.mKeymap2 );
  // printf( "  PITCH::Coarse/TS<%d> Fine<%d> FineHZ<%d>\n", PITCH.mCoarseTimbreShift, PITCH.mFine, PITCH.mFineHz );

  _curALG = calvin->_algorithm;

  getFParamPCH(pitchFP);

  if (_curProgram->_debug) {
    printf("debug prg<%d:%p>\n", _curProgram->_programID, (void*) _curProgram);
    printf(">>  parse calvin<%p> KMID1<%d>\n", (void*) calvin, KMP._kmid1);
    // assert(KMP._kmid1==58);
  }

  // Value jsonCCR(kObjectType);
  // jsonCCR.AddMember("umumum", int(CCR)&1, _japrog );
  // jsonCCR.AddMember("pbmode", int(CCR>>1)&3, _japrog );
  // jsonCCR.AddMember("umumum2", int(CCR>>3), _japrog );
  // jsoncalvseg.AddMember("CCR", jsonCCR, _japrog );
  //_curLayerObject->AddMember("calvinSegment", jsoncalvseg, _japrog );
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitCalvin(const Calvin* c, rapidjson::Value& parent) {
  Value calvinseg(kObjectType);
  AddMember(calvinseg, "ALG", c->_algorithm);
  Value pitchseg(kObjectType);

  const auto& KMP = c->_kmpage;

  auto l  = c->_layer;
  auto p  = l->_program;
  int pid = p->_programID;

  auto calvp = ork::FormatString("%p", c);
  auto progp = ork::FormatString("%p", p);
  auto layrp = ork::FormatString("%p", l);

  Value kmseg(kObjectType);
  // AddMember(kmseg,"calvp", calvp );
  // AddMember(kmseg,"layrp", layrp );
  // AddMember(kmseg,"progp", progp );
  AddMember(kmseg, "transposeTS(st)", makeSigned(KMP._transposeTimbreShift));

  AddMember(kmseg, "km1", KMP._kmid1);
  AddMember(kmseg, "km2", KMP._kmid2);
  AddMember(kmseg, "detune", KMP._detune);

  AddMember(kmseg, "KeyTrack(ct)", getKeyTrack85(KMP._keyTrack));
  AddMember(kmseg, "veltrk", KMP._velTrack);
  AddMember(kmseg, "tcontrol", KMP._tControl);
  AddMember(kmseg, "trange", KMP._tRange);
  AddMember(kmseg, "sampleroot", KMP._sampleRoot);
  AddMember(kmseg, "sampleskp", KMP._sampleSkip);
  AddMember(kmseg, "sampleblkroot", KMP._sampleBlockRoot);
  AddMember(kmseg, "altcontrol", getControlSourceName(KMP._altControl));
  AddMember(kmseg, "timbreshift", makeSigned(KMP._timbreShift));

  switch (KMP._playbackMode >> 1) {
    case 0:
      AddMember(kmseg, "pbmode", std::string("Normal"));
      break;
    case 1:
      AddMember(kmseg, "pbmode", std::string("Reverse"));
      break;
    case 2:
      AddMember(kmseg, "pbmode", std::string("BiDirect"));
      break;
    case 3:
      AddMember(kmseg, "pbmode", std::string("Noise"));
      break;
    default:
      break;
  }

  if (pid == 190) {
    auto l0  = p->_layers[0];
    auto c0  = l0->_calvin;
    auto& kp = c0->_kmpage;
    printf("prg190 emit calvin<%p> KMID1<%d>\n", (void*) c, kp._kmid1);
    assert(kp._kmid1 == 58);
    AddMember(kmseg, "YO", std::string("WHATUP"));
    // assert(false);
  }

  AddMember(calvinseg, "KEYMAP", kmseg);

  fparamOutput(c->_pitchFP, "PITCH", pitchseg);

  AddMember(calvinseg, "PITCH", pitchseg);
  AddMember(parent, "CALVIN", calvinseg);
}

Calvin::Calvin(Layer* l)
    : _layer(l)
    , _algorithm(0)
    , _kmpage(this) {
}
CalvinKmPage::CalvinKmPage(Calvin* c)
    : _calvin(c)
    , _kmid1(0)
    , _kmid2(0)
    , _km1(0)
    , _km2(0)
    , _transposeTimbreShift(0)
    , _detune(0)
    , _keyTrack(0)
    , _velTrack(0)
    , _tControl(0)
    , _tRange(0)
    , _sampleRoot(0)
    , _sampleSkip(0)
    , _sampleBlockRoot(0)
    , _altControl(0)
    , _timbreShift(0)
    , _playbackMode(0) {
}
} // namespace ork::audio::singularity::krzio
