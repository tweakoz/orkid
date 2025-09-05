////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/gfx/gfxmaterial.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

GeometryBufferInterface::GeometryBufferInterface(Context& ctx)
    : _context(ctx) {
}

///////////////////////////////////////////////////////////////////////////////

GeometryBufferInterface::~GeometryBufferInterface() {
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::render2dQuadEML(fvec4 QuadRect, fvec4 UvRect, fvec4 UvRect2, float depth) {
  const auto& conventions = _context.renderingConventions();
  if( !conventions._isLogicalYUp && conventions._isNativeYUp ) {
    float qtop = QuadRect.w;
    QuadRect.w  = QuadRect.y;
    QuadRect.y  = qtop;
    float uvtop  = UvRect.w;
    UvRect.w     = UvRect.y;
    UvRect.y     = uvtop;
    float uv2top = UvRect2.w;
    UvRect2.w    = UvRect2.y;
    UvRect2.y    = uv2top;
  }

  _context.DWI()->quad2DEML(QuadRect, UvRect, UvRect2, depth);
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::render2dQuadEMLCCL(fvec4 QuadRect, fvec4 UvRect, fvec4 UvRect2, float depth) {
  const auto& conventions = _context.renderingConventions();
  if( !conventions._isLogicalYUp && conventions._isNativeYUp ) {
    float qtop = QuadRect.w;
    QuadRect.w  = QuadRect.y;
    QuadRect.y  = qtop;
    float uvtop  = UvRect.w;
    UvRect.w     = UvRect.y;
    UvRect.y     = uvtop;
    float uv2top = UvRect2.w;
    UvRect2.w    = UvRect2.y;
    UvRect2.y    = uv2top;
  }
  _context.DWI()->quad2DEMLCCL(QuadRect, UvRect, UvRect2, depth);
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::BeginFrame() {
  miTrianglesRendered = 0;
  _doBeginFrame();
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::EndFrame() {
  _doEndFrame();
  miTrianglesRendered = 0;
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::FlushVB(VertexBufferBase& VBuf) {
  if (VBuf.IsLocked()) {
    UnLockVB(VBuf);
  }
  // if (VBuf.GetNumVertices()) {
  // DrawPrimitive(VBuf);
  //}
  LockVB(VBuf);
  VBuf.Reset();
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::DrawPrimitive(
    GfxMaterial* mtl, //
    const VtxWriterBase& VW,
    PrimitiveType eType,
    int icount) {
  if (0 == icount) {
    icount = VW.miWriteCounter;
  }
  // printf( "GBI::DrawPrim(VW) ibase<%d> icount<%d>\n", VW.miWriteBase, icount );
  DrawPrimitive(mtl, *VW.mpVB, eType, VW.miWriteBase, icount);
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::DrawPrimitive(
    GfxMaterial* mtl,
    const VertexBufferBase& VBuf,
    PrimitiveType eTyp,
    int ivbase,
    int ivcount) {
  int imax = VBuf.GetMax();
  if (imax) {
    mtl->BeginBlock(&_context);
    DrawPrimitiveEML(VBuf, eTyp, ivbase, ivcount);
    mtl->EndBlock(&_context);
  }
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::DrawIndexedPrimitive(
    GfxMaterial* mtl,
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType) {
  int imax = VBuf.GetMax();

  if (imax) {
    mtl->BeginBlock(&_context);
    _context.FXI()->applyRasterState(*(mtl->_rasterstate));
    DrawIndexedPrimitiveEML(VBuf, IdxBuf, eType);
    mtl->EndBlock(&_context);
  }
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::DrawPrimitiveEML(const VtxWriterBase& VW, PrimitiveType eType, int icount) {
  if (0 == icount) {
    icount = VW.miWriteCounter;
  }
  // printf( "GBI::DrawPrim(VW) ibase<%d> icount<%d>\n", VW.miWriteBase, icount );
  DrawPrimitiveEML(*VW.mpVB, eType, VW.miWriteBase, icount);
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
