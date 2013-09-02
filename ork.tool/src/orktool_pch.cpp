////////////////////////////////////////////////////////////////////////////////
// Copyright 2007, Michael T. Mayers, all rights reserved.
////////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <orktool/toolcore/selection.h>
#include <orktool/toolcore/choiceman.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxanim.h>
#include <ork/kernel/string/string.h>
#include <unittest++/UnitTest++.h>

#include <orktool/toolcore/FunctionManager.h>

#include <QtGui/QMessageBox>

#if defined(ORK_OSX)
 #include <mach-o/dyld.h>
#endif

#include <unistd.h>

///////////////////////////////////////////////////////////////////////////////

static bool exit_gracefully = false;
extern char errorbuffer[];

///////////////////////////////////////////////////////////////////////////////

namespace ork {

void LoadLocalization(const char langcode[2]);

//namespace lev1 { void Init(); }	
namespace lev2 { void Init(); }	
	
namespace tool {


int Main_Filter( tokenlist toklist );
int Main_FilterTree( tokenlist toklist );
int QtTest( int argc, char **argv, bool bgamemode, bool bmenumode );

void MySetToolDataFolder()
{
	//////////////////////////////////////////
	// Register data:// urlbase
	static SFileDevContext WorkingDirContext;
	WorkingDirContext.SetFilesystemBaseEnable( true );
	WorkingDirContext.SetFilesystemBaseRel( "data/" );
	CFileEnv::RegisterUrlBase( "data://", WorkingDirContext );
	//////////////////////////////////////////
}

int main(int argc, char **argv)
{	
#if defined(ORK_OSX)
	char path[1024];
	uint32_t size = sizeof(path);
	if (_NSGetExecutablePath(path, &size) == 0)
	{
		ork::file::Path p(path);
		ork::file::Path::NameType l, r, l2, r2;
		p.Split(l,r,'/');
		ork::file::Path p2(l);
		p2.Split(l2,r2,'/');

	    printf("executable path is %s\n", path);
	    printf("l:%s\n", l.c_str());
	    printf("r:%s\n", r.c_str());
	    printf("l2:%s\n", l2.c_str());
	    printf("r2:%s\n", r2.c_str());

	    if( r2 == ork::file::Path::NameType("MacOS") ) // we are in a bundle
		    chdir(l2.c_str());
	}
	else
    	printf("buffer too small; need size %u\n", size);

#endif
	int iret = 0;

	try
	{
		//////////////////////////////////////////
		// Register lev2:// urlbase

		tokenlist toklist = ork::tool::Init(argc, argv);

		//////////////////////////////////////////
		
		if( toklist.empty() )
		{
			MySetToolDataFolder();
			//////////////////////////////////////////
			#if defined( ORK_CONFIG_QT )
			//ork::lev2::CAnimManager::GetRef().SetLoaderMode( ork::ELOADMODE_FILTER );
			ork::tool::QtTest( argc, argv, false, false );
			#endif
		}
		else if((argc == 1) || (toklist.front() == std::string("-editor")))
		{
			MySetToolDataFolder();
			//////////////////////////////////////////
			#if defined( ORK_CONFIG_QT )
			//ork::lev2::CAnimManager::GetRef().SetLoaderMode( ork::ELOADMODE_FILTER );
			ork::tool::QtTest( argc, argv, false, false );
			#endif
		}
		else if(toklist.front() == std::string("-help"))
		{
			orkprintf( "usage:\n" );
			//orkprintf( "miniork_tool -data foldername                               : set the data:// folder (defaults to working directory\n" );
			orkprintf( "miniork_tool -unittest                                     : run the miniork unittests\n" );
			orkprintf( "miniork_tool -editor                                        : run the miniork editor\n" );
			orkprintf( "miniork_tool -filter list                                   : list registered asset filters (eg miniork_tool -filter sf2:pxv test.sf2 yo.pxv)\n" );
			orkprintf( "miniork_tool -filter <filtername> source dest               : filter a single asset\n" );
			orkprintf( "miniork_tool -filtertree <filtername> sourcebase destbase   : filter a tree of assets\n" );
		}
		else if(toklist.front() == std::string("-unittest"))
		{
    		iret = UnitTest::RunAllTests();
		}
		else if(toklist.front() == std::string("-filter"))
		{
			exit_gracefully = true;
			iret = ork::tool::Main_Filter( toklist );

		}
		else if(toklist.front() == std::string("-execute"))
		{
			exit_gracefully = true;

			toklist.pop_front(); // Remove -execute
			FunctionManager::GetRef().ExecuteFunction(toklist);

		}
		else if(toklist.front() == std::string("-filtertree"))
		{
			exit_gracefully = true;
			ork::tool::Main_FilterTree( toklist );
		}
		else if(toklist.front() == std::string("-game"))
		{
			MySetToolDataFolder();
			#if defined( ORK_CONFIG_QT )
			CSystem::SetGlobalIntVariable( "ViewCollisionSpheres", 1 );
			ork::tool::QtTest( argc, argv, true, true );
			#endif
		}
		else if(toklist.front() == std::string("-gametest"))
		{
			MySetToolDataFolder();
			#if defined( ORK_CONFIG_QT )
			CSystem::SetGlobalIntVariable( "ViewCollisionSpheres", 1 );
			ork::tool::QtTest( argc, argv, true, false );
			#endif
		}
		else
		{
			MySetToolDataFolder();
			ork::tool::QtTest( argc, argv, false, false );
		}
	}
	catch(std::exception&)
	{
		if(exit_gracefully)
		{
			orkprintf( "OrkAssert Occured, exiting... [%s]\n", errorbuffer );
			return -1;
		}
		else
		{
			QMessageBox msgBox;
			msgBox.setText(errorbuffer);
			msgBox.exec();
			//MessageBox( 0, errorbuffer, "Fatal Error", MB_ICONERROR|MB_OK|MB_TASKMODAL );
			assert(false);

		}
	}

	return iret;
}


} // namespace tool
} // namespace ork
