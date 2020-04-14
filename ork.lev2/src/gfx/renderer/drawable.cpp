////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>

#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/math/collision_test.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/kernel/string/deco.inl>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::DrawableOwner, "DrawableOwner");
namespace ork::lev2 {

DrawQueueXfData::DrawQueueXfData() {
  _worldMatrix = std::make_shared<fmtx4>();
}

///////////////////////////////////////////////////////////////////////////////

void Drawable::enqueueOnLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  DrawableBufItem& item = buffer.Queue(xfdata, this);
}

///////////////////////////////////////////////////////////////////////////////
Drawable::Drawable()
    : mDataA(nullptr)
    , mDataB(nullptr)
    , mEnabled(true) {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  fflush(stdout);
}
Drawable::~Drawable() {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  // printf( "Delete Drawable<%p>\n", this );
}

///////////////////////////////////////////////////////////////////////////////

void DrawableOwner::Describe() {
}
DrawableOwner::DrawableOwner() {
}
DrawableOwner::~DrawableOwner() {
}
///////////////////////////////////////////////////////////////////////////////
void DrawableOwner::_addDrawable(const PoolString& layername, drawable_ptr_t pdrw) {
  DrawableVector* pldrawables = GetDrawables(layername);
  if (nullptr == pldrawables) {
    pldrawables = new DrawableVector;
    mLayerMap.AddSorted(layername, pldrawables);
  }
  pldrawables->push_back(pdrw);
}
///////////////////////////////////////////////////////////////////////////////
DrawableOwner::DrawableVector* DrawableOwner::GetDrawables(const PoolString& layer) {
  DrawableVector* pldrawables = 0;

  LayerMap::const_iterator itL = mLayerMap.find(layer);
  if (itL != mLayerMap.end()) {
    pldrawables = itL->second;
  }
  return pldrawables;
}
///////////////////////////////////////////////////////////////////////////////
const DrawableOwner::DrawableVector* DrawableOwner::GetDrawables(const PoolString& layer) const {
  const DrawableVector* pldrawables = 0;

  LayerMap::const_iterator itL = mLayerMap.find(layer);
  if (itL != mLayerMap.end()) {
    pldrawables = itL->second;
  }
  return pldrawables;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
