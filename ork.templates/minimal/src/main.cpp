#include <ork/pch.h>
#include <ork/application/application.h>

////////////////////////////////////////////////////////////////////

class TestApplication : public ork::Application
{ 
	RttiDeclareConcrete(TestApplication, ork::Application );
};
void TestApplication::Describe()
{
}

INSTANTIATE_TRANSPARENT_RTTI(TestApplication, "TestApplication");

////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
	TestApplication the_app;
    ApplicationStack::Push(&the_app);

	ork::rtti::Class::InitializeClasses();

	printf( "yo\n" );

    ApplicationStack::Pop();

	return 0;
}
