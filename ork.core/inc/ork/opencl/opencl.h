#pragma once

#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/kernel/svariant.h>
#include <ork/util/crc.h>
#include <memory>
#include <unordered_set>


namespace ork::opencl {

struct Globals;
struct Platform;
struct Device;
struct Context;
struct Buffer;
struct Kernel;

using globals_ptr_t  = std::shared_ptr<Globals>;
using platform_ptr_t = std::shared_ptr<Platform>;
using device_ptr_t   = std::shared_ptr<Device>;
using context_ptr_t  = std::shared_ptr<Context>;
using buffer_ptr_t   = std::shared_ptr<Buffer>;
using kernel_ptr_t   = std::shared_ptr<Kernel>;

///////////////////////////////////////////////////////////////////////////////

struct Globals {
  Globals();
  svarshp_t _IMPL;
  std::vector<platform_ptr_t> _platforms;
};

///////////////////////////////////////////////////////////////////////////////

struct Platform {
  Platform();
  std::vector<device_ptr_t> _devices;
  svarshp_t _IMPL;
  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////

struct Device {
  Device();
  svarshp_t _IMPL;
};

///////////////////////////////////////////////////////////////////////////////

enum class BufferUsage : uint64_t {
  CrcEnum(READ_WRITE),
  CrcEnum(WRITE_ONLY),
  CrcEnum(READ_ONLY),
};

struct Context {

  Context();
  ~Context();

  buffer_ptr_t createBuffer(BufferUsage usage, size_t size, void* initial_data = nullptr);
  void writeBuffer(buffer_ptr_t buffer, size_t size, size_t offset, void* data);
  void readBuffer(buffer_ptr_t buffer, size_t size, size_t offset);

  kernel_ptr_t createKernelFromString(const std::string& name, const std::string& source);
  void setKernelArg(kernel_ptr_t kernel, size_t index, size_t size, buffer_ptr_t buffer);

  void executeKernel(kernel_ptr_t kernel, size_t global_item_size, size_t local_item_size);
  void flush();
  void finish();

  std::unordered_set<buffer_ptr_t> _buffers;
  std::unordered_set<kernel_ptr_t> _kernels;
  svarshp_t _IMPL;
};

///////////////////////////////////////////////////////////////////////////////

struct Buffer {
  size_t _size = 0;
  std::vector<uint8_t> _read_data;
  svarshp_t _IMPL;
};

///////////////////////////////////////////////////////////////////////////////

struct Kernel {
  std::string _name;
  std::string _source;
  svarshp_t _IMPL;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::opencl
