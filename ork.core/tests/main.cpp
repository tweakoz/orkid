#include <utpp/UnitTest++.h>
#include <ork/kernel/thread.h>

int main(int argc, const char** argv )

{
    int rval = 0;

    ork::SetCurrentThreadName("main");

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
        const char *testname = argv[1];

        if( all_tests )
            return UnitTest::RunAllTests();

        if( blist_tests )
        {
            printf( "//////////////////////////////////\n" );
            printf( "Listing Tests\n" );
            printf( "//////////////////////////////////\n" );
        }

        auto test_list = UnitTest::Test::GetTestList();
        auto ptest = test_list.GetHead();
        int itest = 0;

        while( ptest )
        {
           auto& Details = ptest->m_details;

            if( blist_tests )
            {
                printf( "Test<%d:%s>\n", itest, Details.testName );
            }
            else if( 0 == strcmp( testname, Details.testName ) )
            {   printf( "Running Test<%s>\n", Details.testName );
		        UnitTest::TestResults testResults;
		        ptest->Run(testResults);
            }
            ptest = ptest->next;
            itest++;
        }
    }
    return rval;    return 0;
}
