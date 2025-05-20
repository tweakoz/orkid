#include <ork/lev2/gfx/scenegraph/sgnode_imposter.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/image.h>
#include <ork/kernel/opq.h>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::ImposterDrawableData, "ImposterDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

ImposterDrawableImpl::ImposterDrawableImpl(const ImposterDrawableData* grid)
    : _griddata(grid) {
}
ImposterDrawableImpl::~ImposterDrawableImpl() {
}
void ImposterDrawableImpl::gpuInit(lev2::Context* ctx) {
  _initted = true;
}
void ImposterDrawableImpl::_render(const RenderContextInstData& RCID) {

  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  auto context    = RCID.context();

  if (not _initted) {
    gpuInit(context);
  }
}

///////////////////////////////////////////////////////////////////////////////

void ImposterDrawableImpl::render(RenderContextInstData& RCID) { // static
  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  auto drawable   = renderable->_drawable;
  drawable->_implA.getShared<ImposterDrawableImpl>()->_render(RCID);
}

///////////////////////////////////////////////////////////////////////////////

void ImposterDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t ImposterDrawableData::createDrawable() const {
  auto drw = std::make_shared<CallbackDrawable>(nullptr);
  auto impl = drw->_implA.makeShared<ImposterDrawableImpl>(this);
  drw->_sortkey = 10;
  drw->SetRenderCallback(ImposterDrawableImpl::render);
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

ImposterDrawableData::ImposterDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

ImposterDrawableData::~ImposterDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
