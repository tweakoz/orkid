#include <ork/opencl/opencl.h>

namespace ork::opencl {

static constexpr size_t MAX_SOURCE_SIZE = 0x100000;

///////////////////////////////////////////////////////////////////////////////

Globals::Globals() {
  cl_uint num_platforms = 0;
  cl_int status         = clGetPlatformIDs(0, nullptr, &num_platforms);
  std::vector<cl_platform_id> platform_ids;
  platform_ids.resize(num_platforms);
  status = clGetPlatformIDs(num_platforms, platform_ids.data(), nullptr);
  OrkAssert(status == CL_SUCCESS);

  for (auto platform_id : platform_ids) {
    auto plat = std::make_shared<Platform>(platform_id);
    _platforms.push_back(plat);
  }
}

Globals::~Globals() {
}

///////////////////////////////////////////////////////////////////////////////

Platform::Platform(cl_platform_id pid)
    : _platform_id(pid) {

  cl_uint num_devices = 0;
  cl_int status       = clGetDeviceIDs(_platform_id, CL_DEVICE_TYPE_ALL, 0, nullptr, &num_devices);
  OrkAssert(status == CL_SUCCESS);
  std::vector<cl_device_id> devices;
  devices.resize(num_devices);
  status = clGetDeviceIDs(_platform_id, CL_DEVICE_TYPE_ALL, num_devices, devices.data(), nullptr);
  OrkAssert(status == CL_SUCCESS);

  for (auto device_id : devices) {
    auto dev = std::make_shared<Device>(device_id);
    _devices.push_back(dev);
  }
}

Platform::~Platform() {
}

///////////////////////////////////////////////////////////////////////////////

Device::Device(cl_device_id did)
    : _device_id(did) {
  // Create an OpenCL context
  cl_int status  = 0;
  cl_context ctx = clCreateContext(nullptr, 1, &_device_id, nullptr, nullptr, &status);
  OrkAssert(status == CL_SUCCESS);

  _ctx = std::make_shared<Context>(ctx, did);
}

Device::~Device() {
}

///////////////////////////////////////////////////////////////////////////////

buffer_ptr_t Context::createBuffer(cl_uint usage, size_t size, void* initial_data) {
  auto buf      = std::make_shared<Buffer>();
  cl_int status = 0;
  cl_mem mem    = clCreateBuffer(_context, usage, size, initial_data, &status);
  OrkAssert(status == CL_SUCCESS);
  buf->_cl_object = mem;
  buf->_size      = size;
  _buffers.insert(buf);
  return buf;
}

///////////////////////////////////////////////////////////////////////////////

void Context::writeBuffer(buffer_ptr_t buffer, size_t size, size_t offset, void* data) {
  cl_int status = clEnqueueWriteBuffer(
      _primary_command_queue, // command_queue
      buffer->_cl_object,     // buffer
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
  buffer->_read_data.resize(size);
  cl_int status = clEnqueueReadBuffer(
      _primary_command_queue,    // command_queue
      buffer->_cl_object,        // buffer
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
  cl_int status          = 0;
  const char* source_str = source.c_str();
  size_t source_size     = source.size();
  cl_program program     = clCreateProgramWithSource(_context, 1, (const char**)&source_str, (const size_t*)&source_size, &status);
  OrkAssert(status == CL_SUCCESS);
  status = clBuildProgram(program, 1, &_device_id, nullptr, nullptr, nullptr);
  OrkAssert(status == CL_SUCCESS);
  cl_kernel kernel = clCreateKernel(program, name.c_str(), &status);
  OrkAssert(status == CL_SUCCESS);
  auto krn         = std::make_shared<Kernel>();
  krn->_cl_object  = kernel;
  krn->_cl_program = program;
  krn->_name       = name;
  krn->_source     = source;
  _kernels.insert(krn);
  return krn;
}

///////////////////////////////////////////////////////////////////////////////

void Context::setKernelArg(kernel_ptr_t kernel, size_t index, size_t size, buffer_ptr_t buffer) {
  cl_int status = clSetKernelArg(kernel->_cl_object, index, size, &buffer->_cl_object);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

void Context::executeKernel(kernel_ptr_t kernel, size_t global_item_size, size_t local_item_size) {
  cl_int status = clEnqueueNDRangeKernel(
      _primary_command_queue, kernel->_cl_object, 1, nullptr, &global_item_size, &local_item_size, 0, nullptr, nullptr);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

Context::Context(cl_context ctx, cl_device_id did)
    : _context(ctx) {
  cl_int status          = 0;
  _primary_command_queue = clCreateCommandQueue(_context, did, 0, &status);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

Context::~Context() {
  cl_int status = clReleaseContext(_context);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

void Context::flush() {
  cl_int status = clFlush(_primary_command_queue);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

void Context::finish() {
  cl_int status = clFinish(_primary_command_queue);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

Buffer::~Buffer() {
  cl_int status = clReleaseMemObject(_cl_object);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

Kernel::~Kernel() {
  cl_int status = clReleaseKernel(_cl_object);
  OrkAssert(status == CL_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::opencl
