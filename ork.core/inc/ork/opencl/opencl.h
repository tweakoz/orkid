#pragma once

#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <memory>
#include <unordered_set>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

namespace ork::opencl {

struct Platform;
struct Device;
struct Context;
struct Buffer;
struct Kernel;

using platform_ptr_t = std::shared_ptr<Platform>;
using device_ptr_t   = std::shared_ptr<Device>;
using context_ptr_t  = std::shared_ptr<Context>;
using buffer_ptr_t   = std::shared_ptr<Buffer>;
using kernel_ptr_t   = std::shared_ptr<Kernel>;

///////////////////////////////////////////////////////////////////////////////

struct Globals {
  Globals();
  ~Globals();
  std::vector<platform_ptr_t> _platforms;
};

///////////////////////////////////////////////////////////////////////////////

struct Platform {
  Platform(cl_platform_id pid);
  ~Platform();
  std::vector<device_ptr_t> _devices;
  cl_platform_id _platform_id;
};

///////////////////////////////////////////////////////////////////////////////

struct Device {
  Device(cl_device_id did);
  ~Device();
  cl_device_id _device_id;
  context_ptr_t _ctx;
};

///////////////////////////////////////////////////////////////////////////////

struct Context {

  Context(cl_context ctx, cl_device_id did);
  ~Context();

  buffer_ptr_t createBuffer(cl_uint usage, size_t size, void* initial_data = nullptr);
  void writeBuffer(buffer_ptr_t buffer, size_t size, size_t offset, void* data);
  void readBuffer(buffer_ptr_t buffer, size_t size, size_t offset);

  kernel_ptr_t createKernelFromString(const std::string& name, const std::string& source);
  void setKernelArg(kernel_ptr_t kernel, size_t index, size_t size, buffer_ptr_t buffer);

  void executeKernel(kernel_ptr_t kernel, size_t global_item_size, size_t local_item_size);
  void flush();
  void finish();

  cl_device_id _device_id;
  cl_context _context;
  cl_command_queue _primary_command_queue;
  std::unordered_set<buffer_ptr_t> _buffers;
  std::unordered_set<kernel_ptr_t> _kernels;
};

///////////////////////////////////////////////////////////////////////////////

struct Buffer {
  ~Buffer();
  cl_mem _cl_object;
  size_t _size = 0;
  std::vector<uint8_t> _read_data;
};

///////////////////////////////////////////////////////////////////////////////

struct Kernel {
  ~Kernel();
  std::string _name;
  std::string _source;
  cl_kernel _cl_object;
  cl_program _cl_program;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::opencl
