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
  VkImage _vkimage = VK_NULL_HANDLE;
  VkImageView _vkimageview = VK_NULL_HANDLE;
  VkDeviceMemory _vkdevicemem = VK_NULL_HANDLE;  // For external memory (IOSurface) - dedicated allocation
  vkmemforimg_ptr_t _imgmem;
  VkFormat _format = VK_FORMAT_UNDEFINED;
  VkImageLayout _currentLayout = VK_IMAGE_LAYOUT_UNDEFINED; // Track actual image layout
  size_t _serial_number = 0; // Unique ID to prevent hash collisions from address/handle reuse
  size_t _deletion_frame = 0; // Frame number when marked for deletion (for deferred cleanup)
  vkfence_obj_ptr_t _deletion_fence;  // Fence to wait for before deletion (for external memory)
  svar256_t _external_handle_keepalive;  // Keep external handle (IOSurfaceHandle) alive until deletion
  bool _delete_image = true;
  bool _delete_imageview = true;
  bool _delete_devicemem = false;  // For external memory allocations
  static std::atomic<int> _imgobjcount;
  static std::atomic<size_t> _imgobjSN;
};
///////////////////////////////////////////////////////////////////////////////
struct VulkanSamplerObject {
  VulkanSamplerObject(vkcontext_rawptr_t ctx, vksamplercreateinfo_ptr_t cinfo);
  vksamplercreateinfo_ptr_t _cinfo;
  VkSampler _vksampler = VK_NULL_HANDLE;
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

  // Get the image object that should be used for sampling (the stable one, not being updated)
  vkimageobj_ptr_t samplingImage() const {
    return _img_sampling;
  }

  std::unordered_set<vkbuffer_ptr_t> _staging_buffers;

  // For regular textures: double-buffer for async uploads
  vkimageobj_ptr_t _imgobj[2];       // Ping-pong between two images for async uploads
  int _update_index = 0;             // Which slot is being updated (0 or 1)

  // Chunked-upload double-buffered streaming bookkeeping. Active when the
  // owning lev2::Texture has _streaming=true. Both _imgobj slots are
  // allocated at reserve time; uploads write to _imgobj[1 - _front_idx]
  // ("back"); finalizeUpload flips _front_idx on GPU completion. Render
  // samples _imgobj[_front_idx] via _descset_sampling, which is repointed
  // at the new front in the same completion callback.
  std::atomic<int> _front_idx{0};

  // Swap sequence numbers. `_swap_seq_next` is incremented by each
  // streaming finalizeUpload to stamp its completion callback. The
  // callback compares against `_swap_seq_applied` and only applies its
  // swap if its stamp is the latest; older swaps are dropped.
  // Reason: orkid's completion semaphore poll iterates an unordered_set,
  // so when two finalizes are signaled at the same poll they can fire in
  // hash order rather than submission order. Without the seq check, an
  // older swap firing after a newer one would revert _front_idx, making
  // the visible layer appear to "go backwards a frame".
  std::atomic<uint64_t> _swap_seq_next{0};
  std::atomic<uint64_t> _swap_seq_applied{0};

  // For external memory (IOSurface): VkImages are owned by IoSurfaceTexImpl (1:1 mapping)
  // VulkanExternalTextureImpl manages triple-buffering and provides frames via getLatestFrame()

  // Common: currently sampled image
  vkimageobj_ptr_t _img_sampling;    // Points to the image currently being sampled (null = not ready)

  int _maxmip = 0;
  vktxi_rawptr_t _txi;
  vksampler_obj_ptr_t _vksampler;

  // Descriptor info
  using vkdescriptorinfo_ptr_t = std::shared_ptr<VkDescriptorImageInfo>;
  vkdescriptorinfo_ptr_t _vkdescriptor_info[2];  // For regular textures (per ping-pong slot)
  vkdescriptorinfo_ptr_t _descset_sampling;      // Points to active descriptor (mirrors _img_sampling)

  secondary_commandbuffer_ptr_t _loadCB;
  uint64_t _format_hash = 0;
  boost::Crc64 _imgview_hash;
  std::unordered_set<inflighttextrans_ptr_t> _inflight_transfers;
  std::atomic<uint64_t> _dataVersion{0};
  size_t _deletion_frame = 0;           // Frame number when marked for deletion (for deferred cleanup)

  // Monotonic identity, same role as VulkanImageObject::_serial_number above:
  // defeats ADDRESS reuse. Binders must never decide "same texture" by
  // comparing VulkanTextureObject pointers — the publish path frees one and
  // allocates the next, and the allocator does hand back the same address.
  size_t _serial_number = 0;

  static std::atomic<size_t> _vkto_count;
  static std::atomic<size_t> _vktoSN;
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
// External Texture Implementation (IOSurface/VideoToolbox)
// DOUBLE-BUFFER PING-PONG: VideoToolboxBackend manages buffering, Vulkan just imports
///////////////////////////////////////////////////////////////////////////////
struct IoSurfaceTexImpl;  // Forward declaration (defined in movie_playback_videotoolbox.mm)
using iosurfaceteximpl_ptr_t = std::shared_ptr<IoSurfaceTexImpl>;
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
