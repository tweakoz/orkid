////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// ork.ecs.player.exe (HYPERECS D.5) — the pure-C++ scene player: the C++ lowering of
// ork.ecsplay.py, and the Goal-C acceptance gate. ONE program, ZERO Python:
//
//   deserializeJson(<scene>.ecs)            reflected SceneData (systems, archetypes,
//                                           spawners, embedded dflow graphs)
//   ecs::materializeAndWireScene(...)       the C++ wire step: AssetSystem materializeAll
//                                           (materials / SDF voxelize / particle systems /
//                                           heightfields / hypermeshes) + by-name component
//                                           patching (drawables, envmaps, skybox)
//   Controller bind/create/start            the standard ECS lifecycle (trace-ecs shape)
//   per-frame: update + gpuUpdate + render  simulation on the update thread, GPU work +
//                                           draw on the render thread
//
// usage:  ork.ecs.player.exe <scene.ecs>  [--camdist <d>] [--camheight <h>]
//
// OFFSCREEN (headless, no window) — prime load-time bakes / record a clip:
//   --offscreen              hidden window; render until the scene settles, then exit
//                            (primes the terrain proctex disk cache with no window)
//   --frames <N>             safety cap on offscreen frames (default 1200; normal exit
//                            is driven by load-settle, not this count)
//   --movie <path>           record an OFFSCREEN movie to <path> (implies --offscreen)
//   --moviefps <F>           movie frame rate (default 60)
//   --movieframes <N>        movie length in frames (default 300)
// (ork.scene.materialize.py wraps --offscreen; ork.scene.viewer.py is the windowed path.)
//
// input (mirrors ork.ecsplay.py):
//   mouse / trackpad       EzUiCam orbit / pan / zoom
//   Cmd+Right Arrow        restart: NEW simulation (fresh controller, same scene)
//   Cmd+R                  LIVE ROUND-TRIP (gate 1.7): serialize the RUNNING scene ->
//                          deserialize -> byte-compare (serdes audit) -> re-materialize
//                          -> fresh simulation from the CLONE -> still animating
//   Cmd+Down Arrow         stop the simulation
//   Space                  pause / resume (host-side: the update tick holds)
//
// startup: always the HFSM subsystem-driven init (the first C++ host to use it).
//
////////////////////////////////////////////////////////////////

#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/kernel/async_tracker.h> // offscreen exit waits for pending async work (terrain texbake) to drain
#include <ork/application/application.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/util/movie.inl> // offscreen movie capture (MovieCaptureSettings)
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/ecs/physics/CharacterController.h> // E.2-walk: input forwarding + camera yield
#include <ork/ecs/pysys/PythonComponent.h>          // E.2-walk: scene-declared input-script routing
#include <ork/python/context.h>                     // embedded interpreter (OPT-IN: scene declares PythonSystem)
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <atomic>
#include <mutex>

#include <ork/ecs/ecs.h>
#include <ork/ecs/datatable.h>
#include <ork/ecs/system.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/controller.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/ecs/AssetSystem.h>

#include <ork/ecs/scene.inl>
#include <ork/ecs/archetype.inl>
#include <ork/ecs/controller.inl>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <boost/program_options.hpp>

