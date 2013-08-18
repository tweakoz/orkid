////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/asset/Asset.h>
#include <ork/asset/FileAssetLoader.h>
#include <ork/kernel/string/PieceString.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/file/file.h>
#include <ork/kernel/Array.h>
#include <ork/kernel/string/string.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace asset {
///////////////////////////////////////////////////////////////////////////////

void FileAssetLoader::AddFileExtension(ConstString extension)
{
	mFileExtensions.push_back(extension);
}

///////////////////////////////////////////////////////////////////////////////

bool FileAssetLoader::FindAsset(const PieceString &name, MutableString result, int first_extension)
{
    //orkprintf( "FindAsset<%s>\n", ork::Application::AddPooledString(name).c_str() );
	//////////////////////////////////////////
	// do we already have an extension
	//////////////////////////////////////////

	file::Path pathobjnoq(name);
	file::Path pathobj(name);
	AssetPath::NameType pathsp, qrysp;
	pathobj.SplitQuery( pathsp, qrysp );
	pathobjnoq.Set( pathsp.c_str() );

	file::Path::NameType preext;
	preext.format( ".%s", pathobjnoq.GetExtension().c_str() );
	bool has_extension = pathobjnoq.GetExtension().length()!=0;
	bool has_valid_extension = false;
	if( has_extension )
	{
		for( std::size_t ext = std::size_t(first_extension); ext < mFileExtensions.size(); ext++ )
		{
			if( 0 == strcmp(mFileExtensions[ext].c_str(),preext.c_str()) )
			{
				has_valid_extension = true;
			}
		}
	}

	//////////////////////////////////////////
	// check Munged Paths first (Munged path is a path run thru 1 or more path converters)
	//////////////////////////////////////////

	file::Path::SmallNameType url = pathobjnoq.GetUrlBase();

	const SFileDevContext& ctx = ork::CFileEnv::UrlBaseToContext(url);

	//////////////////////
	// munge the path
	//////////////////////

	const orkvector<SFileDevContext::path_converter_type>& converters = ctx.GetPathConverters();

	int inumc = int( converters.size() );

	ork::fixedvector<ork::file::Path,8> MungedPaths;

	for( int i=0; i<inumc; i++ )
	{
		file::Path MungedPath = pathobjnoq;
		bool bret = converters[i]( MungedPath );
		if( bret )
		{
			MungedPaths.push_back(MungedPath);
    orkprintf( "MungedPaths<%s>\n", MungedPath.c_str() );
		}
	}
	//////////////////////////////////////
	// original path has lower priority
	MungedPaths.push_back( pathobjnoq );
	//////////////////////////////////////
    orkprintf( "MungedPaths<%s>\n", pathobjnoq.c_str() );

	//////////////////////
	// path is munged
	//////////////////////

	size_t inummunged = MungedPaths.size();

	for( size_t i=0; i<inummunged; i++ )
	{
		ork::file::Path MungedPath = MungedPaths[i];

		if( has_valid_extension ) // path already have an extension ?
		{
			if(CFileEnv::DoesFileExist(MungedPath))
			{
				result = MungedPath.c_str();
				return true;
			}
		}
		else // no extension test the registered extensions
		{
			for(std::size_t ext = std::size_t(first_extension); ext < mFileExtensions.size(); ext++)
			{
				ork::ConstString extension = mFileExtensions[ext];

				MungedPath.SetExtension( extension.c_str() );

				if(CFileEnv::DoesFileExist(MungedPath))
				{
					//pathobj.SetExtension( extension.c_str() );

					result = MungedPath.c_str();
					return true;
				}
			}
		}
	}

	//////////////////////////////////////////
	// if we got here then munged paths do not exist
	// try the original path
	//////////////////////////////////////////

	PieceString thename = name;

	if( has_valid_extension )
	{
					//printf( "TESTPTH3<%s>\n", pathobjnoq.c_str() );
		if(CFileEnv::DoesFileExist(pathobjnoq))
		{
			ork::PieceString ps(pathobjnoq.c_str());
			result = ps;
			//intf( "PTH3<%s>\n", pathobjnoq.c_str() );
			return true;
		}
	}
	else
	{
		for(std::size_t ext = std::size_t(first_extension); ext < mFileExtensions.size(); ext++)
		{
			ConstString extension = mFileExtensions[ext];

			pathobjnoq.SetExtension( extension.c_str() );

			//		printf( "TESTPTH4<%s>\n", pathobjnoq.c_str() );
			if(CFileEnv::DoesFileExist(pathobjnoq))
			{
				result = pathobjnoq.c_str();
				//printf( "PTH4<%s>\n", pathobjnoq.c_str() );
				return true;
			}
		}
	}

	//printf( "NOTFOUND\n" );
	return false;
}

///////////////////////////////////////////////////////////////////////////////

bool FileAssetLoader::CheckAsset(const PieceString &name)
{
	ArrayString<0> null_result;

	return FindAsset(name, null_result);
}

///////////////////////////////////////////////////////////////////////////////

bool FileAssetLoader::LoadAsset(Asset *asset)
{
	float ftime1 = ork::CSystem::GetRef().GetLoResRelTime();
#if defined(_XBOX) && defined(PROFILE)
	PIXBeginNamedEvent(0, "FileAssetLoader::LoadAsset(%s)", asset->GetName());
#endif
	ArrayString<256> asset_name;
				
	///////////////////////////////////////////////////////////////////////////////
	if(false == FindAsset(asset->GetName(), asset_name))
	{
		orkprintf("Error Loading File Asset %s\n", asset->GetName().c_str());
#if defined(ORKCONFIG_ASSET_UNLOAD)
		return false;
#else
		OrkAssertI(false, "Can't file asset second-time around");
#endif
	}

	bool out = LoadFileAsset(asset, asset_name);
#if defined(_XBOX) && defined(PROFILE)
	PIXEndNamedEvent();
#endif
	float ftime2 = ork::CSystem::GetRef().GetLoResRelTime();

	static float ftotaltime = 0.0f;
	static int iltotaltime = 0;

	ftotaltime += (ftime2-ftime1);

	int itotaltime = int(ftotaltime);

	//if( itotaltime > iltotaltime )
	{
		std::string outstr = ork::CreateFormattedString(
		"FILEAsset AccumTime<%f>\n", ftotaltime );
		////OutputDebugString( outstr.c_str() );
		iltotaltime = itotaltime;
	}
	return out;
}

///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
