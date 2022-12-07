////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxanim.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/prop.h>

#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/file/chunkfile.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

datablock_ptr_t XgmAnim::Save(const XgmAnim* anm) {
  chunkfile::Writer chunkwriter("xga");
  ///////////////////////////////////
  chunkfile::OutputStream* HeaderStream   = chunkwriter.AddStream("header");
  chunkfile::OutputStream* AnimDataStream = chunkwriter.AddStream("animdata");
  ///////////////////////////////////

  int inumjointchannels    = (int)anm->GetNumJointChannels();
  int inummaterialchannels = (int)anm->GetNumMaterialChannels();

  int inumchannels = inumjointchannels + inummaterialchannels;

  int inumframes = anm->_numframes;

  printf("XGAOUT inumjointchannels<%d> inumframes<%d>\n", inumchannels, inumframes);

  HeaderStream->AddItem(inumframes);
  HeaderStream->AddItem(inumchannels);

  ///////////////////////////////////////////////////////////////////////////////////////////////

  const auto& joint_channels = anm->RefJointChannels();

  HeaderStream->AddItem(int(joint_channels.size()));
  for (auto it : joint_channels ) {
    const std::string& ChannelName          = it.first;
    const std::string& ChannelUsage         = it.second->GetUsageSemantic();
    const XgmMatrixAnimChannel* MtxChannel = rtti::autocast(it.second);
    const std::string& ObjectName           = MtxChannel->GetObjectName();

    int idataoffset = AnimDataStream->GetSize();

    if (MtxChannel) {
      for (int ifr = 0; ifr < inumframes; ifr++) {
        const fmtx4& Matrix = MtxChannel->GetFrame(ifr);
        AnimDataStream->AddItem(Matrix);
      }
    }

    auto channel_clazz = it.second->GetClass();

    int ichnclas = chunkwriter.stringIndex(channel_clazz->Name().c_str());
    int iobjname = chunkwriter.stringIndex(ObjectName.c_str());
    int ichnname = chunkwriter.stringIndex(ChannelName.c_str());
    int iusgname = chunkwriter.stringIndex(ChannelUsage.c_str());

    printf("XGAOUT channelname<%s>\n", ChannelName.c_str());
    HeaderStream->AddItem(ichnclas);
    HeaderStream->AddItem(iobjname);
    HeaderStream->AddItem(ichnname);
    HeaderStream->AddItem(iusgname);
    HeaderStream->AddItem(idataoffset);
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////

  const auto& material_channels = anm->RefMaterialChannels();

  HeaderStream->AddItem(int(material_channels.size()));
  for (auto it : material_channels ) {
    const std::string& ChannelName  = it.first;
    const std::string& ChannelUsage = it.second->GetUsageSemantic();
    const std::string& ObjectName   = it.second->GetObjectName();

    auto MtxChannel  = std::dynamic_pointer_cast<XgmMatrixAnimChannel>(it.second);
    auto F32Channel  = std::dynamic_pointer_cast<XgmFloatAnimChannel>(it.second);
    auto Vec3Channel = std::dynamic_pointer_cast<XgmVect3AnimChannel>(it.second);

    int idataoffset = AnimDataStream->GetSize();

    if (MtxChannel) {
      for (int ifr = 0; ifr < inumframes; ifr++) {
        const fmtx4& Matrix = MtxChannel->GetFrame(ifr);
        AnimDataStream->AddItem(Matrix);
      }
    } else if (F32Channel) {
      for (int ifr = 0; ifr < inumframes; ifr++) {
        const float& value = F32Channel->GetFrame(ifr);
        AnimDataStream->AddItem(value);
      }
    } else if (Vec3Channel) {
      for (int ifr = 0; ifr < inumframes; ifr++) {
        const fvec3& value = Vec3Channel->GetFrame(ifr);
        AnimDataStream->AddItem(value);
      }
    }

    auto channel_clazz = it.second->GetClass();
    int ichnclas = chunkwriter.stringIndex(channel_clazz->Name().c_str());
    int iobjname = chunkwriter.stringIndex(ObjectName.c_str());
    int ichnname = chunkwriter.stringIndex(ChannelName.c_str());
    int iusgname = chunkwriter.stringIndex(ChannelUsage.c_str());
    HeaderStream->AddItem(ichnclas);
    HeaderStream->AddItem(iobjname);
    HeaderStream->AddItem(ichnname);
    HeaderStream->AddItem(iusgname);
    HeaderStream->AddItem(idataoffset);
  }

  ///////////////////////////////////
  // write out pose information

  int inumposebones = (int)anm->_pose.size();

  HeaderStream->AddItem(inumposebones);

  for (auto it : anm->_pose ) {
    const std::string& name = it.first;
    const fmtx4& mtx = it.second;

    // int idataoffset = AnimDataStream->GetSize();
    int ichannelname = chunkwriter.stringIndex(name.c_str());

    HeaderStream->AddItem(ichannelname);
    // HeaderStream->AddItem( idataoffset );
    HeaderStream->AddItem(mtx);
  }

  ////////////////////////////////////////////////////////////////////////////////////

  datablock_ptr_t out_datablock = std::make_shared<DataBlock>();
  chunkwriter.writeToDataBlock(out_datablock);

  ////////////////////////////////////////////////////////////////////////////////////

  return out_datablock;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
