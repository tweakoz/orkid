////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//
#include <utpp/UnitTest++.h>
#include <ork/file/path.h>
#include <ork/file/fileenv.h>
#include <boost/filesystem.hpp>


///////////////////////////////////////////////////////////////////////////////

using namespace ork;
using namespace ork::file;

TEST(fdevctx1) {

  auto incdir =  Path::stage_dir() / "include";
  auto fdevctx = FileEnv::createContextForUriBase("includes://", incdir);

  auto P1 = Path("includes://assimp");
  auto P2 = P1/"anim.h";
  bool p1_exists = P1.doesPathExist();
  bool p1_is_folder = P1.isFolder();
  bool p2_exists = P2.doesPathExist();
  bool p2_is_file = P2.isFile();

  CHECK(p1_exists);
  CHECK(p1_is_folder);
  CHECK(p2_exists);
  CHECK(p2_is_file);

  P1.dump("P1");
  P2.dump("P2");
}

///////////////////////////////////////////////////////////////////////////////
