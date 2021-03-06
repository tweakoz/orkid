////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

void GeometryBufferInterface::render2dQuadEML(const fvec4& QuadRect, const fvec4& UvRect, const fvec4& UvRect2) {
  _context.DWI()->quad2DEML(QuadRect, UvRect, UvRect2);
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
    int inumpasses = mtl->BeginBlock(&_context);
    for (int ipass = 0; ipass < inumpasses; ipass++) {
      bool bDRAW = mtl->BeginPass(&_context, ipass);
      if (bDRAW) {
        if (PrimitiveType::NONE == eTyp) {
          eTyp = VBuf.GetPrimType();
        }

        DrawPrimitiveEML(VBuf, eTyp, ivbase, ivcount);

        mtl->EndPass(&_context);
      }
    }

    mtl->EndBlock(&_context);
  }
}

///////////////////////////////////////////////////////////////////////////////

void GeometryBufferInterface::DrawIndexedPrimitive(
    GfxMaterial* mtl,
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    int ivbase,
    int ivcount) {
  int imax = VBuf.GetMax();

  if (imax) {
    int inumpasses = mtl->BeginBlock(&_context);

    for (int ipass = 0; ipass < inumpasses; ipass++) {
      if (mtl->BeginPass(&_context, ipass)) {
        if (PrimitiveType::NONE == eType)
          eType = VBuf.GetPrimType();
        DrawIndexedPrimitiveEML(VBuf, IdxBuf, eType, ivbase, ivcount);

        mtl->EndPass(&_context);
      }
    }

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
