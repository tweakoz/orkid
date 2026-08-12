#pragma once

#include <set>
#include <unordered_map>
#include <ork/lev2/gfx/shadman.h>
#include "vk_protos.h"
#include "vk_merged_resources.h"

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
////////////////////////////////////////////////////////////////////////////////
// SHADER-SIDE PIPELINE KEY BUDGET
//
// _pipeline_bits_composite (built by VkFxInterface::_pipelineBitsForShader) packs three
// process-lifetime counters; it is the sh_pbits field of the 64-bit pipeline hash in
// vulkan_fxi_pipelines.cpp:
//
//   field                 shift  width  budget
//   program index           0     12     4096   (never recycles — grows all session long)
//   vertex interface id    12      8      256   (distinct vertex-input layouts)
//   geometry interface id  20      8      256   (distinct geometry-stage inputs)
//                                 ---
//   composite width                28           (fits a positive int; -1 = compute sentinel)
//
// The program index was 8 bits and a long VR session exhausted it (the next shader load
// tripped the assert mid-frame). Widening it moved the interface fields up — every shift
// below and the sh_pbits width in the pipeline hash move together.
////////////////////////////////////////////////////////////////////////////////

static constexpr int kbits_pipeline_bits_prg = 12;
static constexpr int kbits_pipeline_bits_vif = 8;
static constexpr int kbits_pipeline_bits_gif = 8;

static constexpr int kshift_pipeline_bits_prg = 0;
static constexpr int kshift_pipeline_bits_vif = kshift_pipeline_bits_prg + kbits_pipeline_bits_prg;
static constexpr int kshift_pipeline_bits_gif = kshift_pipeline_bits_vif + kbits_pipeline_bits_vif;

static constexpr int kbits_pipeline_bits_composite = kshift_pipeline_bits_gif + kbits_pipeline_bits_gif;

static constexpr int kmax_pipeline_bits_prg = (1 << kbits_pipeline_bits_prg);
static constexpr int kmax_pipeline_bits_vif = (1 << kbits_pipeline_bits_vif);
static constexpr int kmax_pipeline_bits_gif = (1 << kbits_pipeline_bits_gif);

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
  int _binding_id = -1; // real SPIR-V binding (matches the generated GLSL; set by DBread, after the SSBOs)
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
// Uniform blocks are UNIFORM_BUFFER_DYNAMIC by default: their contents are
// per-draw, so each draw suballocates from the global ring and hands the offset
// to vkCmdBindDescriptorSets. That costs one dynamic descriptor per block, and
// the offending layouts here reach 16 against a device limit of 15
// (VUID-VkPipelineLayoutCreateInfo-descriptorType-03030 / -pSetLayouts-03038).
//
// A block whose contents are PER-FRAME CONSTANT does not need the ring at all —
// it can live in its own buffer and bind as a plain UNIFORM_BUFFER, off the
// dynamic budget. ublk_sun is that block: the forward prologue
// (_update_sun_cascades) writes it once per frame into PBRMaterial::sunDataBuffer,
// and the per-draw path was only copying that same buffer back out and through
// the ring again.
//
// Membership is by block NAME because that is the only identity shared across
// every program that declares the block. Adding a name here is a claim that the
// block is written exactly once per frame into a dedicated buffer that some
// material binds via bindUniformBuffer — see the N=1 note at the descriptor
// write in vulkan_fxi_pipelines_bind.cpp before adding one.
/////////////////////////////////////////////////////////////////////////////////

