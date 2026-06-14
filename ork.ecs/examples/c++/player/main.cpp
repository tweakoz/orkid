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
// usage:  ork.ecs.player.exe <scene.ecs>  [--adhoc] [--camdist <d>] [--camheight <h>]
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
// startup: uses the NEW HFSM subsystem-driven init (use_subsystems) — the first C++
// host to do so; pass --adhoc to fall back to the legacy inline init for comparison.
//
////////////////////////////////////////////////////////////////

#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
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

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::ecs;

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp) {

  //////////////////////////////////////////////////////////
  // args: <scene.ecs> [--adhoc] [--camdist d] [--camheight h]
  //////////////////////////////////////////////////////////

  std::string scene_path;
  bool use_subsystems = true;
  float cam_dist      = 20.0f;
  float cam_height    = 8.0f;
  float auto_roundtrip = 0.0f; // gate 1.7 scripted: fire the live round-trip at T+N sec
  float auto_walk      = 0.0f; // E.2-walk scripted: hold W for N seconds through the message channel
  bool want_fullscreen = false;
  int want_ssaa        = 0;    // 0/1 = off
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--adhoc")
      use_subsystems = false;
    else if (arg == "--camdist" and (i + 1) < argc)
      cam_dist = atof(argv[++i]);
    else if (arg == "--camheight" and (i + 1) < argc)
      cam_height = atof(argv[++i]);
    else if (arg == "--roundtrip" and (i + 1) < argc)
      auto_roundtrip = atof(argv[++i]);
    else if (arg == "--autowalk" and (i + 1) < argc)
      auto_walk = atof(argv[++i]); // E.2-walk gate: hold W for N seconds (t=1..1+N) via InputKey messages
    else if (arg == "--fullscreen" or arg == "-f")
      want_fullscreen = true;
    else if ((arg == "--ssaa" or arg == "-t") and (i + 1) < argc)
      want_ssaa = atoi(argv[++i]); // supersample multiplier -> the SG screen node (scenegraph.cpp "ssaa")
    else if (arg == "-s" and (i + 1) < argc)
      scene_path = argv[++i];
    else if (arg[0] != '-')
      scene_path = arg;
  }
  //////////////////////////////////////////////////////////
  // SHORT-NAME resolution + discovery (mirrors the python viewers): a bare name
  // resolves to <workspace>/ork.data/ecsscenes/<name>.ecs; no args (or a miss)
  // lists what's available there.
  //////////////////////////////////////////////////////////

  std::string scenes_dir;
  if (const char* ws = getenv("ORKID_WORKSPACE_DIR"))
    scenes_dir = std::string(ws) + "/ork.data/ecsscenes";

  auto list_scenes = [&]() {
    printf("usage: ork.ecs.player.exe <scene.ecs | shortname> [--adhoc] [--camdist d] [--camheight h]\n");
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

  if (scene_path.empty()) {
    list_scenes();
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
    list_scenes();
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
  if (use_subsystems) {
    init_data->_fullscreen         = want_fullscreen;
    init_data->_use_subsystems     = true;
    init_data->_defer_gpu_init     = true;
    init_data->_enabled_subsystems = {"opq", "core", "gpu", "lev2"};
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

  deco::printf(
      fvec3::Yellow(),
      "ork.ecs.player: %s startup, scene<%s> (%zu bytes)\n",
      use_subsystems ? "SUBSYSTEM (HFSM)" : "ad-hoc",
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
  uicam->_base_zmoveamt = 2.0f;
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
    if (walk_mode and (ev->_eventcode == ui::EventCode::KEY_DOWN or ev->_eventcode == ui::EventCode::KEY_UP) and
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
        // RAW keys go to the scene's input SCRIPT (PythonSystem) — it owns the keymap
        // and translates to semantic CharacterControllerSystem actions.
        static int dbg_fwd = 0;
        if (dbg_fwd < 8) {
          dbg_fwd++;
          deco::printf(fvec3::Cyan(), "ork.ecs.player: fwd key<%d> down<%d> -> PythonSystem\n",
                       int(ev->miKeyCode), int(ev->_eventcode == ui::EventCode::KEY_DOWN ? 1 : 0));
        }
        c->systemNotify(pysystem, "InputKey"_tok, keytab);
      }
      return ui::HandlerResult();
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
    controller_ptr_t c;
    {
      std::lock_guard<std::mutex> lock(ctl_mutex);
      c = controller;
    }
    if (c)
      c->render(drwev);
    framecounter++;
    if (fps_timer.SecsSinceStart() > 5.0f) {
      float FPS = float(framecounter) / fps_timer.SecsSinceStart();
      deco::printf(fvec3::White(), "ork.ecs.player FPS<%g>\n", FPS);
      fps_timer.Start();
      framecounter = 0;
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
