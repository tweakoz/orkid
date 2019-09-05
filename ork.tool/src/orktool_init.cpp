///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <orktool/toolcore/selection.h>
#include <orktool/toolcore/choiceman.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxanim.h> // For AnimManager
#include <ork/kernel/string/string.h>
#include <orktool/toolcore/FunctionManager.h>

#include "EditorCamera.h"


//#include <boost/python.hpp>

//extern "C" void xmlInitParser(); // must init libxml in main thread

//using namespace boost::python;

namespace ork {

namespace ent
{
	class LightMapperArchetype;
	class EntArchDeRef;
	class EntArchReRef;
	class EntArchSplit;
}

namespace tweakout { void Init(); }

namespace lev2 { void Init(const std::string& gfxlayer); }

namespace tool {


namespace ged
{
	///////////////////////////////////////////////////////////////////////////////
	class GedFactoryPlug;
	class GraphImportDelegate;
	class GraphExportDelegate;
	///////////////////////////////////////////////////////////////////////////////
}


void LinkMe()
{
	ork::rtti::Link<ork::tool::ged::GedFactoryPlug>();
	ork::rtti::Link<ork::ent::LightMapperArchetype>();

	ork::rtti::Link<ork::tool::ged::GraphImportDelegate>();
	ork::rtti::Link<ork::tool::ged::GraphExportDelegate>();

	ork::rtti::Link<ork::ent::EntArchDeRef>();
	ork::rtti::Link<ork::ent::EntArchReRef>();
	ork::rtti::Link<ork::ent::EntArchSplit>();

	ent::EditorCamArchetype::GetClassStatic();
	ent::EditorCamControllerData::GetClassStatic();
	ent::EditorCamControllerInst::GetClassStatic();
}

///////////////////////////////////////////////////////////////////////////////

tokenlist Init(int argc, char **argv)
{
	printf( "ork::tool::Init()\n");
	LinkMe();

	printf( "CPA\n");
	OldSchool::SetGlobalStringVariable("lev2://", std::string("ork.data/platform_lev2/"));
	OldSchool::SetGlobalStringVariable( "miniorkdata://", CreateFormattedString("ork.data/") );
	OldSchool::SetGlobalStringVariable( "src://", CreateFormattedString("ork.data/src/") );
	OldSchool::SetGlobalStringVariable( "temp://", CreateFormattedString("ork.data/temp/") );

	printf( "CPB\n");
	//////////////////////////////////////////
	// Register data:// urlbase

	static FileDevContext WorkingDirContext;

	auto base_dir = ork::file::GetStartupDirectory();

	if( getenv("ORKDOTBUILD_WORKSPACE_DIR")!=nullptr )
		base_dir = getenv("ORKDOTBUILD_WORKSPACE_DIR");

	OldSchool::SetGlobalStringVariable("data://", base_dir.c_str());

	printf( "base_dir<%s>\n", base_dir.c_str() );

	printf( "CPB2\n");
	//////////////////////////////////////////
	// Register lev2:// data urlbase
	tokenlist toklist;

	static FileDevContext LocPlatformLevel2FileContext;
	LocPlatformLevel2FileContext.SetFilesystemBaseAbs( OldSchool::GetGlobalStringVariable( "lev2://" ).c_str() );
	LocPlatformLevel2FileContext.SetPrependFilesystemBase( true );

	FileEnv::RegisterUrlBase( "lev2://", LocPlatformLevel2FileContext );

	printf( "CPB3\n");

	//////////////////////////////////////////
	// Register src:// data urlbase

	static FileDevContext SrcPlatformLevel2FileContext;
	SrcPlatformLevel2FileContext.SetFilesystemBaseAbs( OldSchool::GetGlobalStringVariable( "src://" ).c_str() );
	SrcPlatformLevel2FileContext.SetPrependFilesystemBase( true );

	FileEnv::RegisterUrlBase( "src://", SrcPlatformLevel2FileContext );

	printf( "CPC\n");

	//////////////////////////////////////////
	// Register temp:// data urlbase

	static FileDevContext TempPlatformLevel2FileContext;
	TempPlatformLevel2FileContext.SetFilesystemBaseAbs( OldSchool::GetGlobalStringVariable( "temp://" ).c_str() );
	TempPlatformLevel2FileContext.SetPrependFilesystemBase( true );

	FileEnv::RegisterUrlBase( "temp://", TempPlatformLevel2FileContext );

	//////////////////////////////////////////
	// Register miniork:// data urlbase

	static FileDevContext LocPlatformMorkDataFileContext;
	LocPlatformMorkDataFileContext.SetFilesystemBaseAbs( OldSchool::GetGlobalStringVariable( "miniorkdata://" ).c_str() );
	LocPlatformMorkDataFileContext.SetPrependFilesystemBase( true );

	FileEnv::RegisterUrlBase( "miniorkdata://", LocPlatformMorkDataFileContext );

	//////////////////////////////////////////

	static FileDevContext DataDirContext;

	DataDirContext.SetFilesystemBaseAbs( "ork.data/pc" );
	DataDirContext.SetPrependFilesystemBase( true );

	static FileDevContext MiniorkDirContext;
	MiniorkDirContext.SetFilesystemBaseAbs( OldSchool::GetGlobalStringVariable( "lev2://" ).c_str() );
	MiniorkDirContext.SetPrependFilesystemBase( true );

	printf( "CPM\n");

	for( int iarg=1; iarg<argc; iarg++ )
	{
		const char *parg = argv[iarg];

		if( strcmp( parg, "-datafolder" ) == 0 )
		{
			file::Path pth( argv[iarg+1] );

			file::Path::NameType dirname = pth.ToAbsolute().c_str();
			std::transform( dirname.begin(), dirname.end(), dirname.begin(), ork::dos2unixpathsep() );

			FileEnvDir* TheDir = FileEnv::GetRef().OpenDir( dirname.c_str() );

			if( TheDir )
			{
				OldSchool::SetGlobalStringVariable( "data://", dirname.c_str() );
				FileEnv::GetRef().CloseDir( TheDir );
				DataDirContext.SetFilesystemBaseAbs( dirname );
			}
			else
			{
				OrkNonFatalAssertI( false, "specified Data Folder Does Not Exist!!\n" );
			}
			iarg ++;
		}
		else if( strcmp( parg, "-lev2folder" ) == 0 )
		{
			file::Path pth( argv[iarg+1] );

			file::Path::NameType dirname = pth.ToAbsolute().c_str();
			std::transform( dirname.begin(), dirname.end(), dirname.begin(), ork::dos2unixpathsep() );

			FileEnvDir* TheDir = FileEnv::GetRef().OpenDir( dirname.c_str() );

			if( TheDir )
			{
				OldSchool::SetGlobalStringVariable( "lev2://", dirname.c_str() );
				FileEnv::GetRef().CloseDir( TheDir );
				MiniorkDirContext.SetFilesystemBaseAbs( dirname );
			}
			else
			{
				OrkNonFatalAssertI( false, "specified MiniorkFolder Does Not Exist!!\n" );
			}
			iarg ++;
		}
		else
		{
			toklist.push_back( parg );
		}
	}

	FileEnv::RegisterUrlBase( "data://", DataDirContext );
	FileEnv::RegisterUrlBase( "lev2://", MiniorkDirContext );

	ork::lev2::Init("");


//	xmlInitParser(); // must init libxml in main thread

    printf( "CPX\n");

	return toklist;

}

///////////////////////////////////////////////////////////////////////////////

}}
