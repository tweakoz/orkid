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

INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::AudioStream, "lev2::audiostream" );

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

class AudioStreamLoader : public ork::asset::FileAssetLoader
{
public:

	AudioStreamLoader();

	/*virtual*/ bool LoadFileAsset(asset::Asset *pAsset, ConstString filename);
	/*virtual*/ void DestroyAsset(asset::Asset *pAsset)
	{
		AudioStream* pstream = rtti::autocast(pAsset);
		AudioDevice::GetDevice()->DestroyStream( pstream );

		//delete compasset->GetComponent();
		//compasset->SetComponent(NULL);
	}
};

///////////////////////////////////////////////////////////////////////////////

AudioStreamLoader::AudioStreamLoader()
	:  FileAssetLoader( AudioStream::GetClassStatic() )
{
#if defined(_XBOX)
	AddLocation("data://",".xwma");
#else
	AddLocation("data://",".wav");
#endif
}

///////////////////////////////////////////////////////////////////////////////

bool AudioStreamLoader::LoadFileAsset(asset::Asset *pAsset, ConstString filename)
{
	AudioStream* pstream = rtti::autocast(pAsset);

	bool bOK = AudioDevice::GetDevice()->LoadStream( pstream,filename );
	OrkAssert( bOK );
		
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void AudioStream::Describe()
{
	auto loader = new AudioStreamLoader;
	GetClassStatic()->AddLoader(loader);
	GetClassStatic()->SetAssetNamer("data://audio/streams");
	GetClassStatic()->AddTypeAlias(ork::AddPooledLiteral("lev2::audiostream"));

}

///////////////////////////////////////////////////////////////////////////////

bool AudioDevice::LoadStream( AudioStream* pstream, ConstString filename )
{
	return DoLoadStream( pstream, filename );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AudioStreamPlayback* AudioDevice::PlayStream( AudioStream* pstream )
{
	return DoPlayStream( pstream );
}

///////////////////////////////////////////////////////////////////////////////

void AudioDevice::StopStream( AudioStreamPlayback* pb )
{
	return DoStopStream( pb );
}

///////////////////////////////////////////////////////////////////////////////

}}
