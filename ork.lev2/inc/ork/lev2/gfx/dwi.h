#pragma once

struct DrawingInterface {
  DrawingInterface(Context& ctx);
  void quad2DEML(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2);
  void quad2DEMLTiled(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2, int numtileseachdim);
  virtual ~DrawingInterface(){}
  Context& _context;
};
