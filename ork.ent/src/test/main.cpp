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

using namespace ork;
using namespace ork::ent;

extern void WaitForOpqExit();

namespace ork { namespace lev2 { void Init(); }}

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

    ork::lev2::Init();
	ork::rtti::Class::InitializeClasses();

    int iret = UnitTest::RunAllTests();

    ApplicationStack::Pop();

    return iret;
}
