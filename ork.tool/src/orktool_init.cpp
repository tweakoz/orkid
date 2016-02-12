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

#define NDEBUG
#define _GNU_SOURCE 
#define __STDC_CONSTANT_MACROS 
#define __STDC_FORMAT_MACROS 
#define __STDC_LIMIT_MACROS 

#include <OSL/oslconfig.h>
#include <OSL/oslversion.h>
#include <OSL/oslexec.h>

#if 0 //defined(_DARWIN) 
#define USE_PYTHON
#endif

#if defined(USE_PYTHON)
#include <Python.h>
#include <dispatch/dispatch.h>
#endif

//#define TEST_OSL

#if defined(TEST_OSL)
OSL_NAMESPACE_ENTER

class SimpleRenderer : public OSL::RendererServices
{
public:
    // Just use 4x4 matrix for transformations
    typedef OSL::Matrix44 Transformation;

    SimpleRenderer () {}
    ~SimpleRenderer () { }

    int supports (string_view feature) const final { return 0; }

    bool get_matrix (ShaderGlobals *sg, Matrix44 &result,
                     TransformationPtr xform,
                     float time) final  { return false; }
    bool get_matrix (ShaderGlobals *sg, Matrix44 &result,
                     ustring from, float time) final  { return false; }
    bool get_matrix (ShaderGlobals *sg, Matrix44 &result,
                     TransformationPtr xform) final  { return false; }
    bool get_matrix (ShaderGlobals *sg, Matrix44 &result,
                     ustring from) final  { return false; }


    bool get_inverse_matrix (ShaderGlobals *sg, Matrix44 &result,
                                     ustring to, float time) final  { return false; }

    bool get_array_attribute (ShaderGlobals *sg, bool derivatives, 
                                      ustring object, TypeDesc type, ustring name,
                                      int index, void *val ) final  { return false; }
    bool get_attribute (ShaderGlobals *sg, bool derivatives, ustring object,
                                TypeDesc type, ustring name, void *val) final  { return false; }
    bool get_userdata (bool derivatives, ustring name, TypeDesc type, 
                               ShaderGlobals *sg, void *val) final  { return false; }
    bool has_userdata (ustring name, TypeDesc type, ShaderGlobals *sg) { return false; }
};

OSL_NAMESPACE_EXIT

using namespace OSL;
static OSL::ErrorHandler errhandler;
static OSL::SimpleRenderer rend;  // RendererServices

void InitOSL()
{
    auto shadingsys = new OSL::ShadingSystem (&rend, NULL, &errhandler);
}

#endif // defined(TEST_OSL)

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

namespace lev2 { void Init(); }

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
		CSystem::SetGlobalStringVariable( "src://", CreateFormattedString("../../data/src/") );
		CSystem::SetGlobalStringVariable( "temp://", CreateFormattedString("../../data/temp/") );
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

	ork::lev2::Init();

#if defined(USE_PYTHON)
	InitPython();
#endif

//	xmlInitParser(); // must init libxml in main thread

    printf( "CPX\n");

#if defined(TEST_OSL)
	InitOSL();
#endif
	return toklist;

}

///////////////////////////////////////////////////////////////////////////////

}}
