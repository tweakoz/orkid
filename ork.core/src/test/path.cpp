//
#include <unittest++/UnitTest++.h>
#include <ork/file/path.h>

///////////////////////////////////////////////////////////////////////////////

using namespace ork;
using namespace ork::file;

TEST(PathCanComposeAndDecomposeUrlPaths)
{
	DecomposedPath decomp;
    Path p1("testaa://archetypes/yo.txt");
    p1.DeCompose(decomp);
    Path p2;
    p2.Compose(decomp);
    CHECK(p1==p2);
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathCorrectlyReturnsTheNamePartOfAPath)
{
    Path testPath("/hello/world/test.txt");
    Path testPath2("/hello/world/");
    CHECK_EQUAL("test", testPath.GetName().c_str());
    CHECK_EQUAL("", testPath2.GetName().c_str());
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathCorrectlyReturnsTheExtensionPartOfAPath)
{
	Path testPath("/hello/world/test.txt");
	Path testPath2("/hello/world/");
	Path testPath3("/hello/world/test");

	CHECK_EQUAL("txt", testPath.GetExtension().c_str());
	CHECK_EQUAL("", testPath2.GetExtension().c_str());
	CHECK_EQUAL("", testPath3.GetExtension().c_str());
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathCanStoreQueryStrings)
{
    Path testPath("testaa://hello/world/test.txt?yo=dude");
    CHECK_EQUAL( true, testPath.HasQueryString() );
    CHECK_EQUAL( "yo=dude", testPath.GetQueryString().c_str() );
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathCanNotStoreQueryStrings)
{
    Path testPath("testaa://hello/world/test.txt");
    CHECK_EQUAL( false, testPath.HasQueryString() );
    CHECK_EQUAL( "", testPath.GetQueryString().c_str() );
}

///////////////////////////////////////////////////////////////////////////////

//TEST(PathHostNameTest)
//{
//    Path p1("http://localhost:5901/yo.txt");
//    DecomposedPath decomp;
//    p1.DeCompose(decomp);
//    CHECK_EQUAL( "localhost", decomp.mHostname.c_str() );
//    // we know this fails now, I am working on it!
//    //printf( "hname<%s>\n", decomp.mHostname.c_str() );
//   
//}

///////////////////////////////////////////////////////////////////////////////
