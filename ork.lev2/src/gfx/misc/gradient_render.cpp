////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <math.h>
#include <ork/pch.h>
#include <ork/math/misc_math.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/math/gradient.h>

namespace ork::lev2 {
////////////////////////////////////////////////////////////////

void gradientGeometry(
    Context* pTARG, //
    const ork::gradient_fvec4_t& grad, //
    VtxWriter<SVtxV16T16C16>& vw,
    int x1, //
    int y1, //
    int w, //
    int h) { //

  auto& data = grad._data;
  const int knumpoints = (int)data.size();
  const int ksegs      = knumpoints - 1;
  if(ksegs){
    DynamicVertexBuffer<SVtxV16T16C16>& VB = GfxEnv::GetSharedDynamicV16T16C16();

    vw.Lock(pTARG, &VB, 6 * ksegs);

    fvec4 uv;

    const float kz = 0.0f;

    float fx = float(x1);
    float fy = float(y1);
    float fw = float(w);
    float fh = float(h);

    for (int i = 0; i < ksegs; i++) {
      std::pair<float, ork::fvec4> data_a = data.GetItemAtIndex(i);
      std::pair<float, ork::fvec4> data_b = data.GetItemAtIndex(i + 1);

      float fia = data_a.first;
      float fib = data_b.first;

      float fx0 = fx + (fia * fw);
      float fx1 = fx + (fib * fw);
      float fy0 = fy;
      float fy1 = fy + fh;

      const fvec4& c0 = data_a.second;
      const fvec4& c1 = data_b.second;

      SVtxV16T16C16 v0(fvec4(fx0, fy0, kz), uv, c0);
      SVtxV16T16C16 v1(fvec4(fx1, fy0, kz), uv, c1);
      SVtxV16T16C16 v2(fvec4(fx1, fy1, kz), uv, c1);
      SVtxV16T16C16 v3(fvec4(fx0, fy1, kz), uv, c0);

      vw.AddVertex(v0);
      vw.AddVertex(v2);
      vw.AddVertex(v1);

      vw.AddVertex(v0);
      vw.AddVertex(v3);
      vw.AddVertex(v2);
    }
    vw.UnLock(pTARG);
  }

}

} // namespace ork::lev2
