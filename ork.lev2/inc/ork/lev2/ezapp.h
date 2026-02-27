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
#include <ork/lev2/lev2_types.h>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/file/file.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/properties/register.h>
#include <ork/application/application.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/ui/profilerview.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/lev2/ez_secondary_win.h>
#include <ork/lev2/subsystem_gpu.h>
#include <ork/lev2/subsystem_audio.h>

namespace ork::lev2 {
////////////////////////////////////////////////////////////////////////////////
static constexpr uint64_t KAPPSTATEFLAG_UPDRUNNING = 1 << 0;
static constexpr uint64_t KAPPSTATEFLAG_JOINING    = 1 << 1;
static constexpr uint64_t KAPPSTATEFLAG_JOINED     = 1 << 2;
////////////////////////////////////////////////////////////////////////////////

#define EZAPP_CHANNEL "ez_app"
#define EZAPP_MAIN_LOOP_SERIES "main_loop"

struct EzAppContext {

  static ezappctx_ptr_t get(appinitdata_ptr_t appinitdata = nullptr);
  ~EzAppContext();

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
  typedef std::function<void(ui::drawevent_constptr_t)> drawcallback_t;
  typedef std::function<void(int w, int h)> onresizecallback_t;
  typedef std::function<void(Context* ctx)> ongpuinit_t;
  typedef std::function<void(Context* ctx)> ongpuupdate_t;
  typedef std::function<void(Context* ctx)> ongpupreframe_t;
  typedef std::function<void(Context* ctx)> ongpupostframe_t;
  typedef std::function<void(Context* ctx)> ongpuexit_t;
  typedef std::function<void(ui::updatedata_ptr_t upd)> onupdate_t;
  typedef std::function<void()> onupdateinit_t;
  typedef std::function<void()> onupdateexit_t;

  typedef std::function<void(Context* ctx, scenegraph::scene_ptr_t)> ongpuinitwitchscene_t;
  typedef std::function<void(ui::updatedata_ptr_t upd, scenegraph::scene_ptr_t)> onupdatewithscene_t;

  typedef std::function<ui::HandlerResult(ui::event_constptr_t ev)> onuieventcallback_t;

  EzMainWin(OrkEzApp& app);
  ~EzMainWin();

  void _updateEnqueueLockedAndReleaseFrame(DrawQueue*dbuf);
  void _updateEnqueueUnlockedAndReleaseFrame(DrawQueue*dbuf);

  const DrawQueue* _tryAcquireDrawBuffer(ui::drawevent_constptr_t drawEvent);
  DrawQueue* _tryAcquireUpdateBuffer();
  void _releaseAcquireUpdateBuffer(DrawQueue*);

  void _beginFrame(const DrawQueue*dbuf);
  void _endFrame(const DrawQueue*dbuf);

  void withAcquiredDrawQueueForUpdate(int debugcode,std::function<void(const AcquiredDrawQueueForUpdate& udb)> l);
  //void withStandardCompositorFrameRender(ui::drawevent_constptr_t drawEvent, StandardCompositorFrame& sframe);


  void enqueueWindowResize(int w, int h);

  OrkEzApp& _app;

  bool _update_rendersync                   = false;
  Context* _curframecontext                 = nullptr;
  appwindow_ptr_t _appwin                   = nullptr;
  CTXBASE* _ctqt                            = nullptr;
  drawcallback_t _onDraw                    = nullptr;
  onresizecallback_t _onResize              = nullptr;
  onuieventcallback_t _onUiEvent            = nullptr;
  ongpuinit_t _onGpuInit                    = nullptr;
  ongpuupdate_t _onGpuUpdate                = nullptr;
  ongpupreframe_t _onGpuPreFrame            = nullptr;
  ongpupostframe_t _onGpuPostFrame          = nullptr;
  ongpuexit_t _onGpuExit                    = nullptr;
  onupdate_t _onUpdate                      = nullptr;
  onupdate_t _onUpdateInternal              = nullptr;
  onupdateinit_t _onUpdateInit              = nullptr;
  onupdateexit_t _onUpdateExit              = nullptr;
  onupdatewithscene_t _onUpdateWithScene    = nullptr;
  scenegraph::scene_ptr_t _execscene;
  varmap::varmap_ptr_t _execsceneparams;

