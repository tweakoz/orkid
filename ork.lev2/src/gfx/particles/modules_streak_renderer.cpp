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
#include <ork/util/triple_buffer.h>

using namespace ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

struct StreakRendererInst : public ParticleModuleInst {

  using triple_buf_t           = concurrent_triple_buffer<ParticlePoolRenderBuffer>;
  using triple_buf_ptr_t       = std::shared_ptr<triple_buf_t>;
  using streak_vtx_t           = SVtxV12N12B12T16;
  using streak_vtxbuf_t        = DynamicVertexBuffer<streak_vtx_t>;
  using streak_vtxbuf_ptr_t    = std::shared_ptr<streak_vtxbuf_t>;
  using streak_vertex_writer_t = lev2::VtxWriter<streak_vtx_t>;

  StreakRendererInst(const StreakRendererData* srd, dataflow::GraphInst* ginst);
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

StreakRendererInst::StreakRendererInst(const StreakRendererData* srd, dataflow::GraphInst* ginst)
    : ParticleModuleInst(srd, ginst)
    , _srd(srd) {
  OrkAssert(srd);
  _triple_buf                         = std::make_shared<triple_buf_t>();
  static constexpr size_t KMAXSTREAKS = 128 << 10;
  _vertexBuffer                       = std::make_shared<streak_vtxbuf_t>(KMAXSTREAKS, 0, PrimitiveType::POINTS);
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
  const fmtx4& VP             = context->MTXI()->RefVPMatrix();
  auto M                      = cmtcs->MVPMONO(fmtx4());
  auto MVP                    = VP * M;

  auto material = _srd->_material;

  if (nullptr == material->_pipeline) {
    material->gpuInit(RCID);
    OrkAssert(material->_pipeline);
  }

  fmtx4 mtx;

  ///////////////////////////////////////////////////////////////
  // compute particle dynamic vertex buffer
  //////////////////////////////////////////
  auto render_buffer = _triple_buf->begin_pull();
  int icnt           = render_buffer->_numParticles;
  if (icnt) {
    ork::fmtx4 mtx_iw;
    mtx_iw.inverseOf(mtx);
    fvec3 obj_nrmz = fvec4(cdata.zNormal(), 0.0f).transform(mtx_iw).normalized();
    ////////////////////////////////////////////////////////////////////////////
    using fetcher_t        = std::function<const particle::BasicParticle*(size_t index)>;
    auto pbase             = render_buffer->_particles;
    bool do_sort           = _srd->_sort;
    fetcher_t get_particle = [&](size_t index) -> const particle::BasicParticle* { return pbase + index; };
    // if (meBlendMode >= Blending::ADDITIVE && meBlendMode <= Blending::ALPHA_SUBTRACTIVE) {
    // bsort = false;
    //}
    ///////////////////////////////////////////////////////////////
    // depth sort ?
    ///////////////////////////////////////////////////////////////

    if (do_sort) {
      using sorter_t                 = ork::fixedlut<float, const particle::BasicParticle*, 32768>;
      using sorter_ptr_t             = std::shared_ptr<sorter_t>;
      static sorter_ptr_t the_sorter = std::make_shared<sorter_t>(EKEYPOLICY_MULTILUT);
      the_sorter->clear();
      OrkAssert(icnt < 32768);
      for (size_t i = 0; i < icnt; i++) {
        auto ptcl  = pbase + i;
        fvec4 proj = ptcl->mPosition.transform(MVP);
        proj.perspectiveDivideInPlace();
        float fv = proj.z;
        the_sorter->AddSorted(fv, ptcl);
      }
      // override fetcher
      size_t ilast = (icnt - 1);
      get_particle = [=](size_t index) -> const particle::BasicParticle* {
        // return the_sorter->GetItemAtIndex(ilast-index).second;
        return the_sorter->GetItemAtIndex(index).second;
      };
    }
    //////////////////////////////////////////////////////////////////////////////
    float fwidth  = _input_width->value();
    float flength = _input_length->value();
    auto LW       = ork::fvec2(flength, fwidth);
    //////////////////////////////////////////////////////////////////////////////
    // compute shader path
    //////////////////////////////////////////////////////////////////////////////
    if (RCID._RCFD->isStereo()) {
      auto pipeline = material->pipeline(RCID, true);
#if defined(ENABLE_COMPUTE_SHADERS)
      auto FXI = context->FXI();
      auto CI  = context->CI();
      ///////////////////////////////////////////////////////////////
      auto params = material->_streakcu_param_buffer;
      auto mapped_params = FXI->mapParamBuffer(params);
      mapped_params->seek(0);
      mapped_params->make<fvec3>(0, 0, 0);
      mapped_params->unmap();
      ///////////////////////////////////////////////////////////////
      auto storage = material->_streakcu_vertex_io_buffer;
      size_t cu_input_size = ((icnt*2) * sizeof(fvec3)) //
                           + (icnt * sizeof(fvec2)) //
                           + sizeof(uint32_t) //
                           + sizeof(fvec3) //
                           + sizeof(fvec2);


      size_t mapping_size = cu_input_size; 

      auto mapped_storage = CI->mapStorageBuffer(storage, 0, mapping_size);
      mapped_storage->seek(0);
      mapped_storage->make<uint32_t>(icnt);
      mapped_storage->make<fvec3>(obj_nrmz);
       mapped_storage->make<fvec2>(LW);
      for (int i = 0; i < icnt; i++) {
        auto ptcl = get_particle(i);
        float fage            = ptcl->mfAge;
        float flspan          = (ptcl->mfLifeSpan != 0.0f) //
                              ? ptcl->mfLifeSpan //
                              : 0.01f;
        float clamped_unitage = std::clamp<float>((fage / flspan), 0, 1);
         mapped_storage->make<fvec3>(ptcl->mPosition);
         mapped_storage->make<fvec3>(ptcl->mVelocity);
         mapped_storage->make<fvec2>(clamped_unitage, ptcl->mfRandom);
      }
      CI->unmapStorageBuffer(mapped_storage.get());
      ///////////////////////////////////////////////////////////////
      CI->bindStorageBuffer(material->_streakcu_shader, 0, storage);
      ///////////////////////////////////////////////////////////////
      int wu_width = 1;
      int wu_height = 1;
      int wu_depth = 1;
      CI->dispatchCompute(material->_streakcu_shader, wu_width, wu_height, wu_depth);
      ///////////////////////////////////////////////////////////////
      material->update(RCID);
///////////////////////////////////////////////////////////////
#endif
    }
    //////////////////////////////////////////////////////////////////////////////
    else { // geometry shader path
           //////////////////////////////////////////////////////////////////////////////
      streak_vertex_writer_t vw;
      vw.Lock(context, _vertexBuffer.get(), icnt);
      {
        ////////////////////////////////////////////////
        // uniform properties
        ////////////////////////////////////////////////
        for (int i = 0; i < icnt; i++) {
          auto ptcl = get_particle(i);
          material->_vertexSetterStreak(
              vw,   //
              ptcl, //
              LW,   //
              obj_nrmz);
        }
      }
      vw.UnLock(context);
      _triple_buf->end_pull(render_buffer);

      auto pipeline = material->pipeline(RCID, true);
      material->update(RCID);
      pipeline->wrappedDrawCall(RCID, [&]() {
        context->RSI()->BindRasterState(material->_material->_rasterstate);
        context->GBI()->DrawPrimitiveEML(vw, ork::lev2::PrimitiveType::POINTS);
      });
    }

  } // icnt
}

///////////////////////////////////////////////////////////////////////////////

void StreakRendererData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t { return StreakRendererData::createShared(); });
  clazz
      ->directObjectProperty("material", &StreakRendererData::_material) //
      ->annotate<ConstString>("editor.factorylistbase", "psys::MaterialBase");

  clazz->directProperty("sort", &StreakRendererData::_sort);

  /*
  ork::reflect::RegisterProperty("DepthSort", &StreakRendererData::mbSort);
  ork::reflect::RegisterProperty("AlphaMux", &StreakRendererData::mAlphaMux);
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

  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Length")->_range            = {-10, 10};
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Width")->_range             = {-10, 10};
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "GradientIntensity")->_range = {0, 10};

  return data;
}

///////////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t StreakRendererData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<StreakRendererInst>(this, ginst);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::StreakRendererData, "psys::StreakRendererData");
