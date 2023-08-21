#include "vulkan_impl.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkFboObject::VkFboObject() {
}

///////////////////////////////////////////////////////////////////////////////

VkTextureAsyncTask::VkTextureAsyncTask() {
}

///////////////////////////////////////////////////////////////////////////////

VkTextureObject::VkTextureObject(vktxi_ptr_t txi) {
}

VkTextureObject::~VkTextureObject() {
}

///////////////////////////////////////////////////////////////////////////////

VkDrawingInterface::VkDrawingInterface(vkcontext_ptr_t ctx)
    : DrawingInterface(*ctx.get())
    , _contextVK(ctx) {
  }

  ///////////////////////////////////////////////////////////////////////////////

  VkImiInterface::VkImiInterface(vkcontext_ptr_t ctx)
      : ImmInterface(*ctx.get())
      , _contextVK(ctx) {
  }
  void VkImiInterface::_doBeginFrame() {
  }
  void VkImiInterface::_doEndFrame() {
  }

  ///////////////////////////////////////////////////////////////////////////////

  VkRasterStateInterface::VkRasterStateInterface(vkcontext_ptr_t ctx)
      : RasterStateInterface(*ctx.get())
      , _contextVK(ctx) {
  }
  void VkRasterStateInterface::BindRasterState(const SRasterState& rState, bool bForce) {
  }

  void VkRasterStateInterface::SetZWriteMask(bool bv) {
  }
  void VkRasterStateInterface::SetRGBAWriteMask(bool rgb, bool a) {
  }
  RGBAMask VkRasterStateInterface::SetRGBAWriteMask(const RGBAMask& newmask) {
    return RGBAMask();
  }
  void VkRasterStateInterface::SetBlending(Blending eVal) {
  }
  void VkRasterStateInterface::SetDepthTest(EDepthTest eVal) {
  }
  void VkRasterStateInterface::SetCullTest(ECullTest eVal) {
  }
  void VkRasterStateInterface::setScissorTest(EScissorTest eVal) {
  }

  ///////////////////////////////////////////////////////////////////////////////

  VkMatrixStackInterface::VkMatrixStackInterface(vkcontext_ptr_t ctx)
      : MatrixStackInterface(*ctx.get())
      , _contextVK(ctx) {
  }

  fmtx4 VkMatrixStackInterface::Ortho(float left, float right, float top, float bottom, float fnear, float ffar) {
    return fmtx4();
  }
  fmtx4 VkMatrixStackInterface::Frustum(float left, float right, float top, float bottom, float zn, float zf) {
    return fmtx4();
  }

  ///////////////////////////////////////////////////////////////////////

  VkGeometryBufferInterface::VkGeometryBufferInterface(vkcontext_ptr_t ctx)
      : GeometryBufferInterface(*ctx.get())
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
      PrimitiveType eType = PrimitiveType::NONE,
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
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
