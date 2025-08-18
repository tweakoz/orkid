#pragma once 
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
struct VulkanImageObject {
  VulkanImageObject(vkcontext_rawptr_t ctx, VkImage img, VkImageView imgview, VkFormat fmt);
  VulkanImageObject(vkcontext_rawptr_t ctx, vkimagecreateinfo_ptr_t cinfo, std::string name = "");
  ~VulkanImageObject();
  vkcontext_rawptr_t _ctx = nullptr;
  vkimagecreateinfo_ptr_t _cinfo;
  VkImage _vkimage;
  VkImageView _vkimageview;
  vkmemforimg_ptr_t _imgmem;
  VkFormat _format = VK_FORMAT_UNDEFINED;
  bool _delete_image = true;
  bool _delete_imageview = true;
  static std::atomic<int> _imgobjcount;
  static std::atomic<size_t> _imgobjSN;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanSamplerObject {
  VulkanSamplerObject(vkcontext_rawptr_t ctx, vksamplercreateinfo_ptr_t cinfo);
  vksamplercreateinfo_ptr_t _cinfo;
  VkSampler _vksampler;
};
///////////////////////////////////////////////////////////////////////////////
struct InFlightTextureTransfer {
  InFlightTextureTransfer(vkcontext_rawptr_t ctx, 
                          vkbuffer_ptr_t stg_buffer,
                          secondary_commandbuffer_ptr_t cmd_buffer);
  ~InFlightTextureTransfer();
  vkbuffer_ptr_t _staging_buffer;
  secondary_commandbuffer_ptr_t _command_buffer;
  static std::atomic<int> _xfercount;
  static std::atomic<size_t> _xferSN;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanTextureObject {

  VulkanTextureObject(vktxi_rawptr_t txi);
  ~VulkanTextureObject();

  std::unordered_set<vkbuffer_ptr_t> _staging_buffers;
  vkimageobj_ptr_t _imgobj;
  int _maxmip = 0;
  vktxi_rawptr_t _txi;
  vksampler_obj_ptr_t _vksampler;
  VkDescriptorImageInfo _vkdescriptor_info;
  secondary_commandbuffer_ptr_t _loadCB;
  uint64_t _image_params_hash = 0;

  std::unordered_set<inflighttextrans_ptr_t> _inflight_transfers;
  std::atomic<uint64_t> _dataVersion{0};

  static std::atomic<size_t> _vkto_count;
};
///////////////////////////////////////////////////////////////////////////////
struct VkTexLoadReq {
  texture_ptr_t ptex;
  const dds::DDS_HEADER* _ddsheader = nullptr;
  vktexobj_ptr_t pTEXOBJ;
  std::string _texname;
  DataBlockInputStream _inpstream;
  std::shared_ptr<CompressedImageMipChain> _cmipchain;
};
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
