#pragma once

#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gbi.h>
#include <ork/math/box.h>

namespace ork::lev2::vtxbuf_from_bbox {

using vtx_t = SVtxV12C4;
using vb_t  = StaticVertexBuffer<vtx_t>;

inline std::shared_ptr<vb_t> create(Context* ctx, const AABox& box) {

  auto gbi = ctx->GBI();

  auto vtx_buf = std::make_shared<vb_t>(24, 0, PrimitiveType::NONE);
  auto vdest = (vtx_t*) gbi->LockVB(*vtx_buf, 0, 24);

  auto fill_vert = [](vtx_t& vout, float x, float y, float z) {
    vout.x = x;
    vout.y = y;
    vout.z = z;
  };

  auto bb_min = box.Min();
  auto bb_max = box.Max();

  fill_vert(vdest[0], bb_min.x, bb_min.y, bb_min.z);
  fill_vert(vdest[1], bb_max.x, bb_min.y, bb_min.z);
  fill_vert(vdest[2], bb_max.x, bb_min.y, bb_min.z);
  fill_vert(vdest[3], bb_max.x, bb_max.y, bb_min.z);
  fill_vert(vdest[4], bb_max.x, bb_max.y, bb_min.z);
  fill_vert(vdest[5], bb_min.x, bb_max.y, bb_min.z);
  fill_vert(vdest[6], bb_min.x, bb_max.y, bb_min.z);
  fill_vert(vdest[7], bb_min.x, bb_min.y, bb_min.z);

  fill_vert(vdest[8], bb_min.x, bb_min.y, bb_max.z);
  fill_vert(vdest[9], bb_max.x, bb_min.y, bb_max.z);
  fill_vert(vdest[10], bb_max.x, bb_min.y, bb_max.z);
  fill_vert(vdest[11], bb_max.x, bb_max.y, bb_max.z);
  fill_vert(vdest[12], bb_max.x, bb_max.y, bb_max.z);
  fill_vert(vdest[13], bb_min.x, bb_max.y, bb_max.z);
  fill_vert(vdest[14], bb_min.x, bb_max.y, bb_max.z);
  fill_vert(vdest[15], bb_min.x, bb_min.y, bb_max.z);

  fill_vert(vdest[16], bb_min.x, bb_min.y, bb_min.z);
  fill_vert(vdest[17], bb_min.x, bb_min.y, bb_max.z);
  fill_vert(vdest[18], bb_max.x, bb_min.y, bb_min.z);
  fill_vert(vdest[19], bb_max.x, bb_min.y, bb_max.z);
  fill_vert(vdest[20], bb_max.x, bb_max.y, bb_min.z);
  fill_vert(vdest[21], bb_max.x, bb_max.y, bb_max.z);
  fill_vert(vdest[22], bb_min.x, bb_max.y, bb_min.z);
  fill_vert(vdest[23], bb_min.x, bb_max.y, bb_max.z);

  gbi->UnLockVB(*vtx_buf);

  return vtx_buf;
}

} // namespace ork::lev2::vtxbuf_from_bbox