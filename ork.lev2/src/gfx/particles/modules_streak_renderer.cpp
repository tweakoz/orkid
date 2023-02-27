////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
#include <ork/util/triple_buffer.h>

using namespace ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

struct StreakRendererInst : public ParticleModuleInst {

  using triple_buf_t           = concurrent_triple_buffer<ParticlePoolRenderBuffer>;
  using triple_buf_ptr_t       = std::shared_ptr<triple_buf_t>;
  using streak_vtx_t           = SVtxV12N12B12T16;
  using streak_vtxbuf_t = DynamicVertexBuffer<streak_vtx_t>;
  using streak_vtxbuf_ptr_t = std::shared_ptr<streak_vtxbuf_t>;
  using streak_vertex_writer_t = lev2::VtxWriter<streak_vtx_t>;

  StreakRendererInst(const StreakRendererData* srd);
  void onLink(GraphInst* inst) final;
  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final;
  void _render(const ork::lev2::RenderContextInstData& RCID);
  const StreakRendererData* _srd;
  floatxf_inp_pluginst_ptr_t _input_length;
  floatxf_inp_pluginst_ptr_t _input_width;
  triple_buf_ptr_t _triple_buf;
  streak_vtxbuf_ptr_t _vertexBuffer;
};

///////////////////////////////////////////////////////////////////////////////

StreakRendererInst::StreakRendererInst(const StreakRendererData* srd)
    : ParticleModuleInst(srd)
    , _srd(srd) {
  OrkAssert(srd);
  _triple_buf = std::make_shared<triple_buf_t>();
  static constexpr size_t KMAXSTREAKS = 128<<10; 
  _vertexBuffer = std::make_shared<streak_vtxbuf_t>(KMAXSTREAKS,0,PrimitiveType::POINTS);
  _vertexBuffer->SetRingLock(true);
}

///////////////////////////////////////////////////////////////////////////////

void StreakRendererInst::onLink(GraphInst* inst) {
  _onLink(inst);
  auto ptcl_context         = inst->_impl.getShared<Context>();
  ptcl_context->_rcidlambda = [this](const RenderContextInstData& RCID) { this->_render(RCID); };
  _input_length             = typedInputNamed<FloatXfPlugTraits>("Length");
  _input_width              = typedInputNamed<FloatXfPlugTraits>("Width");
  //_input_length->setValue(.01);
  //_input_width->setValue(.01);
}

///////////////////////////////////////////////////////////////////////////////

void StreakRendererInst::compute(
    GraphInst* inst, //
    ui::updatedata_ptr_t updata) {
  auto ptcl_context = inst->_impl.getShared<Context>();
  auto drawable     = ptcl_context->_drawable;
  OrkAssert(drawable);
  auto output_buffer = _triple_buf->begin_push();
  output_buffer->update(*_pool);
  _triple_buf->end_push(output_buffer);
}

///////////////////////////////////////////////////////////////////////////////

