////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VulkanRenderPass::VulkanRenderPass(vkcontext_rawptr_t ctxVK, RenderPass* rpass)
    : _contextVK(ctxVK) {
  // topological sort of renderpass's subpasses
  //  to determine execution order

  static size_t counter = 0;

  _seccmdbuffer             = std::make_shared<CommandBuffer>();
  _seccmdbuffer->_debugName = FormatString("renderpass cb<%d>\n", counter);

  if (counter == 6) {
    // OrkAssert(false);
  }

  counter++;

  auto vkcmdbuf = _contextVK->_createVkCommandBuffer(_seccmdbuffer.get());

  std::set<rendersubpass_ptr_t> subpass_set;

  std::function<void(rendersubpass_ptr_t)> visit_subpass;

  visit_subpass = [&](rendersubpass_ptr_t subp) {
    for (auto dep : subp->_subpass_dependencies) {
      if (subpass_set.find(dep) == subpass_set.end()) {
        subpass_set.insert(dep);
        visit_subpass(dep);
      }
    }
    _toposorted_subpasses.push_back(subp.get());
  };

  // visit top
  for (auto subp : rpass->_subpasses) {
    visit_subpass(subp);
  }
}

///////////////////////////////////////////////////

VulkanRenderPass::~VulkanRenderPass() {
  if (_vkrp) {
    // printf( "DESTROY RENDERPASS<%p>\n", (void*) _vkrp );
    vkDestroyRenderPass(_contextVK->_vkdevice, _vkrp, nullptr);
  }
  if (_vkfb) {
    // printf( "DESTROY FRAMEBUFFER<%p>\n", (void*) _vkfb );
    vkDestroyFramebuffer(_contextVK->_vkdevice, _vkfb, nullptr);
  }
}

///////////////////////////////////////////////////////////////////////////////

vksubpass_ptr_t createSubPass(bool has_depth) {
  vksubpass_ptr_t subpass = std::make_shared<VulkanRenderSubPass>();

  subpass->_attach_refs.reserve(2);
  auto& CATR = subpass->_attach_refs.emplace_back();
  initializeVkStruct(CATR);
  CATR.attachment = 0;

  if (has_depth) {
    auto& DATR = subpass->_attach_refs.emplace_back();
    initializeVkStruct(DATR);
    DATR.attachment = 1;
  }

  initializeVkStruct(subpass->_SUBPASS);
  subpass->_SUBPASS.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass->_SUBPASS.colorAttachmentCount = 1;
  subpass->_SUBPASS.pColorAttachments    = &CATR;
  subpass->_SUBPASS.colorAttachmentCount = 1;
  if (has_depth) {
    auto& DATR                                = subpass->_attach_refs.back();
    subpass->_SUBPASS.pDepthStencilAttachment = &DATR;
  }

  return subpass;
}

///////////////////////////////////////////////////////

