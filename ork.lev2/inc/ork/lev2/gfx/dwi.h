#pragma once

struct DrawingInterface {
  DrawingInterface(Context& ctx);
  void quad2DEML(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2);

  void quad2DEML(const fvec2& v0,   const fvec2& v1,   const fvec2& v2,   const fvec2& v3,   //
                 const fvec2& uva0, const fvec2& uva1, const fvec2& uva2, const fvec2& uva3, //
                 const fvec2& uvb0, const fvec2& uvb1, const fvec2& uvb2, const fvec2& uvb3, //
                 const fvec4& vertex_color);

  void quad2DEMLTiled(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2, int numtileseachdim);
  virtual ~DrawingInterface(){}
  Context& _context;
};
