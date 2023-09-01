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

VulkanVertexBuffer::VulkanVertexBuffer(vkcontext_rawptr_t ctx, size_t length) {
  _ctx                   = ctx;
  initializeVkStruct(_vkmem);
  initializeVkStruct(_vkbuf);

  printf( "length<%zu>\n", length );

  initializeVkStruct(_vkbufinfo,VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
  _vkbufinfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  _vkbufinfo.size        = length;
  _vkbufinfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  _vkbufinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VkResult ok = vkCreateBuffer(ctx->_vkdevice, &_vkbufinfo, nullptr, &_vkbuf);
  OrkAssert( ok == VK_SUCCESS );
  //////////////////
  VkMemoryRequirements memRequirements;
  initializeVkStruct(memRequirements);
  vkGetBufferMemoryRequirements(ctx->_vkdevice, _vkbuf, &memRequirements);
  printf( "alignment<%zu>\n", memRequirements.alignment );
  //////////////////
  VkMemoryAllocateInfo allocInfo = {};
  initializeVkStruct(allocInfo,VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
  allocInfo.allocationSize       = memRequirements.size;
  //////////////////
  _vkmemflags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 
              | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // do not need flush...
  //////////////////
  allocInfo.memoryTypeIndex = ctx->_findMemoryType(memRequirements.memoryTypeBits, _vkmemflags);
  printf( "memtypeindex = %u\n", allocInfo.memoryTypeIndex );
  //////////////////

  vkAllocateMemory(ctx->_vkdevice, &allocInfo, nullptr, &_vkmem);
  vkBindBufferMemory(ctx->_vkdevice, _vkbuf, _vkmem, 0);
}
VulkanVertexBuffer::~VulkanVertexBuffer() {
  vkFreeMemory(_ctx->_vkdevice, _vkmem, nullptr);
  vkDestroyBuffer(_ctx->_vkdevice, _vkbuf, nullptr);
}

///////////////////////////////////////////////////////////////////////////////

VulkanIndexBuffer::VulkanIndexBuffer(vkcontext_rawptr_t ctx, size_t length) {
  _ctx                   = ctx;

  initializeVkStruct(_vkbufinfo,VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
  _vkbufinfo.size        = length;
  _vkbufinfo.usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  _vkbufinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vkCreateBuffer(ctx->_vkdevice, &_vkbufinfo, nullptr, &_vkbuf);
  //////////////////
  VkMemoryRequirements memRequirements;
  initializeVkStruct(memRequirements);
  vkGetBufferMemoryRequirements(ctx->_vkdevice, _vkbuf, &memRequirements);
  //////////////////
  VkMemoryAllocateInfo allocInfo = {};
  initializeVkStruct(allocInfo,VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
  allocInfo.allocationSize       = memRequirements.size;
  //////////////////
  _vkmemflags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 
              | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // do not need flush...
  //////////////////
  allocInfo.memoryTypeIndex = ctx->_findMemoryType(memRequirements.memoryTypeBits, _vkmemflags);
  //////////////////
  vkAllocateMemory(ctx->_vkdevice, &allocInfo, nullptr, &_vkmem);
  vkBindBufferMemory(ctx->_vkdevice, _vkbuf, _vkmem, 0);
}
///////////////////////////////////////////////////////////////////////////////
VulkanIndexBuffer::~VulkanIndexBuffer() {
  vkFreeMemory(_ctx->_vkdevice, _vkmem, nullptr);
  vkDestroyBuffer(_ctx->_vkdevice, _vkbuf, nullptr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VkGeometryBufferInterface::VkGeometryBufferInterface(vkcontext_rawptr_t ctx)
    : GeometryBufferInterface(*ctx)
    , _contextVK(ctx) {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void* VkGeometryBufferInterface::LockVB(VertexBufferBase& vtx_buf, int ivbase, int ivcount) {
  _contextVK->makeCurrentContext(); // TODO probably dont need this with queues
  OrkAssert(false == vtx_buf.IsLocked());
  size_t ibasebytes = ivbase * vtx_buf.GetVtxSize();
  size_t isizebytes = ivcount * vtx_buf.GetVtxSize();
  //////////////////////////////////////////////////////////
  // create or reference the vbo
  //////////////////////////////////////////////////////////
  vkvtxbuf_ptr_t vk_impl;
  if (auto try_vk_impl = vtx_buf._impl.tryAsShared<VulkanVertexBuffer>()) {
    vk_impl = try_vk_impl.value();
  } else {
    vk_impl = std::make_shared<VulkanVertexBuffer>(_contextVK, isizebytes);
    vtx_buf._impl.setShared(vk_impl);
  }
  void* rval = nullptr;
  vkMapMemory( _contextVK->_vkdevice, //
               vk_impl->_vkmem, // 
               ibasebytes, // 
               isizebytes, // 
               0, // 
               &rval);
  //////////////////////////////////////////////////////////
  vtx_buf.Lock();
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void VkGeometryBufferInterface::UnLockVB(VertexBufferBase& vtx_buf) {
  auto vk_impl = vtx_buf._impl.getShared<VulkanVertexBuffer>();
  OrkAssert(vtx_buf.IsLocked());
  vkUnmapMemory(_contextVK->_vkdevice, vk_impl->_vkmem);
  vtx_buf.Unlock();
}

///////////////////////////////////////////////////////////////////////////////

const void* VkGeometryBufferInterface::LockVB(const VertexBufferBase& vtx_buf, int ivbase, int ivcount) {
  auto& mutable_vtx_buf = const_cast<VertexBufferBase&>(vtx_buf);
  return LockVB(mutable_vtx_buf, ivbase, ivcount);
}

///////////////////////////////////////////////////////////////////////////////

void VkGeometryBufferInterface::UnLockVB(const VertexBufferBase& vtx_buf) {
  auto& mutable_vtx_buf = const_cast<VertexBufferBase&>(vtx_buf);
  UnLockVB(mutable_vtx_buf);
}

///////////////////////////////////////////////////////////////////////////////

void VkGeometryBufferInterface::ReleaseVB(VertexBufferBase& vtx_buf) {
  auto vk_impl = vtx_buf._impl.getShared<VulkanVertexBuffer>();

  if (vk_impl) {
    vtx_buf._impl = buffer_impl_t();
  }
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void* VkGeometryBufferInterface::LockIB(IndexBufferBase& idx_buf, int ivbase, int icount) {
  _contextVK->makeCurrentContext(); // TODO probably dont need this with queues

  //////////////////////////////////////////////////////////
  // create or reference the ibo
  //////////////////////////////////////////////////////////

  vkidxbuf_ptr_t vk_buf;
  if (auto try_vk_buf = idx_buf._impl.tryAsShared<VulkanIndexBuffer>()) {
    vk_buf = try_vk_buf.value();
  } else {
    size_t size_in_bytes = icount * idx_buf.GetIndexSize();
    vk_buf               = std::make_shared<VulkanIndexBuffer>(_contextVK, size_in_bytes);
    idx_buf._impl.setShared(vk_buf);
  }
  OrkAssert(false);
  return nullptr;
}
void VkGeometryBufferInterface::UnLockIB(IndexBufferBase& idx_buf) {
  auto vk_buf = idx_buf._impl.getShared<VulkanIndexBuffer>();
  // OrkAssert(idx_buf.IsLocked());
  OrkAssert(false);
}

const void* VkGeometryBufferInterface::LockIB(const IndexBufferBase& idx_buf, int ibase, int icount) {
  OrkAssert(false);
  return nullptr;
}
void VkGeometryBufferInterface::UnLockIB(const IndexBufferBase& idx_buf) {
  auto vk_buf = idx_buf._impl.getShared<VulkanIndexBuffer>();
  // OrkAssert(idx_buf.IsLocked());
  OrkAssert(false);
}

void VkGeometryBufferInterface::ReleaseIB(IndexBufferBase& idx_buf) {
  auto vk_buf = idx_buf._impl.getShared<VulkanIndexBuffer>();

  if (vk_buf) {
    idx_buf._impl.clear();
  }
  OrkAssert(false);
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool VkGeometryBufferInterface::BindStreamSources(const VertexBufferBase& vtx_buf, const IndexBufferBase& IBuf) {
  OrkAssert(false);
  auto vk_impl = vtx_buf._impl.getShared<VulkanVertexBuffer>();
  //vkCmdBindVertexBuffers(cmd_buffer, 0, 1, &vk_impl->_vkbuf, offsets);
  return false;
}
bool VkGeometryBufferInterface::BindVertexStreamSource(const VertexBufferBase& vtx_buf) {
  OrkAssert(false);
  return false;
}
void VkGeometryBufferInterface::BindVertexDeclaration(EVtxStreamFormat efmt) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void VkGeometryBufferInterface::DrawPrimitiveEML(
    const VertexBufferBase& vtx_buf, //
    PrimitiveType eType,
    int ivbase,
    int ivcount) {
  OrkAssert(false);
}

#if defined(ENABLE_COMPUTE_SHADERS)
void VkGeometryBufferInterface::DrawPrimitiveEML(
    const FxShaderStorageBuffer* SSBO, //
    PrimitiveType eType,
    int ivbase,
    int ivcount) {
  OrkAssert(false);
}
#endif

void VkGeometryBufferInterface::DrawIndexedPrimitiveEML(
    const VertexBufferBase& vtx_buf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    int ivbase,
    int ivcount) {
  OrkAssert(false);
}

void VkGeometryBufferInterface::DrawInstancedIndexedPrimitiveEML(
    const VertexBufferBase& vtx_buf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    size_t instance_count) {
  OrkAssert(false);
}

//////////////////////////////////////////////
// nvidia mesh shaders
//////////////////////////////////////////////

#if defined(ENABLE_NVMESH_SHADERS)
void VkGeometryBufferInterface::DrawMeshTasksNV(uint32_t first, uint32_t count) {
  OrkAssert(false);
}
void VkGeometryBufferInterface::DrawMeshTasksIndirectNV(int32_t* indirect) {
  OrkAssert(false);
}
void VkGeometryBufferInterface::MultiDrawMeshTasksIndirectNV(int32_t* indirect, uint32_t drawcount, uint32_t stride) {
  OrkAssert(false);
}
void VkGeometryBufferInterface::MultiDrawMeshTasksIndirectCountNV(
    int32_t* indirect,
    int32_t* drawcount,
    uint32_t maxdrawcount,
    uint32_t stride) {
  OrkAssert(false);
}
#endif

//////////////////////////////////////////////

void VkGeometryBufferInterface::_doBeginFrame() {
  //OrkAssert(false);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
