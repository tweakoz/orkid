////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//#include <ork/math/raytracer.h>
#include <ork/lev2/lev2_types.h>
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>
#include <ork/math/frustum.h>
#include <ork/kernel/thread_pool.h>
//#include <ork/util/avi_utils.h>

class QTimer;

template <class Interface> inline void SafeRelease(Interface** ppInterfaceToRelease) {
  if (*ppInterfaceToRelease != NULL) {
    (*ppInterfaceToRelease)->Release();

    (*ppInterfaceToRelease) = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////

class render_graph;
class thread_pool;

///////////////////////////////////////////////////////////////////////////////

class DemoApp {
public:
  DemoApp(int iw, int ih);
  ~DemoApp();

  // Process and dispatch messages
  void Run();

private:
  void Render1();
  void Render2();

  ork::lev2::appwindow_ptr_t _appwin;
  int miNumAviFrames                         = 0;
  int miFrameIndex                           = 0;
  u32* mpFrameBuffer                         = nullptr;
  render_graph* mRenderGraph                 = nullptr;
  ork::threadpool::thread_pool* mpThreadPool = nullptr;
  QTimer* mpTimer                            = nullptr;
  int miWidth                                = 0;
  int miHeight                               = 0;
};
