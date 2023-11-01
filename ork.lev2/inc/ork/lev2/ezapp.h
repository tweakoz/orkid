////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/varmap.inl>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/file/file.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/glfw/ctx_glfw.h>

#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/context.h>

namespace ork::imgui {
void initModule(appinitdata_ptr_t initdata);
}

namespace ork::lev2 {
////////////////////////////////////////////////////////////////////////////////
static constexpr uint64_t KAPPSTATEFLAG_UPDRUNNING = 1 << 0;
static constexpr uint64_t KAPPSTATEFLAG_JOINED     = 1 << 1;
////////////////////////////////////////////////////////////////////////////////

struct EzAppContext {

public:
  static ezappctx_ptr_t get(appinitdata_ptr_t appinitdata = nullptr);
  ~EzAppContext();

private:
  EzAppContext(appinitdata_ptr_t appinitdata=nullptr);

  opq::TrackCurrent* _trackq;
  ork::opq::opq_ptr_t _mainq;
  ork::opq::opq_ptr_t _conq;
  appinitdata_ptr_t _initdata;
  file::Path _orkidWorkspaceDir;
  stringpoolctx_ptr_t _stringpoolctx;
};
////////////////////////////////////////////////////////////////////////////////
struct EzMainWin {
public:
  typedef std::function<void(ui::drawevent_constptr_t)> drawcb_t;
  typedef std::function<void(int w, int h)> onresizecb_t;

  typedef std::function<void(Context* ctx)> ongpuinit_t;
  typedef std::function<void(Context* ctx)> ongpuupdate_t;
  typedef std::function<void(Context* ctx)> ongpuexit_t;
  typedef std::function<void(ui::updatedata_ptr_t upd)> onupdate_t;
  typedef std::function<void()> onupdateinit_t;
  typedef std::function<void()> onupdateexit_t;

  typedef std::function<void(Context* ctx, scenegraph::scene_ptr_t)> ongpuinitwitchscene_t;
  typedef std::function<void(ui::updatedata_ptr_t upd, scenegraph::scene_ptr_t)> onupdatewithscene_t;

  typedef std::function<ui::HandlerResult(ui::event_constptr_t ev)> onuieventcb_t;

  EzMainWin(OrkEzApp& app);
  ~EzMainWin();

  void _updateEnqueueLockedAndReleaseFrame(DrawableBuffer*dbuf);
  void _updateEnqueueUnlockedAndReleaseFrame(DrawableBuffer*dbuf);

  const DrawableBuffer* _tryAcquireDrawBuffer(ui::drawevent_constptr_t drawEvent);
  DrawableBuffer* _tryAcquireUpdateBuffer();
  void _releaseAcquireUpdateBuffer(DrawableBuffer*);

  void _beginFrame(const DrawableBuffer*dbuf);
  void _endFrame(const DrawableBuffer*dbuf);

  void withAcquiredUpdateDrawBuffer(int debugcode,std::function<void(const AcquiredUpdateDrawBuffer& udb)> l);
  //void withStandardCompositorFrameRender(ui::drawevent_constptr_t drawEvent, StandardCompositorFrame& sframe);


  void enqueueWindowResize(int w, int h);

  OrkEzApp& _app;

