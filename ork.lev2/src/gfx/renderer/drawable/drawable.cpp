////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/register.h>

#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/math/collision_test.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/kernel/string/deco.inl>
#include <ork/reflect/properties/registerX.inl>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::DrawableContainer, "DrawableContainer");

ImplementReflectionX(ork::lev2::DrawableData, "DrawableData");

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

void DrawableData::describeX(object::ObjectClass* clazz){
  clazz->directProperty("ModColor", &DrawableData::_modcolor);
}

DrawableData::DrawableData(){
  _modcolor = fvec4(1,1,1,1);
  _vars = std::make_shared<varmap::VarMap>();
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t DrawableCache::fetch(drawabledata_ptr_t data){

  auto it = _cache.find(data);
  if(it!=_cache.end()){
    return it->second;
  }
  auto drw = data->createDrawable();
  drw->_modcolor = data->_modcolor;
  _cache[data]=drw;
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

DrawQueueTransferData::DrawQueueTransferData() {
  _worldTransform = std::make_shared<DecompTransform>();
}

///////////////////////////////////////////////////////////////////////////////

void Drawable::enqueueToRenderQueue(
      drawqueueitem_constptr_t item,
      lev2::IRenderer* prenderer) const{

}

drawqueueitem_ptr_t Drawable::enqueueOnLayer(const DrawQueueTransferData& xfdata, DrawQueueLayer& buffer) const {
  auto item = buffer.enqueueDrawable(xfdata, this);
  item->_onrenderable = this->_onrenderable;
  return item;
}

///////////////////////////////////////////////////////////////////////////////
Drawable::Drawable()
    : _implA(nullptr)
    , _implB(nullptr)
    , mEnabled(true) {
    _modcolor = fvec4(1,1,1,1);
}
Drawable::~Drawable() {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  // printf( "Delete Drawable<%p>\n", this );
}

///////////////////////////////////////////////////////////////////////////////

void DrawableContainer::Describe() {
}
DrawableContainer::DrawableContainer() {
}
DrawableContainer::~DrawableContainer() {
}
///////////////////////////////////////////////////////////////////////////////
void DrawableContainer::_addDrawable(const std::string& layername, drawable_ptr_t pdrw) {
  drawable_vect_ptr_t pldrawables = _getDrawables(layername);
  if (nullptr == pldrawables) {
    pldrawables = std::make_shared<drawable_vect_t>();
    _layerMap[layername] = pldrawables;
  }
  pldrawables->push_back(pdrw);
}
///////////////////////////////////////////////////////////////////////////////
DrawableContainer::drawable_vect_ptr_t DrawableContainer::_getDrawables(const std::string& layer) {
  drawable_vect_ptr_t pldrawables = 0;

  auto itL = _layerMap.find(layer);
  if (itL != _layerMap.end()) {
    pldrawables = itL->second;
  }
  return pldrawables;
}
///////////////////////////////////////////////////////////////////////////////
const DrawableContainer::drawable_vect_ptr_t DrawableContainer::_getDrawables(const std::string& layer) const {
  drawable_vect_ptr_t pldrawables = 0;

  auto itL = _layerMap.find(layer);
  if (itL != _layerMap.end()) {
    pldrawables = itL->second;
  }
  return pldrawables;
}

} // namespace ork::lev2
