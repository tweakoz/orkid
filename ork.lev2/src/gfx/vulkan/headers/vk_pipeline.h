#pragma once

#include <set>
#include <unordered_map>
#include <ork/lev2/gfx/shadman.h>
#include "vk_protos.h"
#include "vk_merged_resources.h"

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderUniformSetItem {
  std::string _datatype;
  std::string _identifier;
  size_t _offset = 0;
  std::shared_ptr<FxShaderParam> _orkparam;
  VkShaderStageFlags _shader_stage = 0;  // Which shader stage this parameter belongs to
  size_t _range_index = 0;                // Which push constant range to use
};

////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderUniformSampler {
  std::string _datatype;
  std::string _identifier;
  std::shared_ptr<FxShaderParam> _orkparam;
};

////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderUniformSet {
  std::string _name;
  std::unordered_map<std::string, vkfxsunisetitem_ptr_t> _items_by_name;
  std::vector<vkfxsunisetitem_ptr_t> _items_by_order;
};

////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderDescriptorSetItem {
  size_t _descriptor_set_id = 0;
};

////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderSamplerSet : public VkFxShaderDescriptorSetItem {
  std::unordered_map<std::string, vkfxsunisampler_ptr_t> _samplers_by_name;
  std::vector<vkfxsunisampler_ptr_t> _samplers_by_order;
  svar64_t _impl;
};

/////////////////////////////////////////////////////////////////////////////////
//  VkFxShaderUniformBlk
//    Shared across all VkContexts — layout/descriptor info only.
//    Owned by VkFxShaderProgram::_vk_uniformblks (shared_ptr map values).
/////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderUniformBlock : public VkFxShaderDescriptorSetItem {
  std::shared_ptr<FxUniformBlock> _orkparamblock;
  std::unordered_map<std::string, vkfxsuniblkitem_ptr_t> _items_by_name;
  std::vector<vkfxsuniblkitem_ptr_t> _items_by_order;

  size_t _buffer_size = 0;
  std::string _name;
};

/////////////////////////////////////////////////////////////////////////////////
//  VkFxShaderUniformBlkState
//    Per-VkContext state for VkFxShaderUniformBlk.
//    Owned by VkFxShaderState::_ubo_states (inline map values).
/////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderUniformBlockState {
  uint32_t                      _binding_id = 0;
  std::vector<uint8_t>          _shadow_buffer;
  VkFxShaderUniformBlock*       _shader_uniform_block = nullptr;
};

/////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderUniformBlkItem {
  std::string _datatype;
  std::string _identifier;
  size_t _offset = 0;
  bool _is_array = false;
  size_t _array_length = 0;
  std::shared_ptr<FxShaderParam> _orkparam;
  struct VkFxShaderUniformBlock* _parent_block = nullptr;
};

////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderUniformSetsReference {
  uniset_map_t _unisets;
};

////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderUniformBlksReference {
  uniblk_map_t _uniblks;
};

/////////////////////////////////////////////////////////////////////////////////
//  VkFxShaderStorageBlock
//    Shared across all VkContexts — layout/descriptor info only.
//    Owned by VkFxShaderProgram::_vk_ssbo_blocks (shared_ptr map values).
/////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderStorageBlock : public VkFxShaderDescriptorSetItem {
  std::shared_ptr<FxShaderStorageBlock> _orkstorageblock;
  std::unordered_map<std::string, fxbuffer_member_ptr_t> _members_by_name;
  std::vector<fxbuffer_member_ptr_t> _members_by_order;

  size_t _buffer_size = 0;
  std::string _name;
  std::string _buffer_name;
};

/////////////////////////////////////////////////////////////////////////////////
//  VkFxShaderStorageBlockState
//    Per-VkContext state for VkFxShaderStorageBlock.
//    Owned by VkFxShaderState::_ssbo_states (inline map values).
/////////////////////////////////////////////////////////////////////////////////
struct VkFxShaderStorageBlockState {
  VkFxShaderStorageBlock*       _shader_storage_block = nullptr;  // back-pointer to shared block definition
  uint32_t                      _binding_id = 0;
  vkbuffer_ptr_t                _bound_buffer;
  FxShaderStorageBuffer*        _bound_ssbo = nullptr;
};

/////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderStorageBlocksReference {
  std::map<std::string, std::shared_ptr<VkFxShaderStorageBlock>> _ssbo_blocks;
};

////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderSamplerSetsReference {
  static size_t descriptor_set_counter;
  smpset_map_t _smpsets;
};

////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderPushConstantBlock {
  uniset_map_t _vtx_unisets;
  uniset_map_t _frg_unisets;

  uniset_item_map_t _vtx_items_by_name;
  uniset_item_map_t _frg_items_by_name;

  vkbufferlayout_ptr_t _data_layout;

  std::vector<VkPushConstantRange> _ranges;
  size_t _blockSize = 0;
};

