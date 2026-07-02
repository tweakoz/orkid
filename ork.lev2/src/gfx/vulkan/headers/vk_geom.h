#pragma once 
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
struct VertexStreamConfigItem {
  std::string _vbuf_datatype;
  std::string _semantic;
  size_t _datasize   = 0;
  size_t _dataoffset = 0;
  VkFormat _vkformat = VK_FORMAT_UNDEFINED;
};

struct VertexStreamConfig {

  void addItem(std::string sem, std::string vb_dt, size_t ds, size_t offset, VkFormat fmt);
  std::unordered_map<std::string, vertex_strconfig_item_ptr_t> _item_by_semantic;
  size_t _stride = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct VkVertexInputConfiguration {
  VkVertexInputBindingDescription _binding_description;
  std::vector<VkVertexInputAttributeDescription> _attribute_descriptions;
  VkPipelineVertexInputStateCreateInfo _vertex_input_state;
  int _pipeline_bits = -1;
};

struct VkPrimitiveClass {
  VkPipelineInputAssemblyStateCreateInfo _input_assembly_state;
  PrimitiveType _primtype;
  int _pipeline_bits = -1;
};

struct VulkanVertexBuffer {
  VulkanVertexBuffer(vkcontext_rawptr_t ctx, VertexBufferBase& vbuf);
  ~VulkanVertexBuffer();
  int pipelineBitsForFormat() const;
  vkbuffer_ptr_t _vkbuffer = VK_NULL_HANDLE;
  vkcontext_rawptr_t _ctx  = nullptr;
  VertexBufferBase& _ork_vtxbuf;
  std::unordered_map<uint64_t, vkvertexinputconfig_ptr_t> _vif_to_layout;

  // DEVICE-resident (static VB) lock path: Lock hands out this host temp instead of a
  // direct map; UnLock stages it up via copyFromHost and frees it. _ever_uploaded makes a
  // RE-lock read-correct (temp is pre-filled from the device copy before returning).
  void*  _lock_temp     = nullptr;
  size_t _lock_temp_len = 0;
  size_t _lock_temp_off = 0;
  bool   _ever_uploaded = false;

  // vkvertexinputconfig_ptr_t _vertexConfig;
};
struct VulkanIndexBuffer {
  VulkanIndexBuffer(vkcontext_rawptr_t ctx, size_t length, bool device_local = false);
  ~VulkanIndexBuffer();
  vkbuffer_ptr_t _vkbuffer = VK_NULL_HANDLE;
  vkcontext_rawptr_t _ctx  = nullptr;
  // DEVICE-resident (static IB) lock path — same shape as VulkanVertexBuffer.
  void*  _lock_temp     = nullptr;
  size_t _lock_temp_len = 0;
  size_t _lock_temp_off = 0;
  bool   _ever_uploaded = false;
};
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
