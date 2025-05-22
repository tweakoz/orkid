#include "gl.h"
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/math/misc_math.h>

namespace ork::lev2 {

////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::downsample2x2(rtgroup_ptr_t src, rtgroup_ptr_t dst) {

  auto DWI = _target.DWI();

  int w   = src->width();
  int h   = src->height();
  int wd2 = w / 2;
  int hd2 = h / 2;

  auto framedata = mTargetGL.topRenderContextFrameData();

  dst->Resize(wd2, hd2);
  PushRtGroup(dst.get());
  auto src_rtb       = src->buffer(0);
  auto dst_rtb       = dst->buffer(0);
  auto src_groupimpl = src->_impl.get<glrtgroupimpl_ptr_t>();
  auto dst_groupimpl = dst->_impl.get<glrtgroupimpl_ptr_t>();
  auto this_buf      = this->GetThisBuffer();

  auto shader = utilshader();

  shader->begin(_tek_downsample2x2, framedata);
  shader->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  shader->bindParamTexture(_fxpColorMap, src->texture(0).get());
  shader->bindParamMatrix(_fxpMVP, fmtx4::Identity());
  ViewportRect extents(0, 0, wd2, hd2);
  this->pushViewport(extents);
  this->pushScissor(extents);
  _target.FXI()->applyRasterState(*(shader->_rasterstate));
  DWI->quad2DEMLCCL(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
  this->popViewport();
  this->popScissor();
  shader->end(framedata);

  PopRtGroup();
}

///////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::rtGroupMipGen(RtGroup* rtg) {
  auto as_impl = rtg->_impl.tryAs<glrtgroupimpl_ptr_t>();
  if (as_impl) {
    int inumtargets = rtg->GetNumTargets();
    for (int it = 0; it < inumtargets; it++) {
      auto b = rtg->buffer(it);
      if (b) {
        auto bufferimpl = b->_impl.get<GlRtBufferImpl*>();
        auto glto       = bufferimpl->_teximpl.get<gltexobj_ptr_t>();
        GLuint tex_obj  = glto->_textureObject;
        if (b->_mipgen == RtBuffer::EMG_AUTOCOMPUTE) {
          GL_ERRORCHECK();
          glBindTexture(GL_TEXTURE_2D, tex_obj);
          glGenerateMipmap(GL_TEXTURE_2D);
          b->texture()->TexSamplingMode().presetPointAndClamp();
          mTargetGL.TXI()->ApplySamplingMode(b->texture());
          GL_ERRORCHECK();
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::msaaBlit(rtgroup_ptr_t src, rtgroup_ptr_t dst) {
  dst->Resize(src->width(), src->height());
  PushRtGroup(dst.get());
  auto src_rtb       = src->buffer(0);
  auto dst_rtb       = dst->buffer(0);
  auto src_groupimpl = src->_impl.get<glrtgroupimpl_ptr_t>();
  auto dst_groupimpl = dst->_impl.get<glrtgroupimpl_ptr_t>();

  // auto src_bufferimpl = src_rtb->_impl.get<GlRtBufferImpl*>();
  // auto dst_bufferimpl = dst_rtb->_impl.get<GlRtBufferImpl*>();
  // GLuint src_texture    = src_glto->_textureObject;
  int numsamples = msaaEnumToInt(src->_msaa_samples);
  OrkAssert(numsamples != 1);
  GL_ERRORCHECK();
  glBindFramebuffer(GL_READ_FRAMEBUFFER, src_groupimpl->_standard->_fbo);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_groupimpl->_standard->_fbo);
  glBlitFramebuffer(
      0,
      0,
      src->width(),
      src->height(), //
      0,
      0,
      dst->width(),
      dst->height(), //
      GL_COLOR_BUFFER_BIT,
      GL_LINEAR);
  GL_ERRORCHECK();
  PopRtGroup();
}

////////////////////////////////////////////////////////////////////////////////

void GlFrameBufferInterface::blit(rtgroup_ptr_t src, rtgroup_ptr_t dst) {

  int w = dst->width();
  int h = dst->height();

  auto framedata = mTargetGL.topRenderContextFrameData();

  // dst->Resize(w,h);
  PushRtGroup(dst.get());
  auto src_rtb       = src->buffer(0);
  auto dst_rtb       = dst->buffer(0);
  auto src_groupimpl = src->_impl.get<glrtgroupimpl_ptr_t>();
  auto dst_groupimpl = dst->_impl.get<glrtgroupimpl_ptr_t>();
  auto this_buf      = this->GetThisBuffer();

  auto shader = utilshader();

  shader->begin(_tek_blit, framedata);
  shader->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
  shader->bindParamTexture(_fxpColorMap, src->texture(0).get());
  shader->bindParamMatrix(_fxpMVP, fmtx4::Identity());
  ViewportRect extents(0, 0, w, h);
  this->pushViewport(extents);
  this->pushScissor(extents);
  mTargetGL.FXI()->applyRasterState(*(shader->_rasterstate));
  this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
  this->popViewport();
  this->popScissor();
  shader->end(framedata);

  PopRtGroup();
}

} //namespace ork::lev2 {
