////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "krzio.h"
#include <fstream>

using namespace rapidjson;
namespace ork::audio::singularity {

const s16* getK2V3InternalSoundBlock();

namespace krzio {

///////////////////////////////////////////////////////////////////////////////

SampleItem::SampleItem()
    : _valid(false) {
}

///////////////////////////////////////////////////////////////////////////////

MultiSample::MultiSample()
    : _isStereo(false) {
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::ParseSampleHeader(const datablock& db, //
                                    datablock::iterator& it, //
                                    int iObjectID, //
                                    std::string ObjName) { //
  bool bOK;
  u8 u8v;
  s8 s8v;


  u16 uBaseID, uNumSoundFilesMinusOne, uOffset, uReserved2, uCopyID;
  u8 uFlags, uReserved1;
  bOK = db.GetData(uBaseID, it);
  bOK = db.GetData(uNumSoundFilesMinusOne, it);
  bOK = db.GetData(uOffset, it);
  bOK = db.GetData(uFlags, it);
  bOK = db.GetData(uReserved1, it);
  bOK = db.GetData(uCopyID, it);
  bOK = db.GetData(uReserved2, it);

  auto itf = _samples.find(iObjectID);

  if(itf!=_samples.end()){
    auto end = _samples.rbegin();
    //printf ("Duplicate sample id<%d> name<%s>\n", iObjectID, ObjName.c_str() );
    iObjectID = end->first + 1;
    OrkAssert(false);
  }


  auto multisample              = new MultiSample;
  //multisample->_objectId        = iObjectID;
  multisample->_multiSampleName = ObjName;
  multisample->_numSoundFiles   = uNumSoundFilesMinusOne + 1;


  //printf( "ParseSample id<%d> name<%s>\n", iObjectID, ObjName.c_str() );

  bool is_krz_rom_sample = (iObjectID >=0 and iObjectID <= 199) or (iObjectID >= 800 and iObjectID <= 999);

  _samples[iObjectID]           = multisample;

  multisample->_isStereo = (uFlags & 1);

  int base_itidx = it.miIndex;

  for (u16 usamp = 0; usamp <= uNumSoundFilesMinusOne; usamp++) {
    it.miIndex = base_itidx + int(usamp) * 32;

    // int inewindex = it.miIndex+32;
    u8 uRootKey, uSubFlags;
    s8 sVolAdjust, sAltVolAdjust;
    u16 uHighestPitch, uName, uEnvOffset1, uEnvOffset2;
    u32 uStart, uAltStart, uLoopOfSpan, uEndOfSpan;
    signed int sSampleRate;
    bOK = db.GetData(uRootKey, it);
    bOK = db.GetData(uSubFlags, it);
    bOK = db.GetData(sVolAdjust, it);
    bOK = db.GetData(sAltVolAdjust, it);
    bOK = db.GetData(uHighestPitch, it);

    //////////////////////////

    int iPlaybackMode = uSubFlags & 3;  // 0 = Normal, 1 = Reverse, 2 = Bi-Direct
    int bIgnRelease   = uSubFlags & 4;  // 1 = on
    int bAltSense     = uSubFlags & 8;  // 0 = Normal, 1 = Reverse
    int bRamBased     = uSubFlags & 16; // 0 = ROM, 1 = RAM
    int bShareware    = uSubFlags & 32; // 0 = copy protected
    int bNeedsLoad    = uSubFlags & 64; // 1 = RAM, 0 = ROM

    bool bLoopSwitch = 0 == (uSubFlags & 0x80); // 0 = ON, 1 = OFF
    float fVolAdj    = float(sVolAdjust) * 0.5f;
    float fAltVolAdj = float(sAltVolAdjust) * 0.5f;

    //////////////////////////
    bOK = db.GetData(uName, it);

    datablock::iterator nameit = it;
    nameit.miIndex += uName - 2;
    const u8* pObjName = db.RefData(nameit);

    //////////////////////////

    bOK = db.GetData(uStart, it);
    bOK = db.GetData(uAltStart, it);
    bOK = db.GetData(uLoopOfSpan, it);
    bOK = db.GetData(uEndOfSpan, it);

    //////////////////////////

    bOK                        = db.GetData(uEnvOffset1, it);
    datablock::iterator env1it = it;
    env1it.miIndex += uEnvOffset1 - 2;
    const s16* pEnv1 = (const s16*)db.RefData(env1it);

    bOK                        = db.GetData(uEnvOffset2, it);
    datablock::iterator env2it = it;
    env2it.miIndex += uEnvOffset2 - 2;
    const s16* pEnv2 = (const s16*)db.RefData(env2it);

    std::vector<s16> env1R, env2R;
    std::vector<u16> env1TF, env2TF;

    for (int idx = 0; idx >= 0; idx += 2) {
      s16 dr = *(pEnv1 + idx);
      swapbytes<s16>(dr);
      u16 tf = *((u16*)pEnv1 + (idx + 1));
      swapbytes<u16>(tf);
      env1R.push_back(dr);
      env1TF.push_back(tf);
      if (0 == tf) {
        idx += 2;
        s16 dr = *(pEnv1 + idx);
        swapbytes<s16>(dr);
        u16 tf = *((u16*)pEnv1 + (idx + 1));
        swapbytes<u16>(tf);
        env1R.push_back(dr);
        env1TF.push_back(tf);
        break;
      }
    }
    for (int idx = 0; idx >= 0; idx += 2) {
      s16 dr = *(pEnv2 + idx);
      swapbytes<s16>(dr);
      u16 tf = *((u16*)pEnv2 + (idx + 1));
      swapbytes<u16>(tf);
      env2R.push_back(dr);
      env2TF.push_back(tf);
      if (0 == tf) {
        idx += 2;
        s16 dr = *(pEnv2 + idx);
        swapbytes<s16>(dr);
        u16 tf = *((u16*)pEnv2 + (idx + 1));
        swapbytes<u16>(tf);
        env2R.push_back(dr);
        env2TF.push_back(tf);
        break;
      }
    }

    //////////////////////////

    bOK = db.GetData(sSampleRate, it);

    //////////////////////////
    float fSampleRate = 1000000000.0f / float(sSampleRate);
    //////////////////////////
    float twroot2  = powf(2.0f, 1.0f / 12.0f);
    float fratio   = 96000.0f / floor((fSampleRate));
    float fcents   = log_base(2.0f, fratio) * 1200.0f;
    float calch    = float(uRootKey) * (fcents / 100.0f);
    float pitchADJ = calch - float(uHighestPitch);
    //////////////////////////

    //if (uStart <= 0x3fffff) // rom block 0 ?
    {
      char buffer[256];
      sprintf(buffer, "samples/%03d_%s_%d.aiff", iObjectID, ObjName.c_str(), usamp);

      SampleOpts opts;
      opts.start        = uStart;
      opts.end          = uEndOfSpan;
      opts.samplerate   = int(fSampleRate);
      int inumsmps      = (uEndOfSpan - uStart);
      SampleItem* pitem = new SampleItem;
      pitem->_uSubFlags = uSubFlags;
      multisample->_subSamples.push_back(pitem);
      pitem->miSampleId      = iObjectID;
      pitem->_subSampleIndex = int(usamp);
      pitem->_sampleData     = malloc(inumsmps * 2);
      pitem->_numSamples     = inumsmps;
      int idlsk              = GenSampleKey(iObjectID, usamp);
      _subSamples[idlsk]     = pitem;
      opts.inumchans         = 1;

  


      pitem->_rootKey      = int(uRootKey);
      pitem->_playbackMode = iPlaybackMode;
      pitem->_volAdj       = fVolAdj;
      pitem->_altVolAdj    = fAltVolAdj;
      pitem->_pitchADJ     = pitchADJ;
      pitem->_highestPitch = uHighestPitch;
      pitem->_sampleRate   = fSampleRate;
      pitem->_start        = int(uStart);
      pitem->_end          = int(uEndOfSpan);
      pitem->_loopPoint    = int(uLoopOfSpan - uStart) - 1;
      pitem->_ustart       = uStart;
      pitem->_ualt         = uAltStart;
      pitem->_uloop        = uLoopOfSpan;
      pitem->_uend         = uEndOfSpan;


      bLoopSwitch = bLoopSwitch & (uLoopOfSpan != uEndOfSpan);
      pitem->_isLooped     = bLoopSwitch;

      std::string nam = ork::FormatString("%s:%d", ObjName.c_str(), int(usamp));
     //printf( "/// sample==<%s>\n", nam.c_str() );
      if (bLoopSwitch) {
        //printf( "///\n");
        //printf( "/// sample==<%s>\n", nam.c_str() );
        //printf( "///\n");
        //printf( "uSubFlags<%02x>\n", (u8) uSubFlags );
        //printf( "uStart<%08x:%d>\n", uStart,uStart );
        //printf( "uEnd<%08x:%d>\n", uEndOfSpan,uEndOfSpan );
        //printf( "uLoopOfSpan<%08x:%d>\n", uLoopOfSpan,uLoopOfSpan );
        opts.loopstart = pitem->_loopPoint;
      }
      // printf( "inumsmps<%d>\n", inumsmps );

      if (inumsmps) {
        // gig::Sample* pdls_sample = _dlsFile.AddSample();
        // pitem->mpDlsSample = pdls_sample;
        std::string nam = ork::FormatString("%s:%d", ObjName.c_str(), int(usamp));
        // pdls_sample->pInfo->Name = nam;
        // pdls_sample->Channels = 1; // mono
        // pdls_sample->BitDepth = 16; // 16 bits
        // pdls_sample->FrameSize = 2;
        // pdls_sample->SamplesPerSecond = int(fSampleRate);
        // pdls_sample->MIDIUnityNote = int(uRootKey);//-int(pitchADJ);
        //////////////////////////////////////
        //////////////////////////////////////
        auto nameascc = (const char*)pObjName;
        if (strlen(nameascc))
          pitem->_subSampleName = nameascc;
        else
          pitem->_subSampleName = ObjName + " " + getMidiNoteName(int(uRootKey));
        //////////////////////////////////////
        auto slopeDBPS = [](s16 k2inp) -> float {
          float dbps = float(k2inp) / 4.0f;
          return dbps;
        };
        auto timeSECS = [](u16 k2inp) -> double {
          double time = double(k2inp); // 96000.0;
          return time;
        };
        //////////////////////////////////////
        for (auto item : env1R) {
          float slope = slopeDBPS(item);
          opts.natenvSlopes.push_back(slope);
          pitem->_natenvSlopes.push_back(slope);
        }
        for (auto item : env1TF) {
          float time = timeSECS(item);
          opts.natenvSegTimes.push_back(time);
          pitem->_natenvSegTimes.push_back(time);
        }
        //////////////////////////////////////
        for (auto item : env2R) {
          float slope = slopeDBPS(item);
          pitem->_altnatenvSlopes.push_back(slope);
        }
        for (auto item : env2TF) {
          float time = timeSECS(item);
          pitem->_altnatenvSegTimes.push_back(time);
        }
        //////////////////////////////////////
        // pdls_sample->Resize(inumsmps);
        void* pdest      = pitem->_sampleData;

        if(is_krz_rom_sample){
          auto sample_block = getK2V3InternalSoundBlock();
          pitem->_sampleData = (void*) (sample_block + opts.start);
        }
        else{
          pitem->_sampleData = (void*) nullptr;
        }
        //const void* psrc = (const void*)(_sfile.mpSampleData + opts.start);
        //memcpy(pdest, psrc, inumsmps * 2);
        //_sfile.WriteSample( buffer, opts );
        //////////////////////////////////////
        pitem->_valid = true;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitMultiSample(int object_id, const MultiSample* ms, rapidjson::Value& parent) {
  //if (ms->_objectId > 200)
  //  return;

  Value jsonobj(kObjectType);
  AddStringKVMember(jsonobj, "MultiSample", ms->_multiSampleName);
  jsonobj.AddMember("objectID", object_id, _japrog);

  rapidjson::Value samplearrayobject(kArrayType);

  for (auto sub : ms->_subSamples) {
    if (sub->_valid)
      emitSample(sub->miSampleId, sub, samplearrayobject);
  }

  jsonobj.AddMember("numSoundFiles", ms->_numSoundFiles, _japrog);
  jsonobj.AddMember("stereo", ms->_isStereo, _japrog);
  jsonobj.AddMember("samples", samplearrayobject, _japrog);
  parent.PushBack(jsonobj, _japrog);
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitSample(int object_id, const SampleItem* si, rapidjson::Value& parent) {

  rapidjson::Value sampleobject(kObjectType);

  sampleobject.AddMember("subSampleIndex", si->_subSampleIndex, _japrog);
  AddStringKVMember(sampleobject, "subSampleName", si->_subSampleName);
  sampleobject.AddMember("sampleLength", si->_numSamples, _japrog);
  sampleobject.AddMember("rootKey", si->_rootKey, _japrog);
  switch (si->_playbackMode) {
    case 0:
      sampleobject.AddMember("playbackMode", "Normal", _japrog);
      break;
    case 1:
      sampleobject.AddMember("playbackMode", "Reverse", _japrog);
      break;
    case 2:
      sampleobject.AddMember("playbackMode", "Bidirectional", _japrog);
      break;
  }
  sampleobject.AddMember("uSubFlags", (int)si->_uSubFlags, _japrog);
  sampleobject.AddMember("uStart", (int)si->_ustart, _japrog);
  sampleobject.AddMember("uAlt", (int)si->_ualt, _japrog);
  sampleobject.AddMember("uLoop", (int)si->_uloop, _japrog);
  sampleobject.AddMember("uEnd", (int)si->_uend, _japrog);

  sampleobject.AddMember("volAdjust", si->_volAdj, _japrog);
  sampleobject.AddMember("altVolAdjust", si->_altVolAdj, _japrog);
  sampleobject.AddMember("pitchAdjust", si->_pitchADJ, _japrog);
  sampleobject.AddMember("highestPitch", si->_highestPitch, _japrog);
  sampleobject.AddMember("sampleRate", si->_sampleRate, _japrog);

  sampleobject.AddMember("isLooped", si->_isLooped, _japrog);
  if (si->_isLooped) {
    sampleobject.AddMember("loopPoint", si->_loopPoint, _japrog);
  }
  /////////////////////////////
  rapidjson::Value env1Rarray(kArrayType);
  for (auto slope : si->_natenvSlopes)
    env1Rarray.PushBack(slope, _japrog);
  rapidjson::Value env1TFarray(kArrayType);
  for (auto time : si->_natenvSegTimes)
    env1TFarray.PushBack(int(time), _japrog);
  rapidjson::Value env1(kObjectType);
  env1.AddMember("segSlope (dB/sec)", env1Rarray, _japrog);
  env1.AddMember("segTime (sec)", env1TFarray, _japrog);
  sampleobject.AddMember("natEnv", env1, _japrog);
  /////////////////////////////
  rapidjson::Value env2Rarray(kArrayType);
  for (auto slope : si->_altnatenvSlopes)
    env2Rarray.PushBack(slope, _japrog);
  rapidjson::Value env2TFarray(kArrayType);
  for (auto time : si->_altnatenvSegTimes)
    env2TFarray.PushBack(int(time), _japrog);
  rapidjson::Value env2(kObjectType);
  env2.AddMember("segSlope (dB/sec)", env2Rarray, _japrog);
  env2.AddMember("segTime (sec)", env2TFarray, _japrog);
  sampleobject.AddMember("natEnv2", env2, _japrog);
  /////////////////////////////
  parent.PushBack(sampleobject, _japrog);
}
}} // namespace ork::audio::singularity::krzio
