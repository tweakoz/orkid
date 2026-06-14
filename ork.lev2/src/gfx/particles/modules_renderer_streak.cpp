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
  floatxf_inp_pluginst_ptr_t _input_scale;
  floatxf_inp_pluginst_ptr_t _input_gradient_phase;
  float_out_pluginst_ptr_t _output_uage;
  triple_buf_ptr_t _triple_buf;
  streak_vtxbuf_ptr_t _vertexBuffer;
  // Per-renderer-instance SSBO for particle vertex data. Allocated lazily
  // on first render (needs Context to construct). Previously lived on
  // MaterialBase, but sharing across renderer instances caused a race
  // when multiple particle entities used the same material.
  FxShaderStorageBuffer* _cu_vertex_io_buffer = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

StreakRendererInst::StreakRendererInst(const StreakRendererData* srd, dataflow::GraphInst* ginst)
    : ParticleModuleInst(srd, ginst)
    , _srd(srd) {
  OrkAssert(srd);
  _triple_buf                         = std::make_shared<triple_buf_t>();
  static constexpr size_t KMAXSTREAKS = 128 << 10;
  _vertexBuffer                       = std::make_shared<streak_vtxbuf_t>(KMAXSTREAKS, 0);
  _vertexBuffer->SetRingLock(true);
}

///////////////////////////////////////////////////////////////////////////////

