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
        ///////////////////////////////////////////////////////////////
        uniform_set ublock_vtx { 
          mat4 mvp;
          mat4 mvp_l;
          mat4 mvp_r;
        }
        ///////////////////////////////////////////////////////////////
        uniform_block ublock_frg {
          vec4 ModColor;
          sampler2D ColorMap;
        }
        ///////////////////////////////////////////////////////////////
        vertex_interface iface_vdefault : ublock_vtx {
          inputs {
            vec4 position : POSITION;
            vec4 vtxcolor : COLOR0;
            vec2 uv0 : TEXCOORD0;
            vec2 uv1 : TEXCOORD1;
          }
          outputs {
            vec4 frg_clr;
            vec2 frg_uv;
          }
        }
        ///////////////////////////////////////////////////////////////
        fragment_interface iface_fdefault {
          inputs {
            vec4 frg_clr;
            vec2 frg_uv;
          }
          outputs {
            vec4 out_clr;
          }
        }
        ///////////////////////////////////////////////////////////////
        fragment_interface iface_fmt : ublock_frg {
          inputs {
            vec2 frg_uv;
          }
          outputs {
            vec4 out_clr;
          }
        }       
        ///////////////////////////////////////////////////////////////
        function abc(int x, float y) {
            float a = 1.0;
            float v = 2.0;
            float b = (x+y)*7.0;
            v = v*2.0;
        }
        ///////////////////////////////////////////////////////////////
        vertex_shader vs_uitext : iface_vdefault {
          gl_Position = mvp * position;
          frg_clr     = vtxcolor;
          frg_uv      = uv0;
        }
        ///////////////////////////////////////////////////////////////
        fragment_shader ps_uitext : iface_fmt {
          vec4 s = texture(ColorMap, frg_uv);
          float texa = pow(s.a*s.r,0.75);
          //out_clr = vec4(ModColor.xyz, texa*ModColor.w);
        }
        ///////////////////////////////////////////////////////////////
        compute_shader cu_xxx {
          vec4 s = myFunction(ColorMap, frg_uv);
          float texa = pow(object.param.A[3],0.75);
          //out_clr = vec4(ModColor.xyz, texa*ModColor.w);
        }
        ///////////////////////////////////////////////////////////////
        function def() {
            float X = (1.0+2.3)*7.0;
        }
    )";
  auto tunit = shadlang::parse(shader_text);
  CHECK(tunit != nullptr);
}