  // TODO delete?
  ork::Timer _render_timer;
  double _render_prevtime        = 0;
  double _render_stats_timeaccum = 0;
  double _render_state_numiters  = 0.0;
  double _perf_render_duration   = 0.0;  // primary window render+swap time
  double _perf_enqueue_duration  = 0.0;  // beginFrame+draw+endFrame time
  double _perf_present_duration  = 0.0;  // swapBuffers time (includes vsync wait)
  double _perf_acquire_duration  = 0.0;  // swapchain acquire wait
  double _perf_fence_wait_duration = 0.0; // fence wait
  double _perf_beginFrame_duration = 0.0; // total beginFrame() time
  double _perf_endFrame_duration = 0.0;   // total endFrame() time
  double _perf_submit_duration = 0.0;     // vkQueueSubmit time
  double _perf_present_vk_duration = 0.0; // vkQueuePresentKHR time
  ork::Timer _perf_render_timer;         // reusable timer for render measurement


};
///////////////////////////////////////////////////////////////////////////////
struct EzUiEventInterceptor : public ui::Widget {
  EzUiEventInterceptor();
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t ev) final;
  ui::Widget* doRouteUiEvent(ui::event_constptr_t ev);
  ui::event_lambda_t _onUiEventLambda;
  varmap::varmap_ptr_t _vars;
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
  const DrawQueue* DB;
};
////////////////////////////////////////////////////////////////////////////////
// OrkEzAppBase now inherits from Application to get HFSM lifecycle support
// This is an "overlay" change - all existing OrkEzApp behavior is preserved
////////////////////////////////////////////////////////////////////////////////
struct OrkEzAppBase : public ork::Application {
public:
  OrkEzAppBase(ezappctx_ptr_t ezapp, appinitdata_ptr_t initdata);
  virtual ~OrkEzAppBase() {}
  ezappctx_ptr_t _ezapp;
  static OrkEzAppBase* get();
  static OrkEzAppBase* _staticapp;
  std::atomic<int> _update_count;
  std::atomic<int> _render_count;
};
////////////////////////////////////////////////////////////////////////////////
struct OrkEzApp : public OrkEzAppBase {
  
  using onauddevfn_t = std::function<void(audiodevice_ptr_t)>;
  using onsynfn_t = std::function<void(audio::singularity::synth_ptr_t)>;

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

  void onDraw(EzMainWin::drawcallback_t callback);
  void onResize(EzMainWin::onresizecallback_t callback);
  void onGpuInit(EzMainWin::ongpuinit_t callback);
  void onGpuUpdate(EzMainWin::ongpuupdate_t callback);
  void onGpuPreFrame(EzMainWin::ongpupreframe_t callback);
  void onGpuPostFrame(EzMainWin::ongpupostframe_t callback);
  void onGpuExit(EzMainWin::ongpuexit_t callback);
  void onUiEvent(EzMainWin::onuieventcallback_t callback);
  void onUpdateInit(EzMainWin::onupdateinit_t callback);
  void onUpdateExit(EzMainWin::onupdateexit_t callback);
  void onUpdate(EzMainWin::onupdate_t callback);
  void setRefreshPolicy(RefreshPolicyItem policy);

  void onAudioInit(onauddevfn_t callback);
  void onAudioExit(onauddevfn_t callback);
  void onSynthInit(onsynfn_t callback);
  void onSynthExit(onsynfn_t callback);
  void onEzAppInit(void_lambda_t callback);
  void onEzAppExit(void_lambda_t callback);

  void _audioInit();
  void _audioExit();
  void _fireDeferredAudioCallbacks();  // Fire audio callbacks after registration in subsystem mode

