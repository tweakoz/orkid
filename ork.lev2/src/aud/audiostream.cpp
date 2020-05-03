////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/file/chunkfile.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/kernel/orklut.hpp>
#include <ork/asset/FileAssetLoader.h>
#include <ork/asset/FileAssetNamer.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::AudioStream, "lev2::audiostream");

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

class AudioStreamLoader final : public ork::asset::FileAssetLoader {
public:
  AudioStreamLoader();

  bool LoadFileAsset(asset::asset_ptr_t asset, ConstString filename) override;
  void DestroyAsset(asset::asset_ptr_t asset) override {
    auto stream = std::dynamic_pointer_cast<AudioStream>(asset);
    AudioDevice::GetDevice()->DestroyStream(stream.get());
  }
};

///////////////////////////////////////////////////////////////////////////////

AudioStreamLoader::AudioStreamLoader()
    : FileAssetLoader(AudioStream::GetClassStatic()) {
  auto datactx = FileEnv::contextForUriProto("data://");
  AddLocation(datactx, ".wav");
}

///////////////////////////////////////////////////////////////////////////////

bool AudioStreamLoader::LoadFileAsset(asset::asset_ptr_t asset, ConstString filename) {
  auto stream = std::dynamic_pointer_cast<AudioStream>(asset);
  bool bOK    = AudioDevice::GetDevice()->LoadStream(stream.get(), filename);
  OrkAssert(bOK);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void AudioStream::Describe() {
  auto loader = new AudioStreamLoader;
  GetClassStatic()->AddLoader(loader);
  GetClassStatic()->SetAssetNamer("data://audio/streams");
  GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("lev2::audiostream"));
}

///////////////////////////////////////////////////////////////////////////////

bool AudioDevice::LoadStream(AudioStream* pstream, ConstString filename) {
  return DoLoadStream(pstream, filename);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AudioStreamPlayback* AudioDevice::PlayStream(AudioStream* pstream) {
  return DoPlayStream(pstream);
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevice::StopStream(AudioStreamPlayback* pb) {
  return DoStopStream(pb);
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