/////////////////////////////////////////////////////////////////////////////////
//  VkFxShaderTechnique
//    Shared across all VkContexts. Immutable after load.
//    Owned by VkFxShaderFile::_vk_techniques (shared_ptr map values).
/////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderTechnique {
  VkFxShaderTechnique();
  ~VkFxShaderTechnique();
  std::vector<vkfxshaderpass_ptr_t> _vk_passes;
  std::shared_ptr<FxShaderTechnique> _orktechnique;
};

/////////////////////////////////////////////////////////////////////////////////
//  VulkanFxShaderStage
//    One compiled SPIR-V shader stage (vertex, fragment, geometry, or compute).
//    Wraps VkShaderModule and VkPipelineShaderStageCreateInfo for one stage.
//    Owned by VkFxShaderFile::_vk_shaderstages; shared across programs that reuse the same stage.
/////////////////////////////////////////////////////////////////////////////////

struct VulkanFxShaderStage {

  VulkanFxShaderStage(vkcontext_rawptr_t ctx, vkfxshader_bin_t bin);
  ~VulkanFxShaderStage();

  vkcontext_rawptr_t _contextVK;
  vkfxshader_bin_t _spirv_binary;
  VkShaderModuleCreateInfo _vk_shadermoduleinfo;
  VkShaderModule _vk_shadermodule;
  VkPipelineShaderStageCreateInfo _shaderstageinfo;
  // shadlang::SHAST::astnode_ptr_t _astnode; // debug only
  vkfxsunisetsref_ptr_t _uniset_refs;
  vkfxsuniblksref_ptr_t _uniblk_refs;
  vkfxssmpsetsref_ptr_t _smpset_refs;
  std::shared_ptr<VkFxShaderStorageBlocksReference> _ssbo_refs;
  std::vector<std::string> _vk_interfaces;

  uint64_t _STAGE = 0;
  VkPushConstantRange _vkpc_range;
  std::string _name;
};

/////////////////////////////////////////////////////////////////////////////////
//  VkFxShaderFile
//    Shared across all VkContexts. Immutable after load.
////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderFile {
  std::string _shader_name;
  // shadlang::SHAST::translationunit_ptr_t _trans_unit;
  std::unordered_map<std::string, vkfxsstage_ptr_t> _vk_shaderstages;
  std::unordered_map<std::string, vkfxstek_ptr_t> _vk_techniques;
  std::unordered_map<std::string, vkfxssmpset_ptr_t> _vk_samplersets;
  std::unordered_map<std::string, vkfxsuniset_ptr_t> _vk_uniformsets;
  std::unordered_map<std::string, vkfxsuniblk_ptr_t> _vk_uniformblks;
  std::unordered_map<std::string, vkfxssbo_ptr_t> _vk_ssbo_blocks;
  std::unordered_map<std::string, vkvertexinterface_ptr_t> _vk_vtxinterfaces;
  std::unordered_map<std::string, rasterstate_ptr_t> _stateblock_rasterstates; // Registry of pre-built rasterstates
  std::unordered_map<std::string, vkgeometryinterface_ptr_t> _vk_geointerfaces;
};

////////////////////////////////////////////////////////////////////////////////

struct VkParamSetItem {
  VkFxShaderUniformSetItem* _vk_param = nullptr;
  fxparam_constptr_t _ork_param       = nullptr;
  svar64_t _value;
};

////////////////////////////////////////////////////////////////////////////////

struct DescBinding {
  uint32_t _set_id     = 0;
  uint32_t _binding_id = 0;
};

/////////////////////////////////////////////////////////////////////////////////
//  VkFxShaderState
//    Per-VkContext state for VkFxShaderProgram. Mutable per frame.
//    Owned by VkFxInterface::_shader_pass_states (inline map values, keyed by program rawptr).
//    Shared across all VkPipelineState instances that use the same program.
/////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderPassState {

  // bindParamTexture re-resolves Texture::_impl -> vktex every frame and
  // writes the freshly resolved vktex here, so format-driven swaps in
  // initTextureFromData (e.g. RGBA8_UNORM -> RGBA32_UINT for SH splat
  // data) are picked up. On bind we compare the incoming vktex against
  // the slot's existing value and zero _samplers_hash on change to
  // force a fresh descriptor set. Per-context (not on the shared
  // VkFxShaderUniformSampler) so multiple VkContexts don't stomp.
  std::unordered_map<fxparam_constptr_t, vktexobj_ptr_t> _textures_by_orkparam;
  std::unordered_map<fxparam_constptr_t, vkbuffer_ptr_t> _uniformbuffers_by_orkparam;
  uint64_t _samplers_hash = 0; // 0 means dirty/needs recompute

  std::vector<VkParamSetItem> _pending_params;
  std::vector<uint8_t>        _pushdatabuffer;

  std::vector<VkFxShaderUniformBlockState*> _ordered_uniform_states; // Ordered by binding ID
  std::vector<VkFxShaderStorageBlockState*> _ordered_storage_states;  // Ordered by binding ID

  vkfxshaderpass_rawptr_t _shader = nullptr;

  uint64_t samplersHash();
};

