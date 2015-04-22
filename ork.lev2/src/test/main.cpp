////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/lev2/gfx/gfxenv.h>

namespace ork { namespace lev2 { void Init(const std::string& gfxlayer); }}

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

    ork::lev2::Init("dummy");

	ork::rtti::Class::InitializeClasses();
	printf("yo\n" );
	return 0;
}
