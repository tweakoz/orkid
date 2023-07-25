////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shadlang.h>
#include <utpp/UnitTest++.h>

using namespace ork::lev2;

TEST(shlang1) {

  auto shader_text =
      R"(
        ///////////////////
        // hello world
        ///////////////////
        function abc(int x, float y) {
            float a = 1.0;
            float v = 2.0;
            float b = (x+y)*7.0;
            v = v*2.0;
        }
        function def() {
            float X = (1.0+2.3)*7.0;
        }
    )";
  auto fndefs = shadlang::parse_fndefs(shader_text);
  CHECK(fndefs != nullptr);

}