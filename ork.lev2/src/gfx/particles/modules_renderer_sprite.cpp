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

struct SpriteRendererInst : public ParticleModuleInst {

  using triple_buf_t        = concurrent_triple_buffer<ParticlePoolRenderBuffer>;
  using triple_buf_ptr_t    = std::shared_ptr<triple_buf_t>;
  using sprite_vtxbuf_t     = DynamicVertexBuffer<sprite_vtx_t>;
  using sprite_vtxbuf_ptr_t = std::shared_ptr<sprite_vtxbuf_t>;

  SpriteRendererInst(const SpriteRendererData* srd, dataflow::GraphInst* ginst);
  void onLink(GraphInst* inst) final;
  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final;
  void _render(const ork::lev2::RenderContextInstData& RCID);
  const SpriteRendererData* _srd;
  floatxf_inp_pluginst_ptr_t _input_size;
  float_out_pluginst_ptr_t _output_uage;
  triple_buf_ptr_t _triple_buf;
  sprite_vtxbuf_ptr_t _vertexBuffer;
};

///////////////////////////////////////////////////////////////////////////////

SpriteRendererInst::SpriteRendererInst(const SpriteRendererData* srd, dataflow::GraphInst* ginst)
    : ParticleModuleInst(srd, ginst)
    , _srd(srd) {
  OrkAssert(srd);
  _triple_buf                         = std::make_shared<triple_buf_t>();
  static constexpr size_t KMAXSpriteS = 128 << 10;
  _vertexBuffer                       = std::make_shared<sprite_vtxbuf_t>(KMAXSpriteS, 0);
  _vertexBuffer->SetRingLock(true);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteRendererInst::onLink(GraphInst* inst) {
  _onLink(inst);
  auto ptcl_context         = inst->_impl.getShared<Context>();
  ptcl_context->_rcidlambda = [this](const RenderContextInstData& RCID) { this->_render(RCID); };
  _input_size               = typedInputNamed<FloatXfPlugTraits>("Size");

  auto pool = _graphinst->firstModuleInst<ParticlePoolModuleInst>();
  OrkAssert(pool);
  _output_uage = pool->typedOutputNamed<FloatPlugTraits>("UnitAge");
  OrkAssert(_output_uage);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteRendererInst::compute(
    GraphInst* inst, //
    ui::updatedata_ptr_t updata) {

  auto ptcl_context = inst->_impl.getShared<Context>();
  auto drawable     = ptcl_context->_drawable;
  OrkAssert(drawable);
  auto output_buffer = _triple_buf->begin_push();
  output_buffer->update(*_pool);
  _triple_buf->end_push(output_buffer);

  auto material = _srd->_material;

  /////////////////////////////////////
  // compute light color
  /////////////////////////////////////

  auto as_grad = dynamic_cast<GradientMaterial*>(material.get());
  if( true and as_grad ){
    auto avg_color = fvec4(0,0,0,0);
    int num_alive = _pool->GetNumAlive();
    int sample_count = 0;
    for (int i = 0; i < num_alive; i+=32) {
      BasicParticle* particle = _pool->GetActiveParticle(i);
      float unit_age = particle->_unit_age;
      int index = int(unit_age * 255.0f);
      avg_color += as_grad->_gradientSamples[index];
      sample_count++;
    }

    avg_color *= as_grad->_gradientColorIntensity;
    //avg_color *= (1.0f/float(sample_count));
    inst->_vars.makeValueForKey<fvec4>("emission_color") = avg_color;

  }
  else{
    inst->_vars.makeValueForKey<fvec4>("emission_color") = material->_averageColor;
  }

  /////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

void SpriteRendererInst::_render(const ork::lev2::RenderContextInstData& RCID) {

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
    Timer gpu_init_timer;
    gpu_init_timer.Start();
    material->gpuInit(RCID);
    OrkAssert(material->_pipeline);
    double gpu_init_time = gpu_init_timer.SecsSinceStart();
    printf("gpu_init_time<%f>\n", gpu_init_time);
  }

  fmtx4 mtx;

  Timer prender_timer;
  prender_timer.Start();

  ///////////////////////////////////////////////////////////////
  // compute particle dynamic vertex buffer
  //////////////////////////////////////////
  auto render_buffer = _triple_buf->begin_pull();
  int icnt           = render_buffer->_numParticles;
  if (0 == icnt) {
    _triple_buf->end_pull(render_buffer);
    return;
  }
  double render_time_1 = prender_timer.SecsSinceStart();

  //////////////////////////////////////////

  double render_time_1a = 0.0f;
  double render_time_1b = 0.0f;
  double render_time_1c = 0.0f;

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
      return the_sorter->GetItemAtIndex(ilast-index).second;
      //return the_sorter->GetItemAtIndex(index).second;
    };
  }
  //////////////////////////////////////////////////////////////////////////////
  // float fwidth  = _input_width->value();
  // float flength = _input_length->value();
  float fsize          = _input_size->value();
  auto LW              = ork::fvec2(fsize, fsize);
  bool size_is_varying = _input_size->connectedIsVarying();
  // printf( "fsize<%f>\n", fsize );
  //////////////////////////////////////////////////////////////////////////////
  // compute shader path
  //////////////////////////////////////////////////////////////////////////////
  if (RCID.rcfd()->isStereo()) {
#if defined(ENABLE_COMPUTE_SHADERS)
    auto FXI = context->FXI();
    auto CI  = context->CI();
    ///////////////////////////////////////////////////////////////
    auto stereocams  = CPD._stereoCameraMatrices;
    auto worldmatrix = RCID.worldMatrix();
    auto SMM         = stereocams->_mono;
    // obj_nrmz = fvec4(SMM->_camdat.zNormal(), 0.0f).transform(mtx_iw).normalized();
    obj_nrmz = fvec4(0, 1, 0, 0);
    // fmtx4 scale_matrix;
    // scale_matrix.setScale(fsize,fsize,fsize);
    // worldmatrix = scale_matrix * worldmatrix;
    ///////////////////////////////////////////////////////////////
    auto storage        = material->_cu_vertex_io_buffer;
    size_t mapping_size = 1 << 20;
    auto mapped_storage = CI->mapStorageBuffer(storage, 0, mapping_size);
    mapped_storage->seek(0);
    mapped_storage->make<int32_t>(icnt);                        // 0
    mapped_storage->make<fmtx4>(stereocams->VL());              // 16
    mapped_storage->make<fmtx4>(stereocams->VR());              // 80
    mapped_storage->make<fmtx4>(stereocams->MVPL(worldmatrix)); // 16
    mapped_storage->make<fmtx4>(stereocams->MVPR(worldmatrix)); // 80
    mapped_storage->make<fvec4>(obj_nrmz);                      // 144
    // OrkAssert(mapped_storage->_cursor == 176);
    mapped_storage->align(16);
    if (size_is_varying) {
      for (int i = 0; i < icnt; i++) {
        auto ptcl             = get_particle(i);
        _output_uage->setValue(ptcl->_unit_age);
        fsize = _input_size->value(); // transformers applied here..
        LW = ork::fvec2(fsize, fsize);
        mapped_storage->make<fvec4>(ptcl->mPosition);
        mapped_storage->make<fvec4>(ptcl->mVelocity);
        mapped_storage->make<fvec4>(LW.x, LW.y, 0, 0);              // 160
        mapped_storage->make<fvec4>(ptcl->_unit_age, ptcl->mfRandom, 0, 0);
      }
    }
    else{
      for (int i = 0; i < icnt; i++) {
        auto ptcl             = get_particle(i);
        mapped_storage->make<fvec4>(ptcl->mPosition);
        mapped_storage->make<fvec4>(ptcl->mVelocity);
        mapped_storage->make<fvec4>(LW.x, LW.y, 0, 0);              // 160
        mapped_storage->make<fvec4>(ptcl->_unit_age, ptcl->mfRandom, 0, 0);
      }
    }
    CI->unmapStorageBuffer(mapped_storage.get());
    render_time_1a = prender_timer.SecsSinceStart();
    ///////////////////////////////////////////////////////////////
    CI->bindStorageBuffer(material->_spritecu_shader, 0, storage);
    ///////////////////////////////////////////////////////////////
    int wu_width   = icnt;
    int wu_height  = 1;
    int wu_depth   = 1;
    render_time_1b = prender_timer.SecsSinceStart();
    CI->dispatchCompute(material->_spritecu_shader, wu_width, wu_height, wu_depth);
    render_time_1c = prender_timer.SecsSinceStart();
    ///////////////////////////////////////////////////////////////
    material->update(RCID);
    auto pipeline = material->pipeline(RCID, false);
    pipeline->wrappedDrawCall(RCID, [&]() {
      context->RSI()->BindRasterState(material->_material->_rasterstate);
      context->GBI()->DrawPrimitiveEML(
          storage,                             //
          ork::lev2::PrimitiveType::TRIANGLES, //
          0,
          icnt * 6);
      FXI->reset();
    });
///////////////////////////////////////////////////////////////
#endif
  }
  //////////////////////////////////////////////////////////////////////////////
  else { // geometry shader path
         //////////////////////////////////////////////////////////////////////////////
    if (icnt) {
      sprite_vertex_writer_t vw;
      vw.Lock(context, _vertexBuffer.get(), icnt);
      {
        ////////////////////////////////////////////////
        // uniform properties
        ////////////////////////////////////////////////
        if (size_is_varying) {
          for (int i = 0; i < icnt; i++) {
            auto ptcl = get_particle(i);
            _output_uage->setValue(ptcl->_unit_age);
            fsize = _input_size->value(); // transformers applied here..
            LW = ork::fvec2(fsize, fsize);
            material->_vertexSetterSprite(
                vw,   //
                ptcl, //
                0.0f, // angle
                LW.x, // size
                0xffffffff);
          }
        }
        else{
          for (int i = 0; i < icnt; i++) {
            auto ptcl = get_particle(i);
            material->_vertexSetterSprite(
                vw,   //
                ptcl, //
                0.0f, // angle
                LW.x, // size
                0xffffffff);
          }
        }
      }
      vw.UnLock(context);

      auto pipeline = material->pipeline(RCID, false);
        //pipeline->_debugBreak = true;
        pipeline->_debugPrint = false;
        material->update(RCID);
      pipeline->wrappedDrawCall(RCID, [&]() {
        context->RSI()->BindRasterState(material->_material->_rasterstate);
        context->GBI()->DrawPrimitiveEML(vw, ork::lev2::PrimitiveType::POINTS);
      });
    }
  }
  double render_time_2 = prender_timer.SecsSinceStart();
  _triple_buf->end_pull(render_buffer);

  double render_time = prender_timer.SecsSinceStart();
  if (render_time > 0.5f) {
    printf("render_time_1<%f>\n", render_time_1);
    printf("render_time_1a<%f>\n", render_time_1a);
    printf("render_time_1b<%f>\n", render_time_1b);
    printf("render_time_1c<%f>\n", render_time_1c);
    printf("render_time_2<%f>\n", render_time_2);
    printf("render_time<%f>\n", render_time);
  }
}