void StreakRendererInst::onLink(GraphInst* inst) {
  _onLink(inst);
  auto ptcl_context         = inst->_impl.getShared<Context>();
  ptcl_context->setRenderLambda(this, _srd->_draw_order, [this](const RenderContextInstData& RCID) { this->_render(RCID); });
  _input_length             = typedInputNamed<FloatXfPlugTraits>("Length");
  _input_width              = typedInputNamed<FloatXfPlugTraits>("Width");
  _input_scale              = typedInputNamed<FloatXfPlugTraits>("Scale");
  _input_gradient_phase     = typedInputNamed<FloatXfPlugTraits>("GradientPhase");

  auto pool = _graphinst->firstModuleInst<ParticlePoolModuleInst>();
  OrkAssert(pool);
  _output_uage = pool->typedOutputNamed<FloatPlugTraits>("UnitAge");
  OrkAssert(_output_uage);
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
  // begin_pull returns nullptr when nothing has been pushed yet — common
  // for ECS pool slots that are FREE (compute has never run for them).
  // Drawables for those slots are still enqueued + rendered each frame
  // but there's no vertex data to draw, so just bail.
  if (not render_buffer) {
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
      // return the_sorter->GetItemAtIndex(ilast-index).second;
      return the_sorter->GetItemAtIndex(index).second;
    };
  }
  //////////////////////////////////////////////////////////////////////////////
  float fwidth  = _input_width->value();
  float flength = _input_length->value();
  float fscale  = _input_scale->value();
  float fgrad_phase = _input_gradient_phase->value();
  auto LW       = ork::fvec2(flength, fwidth);

  bool length_is_varying = _input_length->connectedIsVarying();
  bool width_is_varying  = _input_width->connectedIsVarying();

  int variant =  length_is_varying ? 1 : 0;
      variant |= width_is_varying ? 2: 0;
  //////////////////////////////////////////////////////////////////////////////
  // SSBO-based vertex shader streak rendering (unified path for stereo and non-stereo)
  //////////////////////////////////////////////////////////////////////////////
  if (icnt) {
    auto FXI = context->FXI();

    OrkAssert(icnt <= 262144);

    ///////////////////////////////////////////////////////////////
    // Get camera position and up vector for streak rendering
    ///////////////////////////////////////////////////////////////
    fvec3 camPos = cdata.GetEye();
    fvec3 camUp;

    if (RCID.rcfd()->isStereo()) {
      auto stereocams = CPD._stereo_cam_matrices;
      auto VL = stereocams->VL();
      auto VR = stereocams->VR();
      // Average left/right camera up vectors for stereo
      // Use rows of view matrix (camera axes in world space)
      fvec3 pyL = fvec3(VL.elemXY(0,1), VL.elemXY(1,1), VL.elemXY(2,1));
      fvec3 pyR = fvec3(VR.elemXY(0,1), VR.elemXY(1,1), VR.elemXY(2,1));
      camUp = (pyL + pyR).normalized();
    } else {
      // Extract camera up from view matrix (row, not column)
      auto V = cmtcs->GetVMatrix();
      camUp = fvec3(V.elemXY(0,1), V.elemXY(1,1), V.elemXY(2,1)).normalized();
    }

    ///////////////////////////////////////////////////////////////
    // SSBO header layout (mirrors storage_particles in particle_comshader.i2):
    //   0  camRightSize    xyz=camPos (streaks repurpose), .w unused
    //   16 camUpCount      xyz=camUp, w=numParticles
    //   32 materialParams  x=gradient_phase, yzw=reserved
    //   48 particleData[]  per-particle pos.xyz, width
    //   48 + 262144*16    particleData2[]  per-particle vel.xyz, length
    //   48 + 262144*16*2  particleData3[]  per-particle age, random, aux.x, aux.y
    ///////////////////////////////////////////////////////////////
    // Lazy-allocate this renderer instance's SSBO on first render. Per-
    // instance, NOT material-shared — see field-decl comment for why.
    if (not _cu_vertex_io_buffer) {
      _cu_vertex_io_buffer = FXI->createStorageBuffer(16 << 20);
    }
    auto storage        = _cu_vertex_io_buffer;
    size_t mapping_size = 16 << 20; // 16MB (supports 262144 particles × 3 arrays × 16 bytes)
    auto mapped_storage = FXI->mapStorageBuffer(storage, 0, mapping_size, BufferMapAccess::WRITE_ONLY);

    mapped_storage->seek(0);
    // Header: cam vectors + per-frame material params
    mapped_storage->make<fvec4>(camPos.x, camPos.y, camPos.z, 0.0f);     // offset 0
    mapped_storage->make<fvec4>(camUp.x, camUp.y, camUp.z, float(icnt)); // offset 16
    mapped_storage->make<fvec4>(fgrad_phase, 0.0f, 0.0f, 0.0f);          // offset 32 — materialParams

    // Fill particle data based on variant
    // particleData array at offset 48: pos.xyz, width
    constexpr size_t particleData_offset = 48;
    mapped_storage->seek(particleData_offset);
    switch(variant) {
      case 0: // nothing varying
        for (int i = 0; i < icnt; i++) {
          auto ptcl = get_particle(i);
          mapped_storage->make<fvec4>(ptcl->mPosition.x, ptcl->mPosition.y, ptcl->mPosition.z, LW.y); // width in w
        }
        break;
      case 1: // length_is_varying
        for (int i = 0; i < icnt; i++) {
          auto ptcl = get_particle(i);
          _output_uage->setValue(ptcl->_unit_age);
          // length varies, width constant
          mapped_storage->make<fvec4>(ptcl->mPosition.x, ptcl->mPosition.y, ptcl->mPosition.z, LW.y);
        }
        break;
      case 2: // width_is_varying
        for (int i = 0; i < icnt; i++) {
          auto ptcl = get_particle(i);
          _output_uage->setValue(ptcl->_unit_age);
          fwidth = _input_width->value();
          mapped_storage->make<fvec4>(ptcl->mPosition.x, ptcl->mPosition.y, ptcl->mPosition.z, fwidth);
        }
        break;
      case 3: // both varying
        for (int i = 0; i < icnt; i++) {
          auto ptcl = get_particle(i);
          _output_uage->setValue(ptcl->_unit_age);
          fwidth = _input_width->value();
          mapped_storage->make<fvec4>(ptcl->mPosition.x, ptcl->mPosition.y, ptcl->mPosition.z, fwidth);
        }
        break;
    }

    // particleData2 array at offset 48 + 262144*16 = 4194352: vel.xyz, length
    constexpr size_t particleData2_offset = 48 + 262144 * 16;
    mapped_storage->seek(particleData2_offset);

    switch(variant) {
      case 0: // nothing varying
        for (int i = 0; i < icnt; i++) {
          auto ptcl = get_particle(i);
          mapped_storage->make<fvec4>(ptcl->mVelocity.x, ptcl->mVelocity.y, ptcl->mVelocity.z, LW.x); // length in w
        }
        break;
      case 1: // length_is_varying
        for (int i = 0; i < icnt; i++) {
          auto ptcl = get_particle(i);
          _output_uage->setValue(ptcl->_unit_age);
          flength = _input_length->value();
          mapped_storage->make<fvec4>(ptcl->mVelocity.x, ptcl->mVelocity.y, ptcl->mVelocity.z, flength);
        }
        break;
      case 2: // width_is_varying
        for (int i = 0; i < icnt; i++) {
          auto ptcl = get_particle(i);
          mapped_storage->make<fvec4>(ptcl->mVelocity.x, ptcl->mVelocity.y, ptcl->mVelocity.z, LW.x);
        }
        break;
      case 3: // both varying
        for (int i = 0; i < icnt; i++) {
          auto ptcl = get_particle(i);
          _output_uage->setValue(ptcl->_unit_age);
          flength = _input_length->value();
          mapped_storage->make<fvec4>(ptcl->mVelocity.x, ptcl->mVelocity.y, ptcl->mVelocity.z, flength);
        }
        break;
    }

    // particleData3 array at offset 48 + 262144*16*2 = 8388656:
    //   x = unit_age, y = mfRandom, z = _aux.x, w = _aux.y
    // (aux.z/w are not currently surfaced to the shader; if needed later
    // add a particleData4 array. The CPU-side _aux vec4 is fully populated;
    // this is just a shader-visibility limit.)
    constexpr size_t particleData3_offset = 48 + 262144 * 16 * 2;
    mapped_storage->seek(particleData3_offset);

    for (int i = 0; i < icnt; i++) {
      auto ptcl = get_particle(i);
      mapped_storage->make<fvec4>(ptcl->_unit_age, ptcl->mfRandom,
                                   ptcl->_aux.x, ptcl->_aux.y);
    }

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
    auto pipeline = material->pipeline(RCID, true);
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

static void _reshapeStreakRendererIOs(dataflow::moduledata_ptr_t mdata) {
  auto typed = std::dynamic_pointer_cast<StreakRendererData>(mdata);
  ModuleData::createInputPlug<FloatXfPlugTraits>(typed, EPR_UNIFORM, "Length")->_range            = {-10, 10};
  ModuleData::createInputPlug<FloatXfPlugTraits>(typed, EPR_UNIFORM, "Width")->_range             = {-10, 10};
  ModuleData::createInputPlug<FloatXfPlugTraits>(typed, EPR_UNIFORM, "GradientIntensity")->_range = {0, 10};
  ModuleData::createInputPlug<FloatXfPlugTraits>(typed, EPR_UNIFORM, "Scale")->_range             = {-10, 10};
  // GradientPhase: only meaningful when paired with GradientAtlasMaterial.
  // Shader adds this to particle's aux.x before the V-sample → scrolls the
  // whole atlas vertically. Range is unbounded (fract() in shader wraps).
  ModuleData::createInputPlug<FloatXfPlugTraits>(typed, EPR_UNIFORM, "GradientPhase")->_range     = {-1000, 1000};
}

///////////////////////////////////////////////////////////////////////////////

void StreakRendererData::describeX(class_t* clazz) {
  clazz->setSharedFactory([] -> rtti::castable_ptr_t { return StreakRendererData::createShared(); });
  clazz
      ->directObjectProperty("material", &StreakRendererData::_material) //
      ->annotate<ConstString>("editor.factorylistbase", "psys::MaterialBase");

  clazz->directProperty("sort", &StreakRendererData::_sort);

  clazz->annotateTyped<moduleIOreshape_fn_t>(
      "reshapeIOs", [](dataflow::moduledata_ptr_t mdata) { _reshapeStreakRendererIOs(mdata); });
}

///////////////////////////////////////////////////////////////////////////////

StreakRendererData::StreakRendererData() {
  _material = std::make_shared<FlatMaterial>();
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<StreakRendererData> StreakRendererData::createShared() {
  auto data = std::make_shared<StreakRendererData>();

  _initPoolIOs(data);
  _reshapeStreakRendererIOs(data);
  return data;
}

///////////////////////////////////////////////////////////////////////////////

// void StreakRendererData::reshapeIOs() {
// }

///////////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t StreakRendererData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<StreakRendererInst>(this, ginst);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::StreakRendererData, "psys::StreakRendererData");
