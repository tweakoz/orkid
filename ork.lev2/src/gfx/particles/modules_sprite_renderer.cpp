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
/////////////////////////////////////////

struct SpriteRendererInst : public ParticleModuleInst {

  using triple_buf_t     = concurrent_triple_buffer<ParticlePoolRenderBuffer>;
  using triple_buf_ptr_t = std::shared_ptr<triple_buf_t>;

  SpriteRendererInst(const SpriteRendererData* smd, dataflow::GraphInst* ginst);
  void onLink(GraphInst* inst) final;
  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final;
  void _render(const ork::lev2::RenderContextInstData& RCID);

  BlendingMacro meBlendMode              = BlendingMacro::ALPHA;
  ParticleItemAlignment meAlignment = ParticleItemAlignment::BILLBOARD;
  bool mbSort                       = false;
  int miImageFrame                  = 0;
  float mCurFGI                     = 0.0f;
  int miTexCnt                      = 0;
  float mfTexs                      = 0.0f;
  int miAnimTexDim                  = 1;

  ork::fvec2 UVR0;
  ork::fvec3 NX_NY;
  ork::fvec3 PX_NY;
  ork::fvec3 PX_PY;
  ork::fvec3 NX_PY;

  orklut<PoolString, ork::Object*> mGradients;
  orklut<PoolString, ork::Object*> mMaterials;
  PoolString mActiveGradient;
  PoolString mActiveMaterial;

  const SpriteRendererData* _smd;
  triple_buf_ptr_t _triple_buf;

  floatxf_inp_pluginst_ptr_t _input_size;
};

///////////////////////////////////////////////////////////////////////////////

SpriteRendererInst::SpriteRendererInst(const SpriteRendererData* smd, dataflow::GraphInst* ginst)
    : ParticleModuleInst(smd, ginst)
    , _smd(smd) {

  _triple_buf = std::make_shared<triple_buf_t>();
}

///////////////////////////////////////////////////////////////////////////////

