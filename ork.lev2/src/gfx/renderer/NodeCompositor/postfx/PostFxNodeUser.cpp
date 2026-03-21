////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/pri.h>
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
    if (nullptr == _rtg_out[0]) {
      int w = context->mainSurfaceWidth();
      int h = context->mainSurfaceHeight();

      // Create first RTG (always needed)
      _rtg_out[0] = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X, "user"_crcu);
      auto buf0 = _rtg_out[0]->createRenderTarget(lev2::EBufferFormat::RGBA32F);
      buf0->_debugName = FormatString("PostFxNodeUser::_rtg_out[0]");

      // Create second RTG if double buffering enabled
      if (_node->_double_buffer) {
        _rtg_out[1] = std::make_shared<RtGroup>(context, w, h, lev2::MsaaSamples::MSAA_1X, "user"_crcu);
        auto buf1 = _rtg_out[1]->createRenderTarget(lev2::EBufferFormat::RGBA32F);
        buf1->_debugName = FormatString("PostFxNodeUser::_rtg_out[1]");
      } else {
        // Single buffer mode - both indices point to same RTG
        _rtg_out[1] = _rtg_out[0];
      }

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
    topcomp->topCPD()._single_pass_stereo = false;
    //////////////////////////////////////////////////////
    FBI->SetAutoClear(false);
    //////////////////////////////////////////////////////
    target->debugPushGroup("PostFxNodeUser::render");

    if (auto try_input = drawdata._properties["postfx_in"_crcu].tryAs<rtgroup_ptr_t>()) {
      auto buf0 = try_input.value()->buffer(0);
      if (buf0) {
        assert(buf0 != nullptr);
        auto tex = buf0->texture();
        if (tex) {

          auto rquad = [&](int w, int h){
            ViewportRect extents(0, 0, w, h);
            FBI->pushViewport(extents);
            FBI->pushScissor(extents);
            DWI->fullscreenQuad(_node->_flip_vertical ? fvec4(0, 1, 1, -1) : fvec4(0, 0, 1, 1));
            FBI->popViewport();
            FBI->popScissor();
          };


            auto input_rtg = try_input.value();
            int inputw = input_rtg->width();
            int inputh = input_rtg->height();
            /////////////////////
            // Render to write buffer
            /////////////////////
            //printf( "inputw<%d> inputh<%d>\n", inputw, inputh );
            _rtg_out[_write_index]->Resize(inputw,inputh);
            FBI->PushRtGroup(_rtg_out[_write_index].get());
            _freestyle_mtl->begin(_technique,framedata);
            _freestyle_mtl->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
            for( auto item : _node->_bindings ) {
              auto p = _freestyle_mtl->param(item.first);
              _freestyle_mtl->bindParam(p, item.second);
            }
            _freestyle_mtl->bindParamTexture(_fxpInputMap, tex);
            // Auto-bind depth buffer if shader declares DepthMap
            auto depth_tex = input_rtg->depthTexture();
            if (depth_tex) {
              auto p = _freestyle_mtl->param("DepthMap");
              if (p) _freestyle_mtl->bindParamTexture(p, depth_tex.get());
            }
            rquad(inputw,inputh);
            _freestyle_mtl->end(framedata);
            FBI->PopRtGroup();
            /////////////////////

        }
      }
    }
    target->debugPopGroup();
    topcomp->topCPD()._single_pass_stereo = was_stereo;

    // Swap read/write indices for next frame (double buffering)
    if (_node->_double_buffer) {
      std::swap(_read_index, _write_index);
    }
  }
  ///////////////////////////////////////
  //CompositingMaterial _material;
  freestyle_mtl_ptr_t _freestyle_mtl;
  PostFxNodeUser* _node = nullptr;
  rtgroup_ptr_t _rtg_out[2];  // Double-buffered RTGs
  int _read_index = 0;         // Buffer to read from (previous frame)
  int _write_index = 1;        // Buffer to write to (current frame)
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
  if (auto impl = _impl.tryAsShared<posteffect_user::IMPL>()) {
    impl.value()->init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
void PostFxNodeUser::DoRender(CompositorDrawData& drawdata) // virtual
{
  if (auto impl = _impl.tryAsShared<posteffect_user::IMPL>()) {
    impl.value()->_render(drawdata);
  }
}
///////////////////////////////////////////////////////////////////////////////
rtbuffer_ptr_t PostFxNodeUser::GetOutput() const {
  if (auto impl = _impl.tryAsShared<posteffect_user::IMPL>()) {
    auto& rtg = impl.value()->_rtg_out[impl.value()->_read_index];
    return rtg ? rtg->buffer(0) : nullptr;
  }
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
rtgroup_ptr_t PostFxNodeUser::GetOutputGroup() const {
  if (auto impl = _impl.tryAsShared<posteffect_user::IMPL>()) {
    auto& rtg = impl.value()->_rtg_out[impl.value()->_read_index];
    return rtg ? rtg : nullptr;
  }
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
texture_ptr_t PostFxNodeUser::getCurrentReadTexture() const {
  if (auto impl = _impl.tryAsShared<posteffect_user::IMPL>()) {
    auto& rtg = impl.value()->_rtg_out[impl.value()->_read_index];
    if (rtg) {
      return rtg->buffer(0)->_texture;
    }
  }
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
