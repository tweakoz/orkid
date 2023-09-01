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

VkGeometryBufferInterface::VkGeometryBufferInterface(vkcontext_rawptr_t ctx)
    : GeometryBufferInterface(*ctx)
    , _contextVK(ctx) {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void* VkGeometryBufferInterface::LockVB(VertexBufferBase& vtx_buf, int ivbase, int icount) {
  _contextVK->makeCurrentContext(); // TODO probably dont need this with queues
  OrkAssert(false == vtx_buf.IsLocked());
  //////////////////////////////////////////////////////////
  // create or reference the vbo
  //////////////////////////////////////////////////////////

  vkvtxbuf_ptr_t vk_buf;
  if (auto try_vk_buf = vtx_buf._impl.tryAsShared<VulkanVertexBuffer>()) {
    vk_buf = try_vk_buf.value();
  } else {
    vk_buf = std::make_shared<VulkanVertexBuffer>();
    // vk_buf->CreateVbo(vtx_buf);
    vtx_buf._impl.setShared(vk_buf);
  }
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void VkGeometryBufferInterface::UnLockVB(VertexBufferBase& vtx_buf) {
  auto vk_buf = vtx_buf._impl.getShared<VulkanVertexBuffer>();
  OrkAssert(vtx_buf.IsLocked());
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////

const void* VkGeometryBufferInterface::LockVB(const VertexBufferBase& vtx_buf, int ivbase, int icount) {
  _contextVK->makeCurrentContext(); // TODO probably dont need this with queues
  auto vk_buf = vtx_buf._impl.getShared<VulkanVertexBuffer>();
  OrkAssert(false == vtx_buf.IsLocked());
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void VkGeometryBufferInterface::UnLockVB(const VertexBufferBase& vtx_buf) {
  auto vk_buf = vtx_buf._impl.getShared<VulkanVertexBuffer>();
  OrkAssert(vtx_buf.IsLocked());
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////

void VkGeometryBufferInterface::ReleaseVB(VertexBufferBase& vtx_buf) {
  auto vk_buf = vtx_buf._impl.getShared<VulkanVertexBuffer>();

  if (vk_buf) {
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
    vk_buf = std::make_shared<VulkanIndexBuffer>();
    idx_buf._impl.setShared(vk_buf);
  }
  OrkAssert(false);
  return nullptr;
}
void VkGeometryBufferInterface::UnLockIB(IndexBufferBase& idx_buf) {
  auto vk_buf = idx_buf._impl.getShared<VulkanIndexBuffer>();
  //OrkAssert(idx_buf.IsLocked());
  OrkAssert(false);
}

const void* VkGeometryBufferInterface::LockIB(const IndexBufferBase& idx_buf, int ibase, int icount) {
  OrkAssert(false);
  return nullptr;
}
void VkGeometryBufferInterface::UnLockIB(const IndexBufferBase& idx_buf) {
  auto vk_buf = idx_buf._impl.getShared<VulkanIndexBuffer>();
  //OrkAssert(idx_buf.IsLocked());
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

bool VkGeometryBufferInterface::BindStreamSources(const VertexBufferBase& VBuf, const IndexBufferBase& IBuf) {
  OrkAssert(false);
  return false;
}
bool VkGeometryBufferInterface::BindVertexStreamSource(const VertexBufferBase& VBuf) {
  OrkAssert(false);
  return false;
}
void VkGeometryBufferInterface::BindVertexDeclaration(EVtxStreamFormat efmt) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void VkGeometryBufferInterface::DrawPrimitiveEML(
    const VertexBufferBase& VBuf, //
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
    const VertexBufferBase& VBuf,
    const IndexBufferBase& IdxBuf,
    PrimitiveType eType,
    int ivbase,
    int ivcount) {
  OrkAssert(false);
}

void VkGeometryBufferInterface::DrawInstancedIndexedPrimitiveEML(
    const VertexBufferBase& VBuf,
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
  OrkAssert(false);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
