#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>

namespace ork::lev2 {

DrawingInterface::DrawingInterface(Context& ctx)
    : _context(ctx) {
}

void DrawingInterface::quad2DEML(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2) {
  auto GBI = _context.GBI();
  // align source pixels to target pixels if sizes match
  float fx0 = QuadRect.x;
  float fy0 = QuadRect.y;
  float fx1 = QuadRect.x + QuadRect.z;
  float fy1 = QuadRect.y + QuadRect.w;

  float fua0 = UvRect.x;
  float fva0 = UvRect.y;
  float fua1 = UvRect.x + UvRect.z;
  float fva1 = UvRect.y + UvRect.w;

  float fub0 = UvRect2.x;
  float fvb0 = UvRect2.y;
  float fub1 = UvRect2.x + UvRect2.z;
  float fvb1 = UvRect2.y + UvRect2.w;

  DynamicVertexBuffer<SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();
  U32 uc                                = 0xffffffff;
  ork::lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(GBI, &vb, 6);
  vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fua0, fva0, fub0, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fua1, fva1, fub1, fvb1, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy0, 0.0f, fua1, fva0, fub1, fvb0, uc));

  vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fua0, fva0, fub0, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx0, fy1, 0.0f, fua0, fva1, fub0, fvb1, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fua1, fva1, fub1, fvb1, uc));
  vw.UnLock(GBI);

  GBI->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6);
}
void DrawingInterface::quad2DEMLTiled(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2, int numtileseachdim) {

  auto GBI = _context.GBI();

  float mult = 1.0f / float(numtileseachdim);

  float wd2 = QuadRect.z * mult;
  float hd2 = QuadRect.w * mult;
  float x   = QuadRect.x;
  float y   = QuadRect.y;

  float uwd2 = UvRect.z * mult;
  float vhd2 = UvRect.w * mult;
  float u    = UvRect.x;
  float v    = UvRect.y;

  int num2lock = 6 * numtileseachdim * numtileseachdim;

  DynamicVertexBuffer<SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();
  U32 uc                                = 0xffffffff;
  ork::lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(GBI, &vb, num2lock);

  for (int iu = 0; iu < numtileseachdim; iu++) {
    for (int iv = 0; iv < numtileseachdim; iv++) {
      fvec4 qrect(x + wd2 * float(iu), y + hd2 * float(iv), wd2, hd2);
      fvec4 uvrect(u + uwd2 * float(iu), v + vhd2 * float(iv), uwd2, vhd2);

      float fx0 = qrect.x;
      float fy0 = qrect.y;
      float fx1 = qrect.x + qrect.z;
      float fy1 = qrect.y + qrect.w;

      float fua0 = uvrect.x;
      float fva0 = uvrect.y;
      float fua1 = uvrect.x + uvrect.z;
      float fva1 = uvrect.y + uvrect.w;

      float fub0 = UvRect2.x;
      float fvb0 = UvRect2.y;
      float fub1 = UvRect2.x + UvRect2.z;
      float fvb1 = UvRect2.y + UvRect2.w;

      vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fua0, fva0, fub0, fvb0, uc));
      vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fua1, fva1, fub1, fvb1, uc));
      vw.AddVertex(SVtxV12C4T16(fx1, fy0, 0.0f, fua1, fva0, fub1, fvb0, uc));

      vw.AddVertex(SVtxV12C4T16(fx0, fy0, 0.0f, fua0, fva0, fub0, fvb0, uc));
      vw.AddVertex(SVtxV12C4T16(fx0, fy1, 0.0f, fua0, fva1, fub0, fvb1, uc));
      vw.AddVertex(SVtxV12C4T16(fx1, fy1, 0.0f, fua1, fva1, fub1, fvb1, uc));
    }
  }
  vw.UnLock(GBI);
  GBI->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, num2lock);
}

} // namespace ork::lev2
