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

  clazz->annotateTyped<context_factory_t>("context_factory", []() { return VkContext::makeShared(); });
}

///////////////////////////////////////////////////////

bool VkContext::HaveExtension(const std::string& extname) {
  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////

vkcontext_ptr_t VkContext::makeShared() {
  struct VkContextX : public VkContext {
    VkContextX()
        : VkContext() {
    }
  };
  auto ctx = std::make_shared<VkContextX>();
  return ctx;
}

///////////////////////////////////////////////////////////////////////////////

VkContext::VkContext() {

  ///////////////////////////////////////////////////////////////
  OrkAssert(_GVI != nullptr);
  auto vk_devinfo   = _GVI->_device_infos[0];
  _vkphysicaldevice = vk_devinfo->_phydev;
  _vkdeviceinfo     = vk_devinfo;

  ////////////////////////////
  // get queue families
  ////////////////////////////

  using qset_t = std::set<uint32_t>;

  size_t num_q_types = vk_devinfo->_queueprops.size();
  std::map<int, qset_t> qids;
  for (size_t i = 0; i < num_q_types; i++) {
    auto& qprops = vk_devinfo->_queueprops[i];
    auto qflags  = qprops.queueFlags;
    if (qflags & VK_QUEUE_GRAPHICS_BIT) {
      if (qprops.queueCount > 0) {
        qids[i].insert(VK_QUEUE_GRAPHICS_BIT);
        _vkqfid_graphics = i;
      }
    }
    if (qflags & VK_QUEUE_COMPUTE_BIT) {
      if (qprops.queueCount > 0) {
        // qids[i].insert(VK_QUEUE_COMPUTE_BIT);
        //_vkqfid_compute = i;
      }
    }
    if (qflags & VK_QUEUE_TRANSFER_BIT) {
      if (qprops.queueCount > 0) {
        // qids[i].insert(VK_QUEUE_TRANSFER_BIT);
        //_vkqfid_transfer = i;
      }
    }
  }

  OrkAssert(_vkqfid_graphics != NO_QUEUE);
  // OrkAssert(_vkq_compute!=NO_QUEUE);
  // OrkAssert(_vkq_transfer!=NO_QUEUE);

  _queuePriorities.push_back(1.0f);

  _DQCIs.reserve(qids.size());
  for (auto item : qids) {
    int q_index = item.first;
    for (auto q_type : item.second) {
      VkDeviceQueueCreateInfo& DQCI = _DQCIs.emplace_back();
      initializeVkStruct(DQCI, VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
      DQCI.queueFamilyIndex = q_index;
      DQCI.queueCount       = 1;
      DQCI.pQueuePriorities = _queuePriorities.data();
    }
  }

  ////////////////////////////
  // create device
  ////////////////////////////

  VkDeviceCreateInfo DCI = {};
  initializeVkStruct(DCI, VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
  DCI.queueCreateInfoCount = _DQCIs.size();
  DCI.pQueueCreateInfos    = _DQCIs.data();
  vkCreateDevice(_vkphysicaldevice, &DCI, nullptr, &_vkdevice);

  vkGetDeviceQueue(_vkdevice, //
                   _vkqfid_graphics, //
                   0, //
                   &_vkqueue_graphics);

  ////////////////////////////
  // create command pools
  ////////////////////////////

  VkCommandPoolCreateInfo CPCI_GFX = {};
  initializeVkStruct(CPCI_GFX, VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
  CPCI_GFX.queueFamilyIndex = _vkqfid_graphics;
  CPCI_GFX.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT //
                            | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

  VkResult OK = vkCreateCommandPool(_vkdevice,
                                    &CPCI_GFX,
                                    nullptr,
                                    &_vkcmdpool_graphics);

  OrkAssert(OK == VK_SUCCESS);

  ////////////////////////////
  // create command buffers
  ////////////////////////////

  VkCommandBufferAllocateInfo CBAI_GFX = {};
  initializeVkStruct(CBAI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
  CBAI_GFX.commandPool        = _vkcmdpool_graphics;
  CBAI_GFX.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  CBAI_GFX.commandBufferCount = 1;

  _cmdbuf_pool.reset_size(4);
  for( size_t i=0; i<4; i++ ){
    auto& cmdbufimpl = _cmdbuf_pool.direct_access(i);
    OK = vkAllocateCommandBuffers( _vkdevice, //
                                   &CBAI_GFX, //
                                   &cmdbufimpl._vkcmdbuf );
    OrkAssert(OK == VK_SUCCESS);
  }

  ////////////////////////////
  // create child interfaces
  ////////////////////////////

  _dwi = std::make_shared<VkDrawingInterface>(this);
  _imi = std::make_shared<VkImiInterface>(this);
  //_rsi = std::make_shared<VkRasterStateInterface>(this);
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
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
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

///////////////////////////////////////////////////////

void VkContext::_doBeginFrame() {
  makeCurrentContext();
  VkCommandBufferBeginInfo CBBI_GFX = {};
  initializeVkStruct(CBBI_GFX, VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
  CBBI_GFX.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
                 | VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; 
  CBBI_GFX.pInheritanceInfo = nullptr;  

  _cmdbufcurframe_gfx_pri = _cmdbuf_pool.allocate();


  vkResetCommandBuffer(_cmdbufcurframe_gfx_pri->_vkcmdbuf, 0);
  vkBeginCommandBuffer(_cmdbufcurframe_gfx_pri->_vkcmdbuf, &CBBI_GFX);

}

///////////////////////////////////////////////////////
void VkContext::_doEndFrame() {
  vkEndCommandBuffer(_cmdbufcurframe_gfx_pri->_vkcmdbuf);

  VkSubmitInfo SI = {};
  initializeVkStruct(SI, VK_STRUCTURE_TYPE_SUBMIT_INFO);
  SI.commandBufferCount = 1;
  SI.pCommandBuffers    = &_cmdbufcurframe_gfx_pri->_vkcmdbuf;
  
  vkQueueSubmit(_vkqueue_graphics, 1, &SI, nullptr);
  vkQueueWaitIdle(_vkqueue_graphics);



  _cmdbuf_pool.deallocate(_cmdbufcurframe_gfx_pri);
  _cmdbufcurframe_gfx_pri = nullptr;
}

///////////////////////////////////////////////////////

ctx_platform_handle_t VkContext::_doClonePlatformHandle() const {
  OrkAssert(false);
  return ctx_platform_handle_t();
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
/*RasterStateInterface* VkContext::RSI() {

  return _rsi.get();
}*/
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
struct VkPlatformObject;
using vkplatformobject_ptr_t = std::shared_ptr<VkPlatformObject>;

struct VkPlatformObject {
  CtxGLFW* _ctxbase = nullptr;
  bool _needsInit   = true;
  void_lambda_t _bindop = [](){};
};
struct VkOneTimeInit{

  VkOneTimeInit(){
    _gplato = std::make_shared<VkPlatformObject>();
     auto global_ctxbase = CtxGLFW::globalOffscreenContext();
    _gplato->_ctxbase = global_ctxbase;
    global_ctxbase->makeCurrent();
  }
  vkplatformobject_ptr_t _gplato;
};

static vkplatformobject_ptr_t global_plato(){
  static VkOneTimeInit _ginit;
  return _ginit._gplato;
}
static vkplatformobject_ptr_t _current_plato;
static void platoMakeCurrent(vkplatformobject_ptr_t plato) {
  _current_plato = plato;
  if (plato->_ctxbase) {
    plato->_ctxbase->makeCurrent();
  }
  plato->_bindop();
}
static void platoSwapBuffers(vkplatformobject_ptr_t plato) {
  platoMakeCurrent(plato);
  if (plato->_ctxbase) {
    plato->_ctxbase->swapBuffers();
  }
}

///////////////////////////////////////////////////////////////////////

void VkContext::makeCurrentContext() {
  auto plato = _impl.getShared<VkPlatformObject>();
  platoMakeCurrent(plato);
}

///////////////////////////////////////////////////////

void VkContext::_beginRenderPass(renderpass_ptr_t renpass) {

  if( renpass->_immutable ){
    OrkAssert(false);
  }


}

void VkContext::_endRenderPass(renderpass_ptr_t renpass) {
  OrkAssert(false);

}
void VkContext::_beginSubPass(rendersubpass_ptr_t subpass) {
  OrkAssert(false);
}

void VkContext::_endSubPass(rendersubpass_ptr_t rubpass) {
  OrkAssert(false);

}

commandbuffer_ptr_t VkContext::_beginRecordCommandBuffer() {

  auto cmdbuf = std::make_shared<CommandBuffer>();
  auto vkcmdbuf = cmdbuf->_impl.makeShared<VkCommandBufferImpl>();
  _recordCommandBuffer = cmdbuf;
  return cmdbuf;

}
void VkContext::_endRecordCommandBuffer(commandbuffer_ptr_t cmdbuf) {
  OrkAssert(cmdbuf==_recordCommandBuffer);
  auto vkcmdbuf = cmdbuf->_impl.getShared<VkCommandBufferImpl>();
  _recordCommandBuffer = nullptr;

}

///////////////////////////////////////////////////////

void VkContext::swapBuffers(CTXBASE* ctxbase) {
  auto plato = _impl.getShared<VkPlatformObject>();
  platoSwapBuffers(plato);
}

///////////////////////////////////////////////////////

void VkContext::initializeWindowContext(
    Window* pWin,        //
    CTXBASE* pctxbase) { //
  meTargetType = TargetType::WINDOW;
  ///////////////////////
  auto glfw_container = (CtxGLFW*)pctxbase;
  auto glfw_window    = glfw_container->_glfwWindow;
  ///////////////////////
  vkplatformobject_ptr_t plato = std::make_shared<VkPlatformObject>();
  plato->_ctxbase              = glfw_container;
  mCtxBase                     = pctxbase;
  _impl.setShared<VkPlatformObject>(plato);
  ///////////////////////
  platoMakeCurrent(plato);
  _fbi->SetThisBuffer(pWin);
  VkResult OK = glfwCreateWindowSurface(_GVI->_instance, 
                                        glfw_window, 
                                        nullptr, 
                                        &_vkpresentationsurface);
  OrkAssert(OK == VK_SUCCESS);

} // make a window

///////////////////////////////////////////////////////

void VkContext::initializeOffscreenContext(DisplayBuffer* pbuffer) {
  meTargetType = TargetType::OFFSCREEN;
  miW = pbuffer->GetBufferW();
  miH = pbuffer->GetBufferH();
  ///////////////////////
  auto glfw_container = (CtxGLFW*) global_plato()->_ctxbase;
  auto glfw_window    = glfw_container->_glfwWindow;
  ///////////////////////
  vkplatformobject_ptr_t plato = std::make_shared<VkPlatformObject>();
  plato->_ctxbase              = glfw_container;
  mCtxBase                     = glfw_container;
  _impl.setShared<VkPlatformObject>(plato);
  ///////////////////////
  platoMakeCurrent(plato);
  _fbi->SetThisBuffer(pbuffer);
  ///////////////////////
  plato->_ctxbase = global_plato()->_ctxbase;
  plato->_needsInit   = false;
  _defaultRTG  = new RtGroup(this, miW, miH, MsaaSamples::MSAA_1X);
  auto rtb     = _defaultRTG->createRenderTarget(EBufferFormat::RGBA8);
  auto texture = rtb->texture();
  _fbi->SetBufferTexture(texture);
  ///////////////////////

} // make a pbuffer

///////////////////////////////////////////////////////
void VkContext::initializeLoaderContext() {
  meTargetType = TargetType::LOADING;

  miW = 8;
  miH = 8;

  mCtxBase = 0;

  auto plato = std::make_shared<VkPlatformObject>();
  _impl.setShared<VkPlatformObject>(plato);

  plato->_ctxbase = global_plato()->_ctxbase;
  plato->_needsInit   = false;

  _defaultRTG  = new RtGroup(this, miW, miH, MsaaSamples::MSAA_1X);
  auto rtb     = _defaultRTG->createRenderTarget(EBufferFormat::RGBA8);
  auto texture = rtb->texture();
  FBI()->SetBufferTexture(texture);

  plato->_bindop = [=]() {
    if (this->mTargetDrawableSizeDirty) {
      int w = mainSurfaceWidth();
      int h = mainSurfaceHeight();
      // printf("resizing defaultRTG<%p>\n", _defaultRTG);
      _defaultRTG->Resize(w, h);
      mTargetDrawableSizeDirty = false;
    }
  };}

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
  save_data->_pushedWindow   = current_window;
  // todo make global loading ctx current..
  // loadctx->makeCurrentContext();
  return rval;
}
///////////////////////////////////////////////////////
void VkContext::_doEndLoad(load_token_t ploadtok) {
  auto loadctx = ploadtok.getShared<VkLoadContext>();
  auto pushed  = loadctx->_pushedWindow;
  glfwMakeContextCurrent(pushed);
  _GVI->_loadTokens.push(loadctx);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
