#include <ork/opencl/opencl.h>
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif

namespace ork::opencl {

struct GlobalsImpl {
  GlobalsImpl();
  std::vector<platform_ptr_t> _platforms;
};
struct PlatformImpl {
  PlatformImpl(cl_platform_id pid);
  std::vector<device_ptr_t> _devices;
  cl_platform_id _platform_id;
};
struct DeviceImpl {
  DeviceImpl(cl_device_id did);
  cl_device_id _device_id;
  context_ptr_t _ctx;
};
struct ContextImpl {
  ContextImpl(cl_context ctx, cl_device_id did);
  ~ContextImpl();
  cl_device_id _device_id;
  cl_context _context;
  cl_command_queue _primary_command_queue;
};
struct KernelImpl {
  cl_kernel _cl_object;
  cl_program _cl_program;
};
struct BufferImpl {
  cl_mem _cl_object;
};
///////////////////////////////////////////////////////////////////////////////

GlobalsImpl::GlobalsImpl() {
  cl_uint num_platforms = 0;
  cl_int status         = clGetPlatformIDs(0, nullptr, &num_platforms);
  std::vector<cl_platform_id> platform_ids;
  platform_ids.resize(num_platforms);
  status = clGetPlatformIDs(num_platforms, platform_ids.data(), nullptr);
  OrkAssert(status == CL_SUCCESS);

  for (auto platform_id : platform_ids) {
    auto plat      = std::make_shared<Platform>();
    auto plat_impl = plat->_IMPL.makeShared<PlatformImpl>(platform_id);
    _platforms.push_back(plat);
  }
}

///////////////////////////////////////////////////////////////////////////////

PlatformImpl::PlatformImpl(cl_platform_id pid) {
  _platform_id        = pid;
  cl_uint num_devices = 0;
  cl_int status       = clGetDeviceIDs(_platform_id, CL_DEVICE_TYPE_ALL, 0, nullptr, &num_devices);
  OrkAssert(status == CL_SUCCESS);
  std::vector<cl_device_id> devices;
  devices.resize(num_devices);
  status = clGetDeviceIDs(_platform_id, CL_DEVICE_TYPE_ALL, num_devices, devices.data(), nullptr);
  OrkAssert(status == CL_SUCCESS);

  for (auto device_id : devices) {
    auto dev = std::make_shared<Device>();
    auto dev_impl = dev->_IMPL.makeShared<DeviceImpl>(device_id);
    _devices.push_back(dev);
  }
}

DeviceImpl::DeviceImpl(cl_device_id did)
    : _device_id(did) {
  // Create an OpenCL context
  cl_int status  = 0;
  cl_context ctx = clCreateContext(nullptr, 1, &_device_id, nullptr, nullptr, &status);
  OrkAssert(status == CL_SUCCESS);

  _ctx = std::make_shared<Context>();
  auto ctx_impl = _ctx->_IMPL.makeShared<ContextImpl>(ctx, did);
}

///////////////////////////////////////////////////////////////////////////////

ContextImpl::ContextImpl(cl_context ctx, cl_device_id did)
    : _context(ctx) {
  cl_int status          = 0;
  _primary_command_queue = clCreateCommandQueue(_context, did, 0, &status);
  OrkAssert(status == CL_SUCCESS);
}

ContextImpl::~ContextImpl(){
  cl_int status = clReleaseCommandQueue(_primary_command_queue);
  OrkAssert(status == CL_SUCCESS);
  status = clReleaseContext(_context);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

Globals::Globals() {
  auto impl = _IMPL.makeShared<GlobalsImpl>();
}

///////////////////////////////////////////////////////////////////////////////

Platform::Platform(){
}

///////////////////////////////////////////////////////////////////////////////


Device::Device() {

}

///////////////////////////////////////////////////////////////////////////////

buffer_ptr_t Context::createBuffer(BufferUsage usage, size_t size, void* initial_data) {

  cl_mem_flags usage_flags = 0;
  switch (usage) {
    case BufferUsage::READ_WRITE:
      usage_flags = CL_MEM_READ_WRITE;
      break;
    case BufferUsage::WRITE_ONLY:
      usage_flags = CL_MEM_WRITE_ONLY;
      break;
    case BufferUsage::READ_ONLY:
      usage_flags = CL_MEM_READ_ONLY;
      break;
    default:
      OrkAssert(false);
  }


  auto impl     = _IMPL.getShared<ContextImpl>();
  auto buf      = std::make_shared<Buffer>();
  auto buf_impl = buf->_IMPL.makeShared<BufferImpl>();
  cl_int status = 0;
  cl_mem mem    = clCreateBuffer(impl->_context, usage_flags, size, initial_data, &status);
  OrkAssert(status == CL_SUCCESS);
  buf_impl->_cl_object = mem;
  buf->_size      = size;
  _buffers.insert(buf);
  return buf;
}

///////////////////////////////////////////////////////////////////////////////

void Context::writeBuffer(buffer_ptr_t buffer, size_t size, size_t offset, void* data) {
  auto impl     = _IMPL.getShared<ContextImpl>();
  auto buf_impl = buffer->_IMPL.getShared<BufferImpl>();
  cl_int status = clEnqueueWriteBuffer(
      impl->_primary_command_queue, // command_queue
      buf_impl->_cl_object,     // buffer
      CL_TRUE,                // blocking_write
      offset,                 // offset
      size,                   // size
      data,                   // ptr
      0,                      // num_events_in_wait_list
      nullptr,                // event_wait_list
      nullptr);               // event
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

void Context::readBuffer(buffer_ptr_t buffer, size_t size, size_t offset) {
  auto impl     = _IMPL.getShared<ContextImpl>();
  auto buf_impl = buffer->_IMPL.getShared<BufferImpl>();
  buffer->_read_data.resize(size);
  cl_int status = clEnqueueReadBuffer(
      impl->_primary_command_queue,    // command_queue
      buf_impl->_cl_object,        // buffer
      CL_TRUE,                   // blocking_read
      offset,                    // offset
      size,                      // size
      buffer->_read_data.data(), // ptr
      0,                         // num_events_in_wait_list
      nullptr,                   // event_wait_list
      nullptr);                  // event
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

kernel_ptr_t Context::createKernelFromString(const std::string& name, const std::string& source) {
  auto impl     = _IMPL.getShared<ContextImpl>();
  cl_int status          = 0;
  const char* source_str = source.c_str();
  size_t source_size     = source.size();
  cl_program program     = clCreateProgramWithSource(impl->_context, 1, (const char**)&source_str, (const size_t*)&source_size, &status);
  OrkAssert(status == CL_SUCCESS);
  status = clBuildProgram(program, 1, &impl->_device_id, nullptr, nullptr, nullptr);
  OrkAssert(status == CL_SUCCESS);
  cl_kernel kernel = clCreateKernel(program, name.c_str(), &status);
  OrkAssert(status == CL_SUCCESS);
  auto krn         = std::make_shared<Kernel>();
  auto krn_impl    = krn->_IMPL.makeShared<KernelImpl>(); 
  krn_impl->_cl_object  = kernel;
  krn_impl->_cl_program = program;
  krn->_name       = name;
  krn->_source     = source;
  _kernels.insert(krn);
  return krn;
}

///////////////////////////////////////////////////////////////////////////////

void Context::setKernelArg(kernel_ptr_t kernel, size_t index, size_t size, buffer_ptr_t buffer) {
  auto buf_impl = buffer->_IMPL.getShared<BufferImpl>();
  auto krn_impl = kernel->_IMPL.getShared<KernelImpl>();
  cl_int status = clSetKernelArg(krn_impl->_cl_object, index, size, &buf_impl->_cl_object);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

void Context::executeKernel(kernel_ptr_t kernel, size_t global_item_size, size_t local_item_size) {
  auto impl     = _IMPL.getShared<ContextImpl>();
  auto krn_impl = kernel->_IMPL.getShared<KernelImpl>();
  cl_int status = clEnqueueNDRangeKernel(
      impl->_primary_command_queue, krn_impl->_cl_object, 1, nullptr, &global_item_size, &local_item_size, 0, nullptr, nullptr);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

Context::Context() {}

Context::~Context(){
  auto impl = _IMPL.getShared<ContextImpl>();
  for( auto buf : _buffers ){
    auto buf_impl = buf->_IMPL.getShared<BufferImpl>();
    cl_int status = clReleaseMemObject(buf_impl->_cl_object);
    OrkAssert(status == CL_SUCCESS);
  }
  for( auto krn : _kernels ){
    auto krn_impl = krn->_IMPL.getShared<KernelImpl>();
    cl_int status = clReleaseKernel(krn_impl->_cl_object);
    OrkAssert(status == CL_SUCCESS);
    status = clReleaseProgram(krn_impl->_cl_program);
    OrkAssert(status == CL_SUCCESS);
  }
  // _IMPL will now be destroyed
}

///////////////////////////////////////////////////////////////////////////////

void Context::flush() {
  auto impl     = _IMPL.getShared<ContextImpl>();
  cl_int status = clFlush(impl->_primary_command_queue);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

void Context::finish() {
  auto impl     = _IMPL.getShared<ContextImpl>();
  cl_int status = clFinish(impl->_primary_command_queue);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::opencl
