#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/RegisterProperty.h>
#include <unittest++/UnitTest++.h>

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

	ork::rtti::Class::InitializeClasses();
	printf("yo\n" );
	
	int rval = 0;
    /////////////////////////////////////////////
    // default Run All Tests
    /////////////////////////////////////////////
    if( argc != 2 )
    {    
        printf( "ork.core unit test : usage :\n");
        printf( "<exename> list : list test names\n" );
        printf( "<exename> testname : run 1 test named\n" );
        printf( "<exename> all : run all tests\n" );
    }
    /////////////////////////////////////////////
    // run a single test (higher signal/noise for debugging)
    /////////////////////////////////////////////
    else if( argc == 2 )
    {
        bool blist_tests = (0 == strcmp( argv[1], "list" ));
        bool all_tests = (0 == strcmp( argv[1], "all" ));

        if( all_tests )
            return UnitTest::RunAllTests();

        const char *testname = argv[1];
        const UnitTest::TestList & List = UnitTest::Test::GetTestList();
        const UnitTest::Test* ptest = List.GetHead();
        int itest = 0;
        if( blist_tests )
        {
            printf( "//////////////////////////////////\n" );
            printf( "Listing Tests\n" );
            printf( "//////////////////////////////////\n" );
        }

        while( ptest )
        {
           const UnitTest::TestDetails & Details = ptest->m_details;

            if( blist_tests )
            {
                printf( "Test<%d:%s>\n", itest, Details.testName );
            }
            else if( 0 == strcmp( testname, Details.testName ) )
            {   printf( "Running Test<%s>\n", Details.testName );
                UnitTest::TestResults res;
                ptest->Run(res);
            }
            ptest = ptest->next;
            itest++;
        }
    }
    return rval;

}