  void _mainThreadLoopBegin();
  void _mainThreadLoopEnd();
  void _mainThreadLoopIter();
  int mainThreadLoop();
  void setSceneRunLoop(scenegraph::scene_ptr_t scene);

  void joinUpdate();
  bool checkAppState(uint64_t singlebitmask) const;
  void OnTimer();

  void enqueueOnRenderer(const void_lambda_t& l);

  void signalExit();

  void enqueueWindowResize(int w, int h);

  bool isExiting() const;

  inline appinitdata_ptr_t appInitData() const {
    return _initdata;
  }
  
  ///////////////////////////////////
  void enableMovieRecording(moviecapsettings_ptr_t settings);
  void finishMovieRecording();
  gfxcontext_lambda_t _movie_record_frame_lambda;
  //void stdDraw(const StdDraw& DATA);

  ///////////////////////////////////
  bool shouldUpdateThrottleOnGPU();
  ///////////////////////////////////
public:

  bool _userSpecifiedOnDraw = false;
  file::Path _orkidWorkspaceDir;
  ezmainwin_ptr_t _mainWindow;
  std::map<std::string, filedevctx_ptr_t> _fdevctxmap;
  ork::Timer _update_timer;
  double _update_prevtime        = 0;
  double _update_timeaccumulator = 0;
  double _render_timeaccumulator = 0;
  std::atomic<int> _lockstep_frame_requests = 0;
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
  moviecapcontext_ptr_t _moviecapcontext;
  float _timescale = 1.0f;
  void_lambda_t _onRunLoopIteration;
  void_lambda_t _onEzAppInit;
  void_lambda_t _onEzAppExit;
  rcfd_ptr_t _overrideRCFD;
  int _updateCounter = 0;
  int _gpuFrameCounter = 0;
  int _gpuFrameCounterUP = 0;
  size_t _total_samples_rendered = 0; // lockstep audio sync

  // Frame profiling fields (written each frame on main thread, except _perf_update_duration)
  double _perf_frame_duration = 0.0;       // total frame time (main thread)
  double _perf_gpu_update_duration = 0.0;  // onGpuUpdate callback time (main thread)
  double _perf_update_duration = 0.0;      // update callback time (update thread - benign race for display)
  ork::Timer _perf_gpu_update_timer;       // reusable timer for gpu update measurement
  Thread::thread_lambda_t _update_thread_impl = nullptr;
  onsynfn_t _onSynthInit                    = nullptr;
  onauddevfn_t _onAudioInit                 = nullptr;
  onauddevfn_t _onAudioExit                 = nullptr;
  onsynfn_t _onSynthExit                    = nullptr;
  audiodevice_ptr_t _audiodevice            = nullptr;
  audio::singularity::synth_ptr_t _synth    = nullptr;

  // Subsystems (Phase 2b - HFSM lifecycle)
  subsystem_ptr_t _gpu_subsystem;
  subsystem_ptr_t _audio_subsystem;

  // Secondary window support (Phase 3)
  std::vector<ezsecondarywin_ptr_t> _secondaryWindows;

  // Profiler
  profiler_channel_ptr_t _ezapp_channel = std::make_shared<CpuProfilerChannel>("ez_app");
  profiler_series_ptr_t _main_loop_series    = _ezapp_channel->createSeries("main_loop");

  ezsecondarywin_ptr_t createSecondaryWindow(const EzSecondaryWinConfig& config);
  void closeSecondaryWindow(ezsecondarywin_ptr_t win);
  void closeAllSecondaryWindows();

  // Internal
  void _renderSecondaryWindows();
  void _cleanupClosedSecondaryWindows();

  // Initialization paths (Phase 2b)
  void _initForAdHoc();        // Legacy inline init (use_subsystems=false)
  void _initForSubsystems();   // HFSM-driven init (use_subsystems=true)
  void _initGraphicsContext(); // Graphics context creation (called by either path)
};

} // namespace ork::lev2

ork::lev2::orkezapp_ptr_t lev2appinit(ork::appinitdata_ptr_t initdata = nullptr);
