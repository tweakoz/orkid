////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#if defined(ENABLE_GLFW) && defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#include <Cocoa/Cocoa.h>
#include <Foundation/Foundation.h>
#include <CoreData/CoreData.h>
#include <ork/kernel/objc.h>
#include <objc/objc.h>
#include <objc/message.h>
/*
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/ctxbase.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/pch.h>
*/
////////////////////////////////////////////////////////////////////////////////
//#include <ork/lev2/gfx/camera/uicam.h>
//#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include <GLFW/glfw3native.h>
//#include <ork/lev2/ui/viewport.h>
//#include <ork/lev2/ui/context.h>
//#include <ork/lev2/imgui/imgui_impl_glfw.h>
///////////////////////////////////////////////////////////////////////////////
//#include <ork/kernel/msgrouter.inl>
//#include <ork/math/basicfilters.h>
//#include <ork/lev2/gfx/dbgfontman.h>
//#include <ork/util/logger.h>
//#include "../gfx/vulkan/vulkan_ctx.h"
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
int GLFW_MODIFIER_OSCTRL = GLFW_MOD_SUPER;
bool _macosUseHIDPI = true;
void recomputeHIDPI(Context* ctx){
}
float _currentDPI() {
  return 95.0f;
}
bool _HIDPI() {
  return false;
}
void setAlwaysOnTop(GLFWwindow *window) {
    id glfwWindow = glfwGetCocoaWindow(window);
    //id nsWindow = ((id(*)(id, SEL))objc_msgSend)(glfwWindow, sel_registerName("window"));
    id nsWindow = glfwWindow;

    NSUInteger windowLevel = ((NSUInteger(*)(id, SEL))objc_msgSend)(nsWindow, sel_registerName("level"));
    windowLevel = CGWindowLevelForKey(kCGFloatingWindowLevelKey);
    ((void(*)(id, SEL, NSUInteger))objc_msgSend)(nsWindow, sel_registerName("setLevel:"), windowLevel);
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
#endif