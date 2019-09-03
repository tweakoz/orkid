////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/application/application.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>
#include <unittest++/UnitTest++.h>
#include <ork/file/file.h>

using namespace ork;
using namespace ork::ent;

extern void WaitForOpqExit();

namespace ork { namespace lev2 { void Init(const std::string& gfxlayer); }}
namespace ork { namespace ent { void Init(); }}

class TestApplication : public ork::Application
{
	RttiDeclareConcrete(TestApplication, ork::Application );
};

void TestApplication::Describe()
{

}

INSTANTIATE_TRANSPARENT_RTTI(TestApplication, "TestApplication");

int main(int argc, char** argv)
{
	TestApplication the_app;
    ApplicationStack::Push(&the_app);

	static SFileDevContext SrcPlatformLevel2FileContext;
	SrcPlatformLevel2FileContext.SetFilesystemBaseAbs( "data/src/" );
	SrcPlatformLevel2FileContext.SetPrependFilesystemBase( true );

	FileEnv::RegisterUrlBase( "src://", SrcPlatformLevel2FileContext );

	static SFileDevContext WorkingDirContext;
	OldSchool::SetGlobalStringVariable("data://", ork::file::GetStartupDirectory().c_str());

	static SFileDevContext TempPlatformLevel2FileContext;
	TempPlatformLevel2FileContext.SetFilesystemBaseAbs( "data/temp/" );
	TempPlatformLevel2FileContext.SetPrependFilesystemBase( true );

	FileEnv::RegisterUrlBase( "temp://", TempPlatformLevel2FileContext );


    ork::lev2::Init("dummy");
    ork::ent::Init();
	ork::rtti::Class::InitializeClasses();


	/////////////////////////////////////////////
	ork::Thread testthr("testthread");
	int iret = 0;
	bool testdone = false;
	testthr.start([&]()
	{
	    iret = UnitTest::RunAllTests();
	    testdone = true;
	});
	/////////////////////////////////////////////
	ork::Thread updthr("updatethread");
	bool upddone = false;
	updthr.start([&]()
	{
		OpqTest opqtest(&UpdateSerialOpQ());
	    while(false==testdone)
			UpdateSerialOpQ().Process();
	    upddone = true;
	});
	/////////////////////////////////////////////
	while(false==testdone)
	{
		OpqTest opqtest(&MainThreadOpQ());
		MainThreadOpQ().Process();
	}
	/////////////////////////////////////////////

	MainThreadOpQ().drain();
	UpdateSerialOpQ().drain();

    ApplicationStack::Pop();

    return iret;
}
