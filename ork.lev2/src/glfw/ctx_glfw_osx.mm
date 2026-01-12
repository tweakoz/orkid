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
// Custom view subclass to handle mouse tracking for focus-follows-mouse
// (Objective-C classes must be at global scope)
///////////////////////////////////////////////////////////////////////////////
@interface FocusFollowsMouseView : NSView
@property (nonatomic, assign) GLFWwindow* glfwWindow;
@end

@implementation FocusFollowsMouseView

- (void)updateTrackingAreas {
    [super updateTrackingAreas];

    // Remove existing tracking areas
    for (NSTrackingArea* area in [self trackingAreas]) {
        [self removeTrackingArea:area];
    }

    // Add new tracking area covering entire view
    NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited |
                                    NSTrackingActiveAlways |
                                    NSTrackingInVisibleRect;
    NSTrackingArea* trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                                options:options
                                                                  owner:self
                                                               userInfo:nil];
    [self addTrackingArea:trackingArea];
}

- (void)mouseEntered:(NSEvent*)event {
    // Focus follows mouse: make this window key when mouse enters
    NSWindow* window = [self window];
    if (window && ![window isKeyWindow]) {
        [window makeKeyWindow];
    }
}

- (void)mouseExited:(NSEvent*)event {
    // Optional: could do something on exit
}

@end

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
void enableFocusFollowsMouse(GLFWwindow* glfwWindow) {
    @autoreleasepool {
        NSWindow* nsWindow = glfwGetCocoaWindow(glfwWindow);
        if (!nsWindow) return;

        NSView* contentView = [nsWindow contentView];
        if (!contentView) return;

        // Create an invisible overlay view for mouse tracking
        FocusFollowsMouseView* trackingView = [[FocusFollowsMouseView alloc] initWithFrame:[contentView bounds]];
        trackingView.glfwWindow = glfwWindow;
        trackingView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        [trackingView setWantsLayer:NO];

        // Add as subview (will be on top but transparent to clicks)
        [contentView addSubview:trackingView positioned:NSWindowAbove relativeTo:nil];

        // Force tracking area update
        [trackingView updateTrackingAreas];
    }
}
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
#endif
