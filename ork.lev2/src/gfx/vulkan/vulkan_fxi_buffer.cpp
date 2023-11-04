////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

FxShaderParamBuffer* VkFxInterface::createParamBuffer(size_t length) {
  auto pbuf = new FxShaderParamBuffer;
  auto uniblk_buf = pbuf->_impl.makeShared<VulkanBuffer>(_contextVK, length, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  return pbuf;
}

///////////////////////////////////////////////////////////////////////////////

parambuffermappingptr_t VkFxInterface::mapParamBuffer(FxShaderParamBuffer* b, //
                                                      size_t base, //
                                                      size_t length) { //

  auto bufimpl = b->_impl.getShared<VulkanBuffer>();
  auto mapping = std::make_shared<FxShaderParamBufferMapping>();
  mapping->_buffer = b;
  mapping->_offset   = base;
  if(length==0){
    mapping->_length = bufimpl->_length;
  }
  else{
    mapping->_length   = length;
  }
  mapping->_mappedaddr = bufimpl->map(base, mapping->_length,0);
  return mapping;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::unmapParamBuffer(parambuffermappingptr_t mapping) {
  auto bufimpl = mapping->_buffer->_impl.getShared<VulkanBuffer>();
  bufimpl->unmap();
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////