/////////////////////////////////////////////////////////////////////////////////
//  VkFxShaderPass
//    A single pass composed of multiple shader stages (vertex, fragment, geometry, etc.).
//    Immutable. Shared across all VkContexts.
//    Owned by VkFxShaderTechnique::_vk_passes (shared_ptr). Lifetime == shader asset lifetime.
//    All members populated at load time; treat as read-only at draw time.
//    Per-context draw-time state belongs in VkFxShaderState.
/////////////////////////////////////////////////////////////////////////////////

struct VkFxShaderPass {

  VkFxShaderPass(VkFxShaderFile* file);

  std::string _tek_name;

  vkfxsstage_ptr_t _vtxshader;
  vkfxsstage_ptr_t _geoshader;
  vkfxsstage_ptr_t _tctshader;
  vkfxsstage_ptr_t _tevshader;
  vkfxsstage_ptr_t _frgshader;
  vkfxsstage_ptr_t _comshader;

  vkvertexinterface_ptr_t   _vertexinterface;
  vkgeometryinterface_ptr_t _geometryinterface;

  vkfxpushconstantblk_ptr_t _pushConstantBlock;

  // Storage for merged resource bindings (set_id, binding_id) — populated at load time
  std::unordered_map<fxparam_constptr_t, DescBinding> _merged_resource_bindings;

  // Merged resource descriptors and raster state — populated at load time
  vk_merged_resources_ptr_t _merged_resources;
  rasterstate_ptr_t         _stateblock_rasterstate;

  int _pipeline_bits_prg       = -1;
  int _pipeline_bits_composite = -1;
  std::unordered_map<std::string, vkfxssmpset_ptr_t> _vk_samplersets;
  std::unordered_map<std::string, vkfxsuniset_ptr_t> _vk_uniformsets;
  std::unordered_map<std::string, vkfxsuniblk_ptr_t> _vk_uniformblks;
  std::unordered_map<std::string, vkfxssbo_ptr_t> _vk_ssbo_blocks;
  VkFxShaderFile* _shader_file = nullptr;
  boost::Crc64 _incr_crc64;

  // Synthetic params for auto-registered UBO blocks (to maintain lifetime)
  std::vector<fxparam_ptr_t> _synthetic_ubo_params;

  // Flag indicating this pass has SSBO resources that need descriptor binding
  bool _has_ssbo_resources = false;
};

/////////////////////////////////////////////////////////////////////////////////
//  VulkanDescriptorSetState
//    Per-VkContext state for VkDescriptorSet.
//    Owned by VulkanDescriptorSetCacheState::_vkDescriptorSetByHash (shared_ptr map values).
/////////////////////////////////////////////////////////////////////////////////

struct VulkanDescriptorSetState {
  VkDescriptorSet _vkdescset;
};

/////////////////////////////////////////////////////////////////////////////////
//  VulkanDescriptorSetCacheState
//    Per-VkContext descriptor set cache.
//    Owned by VkPipelineState::_descriptorSetCache (shared_ptr).
/////////////////////////////////////////////////////////////////////////////////

struct VulkanDescriptorSetCacheState {

  VulkanDescriptorSetCacheState(vkcontext_rawptr_t ctx);

  vkdescriptorsetstate_ptr_t fetchDescriptorSetForProgram(vkfxshaderpass_rawptr_t program);
  vkdescriptorsetstate_ptr_t    _createNewDescriptorSetForProgram(vkfxshaderpass_rawptr_t program);

  std::unordered_map<uint64_t, vkdescriptorsetstate_ptr_t> _vkDescriptorSetByHash;
  vkcontext_rawptr_t _ctxVK;
};

/////////////////////////////////////////////////////////////////////////////////
//  VkPipelineState
//    Per-VkContext, per-(program × VB format × raster state × RTG) Vulkan pipeline wrapper.
//    Owned by VkFxInterface::_pipelines (shared_ptr map values, keyed by pipeline hash).
/////////////////////////////////////////////////////////////////////////////////

struct VkPipelineState {

  VkPipelineState(vkcontext_rawptr_t ctx);

