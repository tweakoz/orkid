#include "vulkan_ctx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

  VkGeometryBufferInterface::VkGeometryBufferInterface(vkcontext_rawptr_t ctx)
      : GeometryBufferInterface(*ctx)
      , _contextVK(ctx) {
  }

  void* VkGeometryBufferInterface::LockVB(VertexBufferBase & VBuf, int ivbase, int icount) {
    return nullptr;
  }
  void VkGeometryBufferInterface::UnLockVB(VertexBufferBase & VBuf) {
  }

  const void* VkGeometryBufferInterface::LockVB(const VertexBufferBase& VBuf, int ivbase, int icount) {
    return nullptr;
  }
  void VkGeometryBufferInterface::UnLockVB(const VertexBufferBase& VBuf) {
  }

  void VkGeometryBufferInterface::ReleaseVB(VertexBufferBase & VBuf) {
  }

  //

  void* VkGeometryBufferInterface::LockIB(IndexBufferBase & VBuf, int ivbase, int icount) {
    return nullptr;
  }
  void VkGeometryBufferInterface::UnLockIB(IndexBufferBase & VBuf) {
  }

  const void* VkGeometryBufferInterface::LockIB(const IndexBufferBase& VBuf, int ibase, int icount) {
    return nullptr;
  }
  void VkGeometryBufferInterface::UnLockIB(const IndexBufferBase& VBuf) {
  }

  void VkGeometryBufferInterface::ReleaseIB(IndexBufferBase & VBuf) {
  }

  //

  bool VkGeometryBufferInterface::BindStreamSources(const VertexBufferBase& VBuf, const IndexBufferBase& IBuf){
    return false;
  }
  bool VkGeometryBufferInterface::BindVertexStreamSource(const VertexBufferBase& VBuf){
    return false;
  }
  void VkGeometryBufferInterface::BindVertexDeclaration(EVtxStreamFormat efmt){

  }

  void VkGeometryBufferInterface::DrawPrimitiveEML(
      const VertexBufferBase& VBuf, //
      PrimitiveType eType,
      int ivbase,
      int ivcount) {
  }

#if defined(ENABLE_COMPUTE_SHADERS)
  void VkGeometryBufferInterface::DrawPrimitiveEML(
      const FxShaderStorageBuffer* SSBO, //
      PrimitiveType eType,
      int ivbase,
      int ivcount) {
  }
#endif

  void VkGeometryBufferInterface::DrawIndexedPrimitiveEML(
      const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, PrimitiveType eType, int ivbase, int ivcount) {
  }

  void VkGeometryBufferInterface::DrawInstancedIndexedPrimitiveEML(
      const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, PrimitiveType eType, size_t instance_count) {
  }

  //////////////////////////////////////////////
  // nvidia mesh shaders
  //////////////////////////////////////////////

#if defined(ENABLE_NVMESH_SHADERS)
  void VkGeometryBufferInterface::DrawMeshTasksNV(uint32_t first, uint32_t count) {
  }
  void VkGeometryBufferInterface::DrawMeshTasksIndirectNV(int32_t * indirect) {
  }
  void VkGeometryBufferInterface::MultiDrawMeshTasksIndirectNV(int32_t * indirect, uint32_t drawcount, uint32_t stride) {
  }
  void VkGeometryBufferInterface::MultiDrawMeshTasksIndirectCountNV(
      int32_t * indirect, int32_t * drawcount, uint32_t maxdrawcount, uint32_t stride) {
  }
#endif

  //////////////////////////////////////////////

  void VkGeometryBufferInterface::_doBeginFrame() {

  }
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
