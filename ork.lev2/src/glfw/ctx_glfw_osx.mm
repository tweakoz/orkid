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
  _macosUseHIDPI = false; //(xscale > 1.0f || yscale > 1.0f);
  //printf("w<%d> h<%d> xscale<%f> yscale<%f>\n", width, height, xscale, yscale);
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
void activateWindow(GLFWwindow *window) {
   auto ctx = (CtxGLFW*)glfwGetWindowUserPointer(window);
    //printf("MacOs Activate Window<%p> w<%d> h<%d>\n", window, ctx->_width, ctx->_height);
    id nsWindow = glfwGetCocoaWindow(window);
    //////////////////////
    // we need to set the content view as first responder
    // in order to get mouse and keyboard events
    // on first focus
    //////////////////////
    auto contentView = (NSView *) [nsWindow contentView];
    [NSApp activateIgnoringOtherApps:YES];
    [nsWindow makeKeyAndOrderFront:nil];
    [nsWindow makeFirstResponder:contentView];
    [nsWindow makeKeyWindow];
    [nsWindow makeMainWindow];
}
void setAlwaysOnTop(GLFWwindow *window) {
    id glfwWindow = glfwGetCocoaWindow(window);
    //id nsWindow = ((id(*)(id, SEL))objc_msgSend)(glfwWindow, sel_registerName("window"));
    id nsWindow = glfwWindow;

    NSUInteger windowLevel = ((NSUInteger(*)(id, SEL))objc_msgSend)(nsWindow, sel_registerName("level"));
    windowLevel = CGWindowLevelForKey(kCGFloatingWindowLevelKey);
    ((void(*)(id, SEL, NSUInteger))objc_msgSend)(nsWindow, sel_registerName("setLevel:"), windowLevel);
}
void windowToFront(GLFWwindow* window) {
    @autoreleasepool {
        NSWindow* nswin = glfwGetCocoaWindow(window);
        if (nswin) {
            // Make the app a regular app (not a background app)
            [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
            
            // Activate the app, bringing it to front
            [NSApp activateIgnoringOtherApps:YES];
            
            // Make window key and order front
            [nswin makeKeyAndOrderFront:nil];
            [nswin orderFrontRegardless];
            
            // Set window level temporarily to force it on top
            [nswin setLevel:NSFloatingWindowLevel];
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 0.1 * NSEC_PER_SEC), dispatch_get_main_queue(), ^{
                [nswin setLevel:NSNormalWindowLevel];
            });
            
            // Force focus
            [nswin makeFirstResponder:nil];
            [nswin makeKeyWindow];
            [nswin makeMainWindow];
        }
    }
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
#endif