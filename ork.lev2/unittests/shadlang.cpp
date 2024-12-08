////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadlang.h>
#include <utpp/UnitTest++.h>
#include <string>

using namespace std::string_literals;
using namespace ork::lev2;

std::string snip1 = R"(
///////////////////////////////////////////////////////////////
// FxConfigs
///////////////////////////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "150"; }
///////////////////////////////////////////////////////////////
// Interfaces
///////////////////////////////////////////////////////////////
uniform_set push_constants { //
  mat4 mvp;
  mat4 mvp_l;
  mat4 mvp_r;
  vec4 ModColor;
}
sampler_set samplers (descriptor_set 0) {
  sampler2D ColorMap;
}
vertex_interface iface_vdefault : push_constants {
  inputs {
    vec4 position : POSITION;
    vec4 vtxcolor : COLOR0;
    vec2 uv0 : TEXCOORD0;
    vec2 uv1 : TEXCOORD1;
  }
  outputs {
    //vec4 frg_clr;
    vec2 frg_uv;
  }
}
///////////////////////////////////////////////////////////////
)";

TEST(shadlang1) {
  // this one works
  auto slp_cache = std::make_shared<shadlang::ShadLangParserCache>();
  auto tunit = parseFromString(slp_cache, "yo", snip1);
}

TEST(shadlang2) {
  // remove the last character from the snippet
  // this one does not work, because there is no newline at the end@
  auto snip2 = snip1.substr(0, snip1.size() - 1);
  auto slp_cache = std::make_shared<shadlang::ShadLangParserCache>();
  auto tunit = parseFromString(slp_cache, "yo", snip2);
}