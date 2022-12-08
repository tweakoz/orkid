////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/rtti/downcast.h>
#include <boost/filesystem.hpp>
#include <ork/kernel/datacache.h>
#include <ork/util/logger.h>
#include <ork/kernel/orklut.hpp>

namespace bfs = boost::filesystem;
namespace ork::meshutil {
datablock_ptr_t assimpToXga(datablock_ptr_t inp_datablock);
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_anmio = logger()->createChannel("gfxanim.io.read", fvec3(1, 0.8, 1));

///////////////////////////////////////////////////////////////////////////////
struct chansettter {
  static void set(XgmAnim* anm, animchannel_ptr_t Channel, const void* pdata) {
    XgmFloatAnimChannel* F32Channel  = rtti::autocast(Channel.get());
    XgmVect3AnimChannel* Ve3Channel  = rtti::autocast(Channel.get());
    if (F32Channel) {
      const float* f32Base = (const float*)pdata;
      F32Channel->reserveFrames(anm->_numframes);
      for (size_t ifr = 0; ifr < anm->_numframes; ifr++) {
        float value = f32Base[ifr];
        F32Channel->setFrame(ifr, value);
      }
    } else if (Ve3Channel) {
      const fvec3* Ve3Base = (const fvec3*)pdata;
      Ve3Channel->reserveFrames(anm->_numframes);
      for (size_t ifr = 0; ifr < anm->_numframes; ifr++) {
        fvec3 value = Ve3Base[ifr];
        Ve3Channel->setFrame(ifr, value);
      }
    }
  }
}; // struct chansetter

///////////////////////////////////////////////////////////////////////////////
bool XgmAnim::unloadUnManaged(XgmAnim* anm) {
#if defined(ORKCONFIG_ASSET_UNLOAD)
  anm->mPose.clear();
  anm->miNumFrames = 0;
  anm->mJointAnimationChannels.clear();
  anm->mMaterialAnimationChannels.clear();
#endif
  return true;
}
bool XgmAnim::_loaderSelect(XgmAnim* anm, datablock_ptr_t datablock) {
  DataBlockInputStream datablockstream(datablock);
  Char4 check_magic(datablockstream.getItem<uint32_t>());
  if (check_magic == Char4("chkf")) // its a chunkfile
    return _loadXGA(anm, datablock);
  if (check_magic == Char4("glTF")) // its a glb (binary)
    return _loadAssimp(anm, datablock);
  auto extension = datablock->_vars->typedValueForKey<std::string>("file-extension").value();
  if (extension == "gltf" or //
      extension == "fbx")
    return _loadAssimp(anm, datablock);
  OrkAssert(false);
  return false;
}
bool XgmAnim::_loadXGA(XgmAnim* anm, datablock_ptr_t datablock) {
  // AssetPath fnameext(fname);
  // fnameext.SetExtension("xga");
  // AssetPath ActualPath = fnameext.ToAbsolute();
  /////////////////////////////////////////////////////////////
  OrkHeapCheck();
  chunkfile::DefaultLoadAllocator allocator;
  chunkfile::Reader chunkreader(datablock, allocator);
  OrkHeapCheck();
  /////////////////////////////////////////////////////////////
  if (chunkreader.IsOk()) {
    chunkfile::InputStream* HeaderStream   = chunkreader.GetStream("header");
    chunkfile::InputStream* AnimDataStream = chunkreader.GetStream("animdata");
    ////////////////////////////////////////////////////////
    int inumchannels = 0, inumframes = 0;
    int inumjointchannels    = 0;
    int inummaterialchannels = 0;
    int ichannelclass = 0, iobjname = 0, ichnname = 0, iusgname = 0, idataoffset = 0;
    int inumposebones = 0;
    ////////////////////////////////////////////////////////
    HeaderStream->GetItem(inumframes);
    HeaderStream->GetItem(inumchannels);
    anm->_numframes = inumframes;
    ////////////////////////////////////////////////////////
    auto do_matrix_channel = [&]( std::shared_ptr<XgmMatrixAnimChannel> matrix_channel, //
                                  const fmtx4* matrix_src_data ){ //

      matrix_channel->reserveFrames(anm->_numframes);
      logchan_anmio->log("LDUMP: anm<%p> mtxchan<%p> matrix_src_data<%p>", //
                         anm, //
                         (void*) matrix_channel.get(), //
                         matrix_src_data );

      fmtx4 scalematrix;
      scalematrix.compose(fvec3(0,0,0),fquat(),0.01f);

      for (size_t ifr = 0; ifr < anm->_numframes; ifr++) {
        fmtx4 src_matrix = scalematrix*matrix_src_data[ifr];
        if(ifr==0){
          auto LDUMP = src_matrix.dump4x3cn();
          logchan_anmio->log("LDUMP:   mtx<%s>", LDUMP.c_str());
        }
        matrix_channel->setFrame(ifr, src_matrix);
      }
    };
    ////////////////////////////////////////////////////////
    HeaderStream->GetItem(inumjointchannels);
    for (int ichan = 0; ichan < inumjointchannels; ichan++) {
      HeaderStream->GetItem(ichannelclass);
      HeaderStream->GetItem(iobjname);
      HeaderStream->GetItem(ichnname);
      HeaderStream->GetItem(iusgname);
      HeaderStream->GetItem(idataoffset);
      const char* pchannelclass        = chunkreader.GetString(ichannelclass);
      const char* pobjname             = chunkreader.GetString(iobjname);
      const char* pchnname             = chunkreader.GetString(ichnname);
      const char* pusgname             = chunkreader.GetString(iusgname);
      auto pdata                       = (const fmtx4*) AnimDataStream->GetDataAt(idataoffset);
      ork::object::ObjectClass* pclass = rtti::autocast(rtti::Class::FindClass(pchannelclass));
      auto channelobj = pclass->createShared();
      auto matrixchannel               = std::dynamic_pointer_cast<XgmMatrixAnimChannel>(channelobj);

      logchan_anmio->log(
          "MatrixChannel<%p:%s> ChannelClass<%s> pchnname<%s> objname<%s> numframes<%d>", (void*) matrixchannel.get(), pchnname, pchannelclass, pchnname, pobjname, inumframes);

      matrixchannel->SetChannelName((pchnname));
      matrixchannel->SetObjectName((pobjname));
      matrixchannel->SetChannelUsage((pusgname));
      do_matrix_channel(matrixchannel, pdata);
      anm->AddChannel(pchnname, matrixchannel);
    }
    OrkHeapCheck();
    ////////////////////////////////////////////////////////
    HeaderStream->GetItem(inummaterialchannels);
    for (int ichan = 0; ichan < inummaterialchannels; ichan++) {
      HeaderStream->GetItem(ichannelclass);
      HeaderStream->GetItem(iobjname);
      HeaderStream->GetItem(ichnname);
      HeaderStream->GetItem(iusgname);
      HeaderStream->GetItem(idataoffset);
      const char* pchannelclass        = chunkreader.GetString(ichannelclass);
      const char* pobjname             = chunkreader.GetString(iobjname);
      const char* pchnname             = chunkreader.GetString(ichnname);
      const char* pusgname             = chunkreader.GetString(iusgname);
      void* pdata                      = AnimDataStream->GetDataAt(idataoffset);
      ork::object::ObjectClass* pclass = rtti::autocast(rtti::Class::FindClass(pchannelclass));
      auto Channel                     = std::dynamic_pointer_cast<XgmAnimChannel>(pclass->createShared());
      Channel->SetChannelName((pchnname));
      Channel->SetObjectName((pobjname));
      Channel->SetChannelUsage((pusgname));
      chansettter::set(anm, Channel, pdata);
      anm->AddChannel(Channel->GetChannelName(), Channel);
    }
    OrkHeapCheck();
    ////////////////////////////////////////////////////////
    HeaderStream->GetItem(inumposebones);
    fmtx4 bone_matrix;
    logchan_anmio->log("inumposebones<%d>", inumposebones);
    for (int iposeb = 0; iposeb < inumposebones; iposeb++) {
      HeaderStream->GetItem(ichnname);
      HeaderStream->GetItem(bone_matrix);
      std::string PoseChannelName = chunkreader.GetString(ichnname);
      PoseChannelName             = ork::string::replaced(PoseChannelName, "_", ".");
      anm->_pose.AddSorted(PoseChannelName,bone_matrix);
    }
    ////////////////////////////////////////////////////////
  }
  OrkHeapCheck();
  return chunkreader.IsOk();
}
///////////////////////////////////////////////////////////////////////////////
bool XgmAnim::_loadAssimp(XgmAnim* anm, datablock_ptr_t inp_datablock) {
  auto basehasher = DataBlock::createHasher();
  basehasher->accumulateString("assimp2xga");
  basehasher->accumulateString("version-120622d");
  inp_datablock->accumlateHash(basehasher);
  basehasher->finish();
  uint64_t hashkey   = basehasher->result();
  auto xga_datablock = DataBlockCache::findDataBlock(hashkey);
  if (not xga_datablock) {
    // datablock_ptr_t assimpToXga(datablock_ptr_t inp_datablock);
    xga_datablock = meshutil::assimpToXga(inp_datablock);
    DataBlockCache::setDataBlock(hashkey, xga_datablock);
  }
  return _loadXGA(anm, xga_datablock);
}
///////////////////////////////////////////////////////////////////////////////
bool XgmAnim::LoadUnManaged(XgmAnim* anm, const AssetPath& fname) {
  bool rval       = false;
  auto ActualPath = fname.ToAbsolute();

  printf("ActualPath<%s>\n", ActualPath.c_str());
  // anm->msModelName = (fname.c_str());
  if (auto datablock = datablockFromFileAtPath(ActualPath)) {
    ///////////////////////////////////
    // annotate the datablock with some info
    //  that might be useful in loading the file
    ///////////////////////////////////
    auto actual_as_bfs = ActualPath.toBFS();
    auto base_dir      = actual_as_bfs.parent_path();
    OrkAssert(bfs::exists(actual_as_bfs));
    OrkAssert(bfs::is_regular_file(actual_as_bfs));
    OrkAssert(bfs::exists(base_dir));
    OrkAssert(bfs::is_directory(base_dir));
    datablock->_vars->makeValueForKey<std::string>("file-extension") = ActualPath.GetExtension().c_str();
    datablock->_vars->makeValueForKey<bfs::path>("base-directory")   = base_dir;
    ///////////////////////////////////
    rval = _loaderSelect(anm, datablock);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
