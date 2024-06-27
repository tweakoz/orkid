////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/reflect/properties/register.h>

#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeUser.h>

ImplementReflectionX(ork::lev2::PostFxNodeUser, "PostFxNodeUser");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeUser::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
namespace posteffect_user {
struct IMPL {
  ///////////////////////////////////////
  IMPL(PostFxNodeUser* node)
      : _node(node) {
  }
  ///////////////////////////////////////
  ~IMPL() {
  }
  ///////////////////////////////////////
  void init(lev2::Context* context) {
    if (nullptr == _rtg_out) {
      int w           = context->mainSurfaceWidth();
      int h           = context->mainSurfaceHeight();
      _rtg_out        = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X);
      auto buf        = _rtg_out->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf->_debugName = FormatString("PostFxNodeUser::_rtg_out");
      //buf        = _rtg_out->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      
      //_material.gpuInit(context);
      printf( "Loading shader<%s> for PostFxNodeUser\n", _node->_shader_path.c_str() );
      _freestyle_mtl = std::make_shared<FreestyleMaterial>();
      _freestyle_mtl->gpuInit(context, _node->_shader_path);

      printf( "assigning technique<%s> for PostFxNodeUser\n", _node->_technique_name.c_str() );
      _technique = _freestyle_mtl->technique(_node->_technique_name);
      OrkAssert(_technique != nullptr);
      _fxpInputMap    = _freestyle_mtl->param("MrtMap0");
      
    }
  }
  ///////////////////////////////////////
  void _render(CompositorDrawData& drawdata) {
    Context* target = drawdata.context();
    auto FBI = target->FBI();
    auto DWI = target->DWI();
    auto framedata = target->topRenderContextFrameData();
    auto topcomp = framedata->topCompositor();
    bool was_stereo = framedata->isStereo();
    topcomp->topCPD()._stereo1pass = false;
    //////////////////////////////////////////////////////
    FBI->SetAutoClear(false);
    //////////////////////////////////////////////////////

    if (auto try_input = drawdata._properties["postfx_in"_crcu].tryAs<rtgroup_ptr_t>()) {
      auto buf0 = try_input.value()->GetMrt(0);
      if (buf0) {
        assert(buf0 != nullptr);
        auto tex = buf0->texture();
        if (tex) {

          auto rquad = [&](int w, int h){
            ViewportRect extents(0, 0, w, h);
            FBI->pushViewport(extents);
            FBI->pushScissor(extents);
            DWI->quad2DEMLCCL(fvec4(-1, -1, 2, 2), // pos
                              fvec4(0, 0, 1, 1), // uv0
                              fvec4(0, 0, 1, 1));
            FBI->popViewport();
            FBI->popScissor();
          };

          target->debugPushGroup("PostFxNodeUser::render"); { //

            auto input_rtg = try_input.value();
            int inputw = input_rtg->width();
            int inputh = input_rtg->height();
            target->beginFrame();
            /////////////////////
            // final blit
            /////////////////////
            //printf( "inputw<%d> inputh<%d>\n", inputw, inputh );
            _rtg_out->Resize(inputw,inputh);
            FBI->PushRtGroup(_rtg_out.get());
            _freestyle_mtl->begin(_technique,framedata);
            _freestyle_mtl->_rasterstate.SetBlending(Blending::OFF);
            for( auto item : _node->_bindings ) {
              auto p = _freestyle_mtl->param(item.first);
              _freestyle_mtl->bindParam(p, item.second);
            }
            _freestyle_mtl->bindParamCTex(_fxpInputMap, tex);
            rquad(inputw,inputh);
            _freestyle_mtl->end(framedata);
            FBI->PopRtGroup();
            /////////////////////
            target->endFrame();
          }
          target->debugPopGroup();
        }
      }
    }
    topcomp->topCPD()._stereo1pass = was_stereo;
  }
  ///////////////////////////////////////
  //CompositingMaterial _material;
  freestyle_mtl_ptr_t _freestyle_mtl;
  PostFxNodeUser* _node = nullptr;
  rtgroup_ptr_t _rtg_out;
  const FxShaderTechnique* _technique = nullptr;
  const FxShaderParam* _fxpInputMap;

  fxpipeline_ptr_t _pipeline;
};
} // namespace posteffect_user
///////////////////////////////////////////////////////////////////////////////
PostFxNodeUser::PostFxNodeUser() {
  _impl = std::make_shared<posteffect_user::IMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
PostFxNodeUser::~PostFxNodeUser() {
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeUser::doGpuInit(lev2::Context* pTARG, int iW, int iH) // virtual
{
  _impl.get<std::shared_ptr<posteffect_user::IMPL>>()->init(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeUser::DoRender(CompositorDrawData& drawdata) // virtual
{
  _impl.get<std::shared_ptr<posteffect_user::IMPL>>()->_render(drawdata);
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t PostFxNodeUser::GetOutput() const {
  auto impl = _impl.get<std::shared_ptr<posteffect_user::IMPL>>();
  return (impl->_rtg_out) ? impl->_rtg_out->GetMrt(0) : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t PostFxNodeUser::GetOutputGroup() const {
  auto impl = _impl.get<std::shared_ptr<posteffect_user::IMPL>>();
  return (impl->_rtg_out) ? impl->_rtg_out : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
