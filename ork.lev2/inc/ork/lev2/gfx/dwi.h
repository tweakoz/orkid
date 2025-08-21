////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::lev2 {

struct DrawingInterface {
  DrawingInterface(Context& ctx);

void line2DEML(const fvec2& v0, const fvec2& v1, const fvec4& vertex_color, float depth );

  void quad2DEML(const fvec4& QuadRect, //
                 const fvec4& UvRect, //
                 const fvec4& UvRect2, //
                 float depth = 0.0f);
  void quad2DEML2(const fvec4& QuadRect, //
                 const fvec4& UvRect, //
                 const fvec4& UvRect2, //
                 float depth = 0.0f);
  void quad3DEML(const fvec3& V0, //
                 const fvec3& V1, //
                 const fvec3& V2, //
                 const fvec3& V3, //
                 const fvec2& Uv0, //
                 const fvec2& Uv1, //
                 const fvec2& Uv2, //
                 const fvec2& Uv3, //
                 uint32_t color );
  void quad2DEMLCCL(const fvec4& QuadRect, //
                 const fvec4& UvRect, //
                 const fvec4& UvRect2, //
                 float depth = 0.0f);

  void quad2DEML(const fvec2& v0,   const fvec2& v1,   const fvec2& v2,   const fvec2& v3,   //
                 const fvec2& uva0, const fvec2& uva1, const fvec2& uva2, const fvec2& uva3, //
                 const fvec2& uvb0, const fvec2& uvb1, const fvec2& uvb2, const fvec2& uvb3, //
                 const fvec4& vertex_color, //
                 float depth = 0 );

  void quad2DEMLTiled(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2, int numtileseachdim);
  
  // Tiled quad that automatically uses correct winding based on rendering conventions
  void quadTiled2D(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2, int numtileseachdim);
  
  // Fullscreen quad that automatically uses correct winding based on rendering conventions
  void fullscreenQuad(const fvec4& UvRect = fvec4(0, 0, 1, 1), //
                      const fvec4& UvRect2 = fvec4(0, 0, 1, 1), //
                      float depth = 0.0f);
  
  // General quad that automatically uses correct winding based on rendering conventions
  void quad2D(const fvec4& QuadRect, //
              const fvec4& UvRect = fvec4(0, 0, 1, 1), //
              const fvec4& UvRect2 = fvec4(0, 0, 1, 1), //
              float depth = 0.0f);
  
  virtual ~DrawingInterface(){}
  Context& _context;
};

} // namespace ork::lev2