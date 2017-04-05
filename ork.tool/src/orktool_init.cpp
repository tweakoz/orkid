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
#include <ork/lev2/gfx/gfxanim.h> // For CAnimManager
#include <ork/kernel/string/string.h>
#include <orktool/toolcore/FunctionManager.h>

#include "EditorCamera.h"

#define USE_PYTHON

#if defined(USE_PYTHON)
#include <Python.h>
#include <dispatch/dispatch.h>
#endif

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

#if defined(USE_PYTHON)
void InitPython();
#endif

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

	if(CFileEnv::GetRef().DoesDirectoryExist("../ext/miniork"))
	{
		// Try the relative path from your project directory "data" folder
		CSystem::SetGlobalStringVariable( "lev2://", CreateFormattedString("../ext/miniork/data/platform_lev2/") );
		CSystem::SetGlobalStringVariable( "miniorkdata://", CreateFormattedString("../ext/miniork/data/") );
		CSystem::SetGlobalStringVariable( "src://", CreateFormattedString("../data/src/") );
		CSystem::SetGlobalStringVariable( "temp://", CreateFormattedString("../data/temp/") );
	}
	else if(CFileEnv::GetRef().DoesDirectoryExist("ext/miniork"))
	{

		CSystem::SetGlobalStringVariable( "lev2://", CreateFormattedString("ext/miniork/data/platform_lev2/") );
		CSystem::SetGlobalStringVariable( "miniorkdata://", CreateFormattedString("ext/miniork/data/") );
		CSystem::SetGlobalStringVariable( "src://", CreateFormattedString("data/src/") );
		CSystem::SetGlobalStringVariable( "temp://", CreateFormattedString("data/temp/") );
	}
	else
	{
		// Otherwise, assume we're in the root of miniork already
		CSystem::SetGlobalStringVariable("lev2://", std::string("data/platform_lev2/"));
		CSystem::SetGlobalStringVariable( "miniorkdata://", CreateFormattedString("data/") );
		CSystem::SetGlobalStringVariable( "src://", CreateFormattedString("data/src/") );
		CSystem::SetGlobalStringVariable( "temp://", CreateFormattedString("data/temp/") );
	}

	printf( "CPB\n");
	//////////////////////////////////////////
	// Register data:// urlbase

	static SFileDevContext WorkingDirContext;
	CSystem::SetGlobalStringVariable("data://", ork::file::GetStartupDirectory().c_str());
	
	printf( "CPB2\n");
	//////////////////////////////////////////
	// Register lev2:// data urlbase
	tokenlist toklist;

	static SFileDevContext LocPlatformLevel2FileContext;
	LocPlatformLevel2FileContext.SetFilesystemBaseAbs( CSystem::GetGlobalStringVariable( "lev2://" ).c_str() );
	LocPlatformLevel2FileContext.SetPrependFilesystemBase( true );

	CFileEnv::RegisterUrlBase( "lev2://", LocPlatformLevel2FileContext );

	printf( "CPB3\n");
	//////////////////////////////////////////
	// Register src:// data urlbase

	static SFileDevContext SrcPlatformLevel2FileContext;
	SrcPlatformLevel2FileContext.SetFilesystemBaseAbs( CSystem::GetGlobalStringVariable( "src://" ).c_str() );
	SrcPlatformLevel2FileContext.SetPrependFilesystemBase( true );

	CFileEnv::RegisterUrlBase( "src://", SrcPlatformLevel2FileContext );

	printf( "CPC\n");

	//////////////////////////////////////////
	// Register temp:// data urlbase

	static SFileDevContext TempPlatformLevel2FileContext;
	TempPlatformLevel2FileContext.SetFilesystemBaseAbs( CSystem::GetGlobalStringVariable( "temp://" ).c_str() );
	TempPlatformLevel2FileContext.SetPrependFilesystemBase( true );

	CFileEnv::RegisterUrlBase( "temp://", TempPlatformLevel2FileContext );

	//////////////////////////////////////////
	// Register miniork:// data urlbase

	static SFileDevContext LocPlatformMorkDataFileContext;
	LocPlatformMorkDataFileContext.SetFilesystemBaseAbs( CSystem::GetGlobalStringVariable( "miniorkdata://" ).c_str() );
	LocPlatformMorkDataFileContext.SetPrependFilesystemBase( true );

	CFileEnv::RegisterUrlBase( "miniorkdata://", LocPlatformMorkDataFileContext );

	//////////////////////////////////////////

	static SFileDevContext DataDirContext;
	
	DataDirContext.SetFilesystemBaseAbs( "data/pc" );
	DataDirContext.SetPrependFilesystemBase( true );

	static SFileDevContext MiniorkDirContext;
	MiniorkDirContext.SetFilesystemBaseAbs( CSystem::GetGlobalStringVariable( "lev2://" ).c_str() );
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

			CFileEnvDir* TheDir = CFileEnv::GetRef().OpenDir( dirname.c_str() );

			if( TheDir )
			{
				CSystem::SetGlobalStringVariable( "data://", dirname.c_str() );
				CFileEnv::GetRef().CloseDir( TheDir );
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

			CFileEnvDir* TheDir = CFileEnv::GetRef().OpenDir( dirname.c_str() );

			if( TheDir )
			{
				CSystem::SetGlobalStringVariable( "lev2://", dirname.c_str() );
				CFileEnv::GetRef().CloseDir( TheDir );
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

	CFileEnv::RegisterUrlBase( "data://", DataDirContext );
	CFileEnv::RegisterUrlBase( "lev2://", MiniorkDirContext );

	ork::lev2::Init("");

#if defined(USE_PYTHON)
	InitPython();
#endif

//	xmlInitParser(); // must init libxml in main thread

    printf( "CPX\n");

	return toklist;

}

///////////////////////////////////////////////////////////////////////////////

}}