///////////////////////////////////////////////////////////////////////////////

static void _reshapeSpriteRendererIOs(dataflow::moduledata_ptr_t mdata) {
  auto typed = std::dynamic_pointer_cast<SpriteRendererData>(mdata);
  ModuleData::createInputPlug<FloatXfPlugTraits>(typed, EPR_UNIFORM, "Size")->_range              = {-10, 10};
  ModuleData::createInputPlug<FloatXfPlugTraits>(typed, EPR_UNIFORM, "GradientIntensity")->_range = {0, 10};
  ModuleData::createInputPlug<FloatXfPlugTraits>(typed, EPR_UNIFORM, "Scale")->_range             = {-10, 10};
}

///////////////////////////////////////////////////////////////////////////////

void SpriteRendererData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t { return SpriteRendererData::createShared(); });
  clazz
      ->directObjectProperty("material", &SpriteRendererData::_material) //
      ->annotate<ConstString>("editor.factorylistbase", "psys::MaterialBase");

  clazz->directProperty("sort", &SpriteRendererData::_sort);

  clazz->annotateTyped<moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) { _reshapeSpriteRendererIOs(mdata); });
}

///////////////////////////////////////////////////////////////////////////////

SpriteRendererData::SpriteRendererData() {
  _material = std::make_shared<FlatMaterial>();
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<SpriteRendererData> SpriteRendererData::createShared() {
  auto data = std::make_shared<SpriteRendererData>();

  _initPoolIOs(data);
  _reshapeSpriteRendererIOs(data);
  return data;
}

///////////////////////////////////////////////////////////////////////////////

// void SpriteRendererData::reshapeIOs() {
// }

///////////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t SpriteRendererData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<SpriteRendererInst>(this, ginst);
}

///////////////////////////////////////////////////////////////////////////////

void RendererModuleData::describeX(class_t* clazz) {
}

RendererModuleData::RendererModuleData() {
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::RendererModuleData, "psys::RendererModuleData");
ImplementReflectionX(ptcl::SpriteRendererData, "psys::SpriteRendererData");