void SpriteRendererInst::onLink(GraphInst* inst) {
  _onLink(inst);
  auto ptcl_context         = inst->_impl.getShared<Context>();
  ptcl_context->_rcidlambda = [this](const RenderContextInstData& RCID) { this->_render(RCID); };
  _input_size               = typedInputNamed<FloatXfPlugTraits>("Size");
  _input_size->setValue(4);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteRendererInst::compute(GraphInst* inst, ui::updatedata_ptr_t updata) {
  auto ptcl_context = inst->_impl.getShared<Context>();
  auto drawable     = ptcl_context->_drawable;
  OrkAssert(drawable);
  auto output_buffer = _triple_buf->begin_push();
  output_buffer->update(*_pool);
  _triple_buf->end_push(output_buffer);
}

///////////////////////////////////////////////////////////////////////////////

void SpriteRendererInst::_render(const ork::lev2::RenderContextInstData& RCID) {

  auto render_buffer          = _triple_buf->begin_pull();
  auto context                = RCID.context();
  auto RCFD                   = context->topRenderContextFrameData();
  const auto& CPD             = RCFD->topCPD();
  const CameraMatrices* cmtcs = CPD.cameraMatrices();
  const CameraData& cdata     = cmtcs->_camdat;
  const fmtx4& MVP            = context->MTXI()->RefMVPMatrix();
  auto& vertex_buffer         = GfxEnv::GetSharedDynamicVB();

  auto material = _smd->_material;

  if (nullptr == material->_pipeline) {
    material->gpuInit(RCID);
    OrkAssert(material->_pipeline);
  }

  int icnt             = render_buffer->_numParticles;
  int ivertexlockcount = icnt;

  fmtx4 mtx;

  float input_rot  = 0.0f; // mPlugInpRot.GetValue();
  float anim_frame = 0.0f; // mPlugInpAnimFrame.GetValue()
  float input_size = _input_size->value();



  if (icnt) {
    vertex_writer_t vw;
    vw.Lock(context, &vertex_buffer, ivertexlockcount);
    { // ork::fcolor4 CL;
      switch (meAlignment) {
        case ParticleItemAlignment::BILLBOARD: {
          ork::fmtx4 matrs(mtx);
          matrs.setTranslation(0.0f, 0.0f, 0.0f);
          matrs.transpose();
          fvec3 nx = cdata.xNormal().transform(matrs);
          fvec3 ny = cdata.yNormal().transform(matrs);
          fvec3 NX = nx * -1.0f;
          fvec3 PX = nx;
          fvec3 NY = ny * -1.0f;
          fvec3 PY = ny;
          NX_NY    = (NX + NY);
          PX_NY    = (PX + NY);
          PX_PY    = (PX + PY);
          NX_PY    = (NX + PY);
          break;
        }
        case ParticleItemAlignment::XZ: {
          NX_NY = fvec3(-1.0f, 0.0f, -1.0f);
          PX_NY = fvec3(+1.0f, 0.0f, -1.0f);
          PX_PY = fvec3(+1.0f, 0.0f, +1.0f);
          NX_PY = fvec3(-1.0f, 0.0f, +1.0f);
          break;
        }
        case ParticleItemAlignment::XY: {
          NX_NY = fvec3(-1.0f, -1.0f, 0.0f);
          PX_NY = fvec3(+1.0f, -1.0f, 0.0f);
          PX_PY = fvec3(+1.0f, +1.0f, 0.0f);
          NX_PY = fvec3(-1.0f, +1.0f, 0.0f);
          break;
        }
        case ParticleItemAlignment::YZ: {
          NX_NY = fvec3(0.0f, -1.0f, -1.0f);
          PX_NY = fvec3(0.0f, +1.0f, -1.0f);
          PX_PY = fvec3(0.0f, +1.0f, +1.0f);
          NX_PY = fvec3(0.0f, -1.0f, +1.0f);
          break;
        }
      }

      ////////////////////////////////////////////////////////////////////

      bool bsort = mbSort;

      if (meBlendMode >= BlendingMacro::ADDITIVE && meBlendMode <= BlendingMacro::ALPHA_SUBTRACTIVE) {
        bsort = false;
      }

      fvec4 color(1, 1, 1, 1);
      U32 ucolor  = color.vertexColorU32();
      float fang  = input_rot * DTOR;

      ///////////////////////////////////////////////////////////////
      // default particle fetcher
      ///////////////////////////////////////////////////////////////

      using fetcher_t = std::function<const particle::BasicParticle*(size_t index)>;

      auto pbase  = render_buffer->_particles;
      fetcher_t get_particle = [&](size_t index)->const particle::BasicParticle*{
          return pbase + index;
      };

      ///////////////////////////////////////////////////////////////
      // depth sort ?
      ///////////////////////////////////////////////////////////////

      if (bsort) {
        static ork::fixedlut<float, const particle::BasicParticle*, 32768> SortedParticles(EKEYPOLICY_MULTILUT);
        SortedParticles.clear();
        for (size_t i = 0; i < icnt; i++) {
          auto ptcl  = pbase + i;
          fvec4 proj = ptcl->mPosition.transform(MVP);
          proj.perspectiveDivideInPlace();
          float fv = proj.z;
          SortedParticles.AddSorted(fv, ptcl);
        }
        // override fetcher
        size_t ilast = (icnt - 1);
        get_particle = [&](size_t index)->const particle::BasicParticle*{
          return SortedParticles.GetItemAtIndex(ilast-index).second;
        };
      }

      //////////////////////////////////////////////////////
      // particle loop
      //////////////////////////////////////////////////////

      for (size_t i = 0; i < icnt; i++) {
        material->_vertexSetter(vw,get_particle(i),fang,input_size,ucolor);
      }

      ////////////////////////////////////////////////////////////////////
    }
    vw.UnLock(context);

    material->pipeline(RCID,false)->wrappedDrawCall(RCID, [&]() {
      material->update(RCID);
      // context->MTXI()->PushMMatrix(fmtx4::multiply_ltor(MatScale, mtx));
      context->GBI()->DrawPrimitiveEML(vw, ork::lev2::PrimitiveType::POINTS);
      // context->MTXI()->PopMMatrix();
    });
  }
  _triple_buf->end_pull(render_buffer);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void RendererModuleData::describeX(class_t* clazz) {
}

RendererModuleData::RendererModuleData() {
}

//////////////////////////////////////////////////////////////////////////

void SpriteRendererData::describeX(class_t* clazz) {
  clazz->setSharedFactory( []() -> rtti::castable_ptr_t {
    return SpriteRendererData::createShared();
  });
}

//////////////////////////////////////////////////////////////////////////

SpriteRendererData::SpriteRendererData() {
  _material = std::make_shared<FlatMaterial>();
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<SpriteRendererData> SpriteRendererData::createShared() {
  auto data = std::make_shared<SpriteRendererData>();

  _initShared(data);

  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Size");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "Rot");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "AnimFrame");
  createInputPlug<FloatXfPlugTraits>(data, EPR_UNIFORM, "GradientIntensity");

  createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Noise0");
  createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Noise1");
  createInputPlug<Vec3XfPlugTraits>(data, EPR_UNIFORM, "Noise2");

  return data;
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t SpriteRendererData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<SpriteRendererInst>(this,ginst);
}

/////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::RendererModuleData, "psys::RendererModuleData");
ImplementReflectionX(ptcl::SpriteRendererData, "psys::SpriteRendererData");
