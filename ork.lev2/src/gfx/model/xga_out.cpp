////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

  int inumjointchannels    = (int)anm->_jointanimationchannels.size();
  int inummaterialchannels = (int)anm->GetNumMaterialChannels();

  int inumchannels = inumjointchannels + inummaterialchannels;

  int inumframes = anm->_numframes;

  
  printf("XGAOUT inumjointchannels<%d> inumframes<%d>\n", inumchannels, inumframes);

  HeaderStream->addItem(inumframes);
  HeaderStream->addItem(inumchannels);

  ///////////////////////////////////////////////////////////////////////////////////////////////

  const auto& joint_channels = anm->_jointanimationchannels;

  HeaderStream->addItem(int(joint_channels.size()));
  for (auto it : joint_channels ) {
    const std::string& ChannelName          = it.first;
    const std::string& ChannelUsage         = it.second->GetUsageSemantic();
    auto decomp_channel = it.second;
    const std::string& ObjectName           = decomp_channel->GetObjectName();

    int idataoffset = AnimDataStream->GetSize();

    if (decomp_channel) {
      for (int ifr = 0; ifr < inumframes; ifr++) {
        const DecompMatrix& decomp = decomp_channel->GetFrame(ifr);
        AnimDataStream->addItem(decomp._position);
        AnimDataStream->addItem(decomp._orientation);
        AnimDataStream->addItem(decomp._scale);
      }
    }

    auto channel_clazz = it.second->GetClass();

    int ichnclas = chunkwriter.stringIndex(channel_clazz->Name().c_str());
    int iobjname = chunkwriter.stringIndex(ObjectName.c_str());
    int ichnname = chunkwriter.stringIndex(ChannelName.c_str());
    int iusgname = chunkwriter.stringIndex(ChannelUsage.c_str());

    //printf("XGAOUT channelname<%s>\n", ChannelName.c_str());
    HeaderStream->addItem(ichnclas);
    HeaderStream->addItem(iobjname);
    HeaderStream->addItem(ichnname);
    HeaderStream->addItem(iusgname);
    HeaderStream->addItem(idataoffset);
  }

  ///////////////////////////////////////////////////////////////////////////////////////////////

  const auto& material_channels = anm->RefMaterialChannels();

  HeaderStream->addItem(int(material_channels.size()));
  for (auto it : material_channels ) {
    const std::string& ChannelName  = it.first;
    const std::string& ChannelUsage = it.second->GetUsageSemantic();
    const std::string& ObjectName   = it.second->GetObjectName();

    auto MtxChannel  = std::dynamic_pointer_cast<XgmDecompMatrixAnimChannel>(it.second);
    auto F32Channel  = std::dynamic_pointer_cast<XgmFloatAnimChannel>(it.second);
    auto Vec3Channel = std::dynamic_pointer_cast<XgmVect3AnimChannel>(it.second);

    int idataoffset = AnimDataStream->GetSize();

    if (MtxChannel) {
      for (int ifr = 0; ifr < inumframes; ifr++) {
        const DecompMatrix& decomp = MtxChannel->GetFrame(ifr);
        AnimDataStream->addItem(decomp._position);
        AnimDataStream->addItem(decomp._orientation);
        AnimDataStream->addItem(decomp._scale);
      }
    } else if (F32Channel) {
      for (int ifr = 0; ifr < inumframes; ifr++) {
        const float& value = F32Channel->GetFrame(ifr);
        AnimDataStream->addItem(value);
      }
    } else if (Vec3Channel) {
      for (int ifr = 0; ifr < inumframes; ifr++) {
        const fvec3& value = Vec3Channel->GetFrame(ifr);
        AnimDataStream->addItem(value);
      }
    }

    auto channel_clazz = it.second->GetClass();
    int ichnclas = chunkwriter.stringIndex(channel_clazz->Name().c_str());
    int iobjname = chunkwriter.stringIndex(ObjectName.c_str());
    int ichnname = chunkwriter.stringIndex(ChannelName.c_str());
    int iusgname = chunkwriter.stringIndex(ChannelUsage.c_str());
    HeaderStream->addItem(ichnclas);
    HeaderStream->addItem(iobjname);
    HeaderStream->addItem(ichnname);
    HeaderStream->addItem(iusgname);
    HeaderStream->addItem(idataoffset);
  }

  ///////////////////////////////////
  // write out pose information

  int inumposebones = (int)anm->_static_pose.size();

  HeaderStream->addItem(inumposebones);

  for (auto it : anm->_static_pose ) {
    const std::string& name = it.first;
    const DecompMatrix& decomp = it.second;
    // int idataoffset = AnimDataStream->GetSize();
    int ichannelname = chunkwriter.stringIndex(name.c_str());

    HeaderStream->addItem(ichannelname);
    // HeaderStream->addItem( idataoffset );
    HeaderStream->addItem(decomp._position);
    HeaderStream->addItem(decomp._orientation);
    HeaderStream->addItem(decomp._scale);
  }

  ////////////////////////////////////////////////////////////////////////////////////

  datablock_ptr_t out_datablock = std::make_shared<DataBlock>();
  chunkwriter.writeToDataBlock(out_datablock);

  ////////////////////////////////////////////////////////////////////////////////////

  return out_datablock;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
