////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#include <ork/lev2/gfx/egl.inl>
#include <GL/gl.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
//#include <xf86drm.h>
#include <xf86drmMode.h>

static const EGLint configAttribs[] = {
    EGL_SURFACE_TYPE,
    EGL_PBUFFER_BIT,
    EGL_BLUE_SIZE,
    8,
    EGL_GREEN_SIZE,
    8,
    EGL_RED_SIZE,
    8,
    EGL_DEPTH_SIZE,
    8,
    EGL_RENDERABLE_TYPE,
    EGL_OPENGL_BIT,
    EGL_NONE};

static const int pbufferWidth  = 9;
static const int pbufferHeight = 9;

static const EGLint pbufferAttribs[] = {
    EGL_WIDTH,
    pbufferWidth,
    EGL_HEIGHT,
    pbufferHeight,
    EGL_NONE,
};

int main(int argc, char* argv[]) {
  // 1. Initialize EGL
  EGLDisplay eglDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  EGLint major, minor;

  eglInitialize(eglDpy, &major, &minor);

  static const int MAX_DEVICES = 32;
  EGLDeviceEXT eglDevs[MAX_DEVICES];
  EGLint numDevices;

  auto eglQueryDevicesEXT       = getEglMethod<PFNEGLQUERYDEVICESEXTPROC>("eglQueryDevicesEXT");
  auto eglQueryDeviceAttribEXT  = getEglMethod<PFNEGLQUERYDEVICEATTRIBEXTPROC>("eglQueryDevicesEXT");
  auto eglQueryDeviceStringEXT  = getEglMethod<PFNEGLQUERYDEVICESTRINGEXTPROC>("eglQueryDeviceStringEXT");
  auto eglGetPlatformDisplayEXT = getEglMethod<PFNEGLGETPLATFORMDISPLAYEXTPROC>("eglGetPlatformDisplayEXT");
  auto eglGetOutputPortsEXT     = getEglMethod<PFNEGLGETOUTPUTPORTSEXTPROC>("eglGetOutputPortsEXT");
  auto eglGetOutputLayersEXT    = getEglMethod<PFNEGLGETOUTPUTLAYERSEXTPROC>("eglGetOutputLayersEXT");
  auto eglCreateStreamKHR       = getEglMethod<PFNEGLCREATESTREAMKHRPROC>("eglCreateStreamKHR");
  auto eglDestroyStreamKHR      = getEglMethod<PFNEGLDESTROYSTREAMKHRPROC>("eglDestroyStreamKHR");
  auto eglQueryStreamKHR        = getEglMethod<PFNEGLQUERYSTREAMKHRPROC>("eglQueryStreamKHR");

  eglQueryDevicesEXT(MAX_DEVICES, eglDevs, &numDevices);

  printf("numdevices<%d>\n", numDevices);

  for (int idev = 0; idev < numDevices; idev++) {
    auto egldev = eglDevs[idev];
    printf("///////////////////////////////////////////////////////////////////////////////\n");
    printf("dev<%d> egldev<%p>\n", idev, egldev);

    eglDpy = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, egldev, 0);

    EGLint majorVersion, minorVersion;
    eglInitialize(eglDpy, &majorVersion, &minorVersion);

    printf("dev<%d> eglDpy<%p> MAJOR<%d> MINOR<%d>\n", idev, eglDpy, majorVersion, minorVersion);

    const char* drmDeviceFile = eglQueryDeviceStringEXT(egldev, EGL_DRM_DEVICE_FILE_EXT);
    printf("dev<%d> drmDeviceFile<%s>\n", idev, drmDeviceFile);

    if (drmDeviceFile != nullptr) {
      int drmfd      = open(drmDeviceFile, O_RDWR);
      auto resources = drmModeGetResources(drmfd);
      printf("dev<%d> drmresources<%p>\n", idev, (void*) resources);
      close(drmfd);
    }

    const char* clientAPIs = eglQueryString(eglDpy, EGL_CLIENT_APIS);
    printf("dev<%d> clientAPIs<%s>\n", idev, clientAPIs);

    const char* vendor = eglQueryString(eglDpy, EGL_VENDOR);
    printf("dev<%d> vendor<%s>\n", idev, vendor);

    const char* extensions = eglQueryString(eglDpy, EGL_EXTENSIONS);
    printf("dev<%d> extensions<%s>\n", idev, extensions);

    eglBindAPI(EGL_OPENGL_API);

    EGLint num_ports = 0;
    eglGetOutputPortsEXT(eglDpy, nullptr, nullptr, 0, &num_ports);
    printf("dev<%d> num_ports<%d>\n", idev, num_ports);

    EGLint num_layers = 0;
    eglGetOutputLayersEXT(eglDpy, nullptr, nullptr, 0, &num_layers);
    printf("dev<%d> num_layers<%d>\n", idev, num_layers);

    //////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////
    if ((vendor != nullptr) and (0 == strcmp(vendor, "NVIDIA"))) {

      const EGLint streamAttributes[] = {
          EGL_CONSUMER_LATENCY_USEC_KHR,
          0,
          EGL_NONE,
      };
      auto stream = eglCreateStreamKHR(eglDpy, &streamAttributes[0]);
      printf("dev<%d> eglstream<%p>\n", idev, stream);
      EGLint state;
      eglQueryStreamKHR(eglDpy, stream, EGL_STREAM_STATE_KHR, &state);
      printf("dev<%d> eglstream-state<%d>\n", idev, state);

      eglDestroyStreamKHR(eglDpy, stream);
      // EGL_CUDA_DEVICE_NV crashes (sometimes)
      // EGLAttrib cudaIndex;
      // eglQueryDeviceAttribEXT(egldev, EGL_CUDA_DEVICE_NV, &cudaIndex);
      // printf("dev<%d> cudaidx<%d>\n", idev, int(cudaIndex));
    }
    //////////////////////////////////////////////////////////////

    // 2. Select an appropriate configuration
    EGLint numConfigs;
    EGLConfig eglCfg;

    eglChooseConfig(eglDpy, configAttribs, &eglCfg, 1, &numConfigs);

    // 3. Create a surface
    EGLSurface eglSurf = eglCreatePbufferSurface(eglDpy, eglCfg, pbufferAttribs);

    // 4. Bind the API

    // 5. Create a context and make it current
    EGLContext eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, NULL);

    printf("eglctx<%p>\n", eglCtx);
    if (eglCtx) {
      eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);
      auto renderer_string = glGetString(GL_RENDERER);
      printf("dev<%d> renderer<%s>\n", idev, renderer_string);
    }
    // 6. Terminate EGL when finished
    eglTerminate(eglDpy);

    printf("///////////////////////////////////////////////////////////////////////////////\n");
  }

  return 0;
}
