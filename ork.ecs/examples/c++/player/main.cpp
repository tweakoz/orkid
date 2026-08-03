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
// THE KEY REGISTRY. This file is where the fleet's key bindings are written down —
// the host CONSUMES only what is listed here and forwards every other key transition
// to the scene's PythonSystem, so a scene script's binding collides with a host key
// silently unless it is checked against this list. Taken, in order of precedence:
//   host        ` ~ (perf HUD)   P (walk pause)   Space (pause when not walking)
//               Cmd+Right / Cmd+Down / Cmd+R
//   EzUiCam     Z X C V (rotate / pan / dolly / zoom)
//   --devkeys   E G T H M B R  (see below)
//   PYTHON-SIDE (host forwards; owned by the scene's scripts, listed here so the
//   next binding does not land on top of them):
//     walk_input_system.py  W A S D · cursor keys · Space · / · Shift · CapsLock
//     sky_time_system.py    ] [ scrub time · \ pause sky clock · = - sky speed
//                           ' ; step a day · 0 reset to the authored hour
//
// --devkeys (OPT-IN; default OFF): viewer-grade dev keys (the C++ lowering of
//   ork.ecsplay.py's controls). Injects an ACES+HSVG postfx chain (the "viewer look" —
//   the ACES curve changes the image even before any key), then:
//     E  cycle envmap    G  gamma    T  ACES exposure    H  saturation
//     M  terrain material (declared + scene debug looks)    R  reset post-fx
//   plus an always-on upper-left key legend (deterministic; absent flagless).
//   Flagless is byte-identical to the scene-authored look.
//
// AUDIO (OPT-IN; default OFF = no device opened, scenes play silent):
//   --audio                  open the real audio device (sound emitters + global synth)
//   "audio": true            same, declared by the scene: a TOP-LEVEL key beside "root"
//                            in the .ecs manifest (peeked pre-create; see below)
//   (--movie's STREAM device is a capture sink and wins over --audio when both are given.)
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
#include <ork/lev2/input/gamepaddevice.h> // S1 gamepad: digital day-1 remap (pad -> the same InputKey channel as the keyboard)
#include <ork/lev2/vr/vr.h> // --vr: query the active XR device (orkidvr::device()) to select the VR render model
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeACES.h> // --devkeys viewer-look postfx (tonemap)
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeHSVG.h> // --devkeys viewer-look postfx (gamma/saturation)
#include <ork/ecs/physics/CharacterController.h> // E.2-walk: input forwarding + camera yield
#include <ork/ecs/physics/bullet.h> // --physics-debug: BulletSystemData detection + debug-wireframe notify target
#include <ork/ecs/pysys/PythonComponent.h>          // E.2-walk: scene-declared input-script routing
#include <ork/python/context.h>                     // embedded interpreter (OPT-IN: scene declares PythonSystem)
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <rapidjson/document.h> // pre-create manifest peek (--audio's scene-declared twin)
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
#include "keyshud.h" // --devkeys on-screen key legend (always-on with --devkeys)

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
  float auto_yaw        = 0.0f; // scripted: hold cursor-RIGHT for N seconds (rotation-only discriminator)
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
  std::string snapshot_path;      // --snapshot PATH (implies offscreen): settled frame -> PNG
  int   snapshot_frame   = 0;     // --snapshot-frame N: capture N frames AFTER first-lit (0=at first-lit)
  float settle_timeout   = 0.0f;  // --settle-timeout SEC: absolute wall-clock hang backstop for the
                                  // offscreen settle/snapshot drain (0 -> auto default). While the async
                                  // registry is non-empty the drain keeps PUMPING (a bake makes real
                                  // progress each frame); this ceiling is the ONLY thing that ends a
                                  // still-pending drain — and it ends it as a LOUD FAIL, never a black frame.
  // --devkeys: OPT-IN interactive viewer-grade dev keys (mirror ork.ecsplay.py). DEFAULT OFF
  // so a flagless run is byte-identical to the scene-authored look. When ON the player injects
  // an ACES+HSVG postfx chain (the "viewer look" — ACES changes the image even before any key).
  bool  devkeys          = false;
  std::string devkeys_script;     // TEST HOOK: comma list LABEL:FRAME firing dev keys through the same handler
  // --pysysnotify: TEST HOOK for the SCENE SCRIPTS' own message vocabulary (sky-clock controls,
  // walk actions, ...). Scripted controller messages to the PythonSystem on the same channel the
  // keyboard uses — so an offscreen gate or a scripted movie drives exactly what a key drives.
  // The player stays scene-agnostic: it forwards names and float fields, it interprets nothing.
  std::string pysys_notify_script;
  // --physics-debug: Bullet debug wireframe ON from startup (TOGGLE_DEBUG_DRAW to the
  // BulletSystem). Flag-driven so it works where no keyboard surface exists (VR windowless,
  // offscreen gates); [B] under --devkeys toggles the same state live.
  bool  physics_debug    = false;
  // --vr: play the scene on the HMD through the active XR runtime (the FWDPBRVRDM render model).
  // Opt-in. With no runtime the SAME render model runs against a NoVr device — stereo on the
  // desktop (the mirror blit is the presentation), never a silent demotion to desktop mono.
  bool  want_vr          = false;
  // --audio: bring up the real audio device (scene-declared sound emitters / synth), the
  // C++ equivalent of ork.ecsplay.py's enable_audio/_output/_synth. Independent of --movie,
  // whose STREAM device is a capture sink, not playback. Default OFF: a flagless run of a
  // sound-declaring scene stays silent and opens no device.
  bool  want_audio       = false;

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
      ("autoyaw", po::value<float>(&auto_yaw)->default_value(0.0f), "walk scenes: scripted hold-cursor-RIGHT for N seconds (yaw sweep from a FIXED position; the rotation-only counterpart of --autowalk)")
      ("fullscreen,f", po::bool_switch(&want_fullscreen), "fullscreen window")
      ("width,W", po::value<int>(&want_width)->default_value(0), "initial window width (windowed mode; 0=default)")
      ("height,H", po::value<int>(&want_height)->default_value(0), "initial window height (windowed mode; 0=default)")
      ("hidpi", po::bool_switch(&want_hidpi), "render at the display's backing (Retina) scale; default is LoDPI to save fillrate")
      ("ssaa,t", po::value<int>(&want_ssaa)->default_value(0), "supersample multiplier (>1 enables SSAA)")
      ("offscreen", po::bool_switch(&offscreen), "headless: hidden window, render until settled, then exit (prime caches)")
      ("offscreen-forever", po::bool_switch(&offscreen_forever), "headless: render indefinitely, unthrottled, no settle-exit (kill to stop; ignores --movie)")
      ("frames", po::value<int>(&offscreen_frames)->default_value(0), "offscreen frame safety-cap (0=auto 1200; normal exit is load-settle driven)")
      ("movie,m", po::value<std::string>(&movie_path), "record an offscreen movie to PATH (mp4; implies --offscreen)")
      ("moviefps", po::value<float>(&movie_fps)->default_value(60.0f), "movie frame rate")
      ("movieframes,l", po::value<int>(&movie_frames)->default_value(0), "movie length in frames (0=300)")
      ("snapshot,S", po::value<std::string>(&snapshot_path), "write the settled offscreen frame to PATH (png; implies --offscreen; agent/CI eyeball)")
      ("snapshot-frame,F", po::value<int>(&snapshot_frame)->default_value(0), "capture --snapshot N frames AFTER the composite first goes lit (deterministic; 0=at first-lit)")
      ("settle-timeout", po::value<float>(&settle_timeout)->default_value(0.0f), "offscreen settle/snapshot wall-clock HANG ceiling in seconds (0=auto 180). While async work is still pending the drain keeps pumping until this ceiling; on expiry (or a settled-black scene) it FAILS loud (SNAPSHOT_RESULT=FAIL) and exits NONZERO — never a black+rc=0 snapshot")
      ("devkeys", po::bool_switch(&devkeys), "enable interactive viewer-grade dev keys [E cycle envmap, G gamma, T ACES exposure, H saturation, M terrain material (declared + scene-declared debug looks), B physics debug wireframe, R reset] + an always-on on-screen key legend. INJECTS an ACES+HSVG postfx chain, so the look changes (the 'viewer look') even before any keypress. Default OFF = scene-authored look, byte-identical to today.")
      ("devkeys-script", po::value<std::string>(&devkeys_script)->default_value(""), "TEST HOOK (implies --devkeys): comma list LABEL:FRAME (e.g. \"E:120,G:180,CMDR:60\") firing dev keys through the SAME handler at update-tick FRAME; LABEL is E/G/T/H/M/B/R or CMDR (the live round-trip)")
      ("pysysnotify", po::value<std::string>(&pysys_notify_script)->default_value(""), "TEST HOOK: scripted controller messages to the scene's PythonSystem, on the SAME channel a keyboard/host uses. Semicolon list TIME:EVENT:field=value,... with TIME in seconds of sim abstime (e.g. \"2:SkyTimeSet:hour=18.5;5:InputKey:key=93,down=1;8:InputKey:key=93,down=0\" — a scripted hour, then the ']' key held for 3s). Values are floats; the scene's python script owns the vocabulary. No-op (with a notice) on a scene that declares no PythonSystem.")
      ("physics-debug", po::bool_switch(&physics_debug), "Bullet physics debug wireframe ON from startup (collision shapes + contacts over the visual scene). Works without --devkeys and with no keyboard surface (VR/offscreen); [B] under --devkeys toggles the same state live. Scenes with no BulletSystem log a notice and play normally.")
      ("vr", po::bool_switch(&want_vr), "VR: present the scene on the HMD through the active XR runtime (selects the FWDPBRVRDM render model). Requires ORKID_VR_DRIVER=openxr + a live runtime; with no runtime the same render model runs on a NoVr device — side-by-side stereo on the desktop, mode named in a one-line notice.")
      ("audio", po::bool_switch(&want_audio), "AUDIO: open the real audio device so scene-declared sound emitters / the global synth are audible (the C++ lowering of ork.ecsplay.py's enable_audio). Default OFF = no device opened, scene plays silent. A scene enables itself by declaring a top-level \"audio\": true beside \"root\" in its .ecs manifest.");

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
  if (not devkeys_script.empty())
    devkeys = true; // the test hook drives the dev-key handler, so the chain must be present

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
  // AUDIO enable (--audio, or the scene manifest). The audio subsystem is built from
  // _enable_audio inside OrkEzApp::create, but the scenedata only deserializes in
  // onGpuInit — far too late to gate subsystem construction. So a manifest-declared
  // enable is a PEEK of the scene text (same constraint that forces the
  // PythonSystemData peek further down). Author the key at the TOP LEVEL, beside
  // "root": JsonDeserializer reads _document["root"] only, so siblings are inert on
  // load — but they are also NOT re-emitted by a serialize round-trip (Cmd+R, ecsedit),
  // which is what a reflected SceneData property would eventually buy. The peek PARSES
  // rather than greps: a substring scan would flip on any nested component property
  // that happens to be named "audio", so the key is verified at the actual top level.
  // The three flags are ork.ecsplay.py's set verbatim, and carry the pyext invariant
  // (pyext.cpp:137-138): synth implies output implies audio. An unresolvable or busy
  // device degrades to the NULL device (loud error, app keeps running), so enabling
  // here can never take down a headless/CI boot.
  //////////////////////////////////////////////////////////

  auto manifest_declares_audio = [&]() -> bool {
    rapidjson::Document doc;
    doc.Parse(scene_json.c_str());
    // unparseable here == no declaration; the real deserialize below reports it loudly.
    if (doc.HasParseError() or not doc.IsObject())
      return false;
    auto it = doc.FindMember("audio");
    return it != doc.MemberEnd() and it->value.IsBool() and it->value.GetBool();
  };

  bool scene_wants_audio = manifest_declares_audio();
  if (want_audio or scene_wants_audio) {
    init_data->_enable_audio        = true;
    init_data->_enable_audio_output = true;
    init_data->_enable_audio_synth  = true;
    deco::printf(
        fvec3::Yellow(),
        "ork.ecs.player: AUDIO enabled (%s)\n",
        want_audio ? "--audio" : "scene manifest \"audio\":true");
  }

  //////////////////////////////////////////////////////////
  // OFFSCREEN: hidden window (GLFW_VISIBLE=false, no swapchain), still a full
  // _mainWindow + render thread, so mainThreadLoop renders frames headless. The
  // onDraw frame-budget below drives capture + signalExit. --movie implies offscreen.
  //////////////////////////////////////////////////////////
  if (not movie_path.empty())
    offscreen = true;
  if (not snapshot_path.empty())
    offscreen = true;    // --snapshot implies offscreen (movie wins if both are given)
  if (offscreen_forever) {
    offscreen  = true;   // headless
    movie_path = "";     // forever is the no-movie soak/perf path
    snapshot_path = "";
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
      // Capture WINS over the --audio playback device: with both, the scene's audio
      // lands in the movie file instead of the speakers.
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
    // settle_timeout is the TRUE hang backstop (wall-clock), decoupled from the frame
    // cap: a slow-but-healthy bake (e.g. an HDRI radiancemap prefilter that needs a few
    // thousand frames) drains past offscreen_frames and lights; only a genuine hang runs
    // out this clock. 180s is generous for every current scene on every gate node.
    if (settle_timeout <= 0.0f)
      settle_timeout = 180.0f;
    if (not offscreen_forever)
      deco::printf(fvec3::Yellow(), "ork.ecs.player: OFFSCREEN mode (cap<%d frames> settle-timeout<%gs>%s)\n",
                   offscreen_frames, settle_timeout, movie_path.empty() ? "" : (" movie<" + movie_path + ">").c_str());
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

  // ALPHA ORACLE (mac-verifiable, no VR needed): ORKID_PERFHUD_ALPHA_ORACLE routes the VR
  //  panel RT render onto the desktop path and reads the RT back — proving the panel
  //  TEXTURE carries a translucent bg (alpha ~0.5) and solid text (alpha ~1). This is the
  //  SUSPECT-A check for the panel-translucency fix. One-shot print, then the run settles out.
  bool alpha_oracle = getenv("ORKID_PERFHUD_ALPHA_ORACLE") != nullptr;
  if (alpha_oracle) {
    perfhud._vrmode = true; // frameEndAndDraw -> _renderPanelRT (populates perfhud._hudRTG)
    perfhud._mode   = ork::ecs::player::PerfHud::TEXT;
  }
  // ORKID_FORCE_DMVR: run the REAL DualMonoVr path on the desktop (NoVr preview) so the
  //  actual _drawHudPanel eye-pass executes headless on mac — the panel lands in both down
  //  buffers and the desktop mirror. Pair with --snapshot to eyeball a see-through panel.
  bool force_dmvr = getenv("ORKID_FORCE_DMVR") != nullptr;

  // --devkeys key legend HUD (always-on when --devkeys; upper-left). Deterministic content.
  ork::ecs::player::KeysHud keyshud;
  keyshud._enabled = devkeys;

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
  sys_ref_t bulletsystem; // physics-debug toggle target (resolved only when the scene declares one)
  bool bullet_mode = false; // scene declares a BulletSystemData
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
  int  os_phase     = 0;   // 0=WAIT 1=SETTLE 2=MOVIE 3=DONE 4=SNAPSHOT-drain 5=MOVIE-predrain
  bool os_saw_async = false; // observed registered async work (a bake) — wait for it to drain
  int  os_snapdrain = 0;   // frames pumped while the snapshot's async readback lands
  auto os_snap_done = std::make_shared<std::atomic<bool>>(false); // set once a LIT frame is written
  auto os_snap_fail = std::make_shared<std::atomic<bool>>(false); // set on a FAILED snapshot (hung/black) -> NONZERO exit
  auto os_movie_fail = std::make_shared<std::atomic<bool>>(false); // set on a FAILED movie pre-roll (unsettled/timeout) -> NONZERO exit
  int  os_snap_quiescent = 0;   // consecutive drain frames with the async registry EMPTY (settled-black backstop)
  Timer os_walltimer;           // wall-clock since the first offscreen frame (settle-timeout hang backstop)
  bool  os_walltimer_started = false;
  // SNAPSHOT capture state (phase 4). The single blind capture-at-settle this
  // replaces landed in the composite's post-settle warmup window (the offscreen
  // settle gate keys on loader-idle, which fires ~10-30 frames BEFORE the
  // compositor first writes a non-black frame into _main_rtg) AND leaned on a
  // one-shot async-readback callback that never fired while the loader sat idle
  // (its BGRA->RGBA conversion is enqueued on opq::concurrentQueue, whose worker
  // pool parks itself when idle — so a lone task issued at quiescence was not
  // serviced until teardown). Both are why --snapshot wrote all-black PNGs
  // (task #42). The movie path never hit either: it captures EVERY frame (past
  // the warmup, and keeping the concurrentQueue busy) and POLLS the future's
  // isReady() rather than waiting on a callback. This mirrors that: re-issue a
  // capture each drain frame, poll it, and only accept a frame with real
  // content — self-adjusting past the warmup with no magic settle count.
  captureasync_ptr_t  snap_future;        // in-flight capture (one at a time)
  capturebuffer_ptr_t snap_capbuf;        // its readback buffer
  capturebuffer_ptr_t snap_last_capbuf;   // last landed frame (lit if we got one; else black fallback)
  int  snap_issue_drain = 0;                    // os_snapdrain when snap_future was issued (stale-future guard)
  int  snap_first_lit   = -1;                   // os_snapdrain at which the composite FIRST went settled-lit (-1=not yet)
  bool snap_probe_color = false;                // last landed probe had real color (quiescent-backstop input)

  // Shared drain tuning — snapshot capture (phase 4) AND movie pre-roll (phase 5).
  constexpr int kSnapWarmup  = 5;   // frames rendered before the first capture grab
  constexpr int kSnapQuiesce = 600; // sky-ready + still black this many frames => settled-black FAIL
  constexpr int kFutureStale = 30;  // a capture future un-ready this long => drop it + re-issue

  // Settled-lit color probe — shared by the snapshot capture drain (phase 4) and the
  // movie pre-roll drain (phase 5), so both gate on identical criteria. "Lit" = >=0.1%
  // of pixels carry a non-zero RGB channel; alpha is skipped (main_rtg clears to opaque
  // black (0,0,0,255), so counting alpha would read a black frame as lit). tot/lit/rgbmax
  // are returned for the diagnostic line.
  auto probeCaptureColor = [](image_ptr_t img, size_t& tot_px, size_t& lit_px, uint8_t& rgbmax) -> bool {
    tot_px = 0; lit_px = 0; rgbmax = 0;
    if (img and img->_data) {
      const uint8_t* p = img->_data->data();
      size_t         n = img->_data->length();
      tot_px = n / 4;
      for (size_t i = 0; i + 3 < n; i += 4) {
        uint8_t r = p[i], g = p[i + 1], b = p[i + 2];
        uint8_t m = std::max(r, std::max(g, b));
        if (m > rgbmax) rgbmax = m;
        if (r or g or b) lit_px++;
      }
    }
    return (tot_px > 0) and (lit_px * 1000 >= tot_px); // >=0.1% pixels colored
  };

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
  // --devkeys — OPT-IN viewer-grade dev keys (the C++ lowering of ork.ecsplay.py's
  // E/S/G/T/R controls). The player OWNS the ACES+HSVG postfx nodes: injected into
  // the scene's SceneGraphSystemData before bind, then poked per-frame from the UI
  // thread (DoRender re-reads _exposure/_gamma/_saturation — the same benign race the
  // python viewer accepts, no camera writes so no VR gating needed). Envmap [E] routes
  // through the SG system's SetEnvmap notify (host owns the cycle list). Everything here
  // is inert unless `devkeys` — a flagless run never creates a node or touches the scene.
  // KEY MAP (bare, no super): E=envmap  G=gamma  T=ACES exposure  H=saturation  M=terrain mat  B=physics wireframe  R=reset.
  // Saturation is 'H': python's 'S' is a walk-move key, and 'C' is an EzUiCam dolly modifier
  // (X/C/V = pan/dolly/zoom) — the devkeys block owns ONLY E/G/T/H/M/R and falls through for the rest.
  // The scene-script keys (walk W/A/S/D..., sky-clock ] [ \ = - ' ; 0) are deliberately NOT here:
  // they are forwarded to the PythonSystem below. See the key registry in this file's header.
  //////////////////////////////////////////////////////////
  const std::vector<float> satset = {0.0f, 0.2f, 0.5f, 0.6, 0.75f, 0.8f, 1.0f, 1.25f, 1.5f};
  const std::vector<float> gamset = {0.8f, 1.0f, 1.2f, 1.4f, 1.6f, 1.8f, 2.2f};
  const std::vector<float> expset = {0.0f, 0.5f, 0.75f, 0.9f, 1.0f, 1.25f, 1.5f};
  auto idx_of = [](const std::vector<float>& v, float x) {
    return int(std::find(v.begin(), v.end(), x) - v.begin());
  };
  // defaults: saturation 0.8 / exposure 0.75 (owner-tuned 07-23); gamma 1.0
  const int sat_def = idx_of(satset, 0.6f), gam_def = idx_of(gamset, 1.0f), exp_def = idx_of(expset, 0.9f);
  int sat_idx = sat_def, gam_idx = gam_def, exp_idx = exp_def;
  std::vector<std::string> envmap_paths; // "<assetcache>/envmaps2/<name>.xir"
  std::vector<std::string> envmap_names;
  int envmap_index = -1;
  std::shared_ptr<PostFxNodeACES> aces_node;
  std::shared_ptr<PostFxNodeHSVG> hsvg_node;
  if (devkeys) {
    aces_node              = std::make_shared<PostFxNodeACES>();
    hsvg_node              = std::make_shared<PostFxNodeHSVG>();
    aces_node->_exposure   = expset[exp_idx];
    hsvg_node->_hue        = 0.0f;
    hsvg_node->_saturation = satset[sat_idx];
    hsvg_node->_value      = 1.0f;
    hsvg_node->_gamma      = gamset[gam_idx];
    // Envmap cycle list — filesystem-driven (mirrors ecsplay.py): only files present.
    if (const char* stg = getenv("OBT_STAGE")) {
      auto dir = std::string(stg) + "/assetcache/envmaps2";
      if (std::filesystem::is_directory(dir)) {
        std::vector<std::string> names;
        for (const auto& e : std::filesystem::directory_iterator(dir))
          if (e.path().extension() == ".xir")
            names.push_back(e.path().stem().string());
        std::sort(names.begin(), names.end());
        for (const auto& n : names) {
          envmap_names.push_back(n);
          envmap_paths.push_back("<assetcache>/envmaps2/" + n + ".xir");
        }
      }
    }
    deco::printf(fvec3::Yellow(),
                 "ork.ecs.player: --devkeys ON (viewer look: ACES+HSVG) envmaps<%zu> keys[E/G/T/H/M/B/R]\n",
                 envmap_paths.size());
  }

  // [M] TERRAIN MATERIAL-OVERRIDE cycle: mode 0 = declared (EXACTLY today's path, no override);
  // modes 1..N are the scene-declared debug materials. The label list + cycle LENGTH are DATA,
  // derived at load from the terrain drawable's reflected debug_material_assets — so adding a
  // debug look needs no player edit. Mode 0 stays byte-identical. The mode rides a systemNotify
  // to the SG system (SetEnvmap pattern); the SG system routes it to the terrain drawable(s).
  int mat_mode = 0;
  std::vector<std::string> matmode_labels = {"declared"}; // [0]=declared; [1..] filled from scene at load
  auto matmode_label = [&](int m) -> const char* {
    return (m >= 0 and m < int(matmode_labels.size())) ? matmode_labels[m].c_str() : "declared";
  };

  // [B] physics-debug HUD state: player-side TOGGLE PARITY (the truth lives system-side as
  // Debug ^ _debugToggle; scenes today never declare Debug=true, so parity == effective state).
  // Starts true when --physics-debug fires the startup toggle; flips on every [B].
  bool phys_dbg_on = false;
  auto physdbg_label = [&]() -> std::string {
    if (not bullet_mode)
      return "n/a";
    return phys_dbg_on ? "ON" : "OFF";
  };

  // Rebuild the always-on --devkeys legend from the CURRENT cycle state. DETERMINISTIC —
  // key names + values only (no fps/clock/frame counters) so the snapshot byte-identity
  // gates hold. Called at the end of every fire_devkey and once at startup.
  auto update_keys_hud = [&]() {
    std::string env = (envmap_index >= 0 and envmap_index < int(envmap_names.size()))
                          ? envmap_names[envmap_index]
                          : "(scene default)";
    keyshud.setState(env, gamset[gam_idx], expset[exp_idx], satset[sat_idx], matmode_label(mat_mode), physdbg_label());
    // Deterministic, timing-independent observable of the on-screen legend's CURRENT content
    // (the HUD gate keys on this to distinguish a stale legend from an updated one).
    deco::printf(fvec3::Cyan(),
                 "ork.ecs.player: keyshud [E]%s [G]%.2f [T]%.2f [H]%.2f [M]%s [B]%s\n",
                 env.c_str(), gamset[gam_idx], expset[exp_idx], satset[sat_idx], matmode_label(mat_mode),
                 physdbg_label().c_str());
  };

  // The dev-key action, factored so the real key handler (onUiEvent) and the scripted
  // test hook (onUpdate) drive the IDENTICAL path. Safe to call from either thread — the
  // envmap notify mirrors the autowalk send-key pattern; the postfx pokes are plain float
  // writes the render thread re-reads (the accepted benign race).
  auto fire_devkey = [&](int keycode) {
    switch (keycode) {
      case 'E': {
        if (envmap_paths.empty()) {
          deco::printf(fvec3::Yellow(), "ork.ecs.player: [E] no envmaps in <stage>/assetcache/envmaps2\n");
          break;
        }
        envmap_index = (envmap_index + 1) % int(envmap_paths.size());
        controller_ptr_t c;
        sys_ref_t sgs;
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          c   = controller;
          sgs = sgsystem;
        }
        if (c) {
          auto tab           = std::make_shared<DataTable>();
          (*tab)["path"_tok] = envmap_paths[envmap_index];
          c->systemNotify(sgs, "SetEnvmap"_tok, tab);
          deco::printf(fvec3::Green(), "ork.ecs.player: [E] envmap -> %s\n", envmap_names[envmap_index].c_str());
        }
        break;
      }
      case 'G':
        gam_idx = (gam_idx + 1) % int(gamset.size());
        if (hsvg_node)
          hsvg_node->_gamma = gamset[gam_idx];
        deco::printf(fvec3::Green(), "ork.ecs.player: [G] gamma -> %g\n", gamset[gam_idx]);
        break;
      case 'T':
        exp_idx = (exp_idx + 1) % int(expset.size());
        if (aces_node)
          aces_node->_exposure = expset[exp_idx];
        deco::printf(fvec3::Green(), "ork.ecs.player: [T] ACES exposure -> %g\n", expset[exp_idx]);
        break;
      case 'H': // saturation — NOT 'C' (EzUiCam reserves X/C/V for pan/dolly/zoom)
        sat_idx = (sat_idx + 1) % int(satset.size());
        if (hsvg_node)
          hsvg_node->_saturation = satset[sat_idx];
        deco::printf(fvec3::Green(), "ork.ecs.player: [H] saturation -> %g\n", satset[sat_idx]);
        break;
      case 'M': {
        // cycle over the DATA-derived label list (declared + scene debug materials). A scene with
        // no debug materials => size 1 => M stays at mode 0 (a no-op, already logged at load).
        mat_mode = (mat_mode + 1) % int(matmode_labels.size());
        controller_ptr_t c;
        sys_ref_t sgs;
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          c   = controller;
          sgs = sgsystem;
        }
        if (c) {
          auto tab           = std::make_shared<DataTable>();
          (*tab)["mode"_tok] = mat_mode;
          c->systemNotify(sgs, "SetTerrainMaterialMode"_tok, tab);
          deco::printf(fvec3::Green(), "ork.ecs.player: [M] terrain material -> %s (mode %d)\n",
                       matmode_label(mat_mode), mat_mode);
        }
        break;
      }
      case 'B': {
        // physics debug wireframe toggle — same TOGGLE_DEBUG_DRAW path as --physics-debug.
        // State lives system-side (_debugToggle XOR reflected Debug); the system prints ON/OFF.
        controller_ptr_t c;
        sys_ref_t bsys;
        bool have_bullet;
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          c           = controller;
          bsys        = bulletsystem;
          have_bullet = bullet_mode;
        }
        if (c and have_bullet) {
          c->systemNotify(bsys, "TOGGLE_DEBUG_DRAW"_tok, std::make_shared<DataTable>());
          phys_dbg_on = not phys_dbg_on;
          deco::printf(fvec3::Green(), "ork.ecs.player: [B] physics debug wireframe -> %s\n",
                       phys_dbg_on ? "ON" : "OFF");
        } else {
          deco::printf(fvec3::Yellow(), "ork.ecs.player: [B] scene declares no BulletSystem\n");
        }
        break;
      }
      case 'R':
        sat_idx = sat_def;
        gam_idx = gam_def;
        exp_idx = exp_def;
        if (hsvg_node) {
          hsvg_node->_saturation = satset[sat_idx];
          hsvg_node->_gamma      = gamset[gam_idx];
        }
        if (aces_node)
          aces_node->_exposure = expset[exp_idx];
        deco::printf(fvec3::Green(), "ork.ecs.player: [R] reset post-fx\n");
        break;
      default:
        break;
    }
    update_keys_hud(); // reflect the new state in the always-on legend
  };

  // --devkeys-script: parse "LABEL:FRAME,..." into scripted key events fired on the
  // update thread when the tick reaches FRAME. CMDR fires the live round-trip (Cmd+R).
  struct DevKeyEvent {
    int  keycode = 0;
    bool cmdr    = false;
    int  frame   = 0;
    bool fired   = false;
  };
  std::vector<DevKeyEvent> devkey_events;
  if (not devkeys_script.empty()) {
    std::stringstream ss(devkeys_script);
    std::string item;
    while (std::getline(ss, item, ',')) {
      auto colon = item.find(':');
      if (colon == std::string::npos)
        continue;
      std::string label = item.substr(0, colon);
      DevKeyEvent dke;
      dke.frame = atoi(item.substr(colon + 1).c_str());
      if (label == "CMDR")
        dke.cmdr = true;
      else if (not label.empty()) {
        char c0 = label[0];
        if (c0 >= 'a' and c0 <= 'z')
          c0 = char(c0 - 'a' + 'A');
        dke.keycode = (unsigned char)c0;
      }
      devkey_events.push_back(dke);
    }
    deco::printf(fvec3::Yellow(), "ork.ecs.player: --devkeys-script parsed %zu event(s)\n", devkey_events.size());
  }
  int devkey_tick = 0;
  if (devkeys)
    update_keys_hud(); // seed the legend with the default cycle state (before any keypress)

  // --pysysnotify: parse "TIME:EVENT:field=value,...;..." into scripted PythonSystem
  // messages fired on the update thread once abstime reaches TIME. The player owns NO
  // vocabulary here — event name and float fields go through verbatim, which is what
  // makes one hook serve every scene script (a key is InputKey:key=..,down=..).
  struct PySysNotifyEvent {
    std::string event;
    std::vector<std::pair<std::string, float>> fields;
    float time  = 0.0f;
    bool  fired = false;
  };
  std::vector<PySysNotifyEvent> pysys_notify_events;
  if (not pysys_notify_script.empty()) {
    std::stringstream ss(pysys_notify_script);
    std::string item;
    while (std::getline(ss, item, ';')) {
      if (item.empty())
        continue;
      auto c1 = item.find(':');
      if (c1 == std::string::npos) {
        deco::printf(fvec3::Red(), "ork.ecs.player: --pysysnotify entry <%s> has no TIME:EVENT\n", item.c_str());
        continue;
      }
      auto c2 = item.find(':', c1 + 1);
      PySysNotifyEvent ev;
      ev.time  = float(atof(item.substr(0, c1).c_str()));
      ev.event = (c2 == std::string::npos) ? item.substr(c1 + 1) : item.substr(c1 + 1, c2 - c1 - 1);
      if (c2 != std::string::npos) {
        std::stringstream fs(item.substr(c2 + 1));
        std::string field;
        while (std::getline(fs, field, ',')) {
          auto eq = field.find('=');
          if (eq == std::string::npos)
            continue;
          ev.fields.emplace_back(field.substr(0, eq), float(atof(field.substr(eq + 1).c_str())));
        }
      }
      pysys_notify_events.push_back(ev);
    }
    deco::printf(fvec3::Yellow(), "ork.ecs.player: --pysysnotify parsed %zu message(s)\n", pysys_notify_events.size());
  }

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
    ////////////////////////////////////////////
    // [M] derive the material-cycle labels from scene DATA (the terrain drawable's reflected
    // debug_material_assets) — no hardcoded mode table. Label = the asset-name suffix after
    // "_dbg_". Empty list => the [M] cycle is a no-op (logged once here).
    ////////////////////////////////////////////
    {
      auto dbg_assets = terrainDebugMaterialAssets(scenedata);
      matmode_labels.assign(1, std::string("declared"));
      for (const auto& a : dbg_assets) {
        auto pos = a.rfind("_dbg_");
        matmode_labels.push_back(pos != std::string::npos ? a.substr(pos + 5) : a);
      }
      if (dbg_assets.empty())
        deco::printf(fvec3::Yellow(),
                     "ork.ecs.player: no terrain debug materials declared -- [M] is a no-op\n");
      else
        deco::printf(fvec3::Green(),
                     "ork.ecs.player: [M] terrain material cycle -> %zu debug mode(s)\n",
                     dbg_assets.size());
    }
    if (want_ssaa > 1) { // host display preference -> the SG screen node (link copies userparams)
      for (const auto& it : scenedata->getSystemDatas())
        if (auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second))
          sgd->setUserSceneParam("ssaa", want_ssaa);
      deco::printf(fvec3::Yellow(), "ork.ecs.player: ssaa<%d>\n", want_ssaa);
    }
    ////////////////////////////////////////////
    // --vr: select the VR render model (FWDPBRVRDM). The XR device was selected pre-Vulkan
    // from ORKID_VR_DRIVER and, by now (post graphics-init), is _active iff its session came
    // up. Forcing the preset here (a user scene param, like ssaa above) routes BOTH the
    // compositor (presetForwardPBRVRDM) and the SceneGraphSystem VR-device wiring.
    //
    // The runtime check picks the DEVICE, not the render model: with a live runtime the
    // active XR device drives the HMD; with none, SceneGraphSystem registers a NoVr device
    // and the output node's desktop-mirror blit IS the presentation (side-by-side stereo).
    // --vr therefore always means stereo — a request for VR is never answered with mono.
    //
    // PRECEDENCE with ORKID_FORCE_DMVR (below): none needed here — both arms set the SAME
    // preset param, so the two levers cannot disagree at this level. Where they DO meet is
    // inside the preset resolver (scenegraph.cpp): ORKID_FORCE_DMVR is capability- AND
    // autoselect-immune by its own charter (it means "this node", not "the best node"), so
    // it beats ORKID_SPVR=1 and FWDPBRVRDM stays literal DualMonoVr.
    ////////////////////////////////////////////
    if (want_vr) {
      auto vrdev      = ::ork::lev2::orkidvr::device();
      const char* drv = getenv("ORKID_VR_DRIVER");
      bool vr_active  = vrdev and vrdev->_active and drv and (std::string(drv) == "openxr");
      if (vr_active) {
        for (const auto& it : scenedata->getSystemDatas())
          if (auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second))
            sgd->setUserSceneParam("preset", std::string("FWDPBRVRDM"));
        // perf HUD goes to its VR path: content -> offscreen RT -> head-locked panel in
        //  both eyes (DualMonoVr node). The pad L1 (left bumper) toggles it (see onUpdate).
        perfhud._vrmode = true;
        deco::printf(fvec3::Green(),
                     "ork.ecs.player: --vr ACTIVE — XR runtime up, render model FWDPBRVRDM\n");
      } else {
        // NO RUNTIME -> NoVR STEREO. Same render model, same preset param; SceneGraphSystem
        //  sees no device that ownsHmdPresentation and registers a NoVrDevice, whose
        //  __compositeStereo is a no-op — the output node's desktop mirror presents both eyes.
        //  Routing through the preset param (rather than an output node reached by hand) is
        //  what keeps the ORKID_SPVR autoselect in the loop: the resolver decides dual-mono
        //  vs single-pass from this string.
        for (const auto& it : scenedata->getSystemDatas())
          if (auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second))
            sgd->setUserSceneParam("preset", std::string("FWDPBRVRDM"));
        perfhud._vrmode = true;
        deco::printf(fvec3::Yellow(),
                     "ork.ecs.player: --vr: no XR runtime, using NoVR stereo (DMVR) — render model FWDPBRVRDM on a NoVr device\n");
      }
    }
    // ORKID_FORCE_DMVR (eye-pass verification): force the DualMonoVr preset on desktop so
    //  SceneGraphSystem spins up a NoVr device and runs the REAL DM composite (+ _drawHudPanel)
    //  headless — no HMD needed. Independent of --vr's openxr gate.
    if (force_dmvr) {
      for (const auto& it : scenedata->getSystemDatas())
        if (auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second))
          sgd->setUserSceneParam("preset", std::string("FWDPBRVRDM"));
      perfhud._vrmode = true;
      deco::printf(fvec3::Yellow(), "ork.ecs.player: ORKID_FORCE_DMVR — desktop NoVr DMVR preview (eye-pass verification)\n");
    }
    deco::printf(
        fvec3::Green(),
        "ork.ecs.player: materialized + wired (%zu artifacts)\n",
        artifacts->_themap.size());
    ////////////////////////////////////////////
    // 2b. --devkeys: splice the player-owned ACES+HSVG chain into the SG system data
    // BEFORE bind (the reflected _postfx_nodes/_postfx_order are consumed at _onLink).
    // additive with any scene-declared chain (e.g. "ssss"). NodeCompositor gpuInits
    // registered nodes for us — we never gpuInit these ourselves.
    ////////////////////////////////////////////
    if (devkeys) {
      for (const auto& it : scenedata->getSystemDatas())
        if (auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second)) {
          sgd->addPostFxNode("aces", aces_node);
          sgd->addPostFxNode("hsvg", hsvg_node);
          sgd->appendPostFxOrder("aces");
          sgd->appendPostFxOrder("hsvg");
        }
      deco::printf(fvec3::Yellow(), "ork.ecs.player: --devkeys injected ACES+HSVG postfx chain\n");
    }
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
      if (std::dynamic_pointer_cast<BulletSystemData>(it.second))
        bullet_mode = true;
    }
    if (walk_mode)
      charsystem = controller->findSystem<CharacterControllerSystem>();
    if (pysys_mode)
      pysystem = controller->findSystem<PythonSystem>();
    if (bullet_mode)
      bulletsystem = controller->findSystemWithClassName("BulletSystem");
    if (devkeys)
      update_keys_hud(); // re-seed: [B] phys n/a -> OFF now that bullet_mode is known
    if (physics_debug) {
      if (bullet_mode) {
        // flag-driven startup enable: same TOGGLE_DEBUG_DRAW path as the [B] devkey, fired
        // once before the first tick drains — wireframe is up by first-lit (snapshot-gateable).
        controller->systemNotify(bulletsystem, "TOGGLE_DEBUG_DRAW"_tok, std::make_shared<DataTable>());
        phys_dbg_on = true;
        if (devkeys)
          update_keys_hud(); // legend shows [B] phys: ON from frame 0
        deco::printf(fvec3::Green(), "ork.ecs.player: --physics-debug ON (Bullet debug wireframe)\n");
      } else {
        deco::printf(fvec3::Yellow(), "ork.ecs.player: --physics-debug requested but the scene declares no BulletSystem\n");
      }
    }
    deco::printf(fvec3::Green(), "ork.ecs.player: simulation STARTED%s\n",
                 walk_mode ? " [WALK MODE: W/S move, A/D strafe, arrows turn/pitch, SPACE jump, P pause]" : "");
  });

  //////////////////////////////////////////////////////////
  // update (update thread) — slow orbit camera + simulation tick
  //////////////////////////////////////////////////////////

  bool auto_rt_fired = false;
  bool aw_down = false, aw_up = false; // --autowalk state
  bool ay_down = false, ay_up = false; // --autoyaw state
  // S1 gamepad state-forwarding (update thread). Lazily created only when the scene has a
  // PythonSystem to consume it; the PYTHON input script owns the pad->locomotion mapping.
  gamepaddevice_ptr_t gamepad;
  // HUD toggle (VR): host-side rising edge on L1 (left bumper), plain button. Independent
  //  of the pad->python forwarding below (the L1 bit still forwards as a GamepadButton).
  bool gp_l1_hud_prev = false;
  bool gp_connected_latch = false; // forward only after the pad reports connected once
  bool gp_prev_connected  = false; // send one final frame on the connected->disconnected edge
  uint32_t gp_prev_buttons = 0;    // for button edge-diff (release-all on disconnect)
  // GamepadAxes rate limit. The update thread ticks at ~480 UPS; forwarding a snapshot per
  // tick floods the sim's notify queue through a PYTHON handler (+4 charctl notifies each)
  // faster than it drains — latency compounds until input appears dead. Buttons stay
  // edge-forwarded (sparse). Axes forward on CHANGE (>eps, min ~16ms apart) or a 100ms
  // heartbeat, plus always the final disconnect frame.
  float gp_sent_axes[6]  = {0, 0, 0, 0, 0, 0};
  double gp_last_axes_t  = -1.0; // abstime of last GamepadAxes send
  // STAGE-2 liveness (player forward): counts messages actually forwarded per ~5s window.
  // Prints only after the pad has connected (silent with no pad), only when nonzero OR just
  // transitioned to zero. btn dies -> button dispatch stopped; axes dies -> gating/rate-limit
  // or the update thread wedged.
  int gp_fwd_btn = 0, gp_fwd_axes = 0;
  double gp_fwd_live_t0 = -1.0;
  bool gp_fwd_btn_wasnz = false, gp_fwd_axes_wasnz = false;
  ezapp->onUpdate([&](ui::updatedata_ptr_t updata) {
    abstime = updata->_abstime;
    if (auto_roundtrip > 0.0f and not auto_rt_fired and abstime >= auto_roundtrip) {
      auto_rt_fired = true;
      deco::printf(fvec3::Yellow(), "ork.ecs.player: LIVE ROUND-TRIP (scripted, t=%g)\n", abstime);
      roundtrip_requested = true;
    }
    // E.2-walk scripted input (the gate's lever): W down at t=1, up at t=1+N —
    // through the SAME controller-message channel real keys use.
    // --autoyaw is the ROTATION-ONLY counterpart (cursor-RIGHT = KEY_RIGHT 262, which
    // walk_input_system.py holds into a continuous TurnInput rate): same fixed position,
    // camera sweeping in yaw — the discriminator for motion-class-dependent defects.
    if (walk_mode and (auto_walk > 0.0f or auto_yaw > 0.0f)) {
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
      if (auto_walk > 0.0f) {
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
      if (auto_yaw > 0.0f) {
        constexpr int KEY_RIGHT = 262; // GLFW code the walk input script maps to TurnInput
        if (not ay_down and abstime >= 1.0) {
          ay_down = true;
          send_key(KEY_RIGHT, 1);
          deco::printf(fvec3::Yellow(), "ork.ecs.player: AUTOYAW begin\n");
        }
        if (ay_down and not ay_up and abstime >= 1.0 + auto_yaw) {
          ay_up = true;
          send_key(KEY_RIGHT, 0);
          deco::printf(fvec3::Yellow(), "ork.ecs.player: AUTOYAW end\n");
        }
      }
    }
    // S1 gamepad -> the scene's PythonSystem (the host does NOT map the pad; the python input
    // script owns pad->locomotion, exactly as it owns the keymap for InputKey). Two channels:
    //   GamepadButton {button: <token>, down: int}  — edge transitions (mirrors InputKey; the
    //                                                  abstract button id rides as a crcstring
    //                                                  token so python reads tokens.CROSS, ...).
    //   GamepadAxes   {lx,ly,rx,ry,l2,r2: float, connected: int} — per-tick analog snapshot.
    // Gated on a PythonSystem consumer AND on the pad reporting connected at least once
    // (zero traffic when no pad); on disconnect every held button is released and one final
    // connected=0 frame is sent so the script can zero its inputs.
    // Sample the pad when the scene forwards it to python OR when the VR perf HUD needs
    //  the L2 toggle. On a keyboardless VR rig this is the only way to raise the HUD.
    bool hud_pad = perfhud._vrmode.load();
    if (pysys_mode or hud_pad) {
      if (not gamepad)
        gamepad = GamepadDevice::instance(); // linux: spins up the joydev reader thread
      GamepadState gp = gamepad->sample();
      // HUD toggle (VR): host-side L1 (left bumper) rising edge. Consumed HERE; the L1 bit
      //  ALSO forwards to python as a GamepadButton (walk sprint moved to R1-only so this
      //  toggle doesn't blip sprint) — no input conflict.
      if (hud_pad) {
        bool l1 = gp.connected and gp.buttonDown(GamepadButtonId::L1);
        if (l1 and not gp_l1_hud_prev) {
          perfhud.toggleShown();
          deco::printf(fvec3::Cyan(), "ork.ecs.player: [pad L1] perf HUD %s\n",
                       perfhud._mode.load() ? "ON" : "OFF");
        }
        gp_l1_hud_prev = l1;
      }
      if (pysys_mode) {
      if (gp.connected)
        gp_connected_latch = true;
      if (gp_connected_latch and (gp.connected or gp_prev_connected)) {
        controller_ptr_t c;
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          c = controller;
        }
        if (c) {
          // button edge transitions (on disconnect gp.buttons==0 releases everything held)
          uint32_t changed = gp.buttons ^ gp_prev_buttons;
          for (size_t i = 0; i < kNumGamepadButtons; i++) {
            uint32_t bit = (1u << i);
            if (not(changed & bit))
              continue;
            auto btntab             = std::make_shared<DataTable>();
            (*btntab)["button"_tok] = std::make_shared<CrcString>(uint64_t(kGamepadButtonOrder[i]));
            (*btntab)["down"_tok]   = int((gp.buttons & bit) ? 1 : 0);
            c->systemNotify(pysystem, "GamepadButton"_tok, btntab);
            gp_fwd_btn++;
          }
          gp_prev_buttons = gp.buttons;
          // analog snapshot — rate-limited (see gp_sent_axes above): change-driven at
          // <=60Hz + 100ms heartbeat + always the disconnect edge. NOT per-tick.
          const float ax_now[6] = {gp.lx, gp.ly, gp.rx, gp.ry, gp.l2, gp.r2};
          bool ax_changed       = false;
          for (int a = 0; a < 6; a++)
            if (std::fabs(ax_now[a] - gp_sent_axes[a]) > 1e-3f)
              ax_changed = true;
          double since       = (gp_last_axes_t < 0.0) ? 1e9 : (abstime - gp_last_axes_t);
          bool disconnect_edge = (not gp.connected) and gp_prev_connected;
          bool send_axes     = disconnect_edge                     //
                           or (ax_changed and since >= 0.016)      //
                           or (since >= 0.100);
          if (send_axes) {
            auto axtab                = std::make_shared<DataTable>();
            (*axtab)["lx"_tok]        = float(gp.lx);
            (*axtab)["ly"_tok]        = float(gp.ly);
            (*axtab)["rx"_tok]        = float(gp.rx);
            (*axtab)["ry"_tok]        = float(gp.ry);
            (*axtab)["l2"_tok]        = float(gp.l2);
            (*axtab)["r2"_tok]        = float(gp.r2);
            (*axtab)["connected"_tok] = int(gp.connected ? 1 : 0);
            c->systemNotify(pysystem, "GamepadAxes"_tok, axtab);
            gp_fwd_axes++;
            for (int a = 0; a < 6; a++)
              gp_sent_axes[a] = ax_now[a];
            gp_last_axes_t = abstime;
          }
        }
      }
      gp_prev_connected = gp.connected;
      // STAGE-2 liveness heartbeat (throttled ~5s; silent until a pad has connected)
      if (gp_connected_latch) {
        if (gp_fwd_live_t0 < 0.0)
          gp_fwd_live_t0 = abstime;
        if (abstime - gp_fwd_live_t0 >= 5.0) {
          gp_fwd_live_t0 = abstime;
          if (gp_fwd_btn > 0 or gp_fwd_axes > 0 or gp_fwd_btn_wasnz or gp_fwd_axes_wasnz)
            deco::printf(fvec3::Cyan(), "[GAMEPAD] fwd btn=%d axes=%d/5s\n", gp_fwd_btn, gp_fwd_axes);
          gp_fwd_btn_wasnz = (gp_fwd_btn > 0);
          gp_fwd_axes_wasnz = (gp_fwd_axes > 0);
          gp_fwd_btn = 0;
          gp_fwd_axes = 0;
        }
      }
      } // if (pysys_mode) — pad->python forwarding
    }
    // --pysysnotify (test hook): scripted messages to the scene's PythonSystem once
    // abstime reaches each entry's time. SECONDS, not ticks: a gate and a movie script
    // are written in the time a human would describe (hold ']' for three seconds), and
    // the update rate is not a constant across machines or offscreen modes.
    if (not pysys_notify_events.empty()) {
      controller_ptr_t c;
      sys_ref_t pys;
      {
        std::lock_guard<std::mutex> lock(ctl_mutex);
        c   = controller;
        pys = pysystem;
      }
      for (auto& ev : pysys_notify_events) {
        if (ev.fired or abstime < ev.time)
          continue;
        ev.fired = true;
        if (not pysys_mode or not c) {
          deco::printf(fvec3::Yellow(), "ork.ecs.player: [pysysnotify] %s dropped — scene declares no PythonSystem\n",
                       ev.event.c_str());
          continue;
        }
        auto tab = std::make_shared<DataTable>();
        for (const auto& f : ev.fields) {
          // ints where the value is whole: the key/down fields the input path reads are
          // EXACT-typed svars, and a float there would miss the script's int expectation.
          if (f.second == float(int(f.second)))
            (*tab)[CrcString(f.first.c_str())] = int(f.second);
          else
            (*tab)[CrcString(f.first.c_str())] = f.second;
        }
        c->systemNotify(pys, CrcString(ev.event.c_str()), tab);
        deco::printf(fvec3::Yellow(), "ork.ecs.player: [pysysnotify] t=%.3f %s (%zu field(s))\n",
                     abstime, ev.event.c_str(), ev.fields.size());
      }
    }
    // --devkeys-script (test hook): fire scripted dev keys through the SAME handler once
    // the update tick reaches each event's frame. CMDR sets roundtrip_requested — the very
    // atomic the real Cmd+R handler sets — proving post-round-trip keys still act.
    if (devkeys and not devkey_events.empty()) {
      devkey_tick++;
      for (auto& dke : devkey_events) {
        if (dke.fired or devkey_tick < dke.frame)
          continue;
        dke.fired = true;
        if (dke.cmdr) {
          deco::printf(fvec3::Yellow(), "ork.ecs.player: [devkeys-script] CMDR round-trip @ tick %d\n", devkey_tick);
          roundtrip_requested = true;
        } else {
          deco::printf(fvec3::Yellow(), "ork.ecs.player: [devkeys-script] key<%c> @ tick %d\n", char(dke.keycode), devkey_tick);
          fire_devkey(dke.keycode);
        }
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
        sys_ref_t fresh_bullet;
        if (bullet_mode)
          fresh_bullet = fresh->findSystemWithClassName("BulletSystem"); // [B]/--physics-debug: re-resolve on restart
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          dead_controllers.push_back(controller);
          controller = fresh;
          sgsystem   = fresh_sgsys;
          bulletsystem = fresh_bullet;
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
      // --devkeys: bare E/G/T/H/M/B/R drive the viewer-look controls. Consumed here (before
      // the walk/PythonSystem forward below) ONLY when --devkeys AND only for keys we OWN;
      // every other key (incl. the EzUiCam X/C/V pan/dolly/zoom chords) falls through
      // untouched to the uicam handler below. Flagless these all fall through, so behavior
      // is byte-identical to today. Cmd+R stays the round-trip (SUPER block below); none of
      // E/G/T/H/M/R collide with a walk movement key or a camera modifier.
      if (devkeys and not ev->mbSUPER) {
        int kc = ev->miKeyCode;
        if (kc == 'E' or kc == 'G' or kc == 'T' or kc == 'H' or kc == 'M' or kc == 'B' or kc == 'R') {
          fire_devkey(kc);
          return ui::HandlerResult();
        }
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
          // --devkeys STALENESS FIX: the clone deserialized FRESH aces/hsvg node
          // instances (the reflected postfx chain round-trips through JSON), so the
          // player's held pointers would go stale after the restart. Re-point the
          // clone's entries back to the player-owned originals BEFORE wiring, so the
          // key handlers keep driving the live chain. (Byte-compare above already ran
          // on the pristine clone, so this doesn't perturb the serdes audit.)
          if (devkeys) {
            for (const auto& it : fresh->getSystemDatas())
              if (auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second)) {
                if (sgd->_postfx_nodes.count("aces"))
                  sgd->_postfx_nodes["aces"] = aces_node;
                if (sgd->_postfx_nodes.count("hsvg"))
                  sgd->_postfx_nodes["hsvg"] = hsvg_node;
              }
          }
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
    // ALPHA ORACLE — two stages, both mac-runnable (no VR). Poll pattern mirrors --snapshot
    //  so the device-local readbacks don't stall.
    //   S1 (SUSPECT A): read the panel RT -> bg alpha ~0.5, text alpha ~1.
    //   S2 (SUSPECT B, the eye-pass blend): composite the panel RT over a KNOWN red buffer
    //       with the EXACT eye-pass technique (orkshader://ui uitextured_alpha) and read a
    //       bg-covered corner. Blended => R~128; opaque(bug) => R~0; no-draw => R~255.
    if (alpha_oracle) {
      static int                 ora_frame = 0;
      static int                 ora_phase = 0; // 0=issue S1,1=wait S1,2=render+issue S2,3=wait S2,4=done
      static captureasync_ptr_t  ora_fut;
      static capturebuffer_ptr_t ora_buf;
      static std::shared_ptr<FreestyleMaterial>                  ora_mtl;
      static rtgroup_ptr_t                                       ora_rtg;
      static std::shared_ptr<DynamicVertexBuffer<SVtxV16T16C16>> ora_vb;
      ora_frame++;
      auto octx = drwev->GetTarget();
      auto ofbi = octx->FBI();
      // ---- S1: issue panel-RT capture ----
      if (ora_phase == 0 and ora_frame >= 8 and perfhud._hudRTG) {
        ora_fut   = ofbi->captureAsFormat(perfhud._hudRTG->buffer(0).get(),
                                        (ora_buf = std::make_shared<CaptureBuffer>()), EBufferFormat::RGBA8);
        ora_phase = 1;
      }
      if (ora_phase == 1 and ora_fut and ora_fut->isReady()) {
        auto img = ora_buf ? ora_buf->_image : nullptr;
        if (img and img->_data) {
          const uint8_t* p = img->_data->data();
          int            W = int(img->_width), H = int(img->_height);
          // The RT is now PREMULTIPLIED FOREGROUND (text over transparent, NO slate).
          //  (c) panel bg (2,2, above text) => transparent (a~0). (a) glyph-box background =>
          //  ALSO transparent (a~0), same as bg (no slate to punch -> no boxes). (b) glyph
          //  core => premultiplied green (rgb=color*coverage, a=coverage ~high). A
          //  "dark-but-opaque" texel (rgb~0 AND a>60) would be a leftover slate/box.
          auto AT = [&](int x, int y, int c) { return int(p[(size_t(y) * W + x) * 4 + c]); };
          int  bg_r = AT(2,2,0), bg_g = AT(2,2,1), bg_b = AT(2,2,2), bg_a = AT(2,2,3);
          int  core_a = 0, core_r = 0, core_g = 0, core_b = 0; // brightest green stroke
          long dark_opaque = 0;                         // dark AND non-transparent = box/slate defect
          long ah[4] = {0,0,0,0};                       // alpha histogram: 0-10,11-140,141-230,231-255
          for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
              int r = AT(x,y,0), g = AT(x,y,1), b = AT(x,y,2), a = AT(x,y,3);
              ah[a<=10?0 : a<=140?1 : a<=230?2 : 3]++;
              if (g > 128) { if (a > core_a) { core_a = a; core_r = r; core_g = g; core_b = b; } }
              else if (r < 40 and g < 40 and b < 40 and a > 60) dark_opaque++;
            }
          bool pass = (bg_a <= 12) and (core_a >= 205) and (dark_opaque == 0);
          printf("[perfhud-oracle-S1] premult foreground RT %dx%d\n"
                 "   (c) panel_bg      rgba<%d,%d,%d,%d> (want a~0, transparent)\n"
                 "   (a) dark-opaque (leftover slate/box) texels<%ld> (want 0)\n"
                 "   (b) glyph core    rgba<%d,%d,%d,%d> (want a>205, premult green)\n"
                 "   alpha histogram [0-10]=%ld [11-140]=%ld [141-230]=%ld [231-255]=%ld\n"
                 "   => %s\n",
                 W, H, bg_r, bg_g, bg_b, bg_a, dark_opaque,
                 core_r, core_g, core_b, core_a, ah[0], ah[1], ah[2], ah[3], pass ? "PASS" : "FAIL");
          fflush(stdout);
        }
        ora_fut = nullptr; ora_buf = nullptr; ora_phase = 2;
      }
      // ---- S2: composite panel over RED with the eye-pass technique, then capture ----
      if (ora_phase == 2 and perfhud._hudRTG) {
        int TW = perfhud._hudRTG->width(), TH = perfhud._hudRTG->height();
        if (not ora_mtl) { ora_mtl = std::make_shared<FreestyleMaterial>(); ora_mtl->gpuInit(octx, "orkshader://ui"); }
        if (not ora_rtg) {
          ora_rtg  = std::make_shared<RtGroup>(octx, TW, TH, MsaaSamples::MSAA_1X);
          auto b   = ora_rtg->createRenderTarget(EBufferFormat::RGBA8);
          b->_clearColor = fvec4(1, 0, 0, 1); // known "scene" color = RED
        } else if (ora_rtg->width() != TW or ora_rtg->height() != TH) ora_rtg->Resize(TW, TH);
        if (not ora_vb) { ora_vb = std::make_shared<DynamicVertexBuffer<SVtxV16T16C16>>(64, 0); ora_vb->SetRingLock(true); }
        auto RCFD  = std::make_shared<RenderContextFrameData>(octx);
        ofbi->PushRtGroup(ora_rtg.get()); // autoclear -> RED (the "scene")
        ViewportRect vp(0, 0, TW, TH); ofbi->pushViewport(vp); ofbi->pushScissor(vp);
        auto quad = [&]() {
          VtxWriter<SVtxV16T16C16> vw; vw.Lock(octx, ora_vb.get(), 6);
          fvec4 c(1, 1, 1, 1);
          auto AV = [&](float x, float y, float u, float v) { vw.AddVertex(SVtxV16T16C16(fvec3(x, y, 0), fvec4(u, v, 0, 0), c)); };
          AV(-1,-1,0,0); AV(1,-1,1,0); AV(1,1,1,1); AV(-1,-1,0,0); AV(1,1,1,1); AV(-1,1,0,1);
          vw.UnLock(octx);
          octx->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
        };
        // EXACT two-pass eye-pass: LAYER 1 slate (uidev_modcolor_alpha, ModColor (0,0,0,0.5))
        //  over RED => (127,0,0); LAYER 2 premult text (uitextured_prema) PREMA over the slate.
        ora_mtl->begin(ora_mtl->technique("uidev_modcolor_alpha"), RCFD);
        ora_mtl->bindParamMatrix(ora_mtl->param("mvp"), fmtx4::Identity());
        ora_mtl->bindParamVec4(ora_mtl->param("ModColor"), fvec4(0, 0, 0, 0.5f));
        quad();
        ora_mtl->end(RCFD);
        ora_mtl->begin(ora_mtl->technique("uitextured_prema"), RCFD);
        ora_mtl->bindParamMatrix(ora_mtl->param("mvp"), fmtx4::Identity());
        ora_mtl->bindParamTexture(ora_mtl->param("ColorMap"), perfhud._hudRTG->texture(0).get());
        ora_mtl->bindParamVec4(ora_mtl->param("ModColor"), fvec4(1, 1, 1, 1));
        quad();
        ora_mtl->end(RCFD);
        ofbi->popScissor(); ofbi->popViewport(); ofbi->PopRtGroup();
        ora_fut   = ofbi->captureAsFormat(ora_rtg->buffer(0).get(),
                                        (ora_buf = std::make_shared<CaptureBuffer>()), EBufferFormat::RGBA8);
        ora_phase = 3;
      }
      if (ora_phase == 3 and ora_fut and ora_fut->isReady()) {
        auto img = ora_buf ? ora_buf->_image : nullptr;
        if (img and img->_data) {
          if (const char* pp = getenv("ORKID_PERFHUD_ORACLE_PNG")) img->writeToFile(file::Path(pp));
          const uint8_t* p = img->_data->data();
          int            W = int(img->_width), H = int(img->_height);
          auto AT = [&](int x, int y, int c) { return int(p[(size_t(y) * W + x) * 4 + c]); };
          int  cR = AT(2,2,0), cG = AT(2,2,1), cB = AT(2,2,2);         // corner = slate over red
          // non-green (bg/slate/box/hole) texels: their RED must stay ~127 (slate). A box =>
          //  R<<127 (dark), a hole => R>>127 (full red bleeds through). Count anomalies.
          int  nonGreen_Rmin = 255, nonGreen_Rmax = 0, axx = -1, axy = -1;
          long anomalies = 0;
          int  gcR = 0, gcG = 0, gcB = 0, gcGmax = 0;                  // brightest-green glyph core
          for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
              int r = AT(x,y,0), g = AT(x,y,1), b = AT(x,y,2);
              if (g > 128 and g > r) { if (g > gcGmax) { gcGmax = g; gcR = r; gcG = g; gcB = b; } }
              else { // bg/slate/box/hole
                nonGreen_Rmin = std::min(nonGreen_Rmin, r);
                if (r > nonGreen_Rmax) { nonGreen_Rmax = r; axx = x; axy = y; }
                if (r < 60 or r > 210) anomalies++;                    // real box (dark) / hole (full red)
              }
            }
          bool pass = (cR >= 110 and cR <= 145 and cG <= 20) and (anomalies == 0) and (gcGmax >= 150);
          printf("[perfhud-oracle-S2-eyepass] two-pass (slate+premult text) over RED(255,0,0):\n"
                 "   corner(bg) rgb<%d,%d,%d> (want ~127,0,0 = slate)\n"
                 "   non-green RED range [%d..%d] (maxR @(%d,%d), W-1=%d) box/hole_anomalies<%ld> (want 0)\n"
                 "   glyph core rgb<%d,%d,%d> (want green over slate)\n"
                 "   => %s\n",
                 cR, cG, cB, nonGreen_Rmin, nonGreen_Rmax, axx, axy, W-1, anomalies, gcR, gcG, gcB, pass ? "PASS" : "FAIL");
          fflush(stdout);
        }
        ora_fut = nullptr; ora_buf = nullptr; ora_phase = 4;
      }
    }
    // --devkeys key legend — same post-movie-pump placement; upper-left (no perfhud overlap).
    // Drawn BEFORE the offscreen snapshot capture below so the HUD lands in --snapshot PNGs.
    keyshud.draw(drwev->GetTarget());
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
      if (not os_walltimer_started) { os_walltimer.Start(); os_walltimer_started = true; }
      auto cq           = opq::concurrentQueue();
      bool loader_idle  = cq and (cq->_numPendingOperations.load() == 0)
                             and (cq->_numInFlight.load() == 0);
      // ONE-SHOT census: a recurring producer (the procedural sky's IBL refilter,
      // which re-arms for as long as the sun moves) declares itself steady and is
      // not work that can "drain" — waiting on it would hold every celestial
      // scene here until the frame cap. Everything that genuinely finishes still
      // gates, and asyncWorkSummary() below marks the steady tags STEADY.
      int  async_pend   = ork::asyncWorkPendingOneShot("");
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
              // MOVIE now mirrors the snapshot contract: do NOT start recording at this
              // settle boundary. Enter the pre-roll appearance drain (phase 5); recording
              // begins only once the appearance async has drained AND a captured frame is
              // settled-lit — else FAIL loud (MOVIE_RESULT=FAIL, rc=42), never record an
              // unsettled scene (the old cap-force-advance bug).
              deco::printf(fvec3::Yellow(),
                           "ork.ecs.player: OFFSCREEN MOVIE -> %s (pre-roll appearance drain, then %d frames @ %d fps)\n",
                           movie_path.c_str(), movie_frames, int(movie_fps));
              os_phase     = 5;
              os_snapdrain = 0;
            } else if (not snapshot_path.empty()) {
              // SNAPSHOT: enter the capture/verify drain (phase 4). The actual
              // capture is issued THERE, per-frame, so it lands on a warm
              // (non-black) composite frame rather than this settle boundary.
              // --snapshot-frame N overrides the first-lit heuristic: hold in
              // the drain and only capture once os_frame >= N (deterministic).
              deco::printf(fvec3::Yellow(), "ork.ecs.player: OFFSCREEN SNAPSHOT -> %s (%s)\n",
                           snapshot_path.c_str(),
                           snapshot_frame > 0 ? "capture+verify drain, +N after first-lit" : "capture+verify drain");
              os_phase     = 4;
              os_snapdrain = 0;
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
              if (os_movie == 0) {
                // 0 frames recorded is a FALSE positive, not a success — the frame-cap
                // consumed the whole budget before any frame was captured (e.g. the WAIT
                // phase burned it on a scene whose bake never completes). Fail loud rc=42.
                deco::printf(fvec3::Red(),
                             "ork.ecs.player: MOVIE_RESULT=FAIL reason=zero_frames pending<%s> — recording captured 0 frames\n",
                             ork::asyncWorkSummary().c_str());
                os_movie_fail->store(true);
              } else {
                printf("ork.ecs.player: MOVIE_RESULT=OK frames<%d>\n", os_movie);
              }
              fflush(stdout);
              ezapp->signalExit();
              os_phase = 3;
            }
          }
          break;
        case 4: // SNAPSHOT — wait for the APPEARANCE async to drain, then capture the first LIT frame.
          os_snapdrain++;
          {
            auto ctx = drwev->GetTarget();
            auto fbi = ctx->FBI();
            auto rtb = fbi->_main_rtg->buffer(0);
            // APPEARANCE GATE. The composite's SKY / IBL reflections are painted from the
            // radiancemap; while that (or any other appearance async — asset streaming) is
            // still in flight the frame renders with a BLACK sky over lit terrain. That is
            // exactly the trap the old drain fell into: the lit-probe counts any >=0.1%
            // colored pixels, so the lit TERRAIN alone satisfied it and the snapshot was
            // captured with a dead-black sky, then the process exited rc=0. So we do NOT
            // accept a capture until the appearance async has drained. terrain_texbake is
            // EXCLUDED — it is a background disk-cache bake that stalls offscreen forever
            // (never completes headless) and does not gate the visible frame; waiting on it
            // would hang a perfectly healthy scene (scn_forest). The wall-clock ceiling is
            // the true hang backstop if the appearance async never finishes.
            // STEADY-declared producers are likewise not waited on: the sky-IBL refilter
            // is a recurring feed, not a job with an end, so a chaining celestial scene
            // would otherwise pin this census above zero for the whole run.
            int  appearance_pending = ork::asyncWorkPendingOneShot("terrain_texbake");
            bool sky_ready          = (appearance_pending == 0);
            // (1) Harvest an in-flight capture once its readback lands (captures are only
            //     issued past sky-ready, so this runs only then).
            if (snap_future) {
              if (snap_future->isReady()) {
                auto img = snap_capbuf ? snap_capbuf->_image : nullptr;
                // "Lit" = enough pixels with a non-zero COLOR channel. Scan RGB
                // only (skip alpha: main_rtg clears to opaque black (0,0,0,255),
                // so counting alpha bytes would read a black frame as lit) and
                // require a real pixel population (a stray pixel isn't a render).
                size_t lit_px = 0, tot_px = 0;
                uint8_t rgbmax = 0;
                bool has_color = probeCaptureColor(img, tot_px, lit_px, rgbmax);
                snap_probe_color = has_color;                               // feeds the quiescent backstop
                // "Settled-lit" requires BOTH the appearance async drained (sky present)
                // AND real color. Record when the composite FIRST goes settled-lit; then
                // --snapshot-frame N accepts N drain-frames LATER (relative to that, so the
                // caller need not know the variable settle/warmup). N=0 => at first-lit.
                bool settled_lit = has_color and sky_ready;
                if (settled_lit and snap_first_lit < 0)
                  snap_first_lit = os_snapdrain;
                bool lit = settled_lit and (snap_first_lit >= 0)
                           and (os_snapdrain >= snap_first_lit + snapshot_frame);
                const char* probe_tag = (not has_color) ? "black" : (not sky_ready ? "sky-wait" : (lit ? "LIT" : "wait"));
                printf("ork.ecs.player: SNAPSHOT probe drain<%d> px<%zu> lit_px<%zu> rgbmax<%d> sky<%s> => %s\n",
                       os_snapdrain, tot_px, lit_px, int(rgbmax), sky_ready ? "ready" : "pending", probe_tag);
                snap_last_capbuf = snap_capbuf; // remember the newest landed frame (fallback)
                if (lit) {
                  bool wrote = img->writeToFile(file::Path(snapshot_path.c_str()));
                  if (wrote) {
                    printf("ork.ecs.player: SNAPSHOT wrote %s (lit, drain %d)\n", snapshot_path.c_str(), os_snapdrain);
                    os_snap_done->store(true);
                  } else {
                    // writeToFile fails loud on its own; escalate to a FAIL exit so a bad
                    // --snapshot path can never look like a healthy rc=0 capture.
                    deco::printf(fvec3::Red(),
                                 "ork.ecs.player: SNAPSHOT_RESULT=FAIL reason=write_failed path<%s>\n",
                                 snapshot_path.c_str());
                    os_snap_fail->store(true);
                    ezapp->signalExit();
                    os_phase = 3;
                  }
                }
                snap_future = nullptr;
                snap_capbuf = nullptr;
              } else if (os_snapdrain - snap_issue_drain > kFutureStale) {
                // Readback never completed (idle concurrentQueue) — drop it and
                // re-issue; the fresh per-frame captures keep the pool awake.
                snap_future = nullptr;
                snap_capbuf = nullptr;
              }
            }
            // (2) PAST sky-ready only: no capture in flight and not done yet -> issue one.
            //     Before sky-ready we just PUMP: the loader keeps the prefilter progressing
            //     on its own (a plain --frames run drains it with zero captures), so issuing
            //     captures here would only contend for the GPU and slow the bake. A periodic
            //     line keeps the wait visible (grep-stable, shows the pending async names).
            if (os_phase == 4 and not os_snap_done->load() and not snap_future
                and sky_ready and os_snapdrain >= kSnapWarmup) {
              snap_capbuf      = std::make_shared<CaptureBuffer>();
              snap_future      = fbi->captureAsFormat(rtb.get(), snap_capbuf, EBufferFormat::RGBA8);
              snap_issue_drain = os_snapdrain;
            }
            if (os_phase == 4 and not sky_ready and (os_snapdrain % 120 == 1))
              deco::printf(fvec3::Yellow(),
                           "ork.ecs.player: SNAPSHOT draining appearance async pending<%s> drain<%d> wall<%.1fs/%gs>\n",
                           ork::asyncWorkSummary().c_str(), os_snapdrain, os_walltimer.SecsSinceStart(), settle_timeout);
            // (3) DECIDE. SUCCESS is a settled-LIT frame (written above). Otherwise KEEP
            //     PUMPING while the appearance async is still in flight (real per-frame
            //     progress) — a bake that lights only after a few THOUSAND frames must NOT
            //     be force-captured black (the exact old bug: rc=0 dead-black snapshot).
            //     Two independent HANG backstops turn a genuinely stuck drain into a LOUD,
            //     NONZERO FAIL — a dead-black rc=0 snapshot is impossible:
            //       - wall-clock ceiling (settle_timeout): the absolute hang guard; the
            //         ONLY thing that ends a drain while appearance async is STILL pending
            //         (a real hang — the bake never finished within the budget);
            //       - post-sky-ready quiescence (kSnapQuiesce): once the sky IS ready and
            //         the composite STILL will not light for that many consecutive frames,
            //         the scene is settled-BLACK (broken, not slow) — fail bounded rather
            //         than wait out the whole wall ceiling.
            if (sky_ready and not snap_probe_color)
              os_snap_quiescent++;
            else
              os_snap_quiescent = 0;
            double os_wall       = os_walltimer.SecsSinceStart();
            bool   wall_expired  = os_wall >= settle_timeout;
            bool   settled_black = (os_snap_quiescent >= kSnapQuiesce);
            if (os_phase == 4 and os_snap_done->load()) {
              printf("ork.ecs.player: SNAPSHOT_RESULT=PASS drain<%d> wall<%.1fs>\n", os_snapdrain, os_wall);
              deco::printf(fvec3::Green(), "ork.ecs.player: OFFSCREEN snapshot done (frame %d) — exiting\n", os_frame);
              ezapp->signalExit();
              os_phase = 3;
            } else if (os_phase == 4 and (wall_expired or settled_black)) {
              // LOUD, grep-stable failure (reason + pending async names + drain + wall).
              // Still write the last captured frame so there is an artifact to eyeball, but
              // flag FAIL and exit NONZERO below (see mainThreadLoop return).
              const char* reason = wall_expired ? (sky_ready ? "settled_black_timeout" : "async_timeout")
                                                : "settled_black";
              deco::printf(fvec3::Red(),
                           "ork.ecs.player: SNAPSHOT_RESULT=FAIL reason=%s pending<%s> drain<%d> wall<%.1fs/%gs> — snapshot never went settled-lit\n",
                           reason, ork::asyncWorkSummary().c_str(), os_snapdrain, os_wall, settle_timeout);
              if (snap_last_capbuf and snap_last_capbuf->_image)
                snap_last_capbuf->_image->writeToFile(file::Path(snapshot_path.c_str()));
              os_snap_fail->store(true);
              ezapp->signalExit();
              os_phase = 3;
            }
          }
          break;
        case 5: { // MOVIE pre-roll — parity with the snapshot appearance drain (phase 4).
                  // Gate recording on the SAME contract: appearance async drained AND a
                  // captured frame settled-lit, else FAIL loud. Never records an unsettled
                  // scene; the frame-cap can no longer force-advance into recording garbage.
          os_snapdrain++;
          auto ctx = drwev->GetTarget();
          auto fbi = ctx->FBI();
          auto rtb = fbi->_main_rtg->buffer(0);
          int  appearance_pending = ork::asyncWorkPendingOneShot("terrain_texbake"); // steady feeds excluded (phase 4)
          bool sky_ready          = (appearance_pending == 0);
          // (1) harvest an in-flight capture, probe it (shared probe = identical criteria)
          if (snap_future) {
            if (snap_future->isReady()) {
              auto img = snap_capbuf ? snap_capbuf->_image : nullptr;
              size_t lit_px = 0, tot_px = 0;
              uint8_t rgbmax   = 0;
              bool has_color   = probeCaptureColor(img, tot_px, lit_px, rgbmax);
              snap_probe_color = has_color;
              bool settled_lit = has_color and sky_ready;
              if (settled_lit and snap_first_lit < 0)
                snap_first_lit = os_snapdrain;
              const char* probe_tag = (not has_color) ? "black" : (not sky_ready ? "sky-wait" : "LIT");
              printf("ork.ecs.player: MOVIE preroll probe drain<%d> px<%zu> lit_px<%zu> rgbmax<%d> sky<%s> => %s\n",
                     os_snapdrain, tot_px, lit_px, int(rgbmax), sky_ready ? "ready" : "pending", probe_tag);
              snap_future = nullptr;
              snap_capbuf = nullptr;
            } else if (os_snapdrain - snap_issue_drain > kFutureStale) {
              snap_future = nullptr;
              snap_capbuf = nullptr;
            }
          }
          // (2) PAST sky-ready only: issue a capture if none in flight (before sky-ready we
          //     just PUMP so the prefilter progresses — same as the snapshot drain).
          if (not snap_future and sky_ready and os_snapdrain >= kSnapWarmup) {
            snap_capbuf      = std::make_shared<CaptureBuffer>();
            snap_future      = fbi->captureAsFormat(rtb.get(), snap_capbuf, EBufferFormat::RGBA8);
            snap_issue_drain = os_snapdrain;
          }
          if (not sky_ready and (os_snapdrain % 120 == 1))
            deco::printf(fvec3::Yellow(),
                         "ork.ecs.player: MOVIE draining appearance async pending<%s> drain<%d> wall<%.1fs/%gs>\n",
                         ork::asyncWorkSummary().c_str(), os_snapdrain, os_walltimer.SecsSinceStart(), settle_timeout);
          // (3) HANG backstops — identical to the snapshot drain.
          if (sky_ready and not snap_probe_color)
            os_snap_quiescent++;
          else
            os_snap_quiescent = 0;
          double os_wall       = os_walltimer.SecsSinceStart();
          bool   wall_expired  = os_wall >= settle_timeout;
          bool   settled_black = (os_snap_quiescent >= kSnapQuiesce);
          bool   settled_lit   = (snap_first_lit >= 0);
          // (4) DECIDE. settled-lit => START recording (phase 2). Else FAIL on a hang
          //     backstop OR a frame-cap that expired with appearance STILL pending (the
          //     cap-force-advance is now a loud FAIL, not silent garbage recording).
          if (settled_lit) {
            // Re-anchor the frame cap so recording gets its FULL budget FROM the settled
            // point. The pre-roll may legitimately run well past offscreen_frames (a
            // radiancemap prefilter takes >1000 frames headless on a healthy scene); the
            // cap is a hang-guard, not an appearance gate. Without this re-anchor, recording
            // would start with cap already tripped and capture 0 frames (zero_frames FAIL).
            offscreen_frames = os_frame + movie_frames + 90;
            auto settings             = std::make_shared<MovieCaptureSettings>();
            settings->_filename       = movie_path;
            settings->_fps            = int(movie_fps);
            settings->_preset_name    = "high";
            settings->_max_queue_size = 300;
            ezapp->enableMovieRecording(settings);
            deco::printf(fvec3::Green(),
                         "ork.ecs.player: MOVIE settled-lit (drain %d) — recording %d frames @ %d fps\n",
                         os_snapdrain, movie_frames, int(movie_fps));
            os_phase = 2;
            os_movie = 0;
          } else if (wall_expired or settled_black) {
            // Gate on the WALL CLOCK only (identical to the snapshot drain) — the frame cap
            // fires during NORMAL appearance loading (radiancemap prefilter >> offscreen_frames
            // on a healthy scene), so it must NOT abort the pre-roll or it FAILs healthy
            // scenes. settle_timeout is the true hang backstop; settled_black catches a
            // sky-ready-but-black scene bounded. Reasons kept distinct + documented.
            const char* reason = wall_expired ? (sky_ready ? "settled_black_timeout" : "async_timeout")
                                              : "settled_black";
            deco::printf(fvec3::Red(),
                         "ork.ecs.player: MOVIE_RESULT=FAIL reason=%s pending<%s> drain<%d> wall<%.1fs/%gs> — never went settled-lit; not recording\n",
                         reason, ork::asyncWorkSummary().c_str(), os_snapdrain, os_wall, settle_timeout);
            fflush(stdout);
            os_movie_fail->store(true);
            ezapp->signalExit();
            os_phase = 3;
          }
          break;
        }
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
  // A failed --snapshot or --movie (hung on pending appearance async past the ceiling, or
  // settled-black, or cap expired with appearance still pending) exits NONZERO so a gate /
  // caller can never mistake an unsettled/dead-black capture for a healthy one. Shared rc=42.
  if (os_movie_fail and os_movie_fail->load()) {
    deco::printf(fvec3::Red(), "ork.ecs.player: exiting NONZERO — MOVIE_RESULT=FAIL\n");
    return 42;
  }
  if (os_snap_fail and os_snap_fail->load()) {
    deco::printf(fvec3::Red(), "ork.ecs.player: exiting NONZERO — SNAPSHOT_RESULT=FAIL\n");
    return 42;
  }
  return rval;
}
