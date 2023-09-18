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

VulkanVertexBuffer::VulkanVertexBuffer(vkcontext_rawptr_t ctx, VertexBufferBase& vtx_buf) {

  _ctx = ctx;

  /////////////////////////////////////
  // create vertex buffer object
  /////////////////////////////////////

  initializeVkStruct(_vkbufinfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
  _vkbufinfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  _vkbufinfo.size        = vtx_buf.GetVtxSize() * vtx_buf.GetMax();
  _vkbufinfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  _vkbufinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  initializeVkStruct(_vkbuf);
  VkResult ok = vkCreateBuffer(ctx->_vkdevice, &_vkbufinfo, nullptr, &_vkbuf);
  OrkAssert(ok == VK_SUCCESS);

  /////////////////////////////////////
  // allocate vertex memory
  /////////////////////////////////////

  VkMemoryRequirements memRequirements;
  VkMemoryAllocateInfo allocInfo = {};
  initializeVkStruct(memRequirements);
  initializeVkStruct(allocInfo, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
  vkGetBufferMemoryRequirements(ctx->_vkdevice, _vkbuf, &memRequirements);
  printf( "alignment<%zu>\n", memRequirements.alignment );
  allocInfo.allocationSize  = memRequirements.size;
  _vkmemflags               = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT //
                            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // do not need flush...
  allocInfo.memoryTypeIndex = ctx->_findMemoryType(memRequirements.memoryTypeBits, _vkmemflags);
  // printf( "memtypeindex = %u\n", allocInfo.memoryTypeIndex );
  initializeVkStruct(_vkmem);
  vkAllocateMemory(ctx->_vkdevice, &allocInfo, nullptr, &_vkmem);
  vkBindBufferMemory(ctx->_vkdevice, _vkbuf, _vkmem, 0);

  /////////////////////////////////////
  // find vertex input configuration
  /////////////////////////////////////

  auto vtx_format = vtx_buf.GetStreamFormat();
  auto it         = ctx->_gbi->_vertexInputConfigs.find(vtx_format);
  OrkAssert(it != ctx->_gbi->_vertexInputConfigs.end());
  _vertexConfig = it->second;
}

///////////////////////////////////////////////////////////////////////////////

VulkanVertexBuffer::~VulkanVertexBuffer() {
  vkFreeMemory(_ctx->_vkdevice, _vkmem, nullptr);
  vkDestroyBuffer(_ctx->_vkdevice, _vkbuf, nullptr);
}

///////////////////////////////////////////////////////////////////////////////

VulkanIndexBuffer::VulkanIndexBuffer(vkcontext_rawptr_t ctx, size_t length) {
  _ctx = ctx;

  initializeVkStruct(_vkbufinfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
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
  initializeVkStruct(allocInfo, VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
  allocInfo.allocationSize = memRequirements.size;
  //////////////////
  _vkmemflags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT; // do not need flush...
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

  _instantiateVertexConfig(EVtxStreamFormat::V12C4T16);
  _instantiateVertexConfig(EVtxStreamFormat::V12N12B12T8C4);
  ////////////////////////////////////////////////////////////////
  auto create_primclass = [&](PrimitiveType etype) -> vkprimclass_ptr_t {
    auto rval = std::make_shared<VkPrimitiveClass>();
    rval->_primtype = etype;
    rval->_pipeline_bits = _primclasses.size();
    initializeVkStruct(rval->_input_assembly_state, VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    switch(etype){
      case PrimitiveType::TRIANGLES:
        rval->_input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        break;
      case PrimitiveType::TRIANGLESTRIP:
        rval->_input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        break;
      case PrimitiveType::TRIANGLEFAN:
        rval->_input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        break;
      case PrimitiveType::LINES:
        rval->_input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        break;
      case PrimitiveType::LINESTRIP:
        rval->_input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        break;
      case PrimitiveType::POINTS:
        rval->_input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        break;
      default:
        OrkAssert(false);
        break;
    }
    rval->_input_assembly_state.primitiveRestartEnable = VK_FALSE;
    return rval;
  };
  ////////////////////////////////////////////////////////////////
  _primclasses[uint64_t(PrimitiveType::TRIANGLES)] = create_primclass(PrimitiveType::TRIANGLES);
  ////////////////////////////////////////////////////////////////
  OrkAssert( _primclasses.size() <= 16 ); // validate we only used 4 pipeline_bits
  ////////////////////////////////////////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

vkvertexinputconfig_ptr_t VkGeometryBufferInterface::_instantiateVertexConfig(EVtxStreamFormat format) {
  auto config                  = std::make_shared<VkVertexInputConfiguration>();
  config->_pipeline_bits = _vertexInputConfigs.size();
  OrkAssert(config->_pipeline_bits<=16); // validate we only used 4 pipeline_bits
  _vertexInputConfigs[format] = config;
  config->_binding_description = VkVertexInputBindingDescription{
      0, // binding
      0, // stride
      VK_VERTEX_INPUT_RATE_VERTEX,
  };
  switch (format) {
    case EVtxStreamFormat::V12N12B12T8C4: {
      config->_binding_description.stride = sizeof(SVtxV12N12B12T8C4);
      config->_attribute_descriptions = std::vector<VkVertexInputAttributeDescription>{
          VkVertexInputAttributeDescription{
              // V12
              0, // location
              0, // binding
              VK_FORMAT_R32G32B32_SFLOAT,
              0, // offset
          },
          VkVertexInputAttributeDescription{
              // N12
              1, // location
              0, // binding
              VK_FORMAT_R32G32B32_SFLOAT,
              12, // offset
          },
          VkVertexInputAttributeDescription{
              // B12
              2, // location
              0, // binding
              VK_FORMAT_R32G32B32_SFLOAT,
              24, // offset
          },
          VkVertexInputAttributeDescription{
              // T8
              3, // location
              0, // binding
              VK_FORMAT_R32G32_SFLOAT,
              36, // offset
          },
          VkVertexInputAttributeDescription{
              // C4
              4, // location
              0, // binding
              VK_FORMAT_R8G8B8A8_UNORM,
              44, // offset
          },
      };
      break;
    }
    case EVtxStreamFormat::V12C4T16: {
      static_assert(sizeof(SVtxV12C4T16)==32);
      config->_binding_description.stride = sizeof(SVtxV12C4T16);
      config->_attribute_descriptions = std::vector<VkVertexInputAttributeDescription>{
          VkVertexInputAttributeDescription{
              // V12
              0, // location
              0, // binding
              VK_FORMAT_R32G32B32_SFLOAT,
              0, // offset
          },
          VkVertexInputAttributeDescription{
              // C4
              1, // location
              0, // binding
              VK_FORMAT_R8G8B8A8_UNORM,
              12, // offset
          },
          VkVertexInputAttributeDescription{
              // T16
              2, // location
              0, // binding
              VK_FORMAT_R32G32B32A32_SFLOAT,
              16, // offset
          }
      };
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
  VkPipelineVertexInputStateCreateInfo& vis = config->_vertex_input_state;
  initializeVkStruct(vis, VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
  vis.vertexBindingDescriptionCount   = 1;
  vis.pVertexBindingDescriptions      = &config->_binding_description;
  vis.vertexAttributeDescriptionCount = config->_attribute_descriptions.size();
  vis.pVertexAttributeDescriptions    = config->_attribute_descriptions.data();
  return config;
}

///////////////////////////////////////////////////////////////////////////////

void* VkGeometryBufferInterface::LockVB(VertexBufferBase& vtx_buf, int ivbase, int ivcount) {
  _contextVK->makeCurrentContext(); // TODO probably dont need this with queues
  OrkAssert(false == vtx_buf.IsLocked());
  size_t ibasebytes = ivbase * vtx_buf.GetVtxSize();
  size_t isizebytes = ivcount * vtx_buf.GetVtxSize();
  bool is_static    = vtx_buf.IsStatic();
  //////////////////////////////////////////////////////////
  // create or reference the vbo
  //////////////////////////////////////////////////////////
  vkvtxbuf_ptr_t vk_impl;
  if (auto try_vk_impl = vtx_buf._impl.tryAsShared<VulkanVertexBuffer>()) {
    vk_impl = try_vk_impl.value();
  } else {
    vk_impl = std::make_shared<VulkanVertexBuffer>(_contextVK, vtx_buf);
    vtx_buf._impl.setShared(vk_impl);
  }
  void* vertex_memory = nullptr;
  if (is_static) {
    OrkAssert(ibasebytes == 0); // TODO change api to not require offset for static buffers
    vkMapMemory(
        _contextVK->_vkdevice, // vulkan device
        vk_impl->_vkmem,       // vulkan memory
        0,                     // offset
        isizebytes,            // size
        0,                     // flags
        &vertex_memory);
  } else {
    vkMapMemory(
        _contextVK->_vkdevice, // vulkan device
        vk_impl->_vkmem,       // vulkan memory
        ibasebytes,            // offset
        isizebytes,            // size
        0,                     // flags
        &vertex_memory);
  }
  //////////////////////////////////////////////////////////
  vtx_buf.Lock();
  OrkAssert(vertex_memory != nullptr);
  return vertex_memory;
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

void VkGeometryBufferInterface::DrawPrimitiveEML(
    const VertexBufferBase& vtx_buf, //
    PrimitiveType eType,
    int ivbase,
    int ivcount) {

  OrkAssert(_contextVK->_renderpass_index>=0);

  auto& CB = _contextVK->_cmdbufcur_gfx;

  auto it_pc = _primclasses.find(uint64_t(eType));
  OrkAssert(it_pc != _primclasses.end());
  auto primclass = it_pc->second;

  ///////////////////////
  // find pipeline
  ///////////////////////

  auto vk_vbimpl = vtx_buf._impl.getShared<VulkanVertexBuffer>();
  auto fxi = _contextVK->_fxi;
  auto pipeline = fxi->_fetchPipeline(vk_vbimpl,primclass);

  ///////////////////////
  // bind pipeline
  ///////////////////////

  fxi->_bindPipeline(pipeline);

  ///////////////////////
  // flush params
  ///////////////////////

  pipeline->applyPendingParams(CB);

  ///////////////////////
  // bind vertex buffer
  ///////////////////////

  VkDeviceSize offset = ivbase * vtx_buf.GetVtxSize();
  vkCmdBindVertexBuffers(CB->_vkcmdbuf, // command buffer
                         0,                                              // first binding
                         1,                                              // binding count
                         &vk_vbimpl->_vkbuf,                             // buffers
                         &offset);                                       // offsets

  ///////////////////////
  // draw
  ///////////////////////

  vkCmdDraw( CB->_vkcmdbuf, // command buffer
             ivcount,                                        // vertex count
             1,                                              // instance count
             ivbase,                                         // first vertex
             0);                                             // first instance

}

///////////////////////////////////////////////////////////////////////////////

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
  // OrkAssert(false);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