renderpass_ptr_t VkContext::createRenderPassForRtGroup(RtGroup* rtg, bool clear, std::string name) {
  auto rtg_impl       = rtg->_impl.getShared<VkRtGroupImpl>();
  auto renpass        = std::make_shared<RenderPass>();
  renpass->_debugName = name;
  auto vk_renpass     = renpass->_impl.makeShared<VulkanRenderPass>(this, renpass.get());
  auto color_rtb      = rtg->GetMrt(0);
  auto color_rtbi     = color_rtb->_impl.getShared<VklRtBufferImpl>();
  auto depth_rtb      = rtg->_depthBuffer;
  bool has_depth      = (depth_rtb != nullptr);
  /////////////////////////////////////////////
  color_rtbi->setLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  if (has_depth) {
    auto depth_rtbi = depth_rtb->_impl.getShared<VklRtBufferImpl>();
    depth_rtbi->setLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  }
  /////////////////////////////////////////////
  int num_rtb = rtg->GetNumTargets();
  for (int i = 0; i < num_rtb; i++) {
    auto rtb                     = rtg->GetMrt(i);
    auto rtbi                    = rtb->_impl.getShared<VklRtBufferImpl>();
    rtbi->_attachmentDesc.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;

    if (clear and rtbi->_is_surface) {
      OrkAssert(rtbi->_attachmentDesc.loadOp == VK_ATTACHMENT_LOAD_OP_CLEAR);
    }

    // rtbi->setLayout( VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL );
  }
  /////////////////////////////////////////////
  if (rtg->_depthBuffer) {
    auto rtb                     = rtg->_depthBuffer;
    auto rtbi                    = rtb->_impl.getShared<VklRtBufferImpl>();
    rtbi->_attachmentDesc.loadOp = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  }
  /////////////////////////////////////////////
  auto subpass                    = createSubPass(has_depth);
  subpass->_attach_refs[0].layout = color_rtbi->_currentLayout;
  if (has_depth) {
    auto depth_rtbi                 = depth_rtb->_impl.getShared<VklRtBufferImpl>();
    subpass->_attach_refs[1].layout = depth_rtbi->_currentLayout;
  }
  /////////////////////////////////////////////
  auto attachments = rtg_impl->attachments();
  /////////////////////////////////////////////
  VkRenderPassCreateInfo RPI = {};
  initializeVkStruct(RPI, VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
  RPI.attachmentCount = attachments->_descriptions.size();
  RPI.pAttachments    = attachments->_descriptions.data();
  RPI.subpassCount    = 1;
  RPI.pSubpasses      = &subpass->_SUBPASS;
  /////////////////////////////////////////////
  initializeVkStruct(rtg_impl->_vksubpassdeps);
  rtg_impl->_vksubpassdeps.srcSubpass      = 0; // The index of the subpass in which the barrier is used
  rtg_impl->_vksubpassdeps.dstSubpass      = 0; // The same subpass as srcSubpass for a self-dependency
  rtg_impl->_vksubpassdeps.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; // Adjust as needed
  rtg_impl->_vksubpassdeps.dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;         // Adjust as needed
  rtg_impl->_vksubpassdeps.srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;          // Adjust as needed
  rtg_impl->_vksubpassdeps.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;                     // Adjust as needed
  rtg_impl->_vksubpassdeps.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  // RPI.dependencyCount = 1;
  // RPI.pDependencies = &rtg_impl->_vksubpassdeps;

  VkResult OK = vkCreateRenderPass(_vkdevice, &RPI, nullptr, &vk_renpass->_vkrp);
  OrkAssert(OK == VK_SUCCESS);

  for (auto item : attachments->_imageviews) {
    if (item == VK_NULL_HANDLE) {
      printf("rtg<%s> has null imageview\n", rtg->_name.c_str());
    }
    OrkAssert(item != VK_NULL_HANDLE);
  }

  initializeVkStruct(vk_renpass->_vkfbinfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
  vk_renpass->_vkfbinfo.attachmentCount = attachments->_imageviews.size();
  vk_renpass->_vkfbinfo.pAttachments    = attachments->_imageviews.data();
  vk_renpass->_vkfbinfo.width           = rtg_impl->_width;
  vk_renpass->_vkfbinfo.height          = rtg_impl->_height;
  vk_renpass->_vkfbinfo.layers          = 1;
  vk_renpass->_vkfbinfo.renderPass      = vk_renpass->_vkrp;

  vkCreateFramebuffer(
      _vkdevice,              // device
      &vk_renpass->_vkfbinfo, // pCreateInfo
      nullptr,                // pAllocator
      &vk_renpass->_vkfb);    // pFramebuffer

  return renpass;
}

///////////////////////////////////////////////////////

void VkContext::_beginRenderPass(renderpass_ptr_t renpass) {

  auto rtg      = _fbi->_active_rtgroup;
  auto rtg_impl = rtg->_impl.getShared<VkRtGroupImpl>();
  auto vk_rpass = renpass->_impl.getShared<VulkanRenderPass>();

  VkRenderPassBeginInfo RPBI = {};
  initializeVkStruct(RPBI, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);

  /////////////////////////////////////////
  // perform the clear ?
  /////////////////////////////////////////

  std::vector<VkClearValue> clearValues;

  if (rtg->_autoclear and renpass->_allow_clear) {
    auto color = rtg->_clearColor;

    clearValues.reserve(2);
    clearValues.emplace_back().color        = {{color.x, color.y, color.z, color.w}};
    clearValues.emplace_back().depthStencil = {1.0f, 0};
    //  clear-rect-region
    RPBI.renderArea.offset = {0, 0};
    RPBI.renderArea.extent = {uint32_t(rtg->width()), uint32_t(rtg->height())};
    //  clear-targets
    RPBI.clearValueCount = clearValues.size();
    RPBI.pClearValues    = clearValues.data();
  }

  /////////////////////////////////////////
  // misc renderpass
  /////////////////////////////////////////

  RPBI.renderPass  = vk_rpass->_vkrp;
  RPBI.framebuffer = vk_rpass->_vkfb;

  /////////////////////////////////////////
  // Renderpass !
  /////////////////////////////////////////
  debugPushGroup(_defaultCommandBuffer, renpass->_debugName, fvec4(1, 1, 0, 1));
  vkCmdBeginRenderPass(
      primary_cb()->_vkcmdbuf,                        // must be on primary!
      &RPBI,                                          //
      VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS); // The render pass commands are recorded into secondary command buffers.
  /////////////////////////////////////////
  _renderpass_index++;

  _cur_renderpass = renpass;

  /////////////////////////////////////////
  // any state scoped to renderpasses
  //  commandbuffer scope need to be
  //  re-bound...
  //  so invalidate state caches..
  /////////////////////////////////////////

  _fxi->_flushRenderPassScopedState();
}

///////////////////////////////////////////////////////

void VkContext::_endRenderPass(renderpass_ptr_t renpass) {
  vkCmdEndRenderPass(primary_cb()->_vkcmdbuf); // must be on primary!
  debugPopGroup(_defaultCommandBuffer);
  _cur_renderpass = nullptr;
  // OrkAssert(false);
}

///////////////////////////////////////////////////////
// begin subpass recording
///////////////////////////////////////////////////////

void VkContext::_beginSubPass(rendersubpass_ptr_t subpass) {
  // OrkAssert(_current_subpass == nullptr); // no nesting...
  //_current_subpass = subpass;
  //_exec_subpasses.push_back(subpass);
  //  OrkAssert(false);
}

///////////////////////////////////////////////////////
// end subpass recording
///////////////////////////////////////////////////////

void VkContext::_endSubPass(rendersubpass_ptr_t subpass) {
  // OrkAssert(_current_subpass == subpass); // no nesting...
  //_current_subpass = nullptr;
}

///////////////////////////////////////////////////////
// begin subpass execution
///////////////////////////////////////////////////////

void VkContext::_beginExecuteSubPass(rendersubpass_ptr_t subpass) {
}

///////////////////////////////////////////////////////
// end subpass execution
///////////////////////////////////////////////////////

void VkContext::_endExecuteSubPass(rendersubpass_ptr_t subpass) {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
