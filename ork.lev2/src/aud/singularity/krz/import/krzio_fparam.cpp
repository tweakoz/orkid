////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "krzio.h"

using namespace rapidjson;
namespace ork::audio::singularity::krzio {

void getFParamXFD(fparam& fp) {
  int adj = makeSigned(fp._inputCourse);

  int keytrk = makeSigned(fp._inputKeyTrack);
  int veltrk = makeSigned(fp._inputVelTrack);
  int depth  = makeSigned(fp._inputDepth);
  int minDpt = makeSigned(fp._inputMinDepth);
  int maxDpt = makeSigned(fp._inputMaxDepth);

  // std::string getFreq83( int ival )

  fp._varCoarseAdjust.set<int>("Coarse", "%%", "%%d", adj);
  fp._varKeyTrack.set<int>("KeyTrack", "%%/key", "%%d", keytrk * .02f);
  fp._varVelTrack.set<int>("VelTrack", "%%", "%%d", veltrk * 2.0f);
  fp._varSrc1Depth.set<int>("Depth", "%%", "%%d", depth * 2.0f);
  fp._varSrc2MinDepth.set<int>("MinDepth", "%%", "%%d", minDpt * 2.0f);
  fp._varSrc2MaxDepth.set<int>("MaxDepth", "%%", "%%d", maxDpt * 2.0f);
}

void getFParamFRQ(fparam& fp) {
  int fine = makeSigned(fp._inputFine);
  // int fineHZ = makeSigned(fp._inputFineHZKST);
  int keytrk = makeSigned(fp._inputKeyTrack);
  int veltrk = getVelTrack96(fp._inputVelTrack);
  int depth  = getVelTrack96(fp._inputDepth);
  int minDpt = getVelTrack96(fp._inputMinDepth);
  int maxDpt = getVelTrack96(fp._inputMaxDepth);

  // std::string getFreq83( int ival )

  auto note = getFreq83(fp._inputCourse);

  fp._varCoarseAdjust.set<std::string>("Coarse", "nt", "%%s", note);
  fp._varFine.set<int>("Fine", "ct", "%%d", fine);
  fp._varKeyTrack.set<int>("KeyTrack", "ct/key", "%%d", keytrk * 2);
  fp._varVelTrack.set<int>("VelTrack", "ct", "%%d", veltrk);
  fp._varSrc1Depth.set<int>("Depth", "ct", "%%d", depth);
  fp._varSrc2MinDepth.set<int>("MinDepth", "ct", "%%d", minDpt);
  fp._varSrc2MaxDepth.set<int>("MaxDepth", "ct", "%%d", maxDpt);
}

void getFParamPCH(fparam& fp) {
  int course = makeSigned(fp._inputCourse);
  int fine   = makeSigned(fp._inputFine);
  int keytrk = getKeyTrack85(fp._inputKeyTrack);
  int veltrk = getVelTrack98(fp._inputVelTrack);
  int depth  = getVelTrack98(fp._inputDepth);
  int minDpt = getVelTrack98(fp._inputMinDepth);
  int maxDpt = getVelTrack98(fp._inputMaxDepth);

  // int fineHZ = makeSigned(fp._inputFineHZKST);

  fp._varCoarseAdjust.set<int>("Coarse", "st", "%%d", course);
  fp._varFine.set<int>("Fine", "ct", "%%d", fine);
  fp._varKeyTrack.set<int>("KeyTrack", "ct/key", "%%d", keytrk);
  fp._varVelTrack.set<int>("VelTrack", "ct", "%%d", veltrk);
  fp._varSrc1Depth.set<int>("Depth", "ct", "%%d", depth);
  fp._varSrc2MinDepth.set<int>("MinDepth", "ct", "%%d", minDpt);
  fp._varSrc2MaxDepth.set<int>("MaxDepth", "ct", "%%d", maxDpt);
}

void getFParamDRV(fparam& fp) {
  int adjust = makeSigned(fp._inputCourse);
  int keytrk = makeSigned(fp._inputKeyTrack);
  int veltrk = makeSigned(fp._inputVelTrack);
  int depth  = makeSigned(fp._inputDepth);
  int minDpt = makeSigned(fp._inputMinDepth);
  int maxDpt = makeSigned(fp._inputMaxDepth);

  fp._varCoarseAdjust.set<int>("Coarse", "dB", "%%d", adjust);
  fp._varKeyStart = getKeyStart81(fp._inputFine);
  fp._varKeyTrack.set<float>("KeyTrack", "dB/key", "%%0.1f", float(keytrk) * 0.02f);
  fp._varVelTrack.set<int>("VelTrack", "dB", "%%d", veltrk);
  fp._varSrc1Depth.set<int>("Depth", "dB", "%%d", depth);
  fp._varSrc2MinDepth.set<int>("MinDepth", "ct", "%%d", minDpt);
  fp._varSrc2MaxDepth.set<int>("MaxDepth", "ct", "%%d", maxDpt);
}

void getFParamAMP(fparam& fp) {
  int adjust = makeSigned(fp._inputCourse);
  int keytrk = makeSigned(fp._inputKeyTrack);
  int veltrk = makeSigned(fp._inputVelTrack);
  int depth  = makeSigned(fp._inputDepth);
  int minDpt = makeSigned(fp._inputMinDepth);
  int maxDpt = makeSigned(fp._inputMaxDepth);

  fp._varCoarseAdjust.set<int>("Coarse", "dB", "%%d", adjust);
  fp._varKeyTrack.set<float>("KeyTrack", "dB/key", "%%0.1f", float(keytrk) * 0.02f);
  fp._varVelTrack.set<int>("VelTrack", "dB", "%%d", veltrk);
  fp._varSrc1Depth.set<int>("Depth", "dB", "%%d", depth);
  fp._varSrc2MinDepth.set<int>("MinDepth", "ct", "%%d", minDpt);
  fp._varSrc2MaxDepth.set<int>("MaxDepth", "ct", "%%d", maxDpt);
}

void getFParamPOS(fparam& fp) {
  int adjust = makeSigned(fp._inputCourse);
  int keytrk = makeSigned(fp._inputKeyTrack);
  int veltrk = makeSigned(fp._inputVelTrack);
  int depth  = makeSigned(fp._inputDepth);
  int minDpt = makeSigned(fp._inputMinDepth);
  int maxDpt = makeSigned(fp._inputMaxDepth);

  fp._varCoarseAdjust.set<int>("Coarse", "pct", "%%d", adjust);
  fp._varKeyTrack.set<float>("KeyTrack", "pct/key", "%%0.1f", float(keytrk) * 0.2f);
  fp._varVelTrack.set<int>("VelTrack", "pct", "%%d", veltrk * 2);
  fp._varSrc1Depth.set<int>("Depth", "ct", "%%d", depth * 2);
  fp._varSrc2MinDepth.set<int>("MinDepth", "ct", "%%d", minDpt * 2);
  fp._varSrc2MaxDepth.set<int>("MaxDepth", "ct", "%%d", maxDpt * 2);
}

void getFParamRES(fparam& fp) {
  int adjust = makeSigned(fp._inputCourse);
  int keytrk = makeSigned(fp._inputKeyTrack);
  int veltrk = makeSigned(fp._inputVelTrack);
  int depth  = makeSigned(fp._inputDepth);
  int minDpt = makeSigned(fp._inputMinDepth);
  int maxDpt = makeSigned(fp._inputMaxDepth);

  fp._varCoarseAdjust.set<float>("Coarse", "dB", "%%0.1f", adjust * 0.5f);
  fp._varKeyTrack.set<float>("KeyTrack", "dB/key", "%%0.1f", float(keytrk) * 0.02f);
  fp._varVelTrack.set<float>("VelTrack", "dB", "%%0.1f", float(veltrk) * 0.5f);
  fp._varSrc1Depth.set<float>("Depth", "dB", "%%0.1f", depth * 0.5f);
  fp._varSrc2MinDepth.set<float>("MinDepth", "ct", "%%0.1f", minDpt * 0.5f);
  fp._varSrc2MaxDepth.set<float>("MaxDepth", "ct", "%%0.1f", maxDpt * 0.5f);
}

void getFParamWRP(fparam& fp) {
  int adjust = makeSigned(fp._inputCourse);
  int keytrk = makeSigned(fp._inputKeyTrack);
  int veltrk = makeSigned(fp._inputVelTrack);
  int depth  = makeSigned(fp._inputDepth);
  int minDpt = makeSigned(fp._inputMinDepth);
  int maxDpt = makeSigned(fp._inputMaxDepth);

  fp._varCoarseAdjust.set<float>("Coarse", "dB", "%%0.2f", adjust / 4.0f);
  fp._varKeyTrack.set<float>("KeyTrack", "dB/key", "%%0.1f", float(keytrk) * 0.02f);
  fp._varVelTrack.set<float>("VelTrack", "dB", "%%0.1f", float(veltrk) * 0.5f);
  fp._varSrc1Depth.set<float>("Depth", "dB", "%%0.1f", float(depth) * 0.5f);
  fp._varSrc2MinDepth.set<float>("MinDepth", "dB", "%%0.1f", minDpt * 0.5f);
  fp._varSrc2MaxDepth.set<float>("MaxDepth", "dB", "%%0.1f", maxDpt * 0.5f);
  fp._varKeyStart = getKeyStart81(fp._inputFine);
}

void getFParamDEP(fparam& fp) {
  int adjust = makeSigned(fp._inputCourse);
  int keytrk = makeSigned(fp._inputKeyTrack);
  int veltrk = makeSigned(fp._inputVelTrack);
  int depth  = makeSigned(fp._inputDepth);
  int minDpt = makeSigned(fp._inputMinDepth);
  int maxDpt = makeSigned(fp._inputMaxDepth);

  fp._varCoarseAdjust.set<float>("Coarse", "dB", "%%d", adjust);
  fp._varKeyTrack.set<float>("KeyTrack", "dB/key", "%%0.1f", float(keytrk) * 0.02f);
  fp._varVelTrack.set<int>("VelTrack", "dB", "%%d", veltrk);
  fp._varSrc1Depth.set<int>("Depth", "dB", "%%d", depth);
  fp._varSrc2MinDepth.set<int>("MinDepth", "dB", "%%d", minDpt);
  fp._varSrc2MaxDepth.set<int>("MaxDepth", "dB", "%%d", maxDpt);

  fp._varKeyStart = getKeyStart81(fp._inputFine);
}

void getFParamAMT(fparam& fp) {
  int adjust = makeSigned(fp._inputCourse);
  int keytrk = makeSigned(fp._inputKeyTrack);
  int veltrk = makeSigned(fp._inputVelTrack);
  int depth  = makeSigned(fp._inputDepth);
  int minDpt = makeSigned(fp._inputMinDepth);
  int maxDpt = makeSigned(fp._inputMaxDepth);

  fp._varCoarseAdjust.set<float>("Coarse", "x", "%%0.001f", float(adjust) * 0.025f);
  fp._varKeyTrack.set<float>("KeyTrack", "x/key", "%%0.001f", float(keytrk) * 0.002f);
  fp._varVelTrack.set<float>("VelTrack", "x", "%%d", float(veltrk) * .05f);
  fp._varKeyStart = getKeyStart81(fp._inputFine);
  fp._varSrc1Depth.set<float>("Depth", "x", "%%0.01f", float(depth) * 0.05f);
  fp._varSrc2MinDepth.set<float>("MinDepth", "x", "%%0.01f", minDpt * 0.05f);
  fp._varSrc2MaxDepth.set<float>("MaxDepth", "x", "%%0.01f", maxDpt * 0.05f);
}

void getFParamWID(fparam& fp) {
  int keytrk   = makeSigned(fp._inputKeyTrack);
  float veltrk = getVelTrack97(fp._inputVelTrack);
  float depth  = getVelTrack97(fp._inputDepth);
  float minDpt = getVelTrack97(fp._inputMinDepth);
  float maxDpt = getVelTrack97(fp._inputMaxDepth);

  fp._varCoarseAdjust.set<float>("Coarse", "oct", "%%0.001f", get72Adjust(fp._inputCourse));

  fp._varKeyTrack.set<float>("KeyTrack", "oct/key", "%%0.01f", float(keytrk) * 0.002f);
  fp._varVelTrack.set<float>("VelTrack", "oct", "%%0.01f", veltrk);
  fp._varSrc1Depth.set<float>("Depth", "oct", "%%0.01f", depth);
  fp._varSrc2MinDepth.set<float>("MinDepth", "oct", "%%0.01f", minDpt);
  fp._varSrc2MaxDepth.set<float>("MaxDepth", "oct", "%%0.01f", maxDpt);
}

void getFParamPWM(fparam& fp) {
  int keytrk   = makeSigned(fp._inputKeyTrack);
  float veltrk = makeSigned(fp._inputVelTrack);
  float depth  = makeSigned(fp._inputDepth);
  float minDpt = makeSigned(fp._inputMinDepth);
  float maxDpt = makeSigned(fp._inputMaxDepth);

  fp._varCoarseAdjust.set<float>("Coarse", "pct", "%%d", int(fp._inputCourse));

  fp._varKeyTrack.set<float>("KeyTrack", "pct/key", "%%0.01f", float(keytrk) * 0.1f);
  fp._varVelTrack.set<float>("VelTrack", "pct", "%%d", veltrk);
  fp._varSrc1Depth.set<float>("Depth", "pct", "%%d", depth);
  fp._varSrc2MinDepth.set<float>("MinDepth", "pct", "%%d", minDpt);
  fp._varSrc2MaxDepth.set<float>("MaxDepth", "pct", "%%d", maxDpt);
}

void getFParamSEP(fparam& fp) {
  int keytrk   = 2 * makeSigned(fp._inputKeyTrack);
  float veltrk = getVelTrack96(fp._inputVelTrack);
  float depth  = getVelTrack96(fp._inputDepth);
  float minDpt = getVelTrack96(fp._inputMinDepth);
  float maxDpt = getVelTrack96(fp._inputMaxDepth);

  fp._varCoarseAdjust.set<float>("Coarse", "ct", "%%d", 100 * makeSigned(fp._inputCourse));

  fp._varKeyTrack.set<float>("KeyTrack", "ct/key", "%%d", keytrk);
  fp._varVelTrack.set<float>("VelTrack", "ct", "%%d", veltrk);
  fp._varSrc1Depth.set<float>("Depth", "ct", "%%d", depth);
  fp._varSrc2MinDepth.set<float>("MinDepth", "ct", "%%d", minDpt);
  fp._varSrc2MaxDepth.set<float>("MaxDepth", "ct", "%%d", maxDpt);
}

void getFParamEVNODD(fparam& fp) {
  float keytrk = 0.02f * makeSigned(fp._inputKeyTrack);
  float veltrk = makeSigned(fp._inputVelTrack);
  float depth  = makeSigned(fp._inputDepth);
  float minDpt = makeSigned(fp._inputMinDepth);
  float maxDpt = makeSigned(fp._inputMaxDepth);

  fp._varCoarseAdjust.set<float>("Coarse", "dB", "%%d", makeSigned(fp._inputCourse));

  fp._varKeyTrack.set<float>("KeyTrack", "dB/key", "%%d", keytrk);
  fp._varVelTrack.set<float>("VelTrack", "dB", "%%d", veltrk);
  fp._varSrc1Depth.set<float>("Depth", "dB", "%%d", depth);
  fp._varSrc2MinDepth.set<float>("MinDepth", "dB", "%%d", minDpt);
  fp._varSrc2MaxDepth.set<float>("MaxDepth", "dB", "%%d", maxDpt);
}

void filescanner::fparamVarOutput(const fparamVar& fpv, const std::string& blkname, rapidjson::Value& jsono) {
  const auto& name = fpv._name;
  const auto& unit = fpv._units;
  const auto& val  = fpv._value;

  Value fpvout(kObjectType);

  if (auto as_float = val.tryAs<float>()){
    float fval = as_float.value();
    AddMember(fpvout, "Value", fval);
  }
  else if (auto as_int = val.tryAs<int>()){
    AddMember(fpvout, "Value", as_int.value());
  }
  else if (auto as_str = val.tryAs<std::string>()){
    AddStringKVMember(fpvout, "Value", as_str.value().c_str());
  }
  else {
    OrkAssert(false);
  }
  AddMember(fpvout, "Unit", unit);
  AddMember(jsono, name, fpvout);
}

void filescanner::fparamOutput(const fparam& fp, const std::string& blkname, rapidjson::Value& jsono) {
  if (fp._varCoarseAdjust)
    fparamVarOutput(fp._varCoarseAdjust, blkname, jsono);
  if (fp._varFine)
    fparamVarOutput(fp._varFine, blkname, jsono);
  if (fp._var14)
    fparamVarOutput(fp._var14, blkname, jsono);
  if (fp._var15)
    fparamVarOutput(fp._var15, blkname, jsono);
  if (fp._varKeyStart._note.length()) {
    Value fpvout(kObjectType);

    AddMember(fpvout, "Note", fp._varKeyStart._note);
    AddMember(fpvout, "Octave", fp._varKeyStart._octave);
    AddMember(fpvout, "Mode", fp._varKeyStart._mode);

    AddMember(jsono, "KeyStart", fpvout);
    //        fparamVarOutput( fp._varKeyStart, blkname, jsono );
  }

  if (fp._varKeyTrack)
    fparamVarOutput(fp._varKeyTrack, blkname, jsono);

  if (fp._varVelTrack)
    fparamVarOutput(fp._varVelTrack, blkname, jsono);

  /////////////////

  Value src1out(kObjectType);
  Value src2out(kObjectType);

  /////////////////

  AddStringKVMember(src1out, "Source", getControlSourceName(fp._inputSrc1));
  if (fp._varSrc1Depth)
    fparamVarOutput(fp._varSrc1Depth, blkname, src1out);

  AddStringKVMember(src2out, "Source", getControlSourceName(fp._inputSrc2));
  AddStringKVMember(src2out, "DepthControl", getControlSourceName(fp._inputDptCtl));
  if (fp._varSrc2MinDepth)
    fparamVarOutput(fp._varSrc2MinDepth, blkname, src2out);
  if (fp._varSrc2MaxDepth)
    fparamVarOutput(fp._varSrc2MaxDepth, blkname, src2out);

  /////////////////

  AddMember(jsono, "Src1", src1out);
  AddMember(jsono, "Src2", src2out);

  /////////////////

  AddMember(jsono, "Pad", fp._outputPAD);
  AddStringKVMember(jsono, "FiltAlg", fp._outputFiltAlg);
  AddMember(jsono, "MoreTSCR", fp._inputMoreTSCR);
}
} // namespace ork::audio::singularity::krzio
