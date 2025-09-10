#pragma once 
///////////////////////////////////////////////////////////////////////////////
#include "vk_synchro.h"
#include "vk_pipeline.h"
#include "vk_merged_resources.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformSetItem {
  std::string _datatype;
  std::string _identifier;
  size_t _offset = 0;
  std::shared_ptr<FxShaderParam> _orkparam;
  VkShaderStageFlags _shader_stage = 0;  // Which shader stage this parameter belongs to
  size_t _range_index = 0;                // Which push constant range to use
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformSetSampler {
  std::string _datatype;
  std::string _identifier;
  std::shared_ptr<FxShaderParam> _orkparam;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformSet {
  std::string _name;
  std::unordered_map<std::string, vkfxsunisetitem_ptr_t> _items_by_name;
  std::vector<vkfxsunisetitem_ptr_t> _items_by_order;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderDescriptorSetItem {
  size_t _descriptor_set_id = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderSamplerSet : public VkFxShaderDescriptorSetItem {
  std::unordered_map<std::string, vkfxsunisetsamp_ptr_t> _samplers_by_name;
  std::vector<vkfxsunisetsamp_ptr_t> _samplers_by_order;
  svar64_t _impl;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformBlk : public VkFxShaderDescriptorSetItem {
  std::shared_ptr<FxUniformBlock> _orkparamblock;
  std::unordered_map<std::string, vkfxsuniblkitem_ptr_t> _items_by_name;
  std::vector<vkfxsuniblkitem_ptr_t> _items_by_order;
  
  // Shadow buffer mechanism
  std::vector<uint8_t> _shadow_buffer;
  std::vector<dirtyrange_ptr_t> _dirty_ranges;
  
  size_t _buffer_size = 0;
  std::string _name;         // Name of the uniform block
  
  void addDirtyRange(size_t offset, size_t size);
  void coalesceRanges();
  std::vector<alignedrange_ptr_t> getAlignedRanges(VkDeviceSize atom_size) const;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformBlkItem {
  std::string _datatype;
  std::string _identifier;
  size_t _offset = 0;
  bool _is_array = false;
  size_t _array_length = 0;
  std::shared_ptr<FxShaderParam> _orkparam;
  struct VkFxShaderUniformBlk* _parent_block = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
struct DirtyRange {
  size_t offset;
  size_t size;
};
///////////////////////////////////////////////////////////////////////////////
struct AlignedRange {
  VkDeviceSize offset;
  VkDeviceSize size;
  
  static std::shared_ptr<AlignedRange> fromDirtyRange(
    dirtyrange_ptr_t dirty, 
    VkDeviceSize atom_size, 
    VkDeviceSize buffer_size);
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformSetsReference {
  uniset_map_t _unisets;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderUniformBlksReference {
  uniblk_map_t _uniblks;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderSamplerSetsReference {
  static size_t descriptor_set_counter;
  smpset_map_t _smpsets;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderPushConstantBlock {
  uniset_map_t _vtx_unisets;
  uniset_map_t _frg_unisets;

  uniset_item_map_t _vtx_items_by_name;
  uniset_item_map_t _frg_items_by_name;

  vkbufferlayout_ptr_t _data_layout;

  std::vector<VkPushConstantRange> _ranges;
  size_t _blockSize = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderFile {
  std::string _shader_name;
  // shadlang::SHAST::translationunit_ptr_t _trans_unit;
  std::unordered_map<std::string, vkfxsobj_ptr_t> _vk_shaderobjects;
  std::unordered_map<std::string, vkfxstek_ptr_t> _vk_techniques;
  std::unordered_map<std::string, vkfxssmpset_ptr_t> _vk_samplersets;
  std::unordered_map<std::string, vkfxsuniset_ptr_t> _vk_uniformsets;
  std::unordered_map<std::string, vkfxsuniblk_ptr_t> _vk_uniformblks;
  std::unordered_map<std::string, vkvertexinterface_ptr_t> _vk_vtxinterfaces;
  std::unordered_map<std::string, rasterstate_ptr_t> _stateblock_rasterstates; // Registry of pre-built rasterstates
  std::unordered_map<std::string, vkgeometryinterface_ptr_t> _vk_geointerfaces;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanFxShaderObject {

  VulkanFxShaderObject(vkcontext_rawptr_t ctx, vkfxshader_bin_t bin);
  ~VulkanFxShaderObject();

  vkcontext_rawptr_t _contextVK;
  vkfxshader_bin_t _spirv_binary;
  VkShaderModuleCreateInfo _vk_shadermoduleinfo;
  VkShaderModule _vk_shadermodule;
  VkPipelineShaderStageCreateInfo _shaderstageinfo;
  // shadlang::SHAST::astnode_ptr_t _astnode; // debug only
  vkfxsunisetsref_ptr_t _uniset_refs;
  vkfxsuniblksref_ptr_t _uniblk_refs;
  vkfxssmpsetsref_ptr_t _smpset_refs;
  std::vector<std::string> _vk_interfaces;

  uint64_t _STAGE = 0;
  VkPushConstantRange _vkpc_range;
  std::string _name;
};
///////////////////////////////////////////////////////////////////////////////
struct VkParamSetItem {
  VkFxShaderUniformSetItem* _vk_param = nullptr;
  fxparam_constptr_t _ork_param       = nullptr;
  svar64_t _value;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderProgram {

  VkFxShaderProgram(VkFxShaderFile* file);

  void bindDescriptorTexture(fxparam_constptr_t param, const Texture* pTex);

  vkfxsobj_ptr_t _vtxshader;
  vkfxsobj_ptr_t _geoshader;
  vkfxsobj_ptr_t _tctshader;
  vkfxsobj_ptr_t _tevshader;
  vkfxsobj_ptr_t _frgshader;
  vkfxsobj_ptr_t _comshader;

  vkvertexinterface_ptr_t _vertexinterface;
  vkgeometryinterface_ptr_t _geometryinterface;

  vkfxpushconstantblk_ptr_t _pushConstantBlock;

  std::vector<VkParamSetItem> _pending_params;
  std::vector<uint8_t> _pushdatabuffer;
  std::unordered_map<fxparam_constptr_t, vktexobj_ptr_t> _textures_by_orkparam;
  std::unordered_map<fxparam_constptr_t, vkbuffer_ptr_t> _uniformbuffers_by_orkparam;
  
  // Storage for merged resource bindings (set_id, binding_id)
  std::unordered_map<fxparam_constptr_t, std::pair<uint32_t, uint32_t>> _merged_resource_bindings;
  
  int _pipeline_bits_prg       = -1;
  int _pipeline_bits_composite = -1;

  std::unordered_map<std::string, vkfxssmpset_ptr_t> _vk_samplersets;
  std::unordered_map<std::string, vkfxsuniset_ptr_t> _vk_uniformsets;
  std::unordered_map<std::string, vkfxsuniblk_ptr_t> _vk_uniformblks;
  VkFxShaderFile* _shader_file = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanDescriptorSet {
  VkDescriptorSet _vkdescset;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanDescriptorSetCache {

  VulkanDescriptorSetCache(vkcontext_rawptr_t ctx);

  vkdescriptorset_ptr_t fetchDescriptorSetForProgram(vkfxsprg_ptr_t program);

  std::unordered_map<uint64_t, vkdescriptorset_ptr_t> _vkDescriptorSetByHash;
  vkcontext_rawptr_t _ctxVK;
};
///////////////////////////////////////////////////////////////////////////////
struct VkPipelineObject {

  VkPipelineObject(vkcontext_rawptr_t ctx);

  void applyPendingPushConstants(VkCommandBuffer cmdbuf);
  void applyPendingUboUpdates(VkCommandBuffer cmdbuf, uint32_t frame_index);

  vkfxsprg_ptr_t _vk_program;
  VkGraphicsPipelineCreateInfo _VKGFXPCI;
  VkPipeline _pipeline;
  VkPipelineLayout _pipelineLayout;
  vkdescriptorsetcache_ptr_t _descriptorSetCache;

  vkviewporttracker_ptr_t _viewport;
  vkviewporttracker_ptr_t _scissor;
  
  // Storage for merged resource descriptor set layouts
  std::vector<VkDescriptorSetLayout> _merged_resource_descriptor_set_layouts;
  
  // Dynamic UBO support
  std::vector<VkFxShaderUniformBlk*> _uniform_blocks;  // Ordered by binding ID
  std::map<uint32_t, VkFxShaderUniformBlk*> _ubo_by_binding;  // Quick lookup
  std::vector<uint32_t> _dynamic_offsets;  // Populated at draw time
  
  // Report filename for debugging descriptor set issues
  std::string _report_filename;
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderPass {
  vkfxsprg_ptr_t _vk_program;
  vk_merged_resources_ptr_t _merged_resources;
  std::set<VkFxShaderUniformBlk*> _dirty_uniform_blocks;
  rasterstate_ptr_t _stateblock_rasterstate; // Pre-resolved rasterstate from state block
};
///////////////////////////////////////////////////////////////////////////////
struct VkFxShaderTechnique {
  VkFxShaderTechnique();
  ~VkFxShaderTechnique();
  std::vector<vkfxspass_ptr_t> _vk_passes;
  std::shared_ptr<FxShaderTechnique> _orktechnique;
};
///////////////////////////////////////////////////////////////////////////
struct VulkanVertexInterfaceInput {
  std::string _datatype;
  std::string _identifier;
  std::string _semantic;
  size_t _datasize = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanVertexInterface {
  using input_t = VulkanVertexInterfaceInput;
  std::string _name;
  vkvertexinterface_ptr_t _parent;
  std::vector<vkvertexinterfaceinput_ptr_t> _inputs;
  int _pipeline_bits = -1;
  uint64_t _hash     = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanGeometryInterfaceInput {
  std::string _datatype;
  std::string _identifier;
  std::string _semantic;
  size_t _datasize = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanGeometryInterface {
  using input_t = VulkanGeometryInterfaceInput;
  std::string _name;
  vkgeometryinterface_ptr_t _parent;
  std::vector<vkgeometryinterfaceinput_ptr_t> _inputs;
  int _pipeline_bits = -1;
  uint64_t _hash     = 0;
};
///////////////////////////////////////////////////////////////////////////////
struct VkRasterState {
  VkRasterState(rasterstate_ptr_t rstate, int attachment_count = 1, const std::vector<VkFormat>* formats = nullptr);
  VkPipelineRasterizationStateCreateInfo _VKRSCI;
  VkPipelineDepthStencilStateCreateInfo _VKDSSCI;
  VkPipelineColorBlendStateCreateInfo _VKCBSI;
  VkPipelineColorBlendAttachmentState _VKCBATT; // Base attachment state (for backward compat)
  std::vector<VkPipelineColorBlendAttachmentState> _VKCBATT_array; // Array for MRT
  int _pipeline_bits = -1;
  int _attachment_count = 1;

  using rsmap_t = std::unordered_map<uint64_t, int>;

  static LockedResource<rsmap_t> _global_rasterstate_map;
};
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
