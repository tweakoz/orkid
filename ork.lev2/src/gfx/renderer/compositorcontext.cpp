////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/enum_serializer.inl>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CompositingContext::CompositingContext()
    : miWidth(0)
    , miHeight(0)
    , _compositingTechnique(nullptr) {
}

///////////////////////////////////////////////////////////////////////////////

CompositingContext::~CompositingContext() {
}

///////////////////////////////////////////////////////////////////////////////

void CompositingContext::Init(lev2::Context* pTARG) {
  //if ((miWidth != pTARG->mainSurfaceWidth()) || (miHeight != pTARG->mainSurfaceHeight())) {
    //miWidth  = pTARG->mainSurfaceWidth();
    //miHeight = pTARG->mainSurfaceHeight();
  //}
  if (nullptr == _utilMaterial) {
    _utilMaterial = new GfxMaterial3DSolid;
    _utilMaterial->gpuInit(pTARG);
  }
}

///////////////////////////////////////////////////////////////////////////////

void CompositingContext::Resize(int iW, int iH) {
  miWidth  = iW;
  miHeight = iH;
}

///////////////////////////////////////////////////////////////////////////////

bool CompositingContext::assemble(CompositorDrawData& drawdata) {
  bool rval = false;
  //printf( "assemble<%d %d>\n", miWidth, miHeight );
  Init(drawdata.context()); // fixme lazy init
  if (_compositingTechnique) {
    _compositingTechnique->gpuInit(drawdata.context(), miWidth, miHeight);
    rval = _compositingTechnique->assemble(drawdata);
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////

void CompositingContext::composite(CompositorDrawData& drawdata) {
  //printf( "composite<%d %d>\n", miWidth, miHeight );
  Init(drawdata.context());
  if (_compositingTechnique)
    _compositingTechnique->composite(drawdata);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
