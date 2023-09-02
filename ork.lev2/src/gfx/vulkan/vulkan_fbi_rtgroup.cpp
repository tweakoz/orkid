////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_rtgroup = logger()->createChannel("VKRTG", fvec3(0.8, 0.2, 0.5), true);

///////////////////////////////////////////////////////////////////////////////

VklRtBufferImpl::VklRtBufferImpl(VkRtGroupImpl* par, RtBuffer* rtb) //
  : _parent(par)
  , _rtb(rtb) { //
}
VkRtGroupImpl::VkRtGroupImpl(RtGroup* rtg) : _rtg(rtg) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::_setAsRenderTarget() { // _main_rtg
  _currentRtGroup = _main_rtg.get();
  _active_rtgroup = _main_rtg.get();
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doResizeMainSurface(int iw, int ih) {
  _fbi->_main_rtg->Resize(iw, ih);
}

///////////////////////////////////////////////////////////////////////////////

vkrtgrpimpl_ptr_t VkFrameBufferInterface::_createRtGroupImpl(RtGroup* rtgroup) {
  vkrtgrpimpl_ptr_t RTGIMPL = std::make_shared<VkRtGroupImpl>(rtgroup);
  RTGIMPL->_width           = rtgroup->width();
  RTGIMPL->_height          = rtgroup->height();
  if (rtgroup->_needsDepth) {

  } else {
    // RTGIMPL->_depthbuffer = nullptr;
  }
  int inumtargets = rtgroup->GetNumTargets();
  for (int it = 0; it < inumtargets; it++) {
    rtbuffer_ptr_t rtbuffer = rtgroup->GetMrt(it);
    auto bufferimpl = std::make_shared<VklRtBufferImpl>(RTGIMPL.get(), rtbuffer.get());
    rtbuffer->_impl.setShared<VklRtBufferImpl>(bufferimpl);
  }

  return RTGIMPL;
}

///////////////////////////////////////////////////////////////////////////////

void VkFrameBufferInterface::SetRtGroup(RtGroup* rtgroup) {
  auto prev_rtgroup = _active_rtgroup;
  if (nullptr == rtgroup) {
    _setAsRenderTarget();
  } else {
    _active_rtgroup = rtgroup;
  }
  OrkAssert(_active_rtgroup);
  int iw = _active_rtgroup->width();
  int ih = _active_rtgroup->height();
  /////////////////////////////////////////
  if (_active_rtgroup->_pseudoRTG) {
    static const SRasterState defstate;
    //_target.RSI()->BindRasterState(defstate, true);
    //_currentRtGroup = rtgroup;
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);
    // if( rtgroup->_autoclear ){
    // rtGroupClear(rtgroup);
    //}
    return;
  }
  /////////////////////////////////////////
  int inumtargets = _active_rtgroup->GetNumTargets();
  int numsamples  = msaaEnumToInt(_active_rtgroup->_msaa_samples);
  // printf( "inumtargets<%d> numsamples<%d>\n", inumtargets, numsamples );
  // auto texture_target_2D = (numsamples==1) ? GL_TEXTURE_2D : GL_TEXTURE_2D_MULTISAMPLE;
  vkrtgrpimpl_ptr_t RTGIMPL;
  if (auto as_impl = _active_rtgroup->_impl.tryAsShared<VkRtGroupImpl>()) {
    RTGIMPL = as_impl.value();
  } else {
    RTGIMPL = _createRtGroupImpl(_active_rtgroup);
    _active_rtgroup->_impl.setShared<VkRtGroupImpl>(RTGIMPL);
  }
  /////////////////////////////////////////
  int implw      = RTGIMPL->_width;
  int implh      = RTGIMPL->_height;
  int rtgw       = _active_rtgroup->width();
  int rtgh       = _active_rtgroup->height();
  bool size_diff = (rtgw != implw) || (rtgh != implh);
  if (size_diff) {
    logchan_rtgroup->log("resize FBO iw<%d> ih<%d>", iw, ih);
    RTGIMPL = _createRtGroupImpl(_active_rtgroup);
    _active_rtgroup->_impl.setShared<VkRtGroupImpl>(RTGIMPL);
    _active_rtgroup->SetSizeDirty(false);
  }
  /////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
