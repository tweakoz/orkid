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

void filescanner::parseHobbes(const datablock& db, datablock::iterator& it, u8 code) {
  assert(_curLayer);
  assert(code >= 0x50);
  assert(code <= 0x53);

  auto hobbes = _curLayer->_hobbes[code - 0x50];
  auto& hfp   = hobbes->_hobbesFP;

  auto algCFG = getAlgConfig(_curALG);

  hfp._inputALG      = db.GetTypedData<u8>(it);
  hfp._inputCourse   = db.GetTypedData<u8>(it); // coarse
  hfp._inputFine     = db.GetTypedData<u8>(it); // fine
  hfp._inputKeyTrack = db.GetTypedData<u8>(it); // kscale
  hfp._inputVelTrack = db.GetTypedData<u8>(it); // vscale
  hfp._inputSrc1     = db.GetTypedData<u8>(it); // control
  hfp._inputDepth    = db.GetTypedData<u8>(it); // range
  hfp._inputDptCtl   = db.GetTypedData<u8>(it); // depth
  hfp._inputMinDepth = db.GetTypedData<u8>(it); // mindepth
  hfp._inputMaxDepth = db.GetTypedData<u8>(it); // maxdepth
  hfp._inputSrc2     = db.GetTypedData<u8>(it); // source

  // tscr word
  hfp._tscra = db.GetTypedData<u8>(it); // 2:downshift, 6:filtAlg
  hfp._tscrb = db.GetTypedData<u8>(it); // moreTscr

  // output word
  hfp._owrda = db.GetTypedData<u8>(it); // 3: headroom, 2:pair, 3:rfu2
  hfp._owrdb = db.GetTypedData<u8>(it); // 2: rfu, 2: panmode, 4: pan
  // owrda 12 0x0c | 0b 000 01 100 
  // owrdb 84 0x54 | 0b 0101 01 00 | 0100 01 01
  //

  if( true ){ // big endian
  hfp._inputMoreTSCR  = hfp._tscrb;
  hfp._downshift = (hfp._tscra&3);
  hfp._filtalg = (hfp._tscra>>2)&0x3f;
  _curLayer->_headroom = (hfp._owrda&7);
  _curLayer->_pair = (hfp._owrda>>2)&3;
  _curLayer->_panmode = (hfp._owrdb>>2)&3;
  _curLayer->_pan = (hfp._owrdb>>4)&0xf;
  _curLayer->_headroom = 7-_curLayer->_headroom ;
  _curLayer->_headroom = (_curLayer->_headroom-2)*6;
  }
  else{
    hfp._inputMoreTSCR  = hfp._tscrb;
    hfp._filtalg = hfp._tscra & 0x3F;
    hfp._downshift = (hfp._tscra>>6)&3;
    _curLayer->_headroom = (hfp._owrda>>5)&7;
    _curLayer->_pair = (hfp._owrda>>3)&3;
    _curLayer->_panmode = (hfp._owrdb>>4) & 0x3;
    _curLayer->_pan = (hfp._owrdb) & 0xf;
    _curLayer->_headroom = (_curLayer->_headroom-2)*6;
  }

  //hfp._downshift  = db.GetTypedData<u8>(it); 
  //hfp._inputMoreTSCR = db.GetTypedData<u8>(it); // 3: headroom, 2:pair, 3:rfu2
  //hfp._inputRESERVED = db.GetTypedData<u8>(it); 
  //hfp._input14       = hfp._inputRESERVED;
  //hfp._input15       = db.GetTypedData<u8>(it);

  hfp._blockScheme = getDspBlockScheme(hfp._inputALG);

  hfp._blockName = "???";

  bool continuation_block = false;

  switch (code) {
    case 0x50: // hobbes f1
    {
      hfp._blockName  = "F1";
      hfp._blockIndex = 0;
      break;
    }
    case 0x51: // hobbes f2
    {
      hfp._blockName  = "F2";
      hfp._blockIndex = 1;
      if (algCFG._w2 == 0)
        continuation_block = true;
      break;
    }
    case 0x52: // hobbes f3
    {
      hfp._blockName  = "F3";
      hfp._blockIndex = 2;
      if (algCFG._w3 == 0)
        continuation_block = true;
      break;
    }
    case 0x53: // hobbes f4 (amp)
    {
      hfp._blockName  = "F4AMP";
      hfp._blockIndex = 3;
      if (algCFG._wa == 0)
        continuation_block = true;
      break;
    }
    default:
      OrkAssert(false);
  }

  auto rawalgschm = hfp._blockScheme;

  if (false == continuation_block) {
    hfp._algName = getDspBlockName(hfp._inputALG);

    std::vector<std::string> split_algs;
    ork::SplitString(hfp._blockScheme, split_algs, " ");

    // printf( "algschm<%s>\n", hfp._blockScheme.c_str() );

    for (const auto& item : split_algs) {
      this->_algschmq.push(item);
      // printf( "split_algs<%s>\n", item.c_str() );
    }
  }

  if (this->_algschmq.size()) {
    hfp._blockScheme = this->_algschmq.front();
    this->_algschmq.pop();
    // printf( "algschmpop<%s>\n", hfp._blockScheme.c_str() );
  } else {
    hfp._blockScheme = "???";
  }

  switch (hfp._downshift) {
    case 0:
      hfp._outputPAD = 0; // dB
      break;
    case 1:
      hfp._outputPAD = -6; // dB
      break;
    case 2:
      hfp._outputPAD = -12; // dB
      break;
    case 3:
      hfp._outputPAD = -18; // dB
      break;
  }
  //hfp._outputFiltAlg = ork::FormatString("%d", hfp._inputFiltAlg >> 2);

  hfp._var14.set<int>("Var14", "???", "%d", (int)hfp._input14);
  hfp._var15.set<int>("Var15", "???", "%d", (int)hfp._input15);

  if (hfp._blockScheme == "AMP")
    getFParamAMP(hfp);
  else if (hfp._blockScheme == "FRQ")
    getFParamFRQ(hfp);
  else if (hfp._blockScheme == "DRV")
    getFParamDRV(hfp);
  else if (hfp._blockScheme == "PCH"){
    getFParamPCH(hfp);
  }
  else if (hfp._blockScheme == "POS")
    getFParamPOS(hfp);
  else if (hfp._blockScheme == "RES")
    getFParamRES(hfp);
  else if (hfp._blockScheme == "WRP")
    getFParamWRP(hfp);
  else if (hfp._blockScheme == "DEP")
    getFParamDEP(hfp);
  else if (hfp._blockScheme == "AMT")
    getFParamAMT(hfp);
  else if (hfp._blockScheme == "WID")
    getFParamWID(hfp);
  else if (hfp._blockScheme == "WID(PWM)")
    getFParamPWM(hfp);
  else if (hfp._blockScheme == "SEP")
    getFParamSEP(hfp);
  else if (hfp._blockScheme == "EVN")
    getFParamEVNODD(hfp);
  else if (hfp._blockScheme == "ODD")
    getFParamEVNODD(hfp);
  else if (hfp._blockScheme == "XFD")
    getFParamXFD(hfp);

  _algschmap[hfp._blockName] = hfp._blockScheme;

  const auto& blkname = hfp._blockName;

  hobbes->_ok2emit = (0 != _curALG);

  hobbes->_blockName = blkname;
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitHobbes(const Hobbes* h, rapidjson::Value& parent) {
  if (h->_ok2emit) {
    Value hseg(kObjectType);

    const auto& hfp = h->_hobbesFP;

    if (hfp._algName.length())
      AddStringKVMember(hseg, "BLOCK_ALG ", hfp._algName);

    AddStringKVMember(hseg, "PARAM_SCHEME", hfp._blockScheme);
    AddMember<int>(hseg, "DBG_TSCRA", hfp._tscra);
    AddMember<int>(hseg, "DBG_TSCRB", hfp._tscrb);
    AddMember<int>(hseg, "DBG_OWRDA", hfp._owrda);
    AddMember<int>(hseg, "DBG_OWRDB", hfp._owrdb);
    AddMember<int>(hseg, "DBG_DOWNSHIFT", hfp._downshift);

    fparamOutput(hfp, h->_blockName, hseg);
    AddMember(parent, h->_blockName, hseg);
  }
}

Hobbes::Hobbes()
    : _ok2emit(false) {
}
} // namespace ork::audio::singularity::krzio
