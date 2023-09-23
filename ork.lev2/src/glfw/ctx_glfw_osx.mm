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
////////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/glfw/ctx_glfw.h>
#include <GLFW/glfw3native.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
int GLFW_MODIFIER_OSCTRL = GLFW_MOD_SUPER;
bool _macosUseHIDPI = false;
static float _DPI = 72.0f;
void recomputeHIDPI(GLFWwindow *window){
  // determine if we are on a retina display
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  float xscale, yscale;
  glfwGetWindowContentScale(window, &xscale, &yscale);
  _macosUseHIDPI = (xscale > 1.0f || yscale > 1.0f);

  // determine the DPI
  _DPI = 95.0f;
}
float _currentDPI() {
  return _DPI;
}
bool _HIDPI() {
  // determine if we are on a retina display
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