#include "perfhud.h" // on-screen perf HUD (~ key)

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::ecs;

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {

  //////////////////////////////////////////////////////////
  // args — boost::program_options. Positional <scene> + flags; `--help`/`-h` prints the
  // option list and the available scenes. Unregistered args are tolerated (the launcher /
  // environment may inject extras), matching the old hand-rolled loop's leniency.
  //////////////////////////////////////////////////////////

  namespace po = boost::program_options;

  std::string scene_path;
  float cam_dist        = 20.0f;
  float cam_height      = 8.0f;
  float auto_roundtrip  = 0.0f; // gate 1.7 scripted: fire the live round-trip at T+N sec
  float auto_walk       = 0.0f; // E.2-walk scripted: hold W for N seconds through the message channel
  bool  want_fullscreen = false;
  bool  want_hidpi      = false; // default LoDPI (fillrate); opt into Retina backing scale
  int   want_width      = 0;    // 0 = use AppInitData default; windowed initial width
  int   want_height     = 0;    // 0 = use AppInitData default; windowed initial height
  int   want_ssaa       = 0;    // 0/1 = off
  // OFFSCREEN (headless) mode — hidden window, render until the scene settles, then exit.
  // ork.scene.materialize.py uses it to PRIME the disk cache (terrain texbake) with no
  // window; --movie additionally records a clip.
  bool  offscreen        = false;
  bool  offscreen_forever = false; // headless render indefinitely (no settle-exit; kill to stop)
  int   offscreen_frames = 0;     // safety cap (0 -> default chosen below)
  std::string movie_path;         // --movie PATH (implies offscreen)
  float movie_fps        = 60.0f;
  int   movie_frames     = 0;     // 0 -> default 300

  po::options_description desc(
      "ork.ecs.player.exe — pure-C++ ECS scene player\n"
      "usage: ork.ecs.player.exe <scene.ecs | shortname> [options]\n\noptions");
  desc.add_options()
      ("help,h", "show this help (and the available scenes) and exit")
      ("scene,s", po::value<std::string>(&scene_path), "scene .ecs path or short name (also accepted positionally)")
      ("camdist", po::value<float>(&cam_dist)->default_value(20.0f), "orbit camera distance")
      ("camheight", po::value<float>(&cam_height)->default_value(8.0f), "orbit camera height")
      ("roundtrip", po::value<float>(&auto_roundtrip)->default_value(0.0f), "scripted live serdes round-trip at T+N seconds")
      ("autowalk", po::value<float>(&auto_walk)->default_value(0.0f), "walk scenes: scripted hold-W for N seconds")
      ("fullscreen,f", po::bool_switch(&want_fullscreen), "fullscreen window")
      ("width,W", po::value<int>(&want_width)->default_value(0), "initial window width (windowed mode; 0=default)")
      ("height,H", po::value<int>(&want_height)->default_value(0), "initial window height (windowed mode; 0=default)")
      ("hidpi", po::bool_switch(&want_hidpi), "render at the display's backing (Retina) scale; default is LoDPI to save fillrate")
      ("ssaa,t", po::value<int>(&want_ssaa)->default_value(0), "supersample multiplier (>1 enables SSAA)")
      ("offscreen", po::bool_switch(&offscreen), "headless: hidden window, render until settled, then exit (prime caches)")
      ("offscreen-forever", po::bool_switch(&offscreen_forever), "headless: render indefinitely, unthrottled, no settle-exit (kill to stop; ignores --movie)")
      ("frames", po::value<int>(&offscreen_frames)->default_value(0), "offscreen frame safety-cap (0=auto 1200; normal exit is load-settle driven)")
      ("movie,m", po::value<std::string>(&movie_path), "record an offscreen movie to PATH (mp4; implies --offscreen)")
      ("moviefps,F", po::value<float>(&movie_fps)->default_value(60.0f), "movie frame rate")
      ("movieframes,l", po::value<int>(&movie_frames)->default_value(0), "movie length in frames (0=300)");

  po::positional_options_description pos;
  pos.add("scene", 1);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv)
                  .options(desc)
                  .positional(pos)
                  .allow_unregistered() // tolerate launcher/env-injected args (old loop ignored unknowns)
                  .run(),
              vm);
    po::notify(vm);
  } catch (const std::exception& e) {
    printf("ork.ecs.player: argument error: %s\n", e.what());
    std::cout << desc << std::endl;
    return 1;
  }

  //////////////////////////////////////////////////////////
  // SHORT-NAME resolution + discovery (mirrors the python viewers): a bare name
  // resolves to <workspace>/ork.data/ecsscenes/<name>.ecs; --help / no scene / a miss
  // lists what's available there.
  //////////////////////////////////////////////////////////

  std::string scenes_dir;
  if (const char* ws = getenv("ORKID_WORKSPACE_DIR"))
    scenes_dir = std::string(ws) + "/ork.data/ecsscenes";

  auto print_scenes = [&]() {
    if (scenes_dir.empty() or not std::filesystem::is_directory(scenes_dir)) {
      printf("(no ork.data/ecsscenes directory found via ORKID_WORKSPACE_DIR)\n");
      return;
    }
    printf("available scenes in %s:\n", scenes_dir.c_str());
    std::vector<std::string> names;
    for (const auto& entry : std::filesystem::directory_iterator(scenes_dir))
      if (entry.path().extension() == ".ecs")
        names.push_back(entry.path().stem().string());
    std::sort(names.begin(), names.end());
    for (const auto& n : names)
      printf("  %s\n", n.c_str());
  };

  if (vm.count("help")) {
    std::cout << desc << std::endl;
    print_scenes();
    return 0;
  }
  if (scene_path.empty()) {
    std::cout << desc << std::endl;
    print_scenes();
    return 1;
  }
  // bare short name (no slash, no .ecs) -> the scenes dir
  if (scene_path.find('/') == std::string::npos and
      scene_path.size() >= 4 and scene_path.substr(scene_path.size() - 4) != ".ecs") {
    if (not scenes_dir.empty())
      scene_path = scenes_dir + "/" + scene_path + ".ecs";
  }
  std::ifstream scene_file(scene_path);
  if (not scene_file.good()) {
    printf("ork.ecs.player: scene file not found: %s\n", scene_path.c_str());
    print_scenes();
    return 1;
  }
  std::stringstream scene_json_strm;
  scene_json_strm << scene_file.rdbuf();
  std::string scene_json = scene_json_strm.str();

  //////////////////////////////////////////////////////////
  // init application — class registration BEFORE deserialize, and the NEW
  // HFSM subsystem-driven startup (the first C++ host to use it).
  //////////////////////////////////////////////////////////

  auto init_data = std::make_shared<ork::AppInitData>(argc, argv, envp);
  init_data->_fullscreen         = want_fullscreen;
  init_data->_fullscreen_mode    = ork::AppInitData::EFullScreenMode::Immersive;
  init_data->_allowHIDPI         = want_hidpi;
  if (want_width  > 0) init_data->_width  = want_width;
  if (want_height > 0) init_data->_height = want_height;
  init_data->_use_subsystems     = true;
  init_data->_defer_gpu_init     = true;
  init_data->_enabled_subsystems = {"opq", "core", "gpu", "lev2"};
  if (const char* v = getenv("ORKEXP_FULLSCREEN")) init_data->_fullscreen = (atoi(v) != 0);
  if (const char* v = getenv("ORKID_HIDPI"))       init_data->_allowHIDPI = (atoi(v) != 0);
  if (const char* v = getenv("ORKEXP_LEFT"))       init_data->_left   = atoi(v);
  if (const char* v = getenv("ORKEXP_TOP"))        init_data->_top    = atoi(v);
  if (const char* v = getenv("ORKEXP_WIDTH"))      init_data->_width  = atoi(v);
  if (const char* v = getenv("ORKEXP_HEIGHT"))     init_data->_height = atoi(v);
  if (const char* v = getenv("ORKEXP_MONITOR"))    init_data->_fullscreen_monitor = v;
  if (const char* v = getenv("ORKEXP_DISPLAYLINK")) init_data->_displaylink = (atoi(v) != 0);

  //////////////////////////////////////////////////////////
  // OFFSCREEN: hidden window (GLFW_VISIBLE=false, no swapchain), still a full
  // _mainWindow + render thread, so mainThreadLoop renders frames headless. The
  // onDraw frame-budget below drives capture + signalExit. --movie implies offscreen.
  //////////////////////////////////////////////////////////
  if (not movie_path.empty())
    offscreen = true;
  if (offscreen_forever) {
    offscreen  = true;   // headless
    movie_path = "";     // forever is the no-movie soak/perf path
  }
  if (offscreen) {
    init_data->_offscreen = true;
    if (offscreen_forever) {
      deco::printf(fvec3::Yellow(), "ork.ecs.player: OFFSCREEN-FOREVER mode (unthrottled, no settle-exit; kill to stop)\n");
    }
    if (not movie_path.empty()) {
      // movie capture's encoder thread reads samples from a STREAMING audio device
      // (StrAudioDevice). Provide one — silent if the scene has no synth — so A/V
      // capture works and the encoder never derefs a null device. ASYNC_REALTIME
      // (no _audio_stream_sync) keeps it compatible with the player's freerun loop.
      init_data->_enable_audio  = true;
      init_data->_audio_ioclass = "STREAM";
    }
    if (movie_frames <= 0)
      movie_frames = 300;
    // offscreen_frames is a SAFETY CAP (max frames if the loader never settles); the
    // normal exit is driven by the load-idle + settle state machine in onDraw. A raw
    // frame count alone is unreliable — at offscreen freerun N frames elapse in well
    // under a second while async asset streaming takes seconds, so a small budget exits
    // BEFORE the deferred terrain texbake fires (the bug this replaces).
    if (offscreen_frames <= 0)
      offscreen_frames = movie_path.empty() ? 1200 : (600 + movie_frames);
    if (not offscreen_forever)
      deco::printf(fvec3::Yellow(), "ork.ecs.player: OFFSCREEN mode (cap<%d frames>%s)\n",
                   offscreen_frames, movie_path.empty() ? "" : (" movie<" + movie_path + ">").c_str());
  }

  // CLASS REGISTRATION ORDER: ecs::initModule must run BEFORE OrkEzApp::create — the
  // post-init REFLECTION_LINK op (rtti::Class::InitializeClasses) fires inside create's
  // finalizeInitialization, and only classes registered by then are FindClass-able
  // (the scene deserialize needs EcsSceneData etc.). The ecs family registrations
  // AddPooledLiteral, which needs a string pool on the stack — Python gets one from
  // the core pyext module import; a pure-C++ host pushes its own (the orkpyshell /
  // unittest pattern). EzAppContext pushes another on top later — that's fine, it's
  // a stack.
  static auto s_stringpool = std::make_shared<StringPoolContext>();
  StringPoolStack::push(s_stringpool);
  ecs::initModule(init_data); // ecs (+ lev2, guarded) class registration
  auto ezapp = OrkEzApp::create(init_data);

  // on-screen performance HUD — the '~' key cycles OFF -> TEXT -> TEXT+GRAPH.
  ork::ecs::player::PerfHud perfhud;
  perfhud.init(ezapp);

  deco::printf(
      fvec3::Yellow(),
      "ork.ecs.player: SUBSYSTEM (HFSM) startup, scene<%s> (%zu bytes)\n",
      scene_path.c_str(),
      scene_json.size());

  //////////////////////////////////////////////////////////
  // host state
  //////////////////////////////////////////////////////////

  scenedata_ptr_t scenedata;
  controller_ptr_t controller;
  std::vector<controller_ptr_t> dead_controllers; // stopped controllers are parked, not reused
                                                  // (the Python runtime does the same)
  sys_ref_t sgsystem; // opaque handle for systemNotify
  float abstime = 0.0f;
  Timer fps_timer;
  fps_timer.Start();
  int framecounter = 0;
  // OFFSCREEN exit state machine (materialize / movie): wait for the loader to settle,
  // then a render margin (the deferred terrain texbake fires + writes its cache), then
  // exit — or start/stop a movie recording.
  int  os_frame     = 0;   // total offscreen frames rendered
  int  os_idle      = 0;   // consecutive loader-idle frames (no in-flight async loads)
  int  os_settle    = 0;   // frames since the scene settled
  int  os_movie     = 0;   // movie frames recorded
  int  os_drain     = 0;   // post-record drain frames (pump GPU so captures finish)
  int  os_phase     = 0;   // 0=WAIT 1=SETTLE 2=MOVIE 3=DONE
  bool os_saw_async = false; // observed registered async work (a bake) — wait for it to drain

  // controller swaps (Cmd+Right restart) happen on the update thread while the render
  // thread reads `controller` in gpuUpdate/draw — one small mutex covers all of it.
  std::mutex ctl_mutex;
  enum SimRequest { REQ_NONE, REQ_STOP, REQ_RESTART };
  std::atomic<int> sim_request{REQ_NONE};
  std::atomic<bool> sim_paused{false};
  // gate 1.7 — the LIVE round-trip: the render thread (which owns the GPU ctx the
  // materializers need) performs serialize->deserialize->wire and stashes the clone;
  // the update thread's REQ_RESTART then adopts it (controller lifecycle stays on the
  // update thread, the proven path).
  std::atomic<bool> roundtrip_requested{false};
  scenedata_ptr_t pending_fresh; // guarded by ctl_mutex
  // last known framebuffer size — a RESTARTED controller's compositor initializes at a
  // tiny default and only a live resize event would correct it (none fires mid-run), so
  // the restart path re-sends UpdateFramebufferSize with these. Sampled per-frame in
  // onGpuUpdate (the render thread owns the surface).
  std::atomic<int> fb_w{0}, fb_h{0};

  // EzUiCam — the same mouse/trackpad camera the Python viewers use.
  // E.2-walk: scene-declared walk mode (CharacterControllerSystem present in the .ecs);
  // detected in onUpdateInit (the scenedata deserializes in onGpuInit, which runs first).
  bool walk_mode = false;
  bool pysys_mode = false; // scene declares a PythonSystem -> embedded python (OPT-IN; else zero-python)
  sys_ref_t charsystem;
  sys_ref_t pysystem;

  // E.2-walk OPT-IN embedded python: the interpreter must exist BEFORE any thread
  // might touch it. We can't deserialize yet (no gfx), so PEEK the scene JSON for a
  // PythonSystemData declaration; zero-python scenes never init the interpreter.
  {
    FILE* pf = fopen(scene_path.c_str(), "rb");
    if (pf) {
      std::string txt;
      fseek(pf, 0, SEEK_END);
      long sz = ftell(pf);
      fseek(pf, 0, SEEK_SET);
      txt.resize(size_t(sz));
      fread(txt.data(), 1, size_t(sz), pf);
      fclose(pf);
      if (txt.find("PythonSystemData") != std::string::npos) {
        deco::printf(fvec3::Yellow(), "ork.ecs.player: scene declares PythonSystem — embedding python\n");
        ork::python::context(); // first call constructs the singleton (Py_InitializeEx)
        // EMBEDDING CONTRACT: Py_InitializeEx leaves THIS thread holding the GIL forever —
        // any other thread's Py_NewInterpreter / PyGILState then DEADLOCKS (the pysys-probe
        // hang: update thread parked in bindSubInterpreter at 120 FPS). A python HOST's
        // interpreter thread releases the GIL naturally; an embedding host must do it
        // explicitly. We never run python on the main thread again, so release for good.
        PyEval_SaveThread();
      }
    }
  }

  auto uicam            = std::make_shared<EzUiCam>();
  uicam->_constrainZ    = true;
  uicam->_fov           = 65.0f * DTOR;
  uicam->near_min          = 0.75;
  uicam->far_max           = 100000.0;
  uicam->_base_zmoveamt = 0.05f;
  uicam->mfLoc          = cam_dist;
  uicam->lookAt(fvec3(cam_dist, cam_height, cam_dist), fvec3(0, 2, 0), fvec3(0, 1, 0));
  uicam->updateMatrices();

  //////////////////////////////////////////////////////////
  // gpuInit — the whole Goal-C chain happens HERE, in C++:
  //   deserialize -> materialize+wire -> bind -> createSimulation
  //////////////////////////////////////////////////////////

  ezapp->onGpuInit([&](Context* ctx) {
    ////////////////////////////////////////////
    // 1. reflected deserialize
    ////////////////////////////////////////////
    reflect::serdes::JsonDeserializer deser(scene_json.c_str());
    object_ptr_t instance_out;
    deser.deserializeTop(instance_out);
    scenedata = std::dynamic_pointer_cast<SceneData>(instance_out);
    OrkAssert(scenedata);
    deco::printf(fvec3::Green(), "ork.ecs.player: scene deserialized\n");
    ////////////////////////////////////////////
    // 2. the C++ wire step (materializeAll + by-name component patching)
    ////////////////////////////////////////////
    auto artifacts = materializeAndWireScene(scenedata, ctx);
    if (want_ssaa > 1) { // host display preference -> the SG screen node (link copies userparams)
      for (const auto& it : scenedata->getSystemDatas())
        if (auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second))
          sgd->setUserSceneParam("ssaa", want_ssaa);
      deco::printf(fvec3::Yellow(), "ork.ecs.player: ssaa<%d>\n", want_ssaa);
    }
    deco::printf(
        fvec3::Green(),
        "ork.ecs.player: materialized + wired (%zu artifacts)\n",
        artifacts->_themap.size());
    ////////////////////////////////////////////
    // 3. standard ECS lifecycle (trace-ecs shape)
    ////////////////////////////////////////////
    controller = std::make_shared<Controller>();
    controller->bindScene(scenedata);
    controller->createSimulation();
    controller->gpuInit(ctx);
  });

  //////////////////////////////////////////////////////////
  // updateInit — start the simulation (entities spawn, particles fire)
  //////////////////////////////////////////////////////////

  ezapp->onUpdateInit([&]() {
    controller->startSimulation();
    // the deferred system ref registers during simulation composition — resolve it
    // AFTER start (the trace-ecs ordering); a notify on an unregistered ref asserts.
    sgsystem = controller->findSystem<SceneGraphSystem>();
    // E.2-walk: when the scene declares a CharacterControllerSystem, the player goes
    // WALK MODE — key events forward as controller messages (the character system owns
    // the camera; the host's orbit cam yields).
    for (const auto& it : scenedata->getSystemDatas()) {
      if (std::dynamic_pointer_cast<CharacterControllerSystemData>(it.second))
        walk_mode = true;
      if (std::dynamic_pointer_cast<PythonSystemData>(it.second))
        pysys_mode = true;
    }
    if (walk_mode)
      charsystem = controller->findSystem<CharacterControllerSystem>();
    if (pysys_mode)
      pysystem = controller->findSystem<PythonSystem>();
    deco::printf(fvec3::Green(), "ork.ecs.player: simulation STARTED%s\n",
                 walk_mode ? " [WALK MODE: W/S move, A/D strafe, arrows turn/pitch, SPACE jump, P pause]" : "");
  });

  //////////////////////////////////////////////////////////
  // update (update thread) — slow orbit camera + simulation tick
  //////////////////////////////////////////////////////////

  bool auto_rt_fired = false;
  bool aw_down = false, aw_up = false; // --autowalk state
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    abstime = updata->_abstime;
    if (auto_roundtrip > 0.0f and not auto_rt_fired and abstime >= auto_roundtrip) {
      auto_rt_fired = true;
      deco::printf(fvec3::Yellow(), "ork.ecs.player: LIVE ROUND-TRIP (scripted, t=%g)\n", abstime);
      roundtrip_requested = true;
    }
    // E.2-walk scripted input (the gate's lever): W down at t=1, up at t=1+N —
    // through the SAME controller-message channel real keys use.
    if (walk_mode and auto_walk > 0.0f) {
      auto send_key = [&](int key, int down) {
        controller_ptr_t c;
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          c = controller;
        }
        if (c) {
          auto keytab = std::make_shared<DataTable>();
          (*keytab)["key"_tok]  = key;
          (*keytab)["down"_tok] = down;
          c->systemNotify(pysystem, "InputKey"_tok, keytab);
        }
      };
      if (not aw_down and abstime >= 1.0) {
        aw_down = true;
        send_key('W', 1);
        deco::printf(fvec3::Yellow(), "ork.ecs.player: AUTOWALK begin\n");
      }
      if (aw_down and not aw_up and abstime >= 1.0 + auto_walk) {
        aw_up = true;
        send_key('W', 0);
        deco::printf(fvec3::Yellow(), "ork.ecs.player: AUTOWALK end\n");
      }
    }
    ////////////////////////////////////////////
    // take a LOCAL copy of the controller; NEVER hold ctl_mutex across the calls:
    // controller->update() can synchronously WAIT ON THE RENDER THREAD
    // (Simulation::_runGpuPhaseOnRenderThread during composition/model load) while
    // the render thread enters onGpuUpdate — holding the lock here deadlocks
    // (ecsscn3 grey-screen hang). The mutex only guards the pointer swap.
    ////////////////////////////////////////////
    controller_ptr_t c;
    sys_ref_t sgsys_local;
    {
      std::lock_guard<std::mutex> lock(ctl_mutex);
      c          = controller;
      sgsys_local = sgsystem;
    }
    if (not c)
      return;
    ////////////////////////////////////////////
    // service simulation requests (set from the UI thread)
    ////////////////////////////////////////////
    switch (sim_request.exchange(REQ_NONE)) {
      case REQ_STOP:
        c->stopSimulation();
        deco::printf(fvec3::Yellow(), "ork.ecs.player: simulation STOPPED (Cmd+Right to restart)\n");
        break;
      case REQ_RESTART: {
        // gate 1.7: if a round-tripped CLONE is pending, adopt it — the new simulation
        // runs from deserialize(serialize(live_scene)), not the original object.
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          if (pending_fresh) {
            scenedata    = pending_fresh;
            pending_fresh = nullptr;
            deco::printf(fvec3::Green(), "ork.ecs.player: restarting from the ROUND-TRIPPED clone\n");
          }
        }
        // fresh controller on the SAME (already wired) scenedata — controllers are
        // not reusable across runs; the old one parks in the dead list (Python parity).
        c->stopSimulation();
        auto fresh = std::make_shared<Controller>();
        fresh->bindScene(scenedata);
        fresh->createSimulation();
        fresh->startSimulation(); // gpu init rides the per-frame gpuUpdate FSM
        auto fresh_sgsys = fresh->findSystem<SceneGraphSystem>();
        if (walk_mode)
          charsystem = fresh->findSystem<CharacterControllerSystem>(); // E.2-walk: re-resolve on restart
        if (pysys_mode)
          pysystem = fresh->findSystem<PythonSystem>();
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          dead_controllers.push_back(controller);
          controller = fresh;
          sgsystem   = fresh_sgsys;
        }
        c          = fresh;
        sgsys_local = fresh_sgsys;
        sim_paused = false;
        // size the fresh compositor to the CURRENT window (see fb_w/fb_h above)
        if (fb_w.load() > 0 and fb_h.load() > 0) {
          auto fbsize_data = std::make_shared<DataTable>();
          (*fbsize_data)["width"_tok]  = fb_w.load();
          (*fbsize_data)["height"_tok] = fb_h.load();
          c->systemNotify(sgsys_local, SceneGraphSystem::UpdateFramebufferSize, fbsize_data);
        }
        deco::printf(fvec3::Green(), "ork.ecs.player: simulation RESTARTED\n");
        break;
      }
      default:
        break;
    }
    ////////////////////////////////////////////
    // camera from the EzUiCam. payload MUST be a shared_ptr<DataTable> — the system
    // handlers read it via data.getShared<DataTable>(); a by-value DataTable (the older
    // trace-ecs pattern) reinterprets as a shared_ptr and crashes in the key compare.
    // svar values are EXACT-typed (get<float> on a double asserts).
    ////////////////////////////////////////////
    if (not walk_mode) { // walk mode: the character system publishes the camera
    const auto& UIC  = uicam->_camcamdata;
    auto camera_data = std::make_shared<DataTable>();
    (*camera_data)["eye"_tok]  = UIC->GetEye();
    (*camera_data)["tgt"_tok]  = UIC->GetTarget();
    (*camera_data)["up"_tok]   = UIC->GetUp();
    (*camera_data)["near"_tok] = float(UIC->GetNear());
    (*camera_data)["far"_tok]  = float(UIC->GetFar());
    (*camera_data)["fovy"_tok] = float(UIC->GetAperature());
    c->systemNotify(sgsys_local, SceneGraphSystem::UpdateCamera._token, camera_data);
    }
    c->update(); // UNLOCKED — may block on the render thread (gpu phases)
  });

  //////////////////////////////////////////////////////////
  // ui events (main thread) — sim lifecycle keys, then the EzUiCam
  //////////////////////////////////////////////////////////

  ezapp->onUiEvent([&](ui::event_constptr_t ev) -> ui::HandlerResult {
    if (ev->_eventcode == ui::EventCode::KEY_DOWN) {
      // '~' / '`' (grave) cycles the perf HUD: OFF -> TEXT -> TEXT+GRAPH. Handle it
      // BEFORE the walk/PythonSystem key-forwarding below so the scene can't swallow it.
      if (ev->miKeyCode == '`' or ev->miKeyCode == '~') {
        perfhud.cycleMode();
        return ui::HandlerResult();
      }
      if (ev->mbSUPER) {
        if (ev->miKeyCode == 262) { // Cmd+Right Arrow -> restart NEW simulation
          sim_request = REQ_RESTART;
          return ui::HandlerResult();
        }
        if (ev->miKeyCode == 'R') { // Cmd+R -> LIVE ROUND-TRIP restart (gate 1.7)
          deco::printf(fvec3::Yellow(), "ork.ecs.player: LIVE ROUND-TRIP requested\n");
          roundtrip_requested = true;
          return ui::HandlerResult();
        }
        if (ev->miKeyCode == 264) { // Cmd+Down Arrow -> stop
          sim_request = REQ_STOP;
          return ui::HandlerResult();
        }
      } else if (walk_mode and (ev->miKeyCode == 'P')) { // walk mode: P -> pause (Space = jump)
        bool now = not sim_paused.load();
        sim_paused = now;
        controller_ptr_t c;
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          c = controller;
        }
        if (c) {
          if (now)
            c->pauseSimulation();
          else
            c->resumeSimulation();
        }
        deco::printf(fvec3::Yellow(), "ork.ecs.player: %s\n", now ? "PAUSED" : "RESUMED");
        return ui::HandlerResult();
      } else if ((not walk_mode) and ev->miKeyCode == 32) { // Space -> pause / resume (ENGINE pause: the game
        // clock holds but events keep servicing — the EzUiCam stays live while paused)
        bool now = not sim_paused.load();
        sim_paused = now;
        controller_ptr_t c;
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          c = controller;
        }
        if (c) {
          if (now)
            c->pauseSimulation();
          else
            c->resumeSimulation();
        }
        deco::printf(fvec3::Yellow(), "ork.ecs.player: %s\n", now ? "PAUSED" : "RESUMED");
        return ui::HandlerResult();
      }
    }
    // E.2-walk: INPUT AS CONTROLLER MESSAGES — forward plain key transitions to the
    // CharacterControllerSystem (the host-agnostic input channel; a future PythonSystem
    // can route the same messages). EXACT-typed svar values (int).
    // The player does NOT interpret game keys — it just DELIVERS raw key transitions to the scene's
    // PythonSystem whenever one exists. The python input SCRIPT owns the keymap (walk actions, locomotion, …); "walk vs locomotion" is a python concern, never a host flag.
    if (pysys_mode and
        (ev->_eventcode == ui::EventCode::KEY_DOWN or ev->_eventcode == ui::EventCode::KEY_UP) and
        not ev->mbSUPER) {
      controller_ptr_t c;
      {
        std::lock_guard<std::mutex> lock(ctl_mutex);
        c = controller;
      }
      if (c) {
        auto keytab = std::make_shared<DataTable>();
        (*keytab)["key"_tok]  = int(ev->miKeyCode);
        (*keytab)["down"_tok] = int(ev->_eventcode == ui::EventCode::KEY_DOWN ? 1 : 0);
        static int dbg_fwd = 0;
        if (dbg_fwd < 8) {
          dbg_fwd++;
          deco::printf(fvec3::Cyan(), "ork.ecs.player: fwd key<%d> down<%d> -> PythonSystem\n",
                       int(ev->miKeyCode), int(ev->_eventcode == ui::EventCode::KEY_DOWN ? 1 : 0));
        }
        c->systemNotify(pysystem, "InputKey"_tok, keytab);
      }
      // fall through: the orbit cam may also process it (unused when a system publishes the camera).
    }
    if (uicam->UIEventHandler(ev))
      uicam->updateMatrices();
    return ui::HandlerResult();
  });

  //////////////////////////////////////////////////////////
  // gpuUpdate (render thread, pre-repaint) — the per-frame GPU FSM tick
  // (AssetSystem one-shot gpu-init, system gpu work). NOTE: this hook runs
  // OUTSIDE beginFrame on the windowed loop; in-frame scene gpu work
  // (LightManager init, drawable onGpuUpdate) rides the render entries.
  //////////////////////////////////////////////////////////

  ezapp->onGpuUpdate([&](Context* ctx) {
    fb_w = ctx->mainSurfaceWidth();
    fb_h = ctx->mainSurfaceHeight();
    ////////////////////////////////////////////
    // gate 1.7 — the LIVE round-trip, render-thread phase: serialize the RUNNING
    // scenedata, deserialize a clone, BYTE-COMPARE the clone's re-serialization
    // (pure serdes fidelity — any unreflected state shows up here as a diff),
    // then materialize+wire the clone and hand it to the update thread's restart.
    ////////////////////////////////////////////
    if (roundtrip_requested.exchange(false)) {
      scenedata_ptr_t current;
      {
        std::lock_guard<std::mutex> lock(ctl_mutex);
        current = scenedata;
      }
      if (current) {
        reflect::serdes::JsonSerializer ser;
        ser.serializeRoot(current);
        std::string js_a(ser.output().c_str());
        reflect::serdes::JsonDeserializer deser(js_a.c_str());
        object_ptr_t fresh_obj;
        deser.deserializeTop(fresh_obj);
        auto fresh = std::dynamic_pointer_cast<SceneData>(fresh_obj);
        if (fresh) {
          reflect::serdes::JsonSerializer ser2;
          ser2.serializeRoot(fresh);
          std::string js_b(ser2.output().c_str());
          if (js_a == js_b)
            deco::printf(
                fvec3::Green(),
                "ork.ecs.player: ROUND-TRIP serdes BYTE-IDENTICAL (%zu bytes)\n",
                js_a.size());
          else
            deco::printf(
                fvec3::Red(),
                "ork.ecs.player: ROUND-TRIP serdes DIVERGED (%zu -> %zu bytes) — "
                "unreflected or unstable state in the live scene!\n",
                js_a.size(),
                js_b.size());
          materializeAndWireScene(fresh, ctx);
          {
            std::lock_guard<std::mutex> lock(ctl_mutex);
            pending_fresh = fresh;
          }
          sim_request = REQ_RESTART;
        } else {
          deco::printf(fvec3::Red(), "ork.ecs.player: ROUND-TRIP deserialize FAILED\n");
        }
      }
    }
    controller_ptr_t c;
    {
      std::lock_guard<std::mutex> lock(ctl_mutex);
      c = controller;
    }
    if (c)
      c->gpuUpdate(ctx);
  });

  //////////////////////////////////////////////////////////
  // draw (render thread)
  //////////////////////////////////////////////////////////

  ezapp->onDraw([&](ui::drawevent_constptr_t drwev) {
    perfhud.frameBegin();
    controller_ptr_t c;
    {
      std::lock_guard<std::mutex> lock(ctl_mutex);
      c = controller;
    }
    if (c)
      c->render(drwev);
    // MOVIE CAPTURE pump — the movie frame lambda normally fires in the EzTopWidget
    // draw path (uicontext->draw), but the player renders the ECS scene directly via
    // this onDraw and never enters that path, so the lambda was never called and the
    // recording came out empty/invalid. Invoke it here, AFTER the scene has rendered
    // into the main RtGroup the lambda captures. No-op unless recording is active.
    if (ezapp->_movie_record_frame_lambda)
      ezapp->_movie_record_frame_lambda(drwev->GetTarget());
    // perf HUD draws AFTER the movie pump so it never burns into a recording.
    perfhud.frameEndAndDraw(drwev->GetTarget());
    framecounter++;
    if (fps_timer.SecsSinceStart() > 5.0f) {
      float FPS = float(framecounter) / fps_timer.SecsSinceStart();
      deco::printf(fvec3::White(), "ork.ecs.player FPS<%g>\n", FPS);
      fps_timer.Start();
      framecounter = 0;
    }
    ////////////////////////////////////////////
    // OFFSCREEN exit — driven by the ASYNC-WORK registry, not a frame count. The
    // deferred terrain texbake registers itself as pending async work and clears it
    // when the disk cache is written; we (WAIT) until that drains (a scene with NO
    // registered async work falls back to loader-idle + a warmup), (SETTLE) render a
    // short margin so lighting settles + any GPU work flushes, then exit — or (MOVIE)
    // record the budget + finalize. offscreen_frames is a hang-guard cap.
    ////////////////////////////////////////////
    if (offscreen and not offscreen_forever) {
      os_frame++;
      auto cq           = opq::concurrentQueue();
      bool loader_idle  = cq and (cq->_numPendingOperations.load() == 0)
                             and (cq->_numInFlight.load() == 0);
      int  async_pend   = ork::asyncWorkPending();
      if (async_pend > 0)
        os_saw_async = true;
      os_idle = loader_idle ? (os_idle + 1) : 0;
      // Exit only when BOTH the registered async work (terrain texbake) has drained AND
      // the loader has gone quiet for a sustained run. The loader-idle requirement is the
      // fix for the "--offscreen hangs / --movie survives" bug: exiting while a lazy load
      // (e.g. the envmap, requested only once the terrain first renders) is still in flight
      // strands its GPU upload and DEADLOCKS teardown — the movie path only escaped it by
      // running long enough for the load to finish. The os_frame floor bridges the brief
      // idle window BEFORE that lazy load is even requested.
      bool bakes_done = (not os_saw_async) or (async_pend == 0);
      bool loads_done = (os_idle >= 45) and (os_frame >= 90);
      bool ready      = bakes_done and loads_done;
      bool cap        = (os_frame >= offscreen_frames);
      switch (os_phase) {
        case 0: // WAIT for bakes to drain AND the loader to go quiet (clean teardown)
          if (ready or cap) {
            os_phase  = 1;
            os_settle = 0;
            deco::printf(fvec3::Yellow(),
                         "ork.ecs.player: OFFSCREEN settled frame<%d> async<%s> idle<%d>%s — settling\n",
                         os_frame, ork::asyncWorkSummary().c_str(), os_idle, cap ? " (CAP)" : "");
          }
          break;
        case 1: // SETTLE — short margin (lighting / flush) before exit or recording
          os_settle++;
          if (os_settle >= 20 or cap) {
            if (not movie_path.empty()) {
              auto settings             = std::make_shared<MovieCaptureSettings>();
              settings->_filename       = movie_path;
              settings->_fps            = int(movie_fps);
              settings->_preset_name    = "high";
              settings->_max_queue_size = 300;
              ezapp->enableMovieRecording(settings);
              deco::printf(fvec3::Yellow(), "ork.ecs.player: OFFSCREEN MOVIE -> %s (%d frames @ %d fps)\n",
                           movie_path.c_str(), movie_frames, int(movie_fps));
              os_phase = 2;
              os_movie = 0;
            } else {
              deco::printf(fvec3::Green(), "ork.ecs.player: OFFSCREEN materialize done (frame %d) — exiting\n", os_frame);
              ezapp->signalExit();
              os_phase = 3;
            }
          }
          break;
        case 2: // MOVIE — record movie_frames, then DRAIN before finalize. finishMovieRecording
                // (terminate) blocks the render thread until the frame queue empties, but the
                // queue only drains while THIS thread keeps pumping the GPU (captures are async).
                // So stop enqueuing and pump a margin of frames first — else terminate deadlocks
                // on its own last in-flight capture (the empty 48-byte / no-moov file bug).
          if (os_movie < movie_frames and not cap) {
            os_movie++; // the capture pump above grabbed this frame
          } else {
            if (ezapp->_movie_record_frame_lambda) {
              ezapp->_movie_record_frame_lambda = nullptr; // stop enqueuing new frames
              deco::printf(fvec3::Yellow(), "ork.ecs.player: movie captured %d frames — draining\n", os_movie);
            }
            os_drain++;
            if (os_drain >= 30 or cap) {     // pumped enough for the encoder to drain the queue
              ezapp->finishMovieRecording(); // queue now empty -> terminate returns promptly
              deco::printf(fvec3::Green(), "ork.ecs.player: movie recording finished (%d frames)\n", os_movie);
              ezapp->signalExit();
              os_phase = 3;
            }
          }
          break;
        default: break;
      }
    }
  });

  //////////////////////////////////////////////////////////

  ezapp->onResize([&](int w, int h) {
    if (not controller)
      return;
    auto fbsize_data = std::make_shared<DataTable>();
    (*fbsize_data)["width"_tok]  = w;
    (*fbsize_data)["height"_tok] = h;
    controller->systemNotify(sgsystem, SceneGraphSystem::UpdateFramebufferSize, fbsize_data);
  });

  //////////////////////////////////////////////////////////
  // teardown — updateExit on the update thread, gpuExit on the render thread
  //////////////////////////////////////////////////////////

  ezapp->onUpdateExit([&]() {
    if (controller)
      controller->updateExit();
  });

  ezapp->onGpuExit([&](Context* ctx) {
    if (controller)
      controller->gpuExit(ctx);
    controller = nullptr;
    scenedata  = nullptr;
  });

  //////////////////////////////////////////////////////////

  ezapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  int rval = ezapp->mainThreadLoop();
  opq::concurrentQueue()->drain();
  return rval;
}
