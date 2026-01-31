#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

DrawingInterface::DrawingInterface(Context& ctx)
    : _context(ctx) {
}

///////////////////////////////////////////////////////////////////////////////

void DrawingInterface::line2DEML(const fvec2& v0, const fvec2& v1, const fvec4& vertex_color, float depth ){

  auto GBI = _context.GBI();

  auto vb = GfxEnv::GetSharedDynamicVB();
  U32 uc                                = vertex_color.ABGRU32();
  ork::lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(GBI, vb.get(), 6);
  vw.AddVertex(SVtxV12C4T16(v0.x, v0.y, depth, 0,0,0,0, uc));
  vw.AddVertex(SVtxV12C4T16(v1.x, v1.y, depth, 0,0,0,0, uc));
  vw.UnLock(GBI);

  GBI->DrawPrimitiveEML(vw, PrimitiveType::LINES, 2);

}

///////////////////////////////////////////////////////////////////////////////

void DrawingInterface::quad2DEML(const fvec2& v0, const fvec2& v1, const fvec2& v2, const fvec2& v3, //
               const fvec2& uva0, const fvec2& uva1, const fvec2& uva2, const fvec2& uva3, //
               const fvec2& uvb0, const fvec2& uvb1, const fvec2& uvb2, const fvec2& uvb3, //
               const fvec4& vertex_color, //
               float depth ){

  auto GBI = _context.GBI();

  auto vb = GfxEnv::GetSharedDynamicVB();
  U32 uc                                = vertex_color.ABGRU32();
  ork::lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(GBI, vb.get(), 6);
  vw.AddVertex(SVtxV12C4T16(v0.x, v0.y, depth, uva0.x, uva0.y, uva0.x, uva0.y, uc));
  vw.AddVertex(SVtxV12C4T16(v2.x, v2.y, depth, uva2.x, uva2.y, uvb2.x, uvb2.y, uc));
  vw.AddVertex(SVtxV12C4T16(v1.x, v1.y, depth, uva1.x, uva1.y, uvb1.x, uvb1.y, uc));

  vw.AddVertex(SVtxV12C4T16(v0.x, v0.y, depth, uva0.x, uva0.y, uva0.x, uva0.y, uc));
  vw.AddVertex(SVtxV12C4T16(v3.x, v3.y, depth, uva3.x, uva3.y, uvb3.x, uvb3.y, uc));
  vw.AddVertex(SVtxV12C4T16(v2.x, v2.y, depth, uva2.x, uva2.y, uvb2.x, uvb2.y, uc));
  vw.UnLock(GBI);

  GBI->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6);

}

void DrawingInterface::quad3DEML(
                 const fvec3& V0, //
                 const fvec3& V1, //
                 const fvec3& V2, //
                 const fvec3& V3, //
                 const fvec2& Uv0, //
                 const fvec2& Uv1, //
                 const fvec2& Uv2, //
                 const fvec2& Uv3,
                 uint32_t color ) { //
  auto GBI = _context.GBI();
  auto vb = GfxEnv::GetSharedDynamicVB();
  ork::lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(GBI, vb.get(), 6);
  vw.AddVertex(SVtxV12C4T16(V0.x, V0.y, V0.z, Uv0.x, Uv0.y, Uv0.x, Uv0.y, color));
  vw.AddVertex(SVtxV12C4T16(V1.x, V1.y, V1.z, Uv1.x, Uv1.y, Uv1.x, Uv1.y, color));
  vw.AddVertex(SVtxV12C4T16(V2.x, V2.y, V2.z, Uv2.x, Uv2.y, Uv2.x, Uv2.y, color));

  vw.AddVertex(SVtxV12C4T16(V0.x, V0.y, V0.z, Uv0.x, Uv0.y, Uv0.x, Uv0.y, color));
  vw.AddVertex(SVtxV12C4T16(V2.x, V2.y, V2.z, Uv2.x, Uv2.y, Uv2.x, Uv2.y, color));
  vw.AddVertex(SVtxV12C4T16(V3.x, V3.y, V3.z, Uv3.x, Uv3.y, Uv3.x, Uv3.y, color));
  vw.UnLock(GBI);
  GBI->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6);
 }
  
 ///////////////////////////////////////////////////////////////////////////////

void DrawingInterface::quad2DEML(const fvec4& QuadRect, //
                                 const fvec4& UvRect, //
                                 const fvec4& UvRect2, //
                                 float depth ) {
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

  auto vb = GfxEnv::GetSharedDynamicVB();
  U32 uc                                = 0xffffffff;
  ork::lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(GBI, vb.get(), 6);
  vw.AddVertex(SVtxV12C4T16(fx0, fy0, depth, fua0, fva0, fub0, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy1, depth, fua1, fva1, fub1, fvb1, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy0, depth, fua1, fva0, fub1, fvb0, uc));

  vw.AddVertex(SVtxV12C4T16(fx0, fy0, depth, fua0, fva0, fub0, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx0, fy1, depth, fua0, fva1, fub0, fvb1, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy1, depth, fua1, fva1, fub1, fvb1, uc));
  vw.UnLock(GBI);

  GBI->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6);
}
void DrawingInterface::quad2DEML2(const fvec4& QuadRect, //
                                 const fvec4& UvRect, //
                                 const fvec4& UvRect2, //
                                 float depth ) {
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

  auto vb = GfxEnv::GetSharedDynamicVB();
  U32 uc                                = 0xffffffff;
  ork::lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(GBI, vb.get(), 6);
  vw.AddVertex(SVtxV12C4T16(fx0, fy0, depth, fua0, fva0, fub0, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy0, depth, fua1, fva0, fub1, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy1, depth, fua1, fva1, fub1, fvb1, uc));

  vw.AddVertex(SVtxV12C4T16(fx0, fy0, depth, fua0, fva0, fub0, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy1, depth, fua1, fva1, fub1, fvb1, uc));
  vw.AddVertex(SVtxV12C4T16(fx0, fy1, depth, fua0, fva1, fub0, fvb1, uc));
  vw.UnLock(GBI);

  GBI->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6);
}
///////////////////////////////////////////////////////////////////////////////