  void applyPendingPushConstants(VkCommandBuffer cmdbuf);
  void applyPendingUboUpdates(VkCommandBuffer cmdbuf, uint32_t frame_index);
  
  VkFxShaderPassState* _shader_state = nullptr;

  VkGraphicsPipelineCreateInfo _VKGFXPCI;
  VkPipeline _pipeline;
  VkPipelineLayout _pipelineLayout;
  vkdescriptorsetcache_state_ptr_t _descriptorSetCache;
  vkrasterstate_ptr_t _rasterstate;

  vkviewporttracker_ptr_t _viewport;
  vkviewporttracker_ptr_t _scissor;

  // Storage for merged resource descriptor set layouts
  std::vector<VkDescriptorSetLayout> _dset_layouts;

  // Report filename for debugging descriptor set issues
  std::string _report_filename;
};

///////////////////////////////////////////////////////////////////////////

struct VulkanVertexInterfaceInput {
  std::string _datatype;
  std::string _identifier;
  std::string _semantic;
  size_t _datasize = 0;
};

////////////////////////////////////////////////////////////////////////////////

struct VulkanVertexInterface {
  using input_t = VulkanVertexInterfaceInput;
  std::string _name;
  vkvertexinterface_ptr_t _parent;
  std::vector<vkvertexinterfaceinput_ptr_t> _inputs;
  int _pipeline_bits = -1;
  uint64_t _hash     = 0;
};

////////////////////////////////////////////////////////////////////////////////

struct VulkanGeometryInterfaceInput {
  std::string _datatype;
  std::string _identifier;
  std::string _semantic;
  size_t _datasize = 0;
};

/////////////////////////////////////////////////////////////////////////////////

struct VulkanGeometryInterface {
  using input_t = VulkanGeometryInterfaceInput;
  std::string _name;
  vkgeometryinterface_ptr_t _parent;
  std::vector<vkgeometryinterfaceinput_ptr_t> _inputs;
  int _pipeline_bits = -1;
  uint64_t _hash     = 0;
};

/////////////////////////////////////////////////////////////////////////////////
//  VkComputePipelineState
//    Per-VkContext State for compute VkPipeline
/////////////////////////////////////////////////////////////////////////////////

struct VkComputePipelineState {

  VkComputePipelineState(vkcontext_rawptr_t ctx);
  ~VkComputePipelineState();

  bool createPipeline(vkfxsstage_ptr_t computeShader);
  void bindStorageBuffer(uint32_t binding_index, VkBuffer buffer, VkDeviceSize size);
  void bindSampler(uint32_t binding_index, VkDescriptorImageInfo desc_info);
  void updateDescriptorSet();

  vkcontext_rawptr_t _contextVK = nullptr;
  vkfxsstage_ptr_t _computeShader;              // VulkanFxShaderStage with SPIR-V

  VkPipeline _pipeline = VK_NULL_HANDLE;
  VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorSet _descriptorSet = VK_NULL_HANDLE;
  VkDescriptorPool _descriptorPool = VK_NULL_HANDLE;  // Per-pipeline pool for simplicity

  // Storage buffer bindings (binding_id -> buffer info)
  struct StorageBufferBinding {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize offset = 0;
    VkDeviceSize size = 0;
  };
  std::map<uint32_t, StorageBufferBinding> _ssbo_bindings;

  // Sampler bindings (binding_id -> descriptor image info)
  std::map<uint32_t, VkDescriptorImageInfo> _sampler_bindings;

  // Track if descriptor set needs update
  bool _descriptors_dirty = true;

  std::string _name;
};

/////////////////////////////////////////////////////////////////////////////////

struct VkRasterState {
  VkRasterState(rasterstate_ptr_t rstate, int attachment_count = 1, const std::vector<VkFormat>* formats = nullptr);
  VkPipelineRasterizationStateCreateInfo _VKRSCI;
  VkPipelineDepthStencilStateCreateInfo _VKDSSCI;
  VkPipelineColorBlendStateCreateInfo _VKCBSI;
  VkPipelineColorBlendAttachmentState _VKCBATT; // Base attachment state (for backward compat)
  std::vector<VkPipelineColorBlendAttachmentState> _VKCBATT_array; // Array for MRT
  std::vector<VkFormat> _vkformats; // Store formats for cache invalidation
  int _pipeline_bits = -1;
  int _attachment_count = 1;
  uint64_t _rasterstate_hash = 0; // Hash of rasterstate content for cache invalidation
  RasterState* _ork_rasterstate = nullptr;

  using rsmap_t = std::unordered_map<uint64_t, int>;

  static LockedResource<rsmap_t> _global_rasterstate_map;
};

/////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
/////////////////////////////////////////////////////////////////////////////////