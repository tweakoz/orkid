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

void VkFrameBufferInterface::_setAsRenderTarget() { // _main_rtg
  _currentRtGroup = _main_rtg.get();
  _active_rtgroup = _main_rtg.get();
}

///////////////////////////////////////////////////////////////////////////////

void VkContext::_doResizeMainSurface(int iw, int ih) {
    _fbi->_main_rtg->Resize(iw, ih);
}

///////////////////////////////////////////////////////////////////////////////

vkfbobj_ptr_t VkFrameBufferInterface::_createRtGroupImpl(RtGroup* rtg) {
  vkfbobj_ptr_t FBOIMPL = std::make_shared<VkFboObject>();
  FBOIMPL->_width       = rtg->width();
  FBOIMPL->_height      = rtg->height();
  return FBOIMPL;
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
  vkfbobj_ptr_t FBOIMPL;
  if (auto as_impl = _active_rtgroup->_impl.tryAsShared<VkFboObject>()) {
    FBOIMPL = as_impl.value();
  } else {
    FBOIMPL = _createRtGroupImpl(_active_rtgroup);
    _active_rtgroup->_impl.setShared<VkFboObject>(FBOIMPL);
  }
  /////////////////////////////////////////
  int fbow       = FBOIMPL->_width;
  int fboh       = FBOIMPL->_height;
  int rtgw       = _active_rtgroup->width();
  int rtgh       = _active_rtgroup->height();
  bool size_diff = (rtgw != fbow) || (rtgh != fboh);
  if (size_diff) {
    logchan_rtgroup->log("resize FBO iw<%d> ih<%d>", iw, ih);
    FBOIMPL = _createRtGroupImpl(_active_rtgroup);
    _active_rtgroup->_impl.setShared<VkFboObject>(FBOIMPL);
    _active_rtgroup->SetSizeDirty(false);
  }
  /////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
