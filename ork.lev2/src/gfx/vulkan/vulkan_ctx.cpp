////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
#import <ork/lev2/glfw/ctx_glfw.h>
#include <GLFW/glfw3native.h>

ImplementReflectionX(ork::lev2::vulkan::VkContext, "VkContext");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

void VkContext::describeX(class_t* clazz) {

  clazz->annotateTyped<context_factory_t>("context_factory", [](){
    return VkContext::makeShared();
  });
}

///////////////////////////////////////////////////////

bool VkContext::HaveExtension(const std::string& extname) {
  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////

vkcontext_ptr_t VkContext::makeShared(){
  struct VkContextX : public VkContext {
    VkContextX() : VkContext() {}
  };
  auto ctx = std::make_shared<VkContextX>();
  return ctx;
}

///////////////////////////////////////////////////////////////////////////////

VkContext::VkContext() {

  ///////////////////////////////////////////////////////////////
  OrkAssert(_GVI != nullptr);
  auto vk_devinfo = _GVI->_device_infos[0];

  VkDeviceQueueCreateInfo DQCI = {};
  initializeVkStruct(DQCI,VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);

  ////////////////////////////
  // get graphics queue index
  ////////////////////////////

  size_t num_q_types = vk_devinfo->_queueprops.size();
  int gfx_q_index = -1;
  for (size_t i = 0; i < num_q_types; i++) {
    if (vk_devinfo->_queueprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        gfx_q_index = i;
        break;
    }
  }
  float queuePriority = 1.0f;
  DQCI.queueFamilyIndex = gfx_q_index; 
  DQCI.queueCount = 1;
  DQCI.pQueuePriorities = &queuePriority;

  ////////////////////////////
  // create device
  ////////////////////////////

  VkDeviceCreateInfo DCI = {};
  initializeVkStruct(DCI,VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
  DCI.queueCreateInfoCount = 1;
  DCI.pQueueCreateInfos = &DQCI;
  vkCreateDevice(vk_devinfo->_phydev, &DCI, nullptr, &_vkdevice);
  _vkphysicaldevice = vk_devinfo->_phydev;
  _device_info = vk_devinfo;
  
  ////////////////////////////
  // create child interfaces
  ////////////////////////////

  _dwi = std::make_shared<VkDrawingInterface>(this);
  _imi = std::make_shared<VkImiInterface>(this);
  _rsi = std::make_shared<VkRasterStateInterface>(this);
  _msi = std::make_shared<VkMatrixStackInterface>(this);
  _fbi = std::make_shared<VkFrameBufferInterface>(this);
  _gbi = std::make_shared<VkGeometryBufferInterface>(this);
  _txi = std::make_shared<VkTextureInterface>(this);
  _fxi = std::make_shared<VkFxInterface>(this);
#if defined(ENABLE_COMPUTE_SHADERS)
  _ci = std::make_shared<VkComputeInterface>(this);
#endif 
}

///////////////////////////////////////////////////////

VkContext::~VkContext() {
}

///////////////////////////////////////////////////////

uint32_t VkContext::_findMemoryType(    //
    uint32_t typeFilter,                //
    VkMemoryPropertyFlags properties) { //
  VkPhysicalDeviceMemoryProperties memProperties;
  initializeVkStruct(memProperties);
  vkGetPhysicalDeviceMemoryProperties(_vkphysicaldevice, &memProperties);
  for (uint32_t i=0; i<memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }
  OrkAssert(false);
  return 0;
}

///////////////////////////////////////////////////////

void VkContext::FxInit() {
}

///////////////////////////////////////////////////////////////////////

void VkContext::_doResizeMainSurface(int iw, int ih) {
}

///////////////////////////////////////////////////////

void VkContext::_doBeginFrame() {
  makeCurrentContext();
  OrkAssert(false);
}

///////////////////////////////////////////////////////
void VkContext::_doEndFrame() {
  OrkAssert(false);
}

///////////////////////////////////////////////////////

void* VkContext::_doClonePlatformHandle() const {
  OrkAssert(false);
  return nullptr;
}

//////////////////////////////////////////////
// Interfaces

FxInterface* VkContext::FXI() {
  return _fxi.get();
}

///////////////////////////////////////////////////////

ImmInterface* VkContext::IMI() {
  return _imi.get();
}

///////////////////////////////////////////////////////
RasterStateInterface* VkContext::RSI() {

  return _rsi.get();
}
///////////////////////////////////////////////////////

MatrixStackInterface* VkContext::MTXI() {
  return _msi.get();
}
///////////////////////////////////////////////////////

GeometryBufferInterface* VkContext::GBI() {
  return _gbi.get();
}
///////////////////////////////////////////////////////

FrameBufferInterface* VkContext::FBI() {
  return _fbi.get();
}
///////////////////////////////////////////////////////

TextureInterface* VkContext::TXI() {
  return _txi.get();
}
///////////////////////////////////////////////////////

#if defined(ENABLE_COMPUTE_SHADERS)
ComputeInterface* VkContext::CI() {
  return _ci.get();
};
#endif
///////////////////////////////////////////////////////

DrawingInterface* VkContext::DWI() {
  return _dwi.get();
}

///////////////////////////////////////////////////////////////////////

void VkContext::makeCurrentContext(void) {
}

// void debugLabel(GLenum target, GLuint object, std::string name);

//////////////////////////////////////////////

//////////////////////////////////////////////

// void AttachGLContext(CTXBASE* pCTFL);
// void SwapGLContext(CTXBASE* pCTFL);

///////////////////////////////////////////////////////

void VkContext::swapBuffers(CTXBASE* ctxbase) {
}

///////////////////////////////////////////////////////

void VkContext::initializeWindowContext(Window* pWin, CTXBASE* pctxbase) {

} // make a window

///////////////////////////////////////////////////////

void VkContext::initializeOffscreenContext(DisplayBuffer* pBuf) {

} // make a pbuffer

///////////////////////////////////////////////////////
void VkContext::initializeLoaderContext() {
}

///////////////////////////////////////////////////////

void VkContext::debugPushGroup(const std::string str) {
}

///////////////////////////////////////////////////////

void VkContext::debugPopGroup() {
}

///////////////////////////////////////////////////////

void VkContext::debugMarker(const std::string str) {
}

///////////////////////////////////////////////////////

void VkContext::TakeThreadOwnership() {
}

///////////////////////////////////////////////////////

bool VkContext::SetDisplayMode(DisplayMode* mode) {
  return false;
}
///////////////////////////////////////////////////////
load_token_t VkContext::_doBeginLoad() {
  load_token_t rval = nullptr;

  while (false == _GVI->_loadTokens.try_pop(rval)) {
    usleep(1 << 10);
  }
  auto save_data = rval.getShared<VkLoadContext>();

  GLFWwindow* current_window = glfwGetCurrentContext();
  save_data->_pushedWindow = current_window;
  // todo make global loading ctx current..
  //loadctx->makeCurrentContext();
  return rval;
}
///////////////////////////////////////////////////////
void VkContext::_doEndLoad(load_token_t ploadtok) {
  auto loadctx = ploadtok.getShared<VkLoadContext>();
  auto pushed = loadctx->_pushedWindow;
  glfwMakeContextCurrent(pushed);
  _GVI->_loadTokens.push(loadctx);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