void DrawingInterface::quad2DEMLCCL(const fvec4& QuadRect, //
                                    const fvec4& UvRect, //
                                    const fvec4& UvRect2, //
                                    float depth ) {
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

  auto vb = GfxEnv::GetSharedDynamicVB();
  U32 uc                                = 0xffffffff;
  ork::lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(GBI, vb.get(), 6);
  vw.AddVertex(SVtxV12C4T16(fx0, fy0, depth, fua0, fva0, fub0, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy0, depth, fua1, fva0, fub1, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy1, depth, fua1, fva1, fub1, fvb1, uc));

  vw.AddVertex(SVtxV12C4T16(fx0, fy0, depth, fua0, fva0, fub0, fvb0, uc));
  vw.AddVertex(SVtxV12C4T16(fx1, fy1, depth, fua1, fva1, fub1, fvb1, uc));
  vw.AddVertex(SVtxV12C4T16(fx0, fy1, depth, fua0, fva1, fub0, fvb1, uc));
  vw.UnLock(GBI);

  GBI->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6);
}

///////////////////////////////////////////////////////////////////////////////

void DrawingInterface::quad2DEMLTiled(const fvec4& QuadRect, 
                                      const fvec4& UvRect, 
                                      const fvec4& UvRect2, 
                                      int numtileseachdim) {

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

  auto vb = GfxEnv::GetSharedDynamicVB();
  U32 uc                                = 0xffffffff;
  ork::lev2::VtxWriter<SVtxV12C4T16> vw;
  vw.Lock(GBI, vb.get(), num2lock);

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

///////////////////////////////////////////////////////////////////////////////

void DrawingInterface::fullscreenQuad(const fvec4& UvRect, const fvec4& UvRect2, float depth) {
  // Standard fullscreen quad in NDC space (-1 to 1)
  const fvec4 QuadRect(-1, -1, 2, 2);
  quad2D(QuadRect, UvRect, UvRect2, depth);
}

///////////////////////////////////////////////////////////////////////////////

void DrawingInterface::quad2D(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2, float depth) {
  // Check rendering conventions to determine which winding to use
  const auto& conventions = _context.renderingConventions();

  fvec4 logical_uvrect = UvRect;
  fvec4 logical_uvrect2 = UvRect2;

  if(conventions._isLogicalYUp!=conventions._isNativeYUp){
    // Flip the V coordinate for logical vs native Y-axis orientation
    logical_uvrect.y = UvRect.y + UvRect.w;
    logical_uvrect.w = -UvRect.w;

    logical_uvrect2.y = UvRect2.y + UvRect2.w;
    logical_uvrect2.w = -UvRect2.w;

  }

  if (conventions.useClockwiseWinding()) {
    quad2DEMLCCL(QuadRect, logical_uvrect, logical_uvrect2, depth);
  } else {
    quad2DEML(QuadRect, logical_uvrect, logical_uvrect2, depth);
  }
}

///////////////////////////////////////////////////////////////////////////////

void DrawingInterface::quadTiled2D(const fvec4& QuadRect, 
                                   const fvec4& UvRect, 
                                   const fvec4& UvRect2, 
                                   int numtileseachdim) {
  // Check rendering conventions to determine which winding to use
  const auto& conventions = _context.renderingConventions();
  if (conventions.useClockwiseWinding()) {
    // Use the clockwise version for tiled rendering
    // For now, create a CW version of tiled - TODO: refactor quad2DEMLTiled to quadTiled2DEMLCCL
    // and implement both versions
    
    // Temporarily use the per-tile approach until we implement batched CW version
    float mult = 1.0f / float(numtileseachdim);
    float tile_width  = QuadRect.z * mult;
    float tile_height = QuadRect.w * mult;
    float base_x = QuadRect.x;
    float base_y = QuadRect.y;

    float uv_tile_width  = UvRect.z * mult;
    float uv_tile_height = UvRect.w * mult;
    float base_u = UvRect.x;
    float base_v = UvRect.y;

    for (int iu = 0; iu < numtileseachdim; iu++) {
      for (int iv = 0; iv < numtileseachdim; iv++) {
        fvec4 tile_rect(
          base_x + tile_width * float(iu), 
          base_y + tile_height * float(iv), 
          tile_width, 
          tile_height);
        
        fvec4 tile_uvrect(
          base_u + uv_tile_width * float(iu), 
          base_v + uv_tile_height * float(iv), 
          uv_tile_width, 
          uv_tile_height);
        
        quad2DEMLCCL(tile_rect, tile_uvrect, UvRect2, 0.0f);
      }
    }
  } else {
    // Use the existing CCW batched version for efficiency
    quad2DEMLTiled(QuadRect, UvRect, UvRect2, numtileseachdim);
  }
}

} // namespace ork::lev2
