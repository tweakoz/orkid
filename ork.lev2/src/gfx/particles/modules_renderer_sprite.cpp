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
  floatxf_inp_pluginst_ptr_t _input_gradient_phase;
  float_out_pluginst_ptr_t _output_uage;
  triple_buf_ptr_t _triple_buf;
  // item E — emission-light temporal smoothing (EMA over compute ticks);
  // raw per-tick weighted averages twitch as particles cross ramp stops.
  fvec3 _emission_color_ema;
  fvec3 _emission_center_ema;
  float _emission_fade_ema = 0.0f;
  bool  _emission_seeded   = false;
  sprite_vtxbuf_ptr_t _vertexBuffer;
  // Per-renderer-instance SSBO. See StreakRendererInst for rationale.
  FxShaderStorageBuffer* _cu_vertex_io_buffer = nullptr;
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
  ptcl_context->setRenderLambda(this, _srd->_draw_order, [this](const RenderContextInstData& RCID) { this->_render(RCID); });
  // PRE-RENDER GPU upload (render-pass-safe): refresh the material's gradient LUT texture.
  ptcl_context->setGpuUpdateLambda(this, [this](ork::lev2::Context* ctx) {
    if (_srd->_material)
      _srd->_material->onGpuUpdate(ctx);
  });
  _input_size               = typedInputNamed<FloatXfPlugTraits>("Size");
  _input_gradient_phase     = typedInputNamed<FloatXfPlugTraits>("GradientPhase");

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

  auto as_grad      = dynamic_cast<GradientMaterial*>(material.get());
  auto as_freestyle = dynamic_cast<FreestyleParticleMaterial*>(material.get());
  if( as_grad ){
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
  else if( as_freestyle ){
    // item E — the PREMA emission light. Weight each live particle's ramp
    // sample by its ADDITIVE-ness: w = luminance(rgb) * (1 - occlusion).
    // Fire (bright, a~0) drives the light; smoke (dark, a>0) contributes
    // ~nothing — the weighting falls out of the material contract, no
    // per-system customization needed. Publishes BOTH the light color
    // (weight-averaged chromaticity, count-faded) and the light CENTER
    // (weighted centroid — world-space trails park their sgnode at
    // identity, so the node transform can't position the light).
    fvec3 sum_rgb(0, 0, 0);
    fvec3 sum_pos(0, 0, 0);
    float sum_w     = 0.0f;
    int   num_alive = _pool->GetNumAlive();
    for (int i = 0; i < num_alive; i += 2) {
      BasicParticle* particle = _pool->GetActiveParticle(i);
      int index = int(std::clamp(particle->_unit_age, 0.0f, 1.0f) * 255.0f);
      const fvec4& s = as_freestyle->_gradientSamples[index];
      float lum = (s.x + s.y + s.z) * 0.333333f;
      // lum^p, p AUTHORED on the material (emission_lum_power): p=1 lets
      // the brief bright ignition flash dominate (whiter, twitchier);
      // p<1 biases toward the flame BODY (oranger, steadier).
      float w = std::pow(lum, as_freestyle->_emission_lum_power) //
                * (1.0f - std::clamp(s.w, 0.0f, 1.0f));
      sum_rgb += fvec3(s.x, s.y, s.z) * w;
      sum_pos += particle->mPosition * w;
      sum_w   += w;
    }
    // temporal smoothing: EMA with the AUTHORED time constant
    // (emission_smoothing; 0 = raw per-tick) — flicker is a look knob,
    // owned by the DSL, not engine code. Position tracks 2x faster.
    float dt  = float(updata->_dt);
    float tau = as_freestyle->_emission_smoothing;
    if (sum_w > 1e-4f) {
      float inv   = 1.0f / sum_w;
      fvec3 color = sum_rgb * inv * as_freestyle->_gradientColorIntensity //
                    * as_freestyle->_emission_tint;
      fvec3 cen   = sum_pos * inv;
      float fade  = std::min(sum_w * 0.25f, 1.0f);
      if (tau <= 0.0f or not _emission_seeded) {
        _emission_color_ema  = color;
        _emission_center_ema = cen;
        _emission_fade_ema   = fade;
        _emission_seeded     = true;
      } else {
        float ac = 1.0f - std::exp(-dt / tau);
        float ap = 1.0f - std::exp(-dt / (tau * 0.5f));
        _emission_color_ema  = _emission_color_ema + (color - _emission_color_ema) * ac;
        _emission_center_ema = _emission_center_ema + (cen - _emission_center_ema) * ap;
        _emission_fade_ema   = _emission_fade_ema + (fade - _emission_fade_ema) * ac;
      }
      inst->_vars.makeValueForKey<fvec4>("emission_color")  = fvec4(_emission_color_ema * _emission_fade_ema, 1.0f);
      inst->_vars.makeValueForKey<fvec4>("emission_center") = fvec4(_emission_center_ema, 1.0f);
    } else {
      _emission_seeded = false;
      inst->_vars.makeValueForKey<fvec4>("emission_color")  = fvec4(0, 0, 0, 0);
      inst->_vars.makeValueForKey<fvec4>("emission_center") = fvec4(0, 0, 0, 0);
    }
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
  auto cmtcs                  = CPD.cameraMatrices();
  const CameraData& cdata     = cmtcs->_camdat;
  const fmtx4& VP             = context->MTXI()->RefVPMatrix();
  auto M                      = cmtcs->MVPMONO(fmtx4());
  auto MVP                    = VP * M;

  if (CPD.isPicking()) {
    return;  // Don't render during picking passes (yet)
  }

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
  if (not render_buffer) {
    // Nothing pushed yet — ECS pool slot with no compute history. Bail.
    return;
  }
  int icnt = render_buffer->_numParticles;
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
  // if (meBlendMode >= BlendingMacro::ADDITIVE && meBlendMode <= BlendingMacro::ALPHA_SUBTRACTIVE) {
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
  float fsize          = _input_size->value();
  float fgrad_phase    = _input_gradient_phase->value();
  auto LW              = ork::fvec2(fsize, fsize);
  bool size_is_varying = _input_size->connectedIsVarying();
  //////////////////////////////////////////////////////////////////////////////
  // SSBO-based vertex shader billboarding (unified path for stereo and non-stereo)
  //////////////////////////////////////////////////////////////////////////////
  if (icnt) {
    auto FXI = context->FXI();

    OrkAssert(icnt <= 262144);

    ///////////////////////////////////////////////////////////////
    // Get camera vectors for billboarding
    ///////////////////////////////////////////////////////////////
    fvec3 camRight, camUp;

    if (RCID.rcfd()->isStereo()) {
      auto stereocams = CPD._stereo_cam_matrices;
      auto VL = stereocams->VL();
      auto VR = stereocams->VR();
      // Average left/right camera vectors for stereo
      // Use rows of view matrix (camera axes in world space)
      fvec3 pxL = fvec3(VL.elemXY(0,0), VL.elemXY(1,0), VL.elemXY(2,0));
      fvec3 pyL = fvec3(VL.elemXY(0,1), VL.elemXY(1,1), VL.elemXY(2,1));
      fvec3 pxR = fvec3(VR.elemXY(0,0), VR.elemXY(1,0), VR.elemXY(2,0));
      fvec3 pyR = fvec3(VR.elemXY(0,1), VR.elemXY(1,1), VR.elemXY(2,1));
      camRight = (pxL + pxR).normalized();
      camUp = (pyL + pyR).normalized();
    } else {
      // Extract camera axes in world space from view matrix
      // View matrix rows give camera axes in world space
      auto V = cmtcs->GetVMatrix();
      camRight = fvec3(V.elemXY(0,0), V.elemXY(1,0), V.elemXY(2,0)).normalized();
      camUp = fvec3(V.elemXY(0,1), V.elemXY(1,1), V.elemXY(2,1)).normalized();
    }

    ///////////////////////////////////////////////////////////////
    // SSBO header layout (mirrors storage_particles in particle_comshader.i2):
    //   0  camRightSize    xyz=camRight, .w unused
    //   16 camUpCount      xyz=camUp, w=numParticles
    //   32 materialParams  x=gradient_phase, yzw=reserved
    //   48 particleData[]  per-particle pos.xyz, size
    //   48 + 262144*16    particleData2[]  unused for sprites
    //   48 + 262144*16*2  particleData3[]  per-particle age, random, aux.x, aux.y
    ///////////////////////////////////////////////////////////////
    if (not _cu_vertex_io_buffer) {
      // BAR: CPU-written every frame (forward writes through the mapping below), GPU-read
      // by the particle passes — VRAM-local reads, direct-map writes. Degrades to HOST
      // when the BAR heap is exhausted.
      _cu_vertex_io_buffer = FXI->createStorageBuffer(16 << 20, StorageBufferUsage::DEFAULT, BufferResidency::BAR);
    }
    auto storage        = _cu_vertex_io_buffer;
    size_t mapping_size = 16 << 20; // 16MB (supports 262144 particles × 3 arrays × 16 bytes)
    auto mapped_storage = FXI->mapStorageBuffer(storage, 0, mapping_size, BufferMapAccess::WRITE_ONLY);

    mapped_storage->seek(0);
    // Header: camera vectors, count, per-frame material params
    mapped_storage->make<fvec4>(camRight.x, camRight.y, camRight.z, 0.0f);      // offset 0
    mapped_storage->make<fvec4>(camUp.x, camUp.y, camUp.z, float(icnt));        // offset 16
    mapped_storage->make<fvec4>(fgrad_phase, 0.0f, 0.0f, 0.0f);                 // offset 32 — materialParams

    // particleData array at offset 48: pos.xyz, size
    if (size_is_varying) {
      for (int i = 0; i < icnt; i++) {
        auto ptcl = get_particle(i);
        _output_uage->setValue(ptcl->_unit_age);
        fsize = _input_size->value(); // transformers applied here..
        mapped_storage->make<fvec4>(ptcl->mPosition.x, ptcl->mPosition.y, ptcl->mPosition.z, fsize);
      }
    } else {
      for (int i = 0; i < icnt; i++) {
        auto ptcl = get_particle(i);
        mapped_storage->make<fvec4>(ptcl->mPosition.x, ptcl->mPosition.y, ptcl->mPosition.z, fsize);
      }
    }

    // Skip particleData2 (not used for sprites) - seek to particleData3
    // particleData2 starts at offset 48 + 262144*16 = 4194352
    // particleData3 starts at offset 48 + 262144*16*2 = 8388656
    constexpr size_t particleData3_offset = 48 + 262144 * 16 * 2;
    mapped_storage->seek(particleData3_offset);

    // particleData3 array: x=unit_age, y=mfRandom, z=_aux.x, w=_aux.y.
    // aux.z/w aren't surfaced to the shader in v1; CPU-side _aux is still
    // the full vec4 (other consumers could read it directly).
    for (int i = 0; i < icnt; i++) {
      auto ptcl = get_particle(i);
      mapped_storage->make<fvec4>(ptcl->_unit_age, ptcl->mfRandom,
                                   ptcl->_aux.x, ptcl->_aux.y);
    }

    // gradient LUT (256 entries) at the TAIL of the SSBO (after the 3 particle arrays) —
    // the VS samples it by unit_age into frg_clr. White for non-gradient materials. This is
    // a host write to mapped memory, so it's render-pass-agnostic (the gradient texture
    // upload could never land in this all-mid-pass particle path).
    constexpr size_t gradientLUT_offset = 48 + size_t(262144) * 16 * 3;
    mapped_storage->seek(gradientLUT_offset);
    const fvec4* lut = material->_gradientSamples;
    for (int i = 0; i < 256; i++)
      mapped_storage->make<fvec4>(lut[i].x, lut[i].y, lut[i].z, lut[i].w);

    FXI->unmapStorageBuffer(mapped_storage.get());
    render_time_1a = prender_timer.SecsSinceStart();

    ///////////////////////////////////////////////////////////////
    // Bind SSBO and draw
    ///////////////////////////////////////////////////////////////
    FXI->bindStorageBuffer(material->_cu_storage_block, storage);
    render_time_1b = prender_timer.SecsSinceStart();

    // AUX-CHANNEL subpass (E2B item D): pipeline() returns nullptr when the
    // material doesn't write the active channel — skip the draw. The color
    // pass earlier this frame already ran material->update() (gradient
    // re-bake etc.); aux passes don't repeat it.
    bool aux_pass = (RCID.rcfd()->_subpassID == "AUX"_crcu);
    if (not aux_pass)
      material->update(RCID);
    auto pipeline = material->pipeline(RCID, false);
    if (pipeline) {
    // the pipeline's rasterstate carries the PASS-correct state (e.g. the
    // aux pipeline's additive/no-Z); fall back to the material state.
    auto rstate = pipeline->_rasterstate ? pipeline->_rasterstate : material->_material->_rasterstate;
    rstate->_priority = 1 << 10;
    FXI->pushRasterState(rstate);
    pipeline->wrappedDrawCall(RCID, [&]() {
      context->GBI()->DrawPrimitiveEML(
          storage,                             //
          ork::lev2::PrimitiveType::TRIANGLES, //
          0,
          icnt * 6);
      FXI->reset();
    });
    FXI->popRasterState();
    }
    render_time_1c = prender_timer.SecsSinceStart();
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
  ModuleData::createInputPlug<FloatXfPlugTraits>(typed, EPR_UNIFORM, "GradientPhase")->_range     = {-1000, 1000};
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
  clazz->directProperty("draw_order", &RendererModuleData::_draw_order);
}

RendererModuleData::RendererModuleData() {
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::RendererModuleData, "psys::RendererModuleData");
ImplementReflectionX(ptcl::SpriteRendererData, "psys::SpriteRendererData");