void StreakRendererInst::_render(const ork::lev2::RenderContextInstData& RCID) {

  auto context                = RCID.context();
  auto RCFD                   = context->topRenderContextFrameData();
  const auto& CPD             = RCFD->topCPD();
  const CameraMatrices* cmtcs = CPD.cameraMatrices();
  const CameraData& cdata     = cmtcs->_camdat;
  const fmtx4& MVP            = context->MTXI()->RefMVPMatrix();

  auto material = _srd->_material;

  if (nullptr == material->_pipeline) {
    material->gpuInit(RCID);
    OrkAssert(material->_pipeline);
  }

  fmtx4 mtx;

  ///////////////////////////////////////////////////////////////
  // compute particle dynamic vertex buffer
  //////////////////////////////////////////
  auto render_buffer          = _triple_buf->begin_pull();
  int icnt = render_buffer->_numParticles;
  if (icnt) {
    ork::fmtx4 mtx_iw;
    mtx_iw.inverseOf(mtx);
    fvec3 obj_nrmz = fvec4(cdata.zNormal(), 0.0f).transform(mtx_iw).normalized();
    ////////////////////////////////////////////////////////////////////////////
    streak_vertex_writer_t vw;
    vw.Lock(context, _vertexBuffer.get(), icnt);
    {
      ////////////////////////////////////////////////
      // uniform properties
      ////////////////////////////////////////////////

      auto ptclbase = render_buffer->_particles;

      float fwidth    = _input_width->value();
      float flength   = _input_length->value();
      fvec4 color     = fvec4(1, 1, 1, 1); // mGradient.Sample(mOutDataUnitAge) * fgi;
      uint32_t ucolor = color.VtxColorAsU32();

      for (int i = 0; i < icnt; i++) {
        auto ptcl = ptclbase + i;
        ////////////////////////////////////////////////
        // varying properties
        ////////////////////////////////////////////////
        float fage = ptcl->mfAge;
        float unit_age = std::clamp((fage / ptcl->mfLifeSpan), 0.0f, 1.0f);
        //
        // fvec4 color   = mGradient.Sample(mOutDataUnitAge) * fgi;
        ////////////////////////////////////////////////
        vw.AddVertex(streak_vtx_t(
            ptcl->mPosition,             //
            obj_nrmz,                    //
            ptcl->mVelocity,             //
            ork::fvec2(flength, fwidth), //
            ork::fvec2(unit_age,ptcl->mfRandom)));
        ////////////////////////////////////////////////
      }
    }
    vw.UnLock(context);
    _triple_buf->end_pull(render_buffer);

    material->pipeline(true)->wrappedDrawCall(RCID, [&]() {
      material->update(RCID);
      context->GBI()->DrawPrimitiveEML(vw, ork::lev2::PrimitiveType::POINTS);
    });



  } // icnt
}

///////////////////////////////////////////////////////////////////////////////

void StreakRendererData::describeX(class_t* clazz) {
  /*
  RegisterFloatXfPlug(StreakRendererData, Length, -10.0f, 10.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(StreakRendererData, Width, -10.0f, 10.0f, ged::OutPlugChoiceDelegate);
  RegisterFloatXfPlug(StreakRendererData, GradientIntensity, 0.0f, 10.0f, ged::OutPlugChoiceDelegate);
  ork::reflect::RegisterProperty("Gradient", &StreakRendererData::GradientAccessor);
  ork::reflect::RegisterProperty("BlendMode", &StreakRendererData::meBlendMode);
  ork::reflect::RegisterProperty("Texture", &StreakRendererData::GetTextureAccessor, &StreakRendererData::SetTextureAccessor);

  ork::reflect::RegisterProperty("DepthSort", &StreakRendererData::mbSort);
  ork::reflect::RegisterProperty("AlphaMux", &StreakRendererData::mAlphaMux);
  // ork::reflect::annotatePropertyForEditor<StreakRendererData>("Gradient", "editor.class", "ged.factory.gradient" );

  ork::reflect::annotatePropertyForEditor<StreakRendererData>("BlendMode", "editor.class", "ged.factory.enum");
  ork::reflect::annotatePropertyForEditor<StreakRendererData>("Texture", "editor.class", "ged.factory.assetlist");
  ork::reflect::annotatePropertyForEditor<StreakRendererData>("Texture", "editor.assettype", "lev2tex");
  ork::reflect::annotatePropertyForEditor<StreakRendererData>("Texture", "editor.assetclass", "lev2tex");
  static const char* EdGrpStr =
      "grp://StreakRendererData Input DepthSort AlphaMux Length Width BlendMode Gradient GradientIntensity Texture ";
  reflect::annotateClassForEditor<StreakRendererData>("editor.prop.groups", EdGrpStr);
*/
}

///////////////////////////////////////////////////////////////////////////////

StreakRendererData::StreakRendererData() {
  _material = std::make_shared<FlatMaterial>();
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<StreakRendererData> StreakRendererData::createShared() {
  auto data = std::make_shared<StreakRendererData>();

  _initShared(data);

  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Length");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Width");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "GradientIntensity");

  return data;
}

///////////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t StreakRendererData::createInstance() const {
  return std::make_shared<StreakRendererInst>(this);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::StreakRendererData, "psys::StreakRendererData");
