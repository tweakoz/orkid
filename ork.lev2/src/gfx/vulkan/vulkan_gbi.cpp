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

VulkanVertexBuffer::VulkanVertexBuffer(vkcontext_rawptr_t ctx, VertexBufferBase& vtx_buf)
  : _ork_vtxbuf(vtx_buf) {

  _ctx = ctx;

  /////////////////////////////////////
  // create vertex buffer object
  /////////////////////////////////////

  _vkbuffer = std::make_shared<VulkanBuffer>(ctx, vtx_buf.GetVtxSize() * vtx_buf.GetMax(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

  /////////////////////////////////////
  // find vertex input configuration
  /////////////////////////////////////

  //auto vtx_format = vtx_buf.GetStreamFormat();
  //auto it         = ctx->_gbi->_vertexInputConfigs.find(vtx_format);
  //OrkAssert(it != ctx->_gbi->_vertexInputConfigs.end());
  //_vertexConfig = it->second;
}

///////////////////////////////////////////////////////////////////////////////

VulkanVertexBuffer::~VulkanVertexBuffer() {
  _vkbuffer = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

VulkanIndexBuffer::VulkanIndexBuffer(vkcontext_rawptr_t ctx, size_t length) {
  OrkAssert(length > 0);
  _ctx = ctx;

  _vkbuffer = std::make_shared<VulkanBuffer>(ctx, length, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}
///////////////////////////////////////////////////////////////////////////////
VulkanIndexBuffer::~VulkanIndexBuffer() {
  _vkbuffer = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

VkGeometryBufferInterface::VkGeometryBufferInterface(vkcontext_rawptr_t ctx)
    : GeometryBufferInterface(*ctx)
    , _contextVK(ctx) {

  _instantiateVertexStreamConfig(EVtxStreamFormat::V12C4T16);
  _instantiateVertexStreamConfig(EVtxStreamFormat::V12N12B12T8C4);
  _instantiateVertexStreamConfig(EVtxStreamFormat::V16T16C16);
  ////////////////////////////////////////////////////////////////
  auto create_primclass = [&](PrimitiveType etype) -> vkprimclass_ptr_t {
    auto rval            = std::make_shared<VkPrimitiveClass>();
    rval->_primtype      = etype;
    rval->_pipeline_bits = _primclasses.size();
    initializeVkStruct(rval->_input_assembly_state, VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    switch (etype) {
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
  _primclasses[uint64_t(PrimitiveType::TRIANGLESTRIP)] = create_primclass(PrimitiveType::TRIANGLESTRIP);
  _primclasses[uint64_t(PrimitiveType::TRIANGLEFAN)] = create_primclass(PrimitiveType::TRIANGLEFAN);
  _primclasses[uint64_t(PrimitiveType::LINES)] = create_primclass(PrimitiveType::LINES);
  _primclasses[uint64_t(PrimitiveType::LINESTRIP)] = create_primclass(PrimitiveType::LINESTRIP);
  _primclasses[uint64_t(PrimitiveType::POINTS)] = create_primclass(PrimitiveType::POINTS);
  ////////////////////////////////////////////////////////////////
  OrkAssert(_primclasses.size() <= 16); // validate we only used 4 pipeline_bits
  ////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void VertexStreamConfig::addItem(std::string sem, std::string vb_dt, size_t ds, size_t offset, VkFormat vkfmt){
  auto item = std::make_shared<VertexStreamConfigItem>();
  item->_semantic = sem;
  item->_vbuf_datatype = vb_dt;
  item->_dataoffset   = offset;
  item->_datasize   = ds;
  item->_vkformat = vkfmt;
  _item_by_semantic[sem] = item;
}

///////////////////////////////////////////////////////////////////////////////

vertex_strconfig_ptr_t VkGeometryBufferInterface::_instantiateVertexStreamConfig(EVtxStreamFormat format) {

  auto it = _vertexStreamConfigs.find(format);
  OrkAssert(it == _vertexStreamConfigs.end());
  auto config            = std::make_shared<VertexStreamConfig>();
  _vertexStreamConfigs[format] = config;
  switch (format) {
    case EVtxStreamFormat::V12N12B12T8C4: {
      config->addItem("POSITION", "vec3", sizeof(fvec3), 0, VK_FORMAT_R32G32B32_SFLOAT);
      config->addItem("NORMAL", "vec3", sizeof(fvec3), 12, VK_FORMAT_R32G32B32_SFLOAT);
      config->addItem("BINORMAL0", "vec3", sizeof(fvec3), 24, VK_FORMAT_R32G32B32_SFLOAT);
      config->addItem("TEXCOORD0", "vec2", sizeof(fvec2), 36, VK_FORMAT_R32G32_SFLOAT);
      config->addItem("COLOR0", "vec4", sizeof(uint32_t), 44, VK_FORMAT_R8G8B8A8_UNORM);
      config->_stride = sizeof(SVtxV12N12B12T8C4);
      break;
    }
    case EVtxStreamFormat::V12C4T16: {
      config->addItem("POSITION", "vec3", sizeof(fvec3), 0, VK_FORMAT_R32G32B32_SFLOAT);
      config->addItem("COLOR0", "vec4", sizeof(uint32_t), 12, VK_FORMAT_R8G8B8A8_UNORM);
      config->addItem("TEXCOORD0", "vec2", sizeof(fvec2), 16, VK_FORMAT_R32G32_SFLOAT);
      config->addItem("TEXCOORD1", "vec2", sizeof(fvec2), 24, VK_FORMAT_R32G32_SFLOAT);
      config->_stride = sizeof(SVtxV12C4T16);
      break;
    }
    case EVtxStreamFormat::V16T16C16:{
      config->addItem("POSITION", "vec4", sizeof(fvec4), 0, VK_FORMAT_R32G32B32A32_SFLOAT);
      config->addItem("TEXCOORD0", "vec4", sizeof(fvec4), 16, VK_FORMAT_R32G32B32A32_SFLOAT);
      config->addItem("COLOR0", "vec4", sizeof(fvec4), 32, VK_FORMAT_R32G32B32A32_SFLOAT);
      config->_stride = sizeof(SVtxV16T16C16);
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
   return config;
}

///////////////////////////////////////////////////////////////////////////////

vkvertexinputconfig_ptr_t VkGeometryBufferInterface::vertexInputState(vkvtxbuf_ptr_t vbuf, //
                                                                      vkvertexinterface_ptr_t vif){ //

  EVtxStreamFormat vb_format = vbuf->_ork_vtxbuf.GetStreamFormat();
  uint64_t vif_hash = vif->_hash; // hashed from shader input layout ordered(semantic, datatype)
  auto it = vbuf->_vif_to_layout.find(vif_hash);
  if( it != vbuf->_vif_to_layout.end() ){
    return it->second;
  }

  ////////////////////////////////////////////////////
  // fetch stream config for this vertex buffer format
  //  this contains the data by semantic of the vertex buffer format
  ////////////////////////////////////////////////////

  auto it_stream_config = _vertexStreamConfigs.find(vb_format);
  OrkAssert(it_stream_config != _vertexStreamConfigs.end());
  auto vsc = it_stream_config->second;

  ////////////////////////////////////////////////////
  // layout not cached, create it
  // match vertex buffer format to vertex shader input format
  // and generate or load a cached VkVertexInputConfiguration
  ////////////////////////////////////////////////////

  vkvertexinputconfig_ptr_t rval;

  rval = std::make_shared<VkVertexInputConfiguration>();

  rval->_binding_description = VkVertexInputBindingDescription{
      0, // binding
      uint32_t(vsc->_stride), // stride
      VK_VERTEX_INPUT_RATE_VERTEX,
  };

  //// vtx input map

  size_t num_vtx_shader_inputs = vif->_inputs.size();
  size_t location = 0;
  for( auto input : vif->_inputs ){
    auto semantic = input->_semantic; // "POSITION", "NORMAL", "BINORMALn, "TANGENTn", "TEXCOORDn", "COLORn"
    auto shader_datatype = input->_datatype; // "vec4", "vec3", "vec2", "float", "half4", "half3", "half2", "half"

    auto it = vsc->_item_by_semantic.find(semantic);
    if( it == vsc->_item_by_semantic.end() ){

      auto vbformatstr = EVtxStreamFormatToName(vb_format);
      printf( "MISSING: vif<%s> semantic<%s> shader_datatype<%s> vbfmt<%s>\n"
            , vif->_name.c_str()
            , semantic.c_str()
            , shader_datatype.c_str()
            , vbformatstr.c_str() );
      OrkAssert(false);
    }

    auto item = it->second;

    auto& atdesc = rval->_attribute_descriptions.emplace_back();
    atdesc.binding  = 0;
    atdesc.location = location++;
    atdesc.format   = item->_vkformat;
    atdesc.offset   = item->_dataoffset;

    if(item->_vbuf_datatype != shader_datatype){

      ////////////////////////////////////////////
      // MISMATCH exceptions:
      // try trimming data (when vbuf has more than shader needs)
      ////////////////////////////////////////////

      if( shader_datatype == "vec3" and item->_vbuf_datatype == "vec4" ){
        atdesc.format = VK_FORMAT_R32G32B32_SFLOAT;
      }
      else if( shader_datatype == "vec4" and item->_vbuf_datatype == "vec3" ){
        atdesc.format = VK_FORMAT_R32G32B32_SFLOAT;
      }
      else if( shader_datatype == "vec2" and item->_vbuf_datatype == "vec4" ){
        atdesc.format = VK_FORMAT_R32G32_SFLOAT;
      }
      else{
        printf( "MISMATCH: shader_datatype<%s> semantic<%s> item->_vbuf_datatype<%s>\n"
              , shader_datatype.c_str()
              , semantic.c_str()
              , item->_vbuf_datatype.c_str());
        OrkAssert(false);
      }
    }
  }

  VkPipelineVertexInputStateCreateInfo& vis = rval->_vertex_input_state;
  initializeVkStruct(vis, VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
  vis.vertexBindingDescriptionCount   = 1;
  vis.pVertexBindingDescriptions      = &rval->_binding_description;
  vis.vertexAttributeDescriptionCount = rval->_attribute_descriptions.size();
  vis.pVertexAttributeDescriptions    = rval->_attribute_descriptions.data();
  
  // add to cache
  vbuf->_vif_to_layout[vif_hash] = rval;

  // return
  return rval;
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
    vertex_memory = vk_impl->_vkbuffer->map(0, isizebytes, 0);
  } else {
    vertex_memory = vk_impl->_vkbuffer->map(ibasebytes, isizebytes, 0);
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
  vk_impl->_vkbuffer->unmap();
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
  size_t ibasebytes = ivbase * idx_buf.GetIndexSize();
  size_t isizebytes = icount * idx_buf.GetIndexSize();
  bool is_static    = idx_buf.IsStatic();

  if(icount==0){
    icount=idx_buf.GetNumIndices();
  }

  //////////////////////////////////////////////////////////
  // create or reference the ibo
  //////////////////////////////////////////////////////////

  vkidxbuf_ptr_t vk_impl;
  if (auto try_vk_buf = idx_buf._impl.tryAsShared<VulkanIndexBuffer>()) {
    vk_impl = try_vk_buf.value();
  } else {
    size_t size_in_bytes = icount * idx_buf.GetIndexSize();
    OrkAssert(size_in_bytes > 0);
    vk_impl               = std::make_shared<VulkanIndexBuffer>(_contextVK, size_in_bytes);
    idx_buf._impl.setShared(vk_impl);
  }
  void* index_memory = nullptr;
  //////////////////////////////////////////////////////////
  if (is_static) {
    OrkAssert(ibasebytes == 0); // TODO change api to not require offset for static buffers
    index_memory = vk_impl->_vkbuffer->map(0, isizebytes, 0);
  } else {
    index_memory = vk_impl->_vkbuffer->map(ibasebytes, isizebytes, 0);
  }
  //////////////////////////////////////////////////////////
  idx_buf._locked = true;
  OrkAssert(index_memory != nullptr);
  return index_memory;
}
//////////////////////////
void VkGeometryBufferInterface::UnLockIB(IndexBufferBase& idx_buf) {
  auto vk_impl = idx_buf._impl.getShared<VulkanIndexBuffer>();
  OrkAssert(idx_buf._locked);
  idx_buf._locked = false;
  vk_impl->_vkbuffer->unmap();
  // OrkAssert(idx_buf.IsLocked());
}
//////////////////////////
const void* VkGeometryBufferInterface::LockIB(const IndexBufferBase& idx_buf, int ibase, int icount) {
  auto& mutable_idx_buf = const_cast<IndexBufferBase&>(idx_buf);
  return LockIB(mutable_idx_buf, ibase, icount);
}
//////////////////////////
void VkGeometryBufferInterface::UnLockIB(const IndexBufferBase& idx_buf) {
  auto& mutable_idx_buf = const_cast<IndexBufferBase&>(idx_buf);
  UnLockIB(mutable_idx_buf);
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

  OrkAssert(_contextVK->_renderpass_index >= 0);

  auto& CB = _contextVK->_cmdbufcur_gfx;

  ///////////////////////
  // get primclass (input to pipeline search)
  ///////////////////////

  auto it_pc = _primclasses.find(uint64_t(eType));
  OrkAssert(it_pc != _primclasses.end());
  auto primclass = it_pc->second;

  ///////////////////////
  // find pipeline, pass, prog
  ///////////////////////

  auto vk_vbimpl = vtx_buf._impl.getShared<VulkanVertexBuffer>();
  auto fxi       = _contextVK->_fxi;
  auto pipeline  = fxi->_fetchPipeline(vk_vbimpl, primclass);
  auto pass      = fxi->_currentVKPASS;
  auto prog      = pass->_vk_program;

  ///////////////////////
  // bind pipeline
  // bind descriptor set
  // flush push constants
  // bind vertex buffer
  ///////////////////////

  fxi->_bindPipeline(pipeline);
  auto desc_set = pipeline->_descriptorSetCache->fetchDescriptorSetForProgram(prog);
  fxi->_bindGfxDescriptorSetOnSlot(desc_set, 0);
  pipeline->applyPendingPushConstants(CB);
  fxi->_bindVertexBufferOnSlot(vk_vbimpl, 0);

  ///////////////////////
  // draw
  ///////////////////////

  vkCmdDraw(
      CB->_vkcmdbuf, // command buffer
      ivcount,       // vertex count
      1,             // instance count
      ivbase,        // first vertex
      0);            // first instance
}

///////////////////////////////////////////////////////////////////////////////

void VkGeometryBufferInterface::DrawIndexedPrimitiveEML(
    const VertexBufferBase& vtx_buf,
    const IndexBufferBase& idx_buf,
    PrimitiveType eType) {
  OrkAssert(_contextVK->_renderpass_index >= 0);

  auto& CB = _contextVK->_cmdbufcur_gfx;

  int num_indices = idx_buf.GetNumIndices();

  ///////////////////////
  // get primclass (input to pipeline search)
  ///////////////////////

  auto it_pc = _primclasses.find(uint64_t(eType));
  OrkAssert(it_pc != _primclasses.end());
  auto primclass = it_pc->second;

  ///////////////////////
  // find pipeline, pass, prog
  ///////////////////////

  auto vk_vbimpl = vtx_buf._impl.getShared<VulkanVertexBuffer>();
  auto vk_ibimpl = idx_buf._impl.getShared<VulkanIndexBuffer>();
  auto fxi       = _contextVK->_fxi;
  auto pipeline  = fxi->_fetchPipeline(vk_vbimpl, primclass);
  auto pass      = fxi->_currentVKPASS;
  auto prog      = pass->_vk_program;

  ///////////////////////
  // bind pipeline
  // bind descriptor set
  // flush push constants
  // bind vertex buffer
  ///////////////////////

  fxi->_bindPipeline(pipeline);
  auto desc_set = pipeline->_descriptorSetCache->fetchDescriptorSetForProgram(prog);
  fxi->_bindGfxDescriptorSetOnSlot(desc_set, 0);
  pipeline->applyPendingPushConstants(CB);
  fxi->_bindVertexBufferOnSlot(vk_vbimpl, 0);

  ///////////////////////
  // bind index buffer
  ///////////////////////

  auto vk_index_size = idx_buf.GetIndexSize()==2 //
                     ? VK_INDEX_TYPE_UINT16 //
                     : VK_INDEX_TYPE_UINT32;

  auto& vk_buffer = vk_ibimpl->_vkbuffer->_vkbuffer;

  vkCmdBindIndexBuffer( CB->_vkcmdbuf,  // command buffer 
                        vk_buffer,      // index buffer
                        0,              // start at first index in index buffer
                        vk_index_size); // index type

  ///////////////////////
  // draw
  ///////////////////////

  vkCmdDrawIndexed(
      CB->_vkcmdbuf, // command buffer
      num_indices,   // index count
      1,             // instance count
      0,             // first vertex
      0,             // vertex offset
      0);            // first instance
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
