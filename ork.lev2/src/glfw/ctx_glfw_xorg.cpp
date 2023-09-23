////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#if defined(ENABLE_GLFW) && defined(LINUX)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_GLX
////////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include <GLFW/glfw3native.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/msgrouter.inl>
#include <ork/math/basicfilters.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/util/logger.h>
#if defined(ENABLE_VULKAN)
#include "../gfx/vulkan/vulkan_ctx.h"
#endif
///////////////////////////////////////////////////////////////////////////////
extern "C" {
#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
}
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
int GLFW_MODIFIER_OSCTRL = GLFW_MOD_CONTROL;

using window_t = ::Window;

bool g_allow_HIDPI   = false;
bool _hakHIDPI       = false;
bool _hakMixedDPI    = false;
float _hakCurrentDPI = 95.0f;
bool _HIDPI() {
  return _hakHIDPI;
}
bool _MIXEDDPI() {
  return _hakMixedDPI;
}
float _currentDPI() {
  return _hakCurrentDPI;
}
void setAlwaysOnTop(GLFWwindow *window) {
    Display *display = glfwGetX11Display();
    auto x11window = glfwGetX11Window(window);

    Atom wmStateAbove = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);

    XEvent event;
    memset(&event, 0, sizeof(event));
    event.type = ClientMessage;
    event.xclient.window = x11window;
    event.xclient.message_type = wmState;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
    event.xclient.data.l[1] = wmStateAbove;
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 0;
    event.xclient.data.l[4] = 0;

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
}


#if 1
void recomputeHIDPI(GLFWwindow *glfw_window) {

  ///////////////////////
  Display *display = glfwGetX11Display();
  auto x_window = glfwGetX11Window(glfw_window);
  int x_screen   = DefaultScreen(display);
  ///////////////////////
  int winpos_x = 0;
  int winpos_y = 0;
  window_t child;
  window_t root_window = DefaultRootWindow(display);
  XTranslateCoordinates(display, x_window, root_window, 0, 0, &winpos_x, &winpos_y, &child);
  XWindowAttributes xwa;
  XGetWindowAttributes(display, x_window, &xwa);
  winpos_x -= xwa.x;
  winpos_y -= xwa.y;

  int numlodpi = 0;
  int numhidpi = 0;
  // printf("winx: %d winy: %d\n", winpos_x, winpos_y);
  ///////////////////////
  // int DWMM       = DisplayWidthMM(display, x_screen);
  // int DHMM       = DisplayHeightMM(display, x_screen);
  // int RESW       = DisplayWidth(display, x_screen);
  // int RESH       = DisplayHeight(display, x_screen);
  // float CDPIX    = float(RESW) / float(DWMM) * 25.4f;
  // float CDPIY    = float(RESH) / float(DHMM) * 25.4f;
  // int DPIX       = QX11Info::appDpiX(x_screen);
  // int DPIY       = QX11Info::appDpiY(x_screen);
  // float avgdpi = (CDPIX + CDPIY) * 0.5f;
  // printf("qx11dpi<%d %d>\n", DPIX, DPIY);
  //_hakHIDPI = avgdpi > 180.0;

  if (0) { // get DPI for screen
    XRRScreenResources* xrrscreen = XRRGetScreenResources(display, x_window);

    // printf("display<%p> x_window<%d> xrrscreen<%p>\n", display, x_window, xrrscreen);

    if (xrrscreen) {
      for (int iscres = xrrscreen->noutput; iscres > 0;) {
        --iscres;
        XRROutputInfo* info = XRRGetOutputInfo(display, xrrscreen, xrrscreen->outputs[iscres]);
        double mm_width     = info->mm_width;
        double mm_height    = info->mm_height;

        RRCrtc crtcid          = info->crtc;
        XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(display, xrrscreen, crtcid);

        /*printf(
            "iscres<%d> info<%p> mm_width<%g> mm_height<%g> crtcid<%lu> crtc_info<%p>\n",
            iscres,
            info,
            mm_width,
            mm_height,
            crtcid,
            crtc_info);*/

        if (crtc_info) {
          double pix_left   = crtc_info->x;
          double pix_top    = crtc_info->y;
          double pix_width  = crtc_info->width;
          double pix_height = crtc_info->height;
          int rot           = crtc_info->rotation;
          float CDPIX       = pix_width / mm_width * 25.4f;
          float CDPIY       = pix_height / mm_height * 25.4f;
          float avgdpi      = (CDPIX + CDPIY) * 0.5f;
          float is_hidpi    = avgdpi > 180.0f;

          if (not g_allow_HIDPI)
            is_hidpi = false;

          if (is_hidpi)
            numhidpi++;
          else
            numlodpi++;

          if ((winpos_x >= pix_left) and (winpos_x < (pix_left + pix_width)) and (winpos_y >= pix_top) and
              (winpos_y < (pix_left + pix_height))) {
            _hakHIDPI      = is_hidpi;
            _hakCurrentDPI = avgdpi;
            // printf("_hakHIDPI<%d>\n", int(_hakHIDPI));
          }

          switch (rot) {
            case RR_Rotate_0:
              break;
            case RR_Rotate_90: //
              break;
            case RR_Rotate_180:
              break;
            case RR_Rotate_270:
              break;
          }

          // printf("  x<%g> y<%g> w<%g> h<%g> rot<%d> avgdpi<%g>\n", pix_left, pix_top, pix_width, pix_height, rot, avgdpi);

          XRRFreeCrtcInfo(crtc_info);
        }

        XRRFreeOutputInfo(info);
      } // for each screen
      // printf("numhidpi<%d>\n", numhidpi);
      // printf("numlodpi<%d>\n", numlodpi);
      _hakMixedDPI = (numhidpi > 0) and (numlodpi > 1);
      // printf("_hakMixedDPI<%d>\n", int(_hakMixedDPI));
    }
  }
}
#endif

} // namespace ork::lev2
#endif