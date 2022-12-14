////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

//
#include <utpp/UnitTest++.h>
#include <ork/file/path.h>
#include <boost/filesystem.hpp>

///////////////////////////////////////////////////////////////////////////////

using namespace ork;
using namespace ork::file;

TEST(PathCanComposeAndDecomposeUrlPaths) {
  DecomposedPath decomp;
  Path p1("testaa://archetypes/yo.txt");
  p1.decompose(decomp);
  Path p2;
  p2.compose(decomp);
  CHECK(p1 == p2);
}

TEST(PathCanRemoveDoubleSlashesCorrectly) {
  Path p1("//path/to/leading_doubleslash_bad");
  CHECK(strcmp(p1.toAbsolute().c_str(),"/path/to/leading_doubleslash_bad")==0);
  Path p1a("///path/to/leading_doubleslash_bad");
  CHECK(strcmp(p1a.toAbsolute().c_str(),"/path/to/leading_doubleslash_bad")==0);
  Path p1b("////path/to/leading_doubleslash_bad");
  CHECK(strcmp(p1b.toAbsolute().c_str(),"/path/to/leading_doubleslash_bad")==0);
  Path p2("/path/to//nonleading_doubleslash_bad");
  CHECK(strcmp(p2.toAbsolute().c_str(),"/path/to/nonleading_doubleslash_bad")==0);
  Path p2a("/path/to///nonleading_doubleslash_bad");
  CHECK(strcmp(p2a.toAbsolute().c_str(),"/path/to/nonleading_doubleslash_bad")==0);
  Path p2b("/path///to////nonleading_doubleslash_bad");
  CHECK(strcmp(p2b.toAbsolute().c_str(),"/path/to/nonleading_doubleslash_bad")==0);
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathOperatorAssignmentEquals) {
  Path p1("/archetypes/yo.txt");
  Path p2 = p1;
  CHECK(p1 == p2);
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathOperatorNotEqualTo) {
  Path p1("/archetypes/yo.aaa");
  Path p2("/archetypes/yo.bbb");
  CHECK(p1 != p2);
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathOperatorEqualTo) {
  Path p1("/archetypes/yo.aaa");
  Path p2("/archetypes/yo.aaa");
  CHECK(p1 == p2);
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathCopyConstruct) {
  Path p1("/archetypes/yo.aaa");
  Path p2(p1);
  CHECK(p1 == p2);
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathCorrectlyReturnsTheNamePartOfAPath) {
  Path testPath("/hello/world/test.txt");
  Path testPath2("/hello/world/");
  CHECK_EQUAL("test", testPath.getName().c_str());
  CHECK_EQUAL("", testPath2.getName().c_str());
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathCorrectlyReturnsTheExtensionPartOfAPath) {
  Path testPath("/hello/world/test.txt");
  Path testPath2("/hello/world/");
  Path testPath3("/hello/world/test");

  CHECK_EQUAL("txt", testPath.getExtension().c_str());
  CHECK_EQUAL("", testPath2.getExtension().c_str());
  CHECK_EQUAL("", testPath3.getExtension().c_str());
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathCanStoreQueryStrings) {
  Path testPath("testaa://hello/world/test.txt?yo=dude");
  CHECK_EQUAL(true, testPath.hasQueryString());
  CHECK_EQUAL("yo=dude", testPath.getQueryString().c_str());
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathCanNotStoreQueryStrings) {
  Path testPath("testaa://hello/world/test.txt");
  CHECK_EQUAL(false, testPath.hasQueryString());
  CHECK_EQUAL("", testPath.getQueryString().c_str());
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathSplit) {
  Path testPath("testaa://hello/world/test.txt");
  Path::NameType l, r;
  testPath.split(l, r, '.');
  CHECK_EQUAL(l.c_str(), "testaa://hello/world/test");
  CHECK_EQUAL(r.c_str(), "txt");
  Path::NameType l2, r2;
  Path p2(l);
  p2.split(l2, r2, '/');
  CHECK_EQUAL(l2.c_str(), "testaa://hello/world");
  CHECK_EQUAL(r2.c_str(), "test");
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathJoin) {
  Path a("/what/up");
  auto b = a / "yo";
  printf("b<%s>\n", b.c_str());
  CHECK_EQUAL(b.c_str(), "/what/up/yo");
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathToBfs) {
  Path a("/what/up/yo");
  auto b = a.toBFS();
  printf("b<%s>\n", b.c_str());
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathFromBfs) {
  boost::filesystem::path a("/what/up/yo");
  auto b = Path(a);
  printf("b<%s>\n", b.c_str());
}

///////////////////////////////////////////////////////////////////////////////

TEST(PathExists) {
  auto a      = Path::lib_dir();
  bool exists = boost::filesystem::exists(a.toBFS());
  printf("a<%s> exists<%d>\n", a.c_str(), int(exists));
}

///////////////////////////////////////////////////////////////////////////////

// TEST(PathHostNameTest)
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
