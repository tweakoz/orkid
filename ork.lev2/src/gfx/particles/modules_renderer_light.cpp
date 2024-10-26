////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/lev2/gfx/particle/modular_renderers.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/util/triple_buffer.h>

using namespace ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

struct LightRendererInst : public ParticleModuleInst {

  using triple_buf_t        = concurrent_triple_buffer<ParticlePoolRenderBuffer>;
  using triple_buf_ptr_t    = std::shared_ptr<triple_buf_t>;
  using sprite_vtxbuf_t     = DynamicVertexBuffer<sprite_vtx_t>;
  using sprite_vtxbuf_ptr_t = std::shared_ptr<sprite_vtxbuf_t>;

  LightRendererInst(const LightRendererData* srd, dataflow::GraphInst* ginst);
  void onLink(GraphInst* inst) final;
  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final;
  void _render(const ork::lev2::RenderContextInstData& RCID);
  const LightRendererData* _srd;
};

///////////////////////////////////////////////////////////////////////////////

LightRendererInst::LightRendererInst(const LightRendererData* srd, dataflow::GraphInst* ginst)
    : ParticleModuleInst(srd, ginst)
    , _srd(srd) {
  OrkAssert(srd);
  //_triple_buf                         = std::make_shared<triple_buf_t>();
  static constexpr size_t KMAXSpriteS = 128 << 10;
  //_vertexBuffer                       = std::make_shared<sprite_vtxbuf_t>(KMAXSpriteS, 0);
  //_vertexBuffer->SetRingLock(true);
}

///////////////////////////////////////////////////////////////////////////////

void LightRendererInst::onLink(GraphInst* inst) {
  _onLink(inst);
  auto ptcl_context         = inst->_impl.getShared<Context>();
  ptcl_context->_rcidlambda = [this](const RenderContextInstData& RCID) { //
    this->_render(RCID); //
  };
}

///////////////////////////////////////////////////////////////////////////////

void LightRendererInst::compute(
    GraphInst* inst, //
    ui::updatedata_ptr_t updata) {
}

///////////////////////////////////////////////////////////////////////////////

void LightRendererInst::_render(const ork::lev2::RenderContextInstData& RCID) {
}

///////////////////////////////////////////////////////////////////////////////

static void _reshapeLightRendererIOs(dataflow::moduledata_ptr_t mdata) {
  auto typed = std::dynamic_pointer_cast<LightRendererData>(mdata);

}

///////////////////////////////////////////////////////////////////////////////

void LightRendererData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t { return LightRendererData::createShared(); });
  clazz
      ->directObjectProperty("material", &LightRendererData::_material) //
      ->annotate<ConstString>("editor.factorylistbase", "psys::MaterialBase");

  clazz->directProperty("sort", &LightRendererData::_sort);

  clazz->annotateTyped<moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) { _reshapeLightRendererIOs(mdata); });
}

///////////////////////////////////////////////////////////////////////////////

LightRendererData::LightRendererData() {
  _material = std::make_shared<FlatMaterial>();
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<LightRendererData> LightRendererData::createShared() {
  auto data = std::make_shared<LightRendererData>();

  _initPoolIOs(data);
  _reshapeLightRendererIOs(data);
  return data;
}

///////////////////////////////////////////////////////////////////////////////

// void LightRendererData::reshapeIOs() {
// }

///////////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t LightRendererData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<LightRendererInst>(this, ginst);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::LightRendererData, "psys::LightRendererData");