inline bool isNonDynamicUniformBlock(const std::string& block_name) {
  // ublk_stereo joins ublk_sun for the same reason: both are per-FRAME view state written
  //  once and read by every draw. The multiview injection puts ublk_stereo in scope for
  //  every view-transforming shader, so routing it through the dynamic ring would have cost
  //  one ring suballocation and one dynamic-offset slot per draw across the whole engine —
  //  for a value that does not change between draws.
  return (block_name == "ublk_sun") or (block_name == "ublk_stereo");
}

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
  size_t _binding_id  = 0; // real SPIR-V binding (NOT _descriptor_set_id, which is the set)
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
  // sub-range bind: descriptor reads [_bound_offset, end). Same buffer at DIFFERENT offsets must hash
  // to DISTINCT descriptor sets (else the second draw reuses the first's offset) — see the descset_bits
  // combine in vulkan_fxi_pipelines_bind.cpp. Must satisfy minStorageBufferOffsetAlignment. 0 = whole.
  VkDeviceSize                  _bound_offset = 0;
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
  // VulkanTextureObject::_serial_number snapshot per slot, taken at bind time.
  // The pointer compare above is ABA-blind across publish-and-free cycles;
  // this is what actually detects "different texture at the same address".
  std::unordered_map<fxparam_constptr_t, size_t> _texture_serials;
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
  vkfxsstage_ptr_t _mshshader; // mesh stage; mutually exclusive with _vtxshader
  vkfxsstage_ptr_t _tskshader; // task (amplification) stage; only ever set alongside _mshshader
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

  // shader-side pipeline key fields — layout + budgets at the top of this header.
  // -1 = not yet assigned (compute passes keep the -1 composite sentinel).
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

  // true iff this pipeline declared VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE. Issuing
  // vkCmdSetDepthWriteEnable for a pipeline that bakes depth-write statically is an error
  // (VUID-vkCmdDraw-None-08608), so _bindPipeline gates the setter on this flag. Only the
  // MESH path declares it today — see _createPipelineMesh.
  bool _dynamicDepthWrite = false;

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

  // Per-dispatch descriptor sets. Each dispatch recorded into a command buffer needs
  // its OWN set, else multiple dispatches of this pipeline in one submit would all
  // observe the LAST binding (a single set rewritten in place). acquireDescriptorSet
  // hands out a fresh set per dispatch from a growable ring that is reused across
  // dispatch phases — a new `generation` (one per beginDispatchPhase) resets the
  // cursor so prior-phase sets (whose command buffer has completed) are recycled.
  VkDescriptorSet acquireDescriptorSet(uint64_t generation);
  void writeDescriptorSet(VkDescriptorSet set); // populate `set` from current bindings

  // Inline (frame-CB) dispatch descriptor sets: a SEPARATE small ring cycled per inline dispatch,
  // NOT recycled on the dispatch-phase generation (that recycle assumes the compute CB was
  // submitted+waited — the inline path skips that fence, so its sets must outlive the frame CB
  // they were recorded into, which stays in flight up to `frames-in-flight` frames). Depth
  // kInlineRingDepth > frames-in-flight × inline-dispatches-per-frame guarantees a slot is never
  // rewritten while a primary CB that referenced it is still executing. Distinct from the
  // per-dispatch ring so the ordinary dispatchCompute path is byte-untouched.
  VkDescriptorSet acquireInlineDescriptorSet();
  static constexpr size_t kInlineRingDepth = 8;
  std::vector<VkDescriptorSet> _inlineSetRing;
  size_t _inlineCursor = 0;

  vkcontext_rawptr_t _contextVK = nullptr;
  vkfxsstage_ptr_t _computeShader;              // VulkanFxShaderStage with SPIR-V

  VkPipeline _pipeline = VK_NULL_HANDLE;
  VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
  VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;

  // growable per-dispatch descriptor-set ring (see acquireDescriptorSet)
  void _growSetPool();
  std::vector<VkDescriptorPool> _setPools; // each holds kSetsPerPool sets; freed in dtor
  std::vector<VkDescriptorSet>  _setRing;  // all allocated sets, reused across generations
  size_t   _setCursor = 0;                 // next set to hand out in the current generation
  uint64_t _setGeneration = 0;             // generation the cursor was last reset on
  uint32_t _poolFreeSlots = 0;             // unused sets remaining in _setPools.back()
  uint32_t _ssboCount = 0, _uboCount = 0, _samplerCount = 0; // per-set descriptor counts
  bool _hasDescriptors = false;            // false => empty layout (no set to bind)

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
  bool _alphaToCoverage = false;  // copied from the ork RasterState; read at pipeline-create (MSAA state)

  using rsmap_t = std::unordered_map<uint64_t, int>;

  static LockedResource<rsmap_t> _global_rasterstate_map;
};

/////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
/////////////////////////////////////////////////////////////////////////////////