  bool _update_rendersync                   = false;
  Context* _curframecontext                 = nullptr;
  appwindow_ptr_t _appwin                        = nullptr;
  CtxGLFW* _ctqt                            = nullptr;
  drawcb_t _onDraw                          = nullptr;
  onresizecb_t _onResize                    = nullptr;
  onuieventcb_t _onUiEvent                  = nullptr;
  ongpuinit_t _onGpuInit                    = nullptr;
  ongpuupdate_t _onGpuUpdate                = nullptr;
  ongpuexit_t _onGpuExit                    = nullptr;
  onupdate_t _onUpdate                      = nullptr;
  onupdate_t _onUpdateInternal              = nullptr;
  onupdateinit_t _onUpdateInit              = nullptr;
  onupdateexit_t _onUpdateExit              = nullptr;
  onupdatewithscene_t _onUpdateWithScene    = nullptr;
  scenegraph::scene_ptr_t _execscene;
  varmap::varmap_ptr_t _execsceneparams;
  ork::Timer _render_timer;
  double _render_prevtime        = 0;
  double _render_stats_timeaccum = 0;
  double _render_state_numiters  = 0.0;


};
///////////////////////////////////////////////////////////////////////////////
struct EzTopWidget : public ui::Group {
  EzTopWidget(EzMainWin* mainwin);
  void _doGpuInit(ork::lev2::Context* pTARG) final;
  void DoDraw(ui::drawevent_constptr_t drwev) final;
  void enableUiDraw();
  void _doOnResized() final;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t ev) final;
  EzMainWin* _mainwin;
  ui::layoutgroup_ptr_t _topLayoutGroup;
  std::atomic<int> _initstate;
};
////////////////////////////////////////////////////////////////////////////////
struct StdDraw {
  const RenderContextFrameData* RCFD;
  const DrawableBuffer* DB;
};
////////////////////////////////////////////////////////////////////////////////
struct OrkEzAppBase {
public:
  OrkEzAppBase(ezappctx_ptr_t ezapp);
  virtual ~OrkEzAppBase() {}
  ezappctx_ptr_t _ezapp;
  static OrkEzAppBase* get();
  static OrkEzAppBase* _staticapp;
  std::atomic<int> _update_count;
  std::atomic<int> _render_count;
};
////////////////////////////////////////////////////////////////////////////////
struct OrkEzApp : public OrkEzAppBase {
  
public:
  ///////////////////////////////////
  OrkEzApp(appinitdata_ptr_t initdata);
  ~OrkEzApp();
  ///////////////////////////////////
  static orkezapp_ptr_t create(appinitdata_ptr_t appinitdata);
  static orkezapp_ptr_t createWithScene(varmap::varmap_ptr_t sceneparams);
  static boost::program_options::options_description_easy_init createDefaultOptions(appinitdata_ptr_t appinitdata, //
                                                            std::string appinfo);
  ///////////////////////////////////

  filedevctx_ptr_t newFileDevContext(std::string uriproto, const file::Path& basepath);

  void onDraw(EzMainWin::drawcb_t cb);
  void onResize(EzMainWin::onresizecb_t cb);
  void onGpuInit(EzMainWin::ongpuinit_t cb);
  void onGpuUpdate(EzMainWin::ongpuupdate_t cb);
  void onGpuExit(EzMainWin::ongpuexit_t cb);
  void onUiEvent(EzMainWin::onuieventcb_t cb);
  void onUpdateInit(EzMainWin::onupdateinit_t cb);
  void onUpdateExit(EzMainWin::onupdateexit_t cb);
  void onUpdate(EzMainWin::onupdate_t cb);
  void setRefreshPolicy(RefreshPolicyItem policy);

  int mainThreadLoop();
  void setSceneRunLoop(scenegraph::scene_ptr_t scene);

  void joinUpdate();
  bool checkAppState(uint64_t singlebitmask) const;
  void OnTimer();

  void enqueueOnRenderer(const void_lambda_t& l);

  void signalExit();

  void enqueueWindowResize(int w, int h);

  bool isExiting() const;

  ///////////////////////////////////
  void enableMovieRecording(file::Path output_path);
  void finishMovieRecording();
  gfxcontext_lambda_t _movie_record_frame_lambda;
  //void stdDraw(const StdDraw& DATA);

  ///////////////////////////////////
public:

  bool _userSpecifiedOnDraw = false;
  file::Path _orkidWorkspaceDir;
  appinitdata_ptr_t _initdata;
  ezmainwin_ptr_t _mainWindow;
  std::map<std::string, filedevctx_ptr_t> _fdevctxmap;
  ork::Timer _update_timer;
  double _update_prevtime        = 0;
  double _update_timeaccumulator = 0;
  ork::Thread _updateThread;
  ork::opq::opq_ptr_t _mainq;
  ork::opq::opq_ptr_t _updq;
  ork::opq::opq_ptr_t _conq;
  varmap::varmap_ptr_t _vars;
  std::atomic<uint64_t> _appstate;
  ui::updatedata_ptr_t _update_data;
  ui::context_ptr_t _uicontext;
  ui::layoutgroup_ptr_t _topLayoutGroup;
  eztopwidget_ptr_t _eztopwidget;
  ork::opq::opq_ptr_t _rthreadq;
  EzMainWin::onupdateexit_t _onAppEarlyTerminated = nullptr;
  moviecontext_ptr_t _moviecontext;
  float _timescale = 1.0f;
  void_lambda_t _onRunLoopIteration;
  rcfd_ptr_t _overrideRCFD;
};

} // namespace ork::lev2
