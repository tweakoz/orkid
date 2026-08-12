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
//   host        ` ~ (perf HUD page ring: off/1 FRAME/2 GPU/3 PASSES/4 CULL/5 SYSTEMS,
//                   then the registered EDITOR pages: 6 SKY / 7 POST)
//               [ ] - = and \ — ONLY while an editor page is up ([ ] select the item,
//                   - = decrement / increment it, \ opens / closes a COLOR row's H/S/V
//                   sub-editor). MODAL: off an editor page they fall through to the scene
//                   exactly as before (all five are sky-clock keys), and their KEY-UP is
//                   never swallowed, so a scene script can't be left holding one. \ is
//                   narrower still — it is consumed only ON a color row. THE CURSOR KEYS
//                   ARE DELIBERATELY NOT USED: they stay the scene's rotation, so you can
//                   steer while editing.
//               P (walk pause)   Space (pause when not walking)
//               Cmd+Right / Cmd+Down / Cmd+R
//   EzUiCam     Z X C V (rotate / pan / dolly / zoom)
//   PYTHON-SIDE (host forwards; owned by the scene's scripts, listed here so the
//   next binding does not land on top of them):
//     walk_input_system.py  W A S D · cursor keys · Space · Enter(shoot) · Shift · CapsLock
//     sky_time_system.py    ] [ scrub time · \ pause sky clock · = - sky speed
//                           ' ; step a day · 0 reset to the authored hour
//
// THE PAD REGISTRY. Same contract as the keys, for the gamepad: the host CONSUMES only
// what is listed here and forwards every other button transition (plus the analog axes)
// to the scene's PythonSystem as GamepadButton / GamepadAxes. Abstract button ids are
// DS4-named but position-based, so an Xbox pad reports A/B/X/Y as CROSS/CIRCLE/SQUARE/
// TRIANGLE. Taken, in order of precedence:
//   host        R1  perf HUD page ring FORWARD, L1 the same ring BACKWARD (the pad
//                   analogue of ` and SHIFT-`; desktop + VR). DEDICATED: the bumpers are
//                   the host's, which is why walk's jump sits on TRIANGLE.
//               R3  perf HUD page ring forward (the stick-click twin of R1)
//               CROSS — ONLY while an editor page is up AND a COLOR row is selected
//                   (the pad twin of X above); the scene keeps CROSS (fire) otherwise.
//               DPAD UP/DOWN + L2/R2 — ONLY while an editor page is up (select item /
//                   adjust value). Same modal rule as the cursor keys: the two dpad
//                   bits are masked out of the python forward while editing (so the
//                   scene sees a clean release edge, never a stuck hold) and L2/R2
//                   are zeroed in the forwarded axes snapshot.
//   PYTHON-SIDE (host forwards; owned by the scene's scripts):
//     walk_input_system.py  left stick + dpad locomotion · TRIANGLE jump · CROSS fire
//                           SQUARE / CIRCLE discrete camera YAW steps (no pitch step)
//                           (right stick axes + both analog triggers unbound scene-side,
//                            which is what leaves L2/R2 to the HUD editor pages)
// Deadzone is a PYTHON concern (walk_input_system.py), never the C++ backend — the
// backends deliver raw normalized axes so every scene can choose its own feel.
//
// POST CHAIN (no flag): the HUD's POST editor page IS the tone/grade surface, so the
//   player attaches whichever of the reflected "aces" / "hsvg" nodes the scene left
//   out, default-constructed, at bind. A scene's own nodes always win.
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
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeACES.h> // HUD POST editor page: the tone stage
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeHSVG.h> // HUD POST editor page: the grade stage
#include <ork/lev2/gfx/renderer/NodeCompositor/sky_atmosphere.h>  // HUD SKY editor page: the scene's live atmosphere
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>      // HUD POST editor page: the direct diffuse lobe
#include <ork/lev2/gfx/material_pbr.inl> // HUD CLOUDS editor page: rebinds the deck materials' band / radiance
#include <ork/ecs/physics/CharacterController.h> // E.2-walk: input forwarding + camera yield
#include <ork/ecs/physics/bullet.h> // --physics-debug: BulletSystemData detection + debug-wireframe notify target
#include <ork/ecs/pysys/PythonComponent.h>          // E.2-walk: scene-declared input-script routing
#include <ork/python/context.h>                     // embedded interpreter (OPT-IN: scene declares PythonSystem)
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <rapidjson/document.h> // pre-create manifest peek (--audio's scene-declared twin)
#include <algorithm> // HUD FOLIAGE page: the declared visgroup set is sorted for a stable row ring
#include <atomic>
#include <mutex>

#include <ork/ecs/ecs.h>
#include <ork/ecs/datatable.h>
#include <ork/ecs/system.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/controller.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/ecs/HypermeshComponent.h> // HUD FOLIAGE page: the scene's declared visgroups + SET_VISGROUP
#include <ork/ecs/AssetSystem.h>

#include <ork/ecs/scene.inl>
#include <ork/ecs/entity.inl>  // SpawnData::typedComponent — the walker's spawn, found by archetype
#include <ork/ecs/archetype.inl>
#include <ork/ecs/controller.inl>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <boost/program_options.hpp>

#include "perfhud.h"      // on-screen perf HUD (~ key)
#include "editor_state.h" // saved editor values, keyed to the scene source (3 tiers, per mode)

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
using namespace ork::ecs;
using ork::ecs::player::HudEditProp;   // HUD editor page descriptors (perfhud_pages.h)
using ork::ecs::player::hudColorProp;
using ork::ecs::player::HudRGB;
using ork::ecs::player::hudEnumProp;
using ork::ecs::player::hudFloatProp;
using ork::ecs::player::hudActionProp;         // SETTINGS: the command rows
using ork::ecs::player::EditorStateIO;         // saved editor values (editor_state.h)
using ork::ecs::player::EditorValueMap;
using ork::ecs::player::EditorStateReport;
using ork::ecs::player::editorStateModeKey;

// GLFW cursor keycodes as the ezapp ui events carry them (walk_input_system.py names the
// same four). The player only SENDS these (--autoyaw); the cursor keys belong to the
// scene's rotation, and nothing here consumes them.
static constexpr int KEY_CURSOR_RIGHT = 262;

// HUD EDITOR keys, consumed MODALLY (only while an editor page is up) — see the key
// registry above. ALL FIVE are the sky clock's ('[' ']' scrub, '-' '=' speed, '\' pause),
// borrowed for the duration of an editor page — the page that supersedes them is the one
// showing time of day. Their key-UP is still forwarded, and sky_time_system.py pops an
// unheld key safely.
static constexpr int KEY_EDIT_PREV  = 91; // [
static constexpr int KEY_EDIT_NEXT  = 93; // ]
static constexpr int KEY_EDIT_DEC   = 45; // -
static constexpr int KEY_EDIT_INC   = 61; // =
static constexpr int KEY_EDIT_COLOR = 92; // backslash

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
  std::string editor_script;      // TEST HOOK: comma list LABEL:FRAME driving the HUD editor pages
  // --pysysnotify: TEST HOOK for the SCENE SCRIPTS' own message vocabulary (sky-clock controls,
  // walk actions, ...). Scripted controller messages to the PythonSystem on the same channel the
  // keyboard uses — so an offscreen gate or a scripted movie drives exactly what a key drives.
  // The player stays scene-agnostic: it forwards names and float fields, it interprets nothing.
  std::string pysys_notify_script;
  // --physics-debug: Bullet debug wireframe ON from startup (TOGGLE_DEBUG_DRAW to the
  // BulletSystem). Flag-driven so it works where no keyboard surface exists (VR windowless,
  // offscreen gates).
  bool  physics_debug    = false;
  // --vr: play the scene on the HMD through the active XR runtime (preset FWDPBRVRDM, which
  // resolves to the single-pass stereo output node). Opt-in. With no runtime the SAME render
  // model runs against a NoVr device — stereo on the desktop (the mirror blit is the
  // presentation), never a silent demotion to desktop mono.
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
      ("editscript", po::value<std::string>(&editor_script)->default_value(""), "TEST HOOK: comma list LABEL:FRAME (e.g. \"DOWN:60,RIGHT:90,RIGHT:91\") driving the HUD EDITOR pages through the SAME handler a key/pad press uses, at update-tick FRAME. LABEL is PAGE (step the page ring), UP/DOWN (move the selection, or the H/S/V channel of an open color -- the '[' / ']' keys), LEFT/RIGHT (adjust one step -- the '-' / '=' keys) or COLOR (open/close the selected color row's sub-editor). Pair with ORKID_PERFHUD=<page name> to open on an editor page.")
      ("pysysnotify", po::value<std::string>(&pysys_notify_script)->default_value(""), "TEST HOOK: scripted controller messages to the scene's PythonSystem, on the SAME channel a keyboard/host uses. Semicolon list TIME:EVENT:field=value,... with TIME in seconds of sim abstime (e.g. \"2:SkyTimeSet:hour=18.5;5:InputKey:key=93,down=1;8:InputKey:key=93,down=0\" — a scripted hour, then the ']' key held for 3s). Values are floats; the scene's python script owns the vocabulary. No-op (with a notice) on a scene that declares no PythonSystem.")
      ("physics-debug", po::bool_switch(&physics_debug), "Bullet physics debug wireframe ON from startup (collision shapes + contacts over the visual scene). Works with no keyboard surface (VR/offscreen). Scenes with no BulletSystem log a notice and play normally.")
      ("vr", po::bool_switch(&want_vr), "VR: present the scene on the HMD through the active XR runtime (selects preset FWDPBRVRDM, which resolves to the single-pass stereo output node). Requires ORKID_VR_DRIVER=openxr + a live runtime; with no runtime the same render model runs on a NoVr device — side-by-side stereo on the desktop, mode named in a one-line notice.")
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
  // A SCRIPTED OR HEADLESS RUN MUST NOT REWRITE A SCENE'S SAVED LOOK: --editscript exists
  // to drive the editor from a test, and an offscreen/movie run is a gate, not a grading
  // session. The SETTINGS row still exists in both — it reports the refusal rather than
  // going quiet, so a scripted run can PROVE the refusal happened.
  const bool save_allowed = movie_path.empty() and snapshot_path.empty() and editor_script.empty() and
                            not offscreen and not offscreen_forever;
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
    perfhud._page   = ork::ecs::player::PerfHud::PAGE_FRAME;
  }

  //////////////////////////////////////////////////////////
  // host state
  //////////////////////////////////////////////////////////

  scenedata_ptr_t scenedata;
  // The materialized {asset_name -> artifact} map from the wire step. Held past that step
  // because the HUD editor pages resolve LIVE MATERIALS out of it by asset name (the CLOUDS
  // page rebinds the deck materials' band + radiance) — the artifacts are the only handle a
  // host has on a scene's materials, and nothing else in the process can hand them over.
  varmap::varmap_ptr_t scene_artifacts;
  controller_ptr_t controller;
  std::vector<controller_ptr_t> dead_controllers; // stopped controllers are parked, not reused
                                                  // (the Python runtime does the same)
  sys_ref_t sgsystem; // opaque handle for systemNotify
  sys_ref_t bulletsystem; // physics-debug toggle target (resolved only when the scene declares one)
  sys_ref_t hypermeshsystem; // FOLIAGE page target (resolved only when the scene declares visgroups)
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

  // A pixel counts toward the lit population only at or above this 8-bit LEVEL.
  // The probe used to accept ANY non-zero channel, and the frame that taught us
  // why was not the black frame it looked like at all — it was a real render
  // driven through a mis-ordered post chain, arriving at rgbmax 1: the output
  // dither's own least-significant bit, on 13% of the pixels, which cleared the
  // population rule while the PNG was black to any eye and to every image
  // oracle. Anything a display can resolve clears 4 by orders of magnitude,
  // including the darkest content this engine grades for — a calibrated
  // moonless night reads its sky at 4.5 and its stars far above that
  // (PostFxNodeACES.h), so the settled-lit semantics are unchanged and only the
  // dither floor is now excluded.
  constexpr uint8_t kLitLevel = 4;

  // Settled-lit color probe — shared by the snapshot capture drain (phase 4) and the
  // movie pre-roll drain (phase 5), so both gate on identical criteria. "Lit" = >=0.1%
  // of pixels carry an RGB channel at kLitLevel or above; alpha is skipped (main_rtg
  // clears to opaque black (0,0,0,255), so counting alpha would read a black frame as
  // lit). tot/lit/rgbmax are returned for the diagnostic line.
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
        if (m >= kLitLevel) lit_px++;
      }
    }
    return (tot_px > 0) and (lit_px * 1000 >= tot_px); // >=0.1% pixels at/above the lit level
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
  // The live objects the HUD editor pages poke, resolved once at bind (see
  // register_editor_pages). Held here so the Cmd+R round-trip can re-point the CLONE at
  // them — the clone deserializes fresh instances, which would leave every editor row
  // driving an object no longer in the frame.
  //////////////////////////////////////////////////////////
  pbr::skyatmospheredata_ptr_t    hud_atmosphere;
  std::shared_ptr<PostFxNodeACES> hud_aces;
  std::shared_ptr<PostFxNodeHSVG> hud_hsvg;

  //////////////////////////////////////////////////////////
  // HUD EDITOR PAGES (SKY / POST) — live properties on the perf HUD's page ring.
  //
  // The mechanism is perfhud.h's; this is only the WIRING, and every row here points at
  // an object the SCENE declared, reached once at bind and poked thereafter:
  //
  //   SKY   the SkyAtmosphereData the scene handed the scenegraph under the author param
  //         "SkyAtmosphere". Scene::applyRuntimeParams stores THAT instance on the pbr
  //         common block, so a write here IS the engine's live-edit path: every knob
  //         below is presentation-tier (deliberately outside mediumHash()), so it lands
  //         on the next frame and triggers a refilter cycle rather than a LUT re-bake.
  //         TIME OF DAY is the exception — the clock is the scene's (sky_time_system.py,
  //         the ']' '[' scrub keys), so this row drives the SAME SkyTimeSet message those
  //         keys do rather than opening a second path to the sun.
  //   POST  the reflected postfx nodes under "aces" / "hsvg" — the scene's own where it
  //         declared them, else the engine defaults the player attached at bind (see the
  //         POST CHAIN SELF-DEFENSE block in gpuInit), so the page is whole either way.
  //
  // NOT REGISTERED, and deliberately: a row that cannot bite is worse than an absent one.
  // A scene with no atmosphere gets no SKY page, one with no SceneGraphSystem at all gets
  // no POST page, and time-of-day appears only when the scene declares a PythonSystem.
  //
  // PLAIN CLOSURES, not reflection: the descriptors need a curated subset with per-knob
  // ranges either way, and one of them (time of day) is a MESSAGE, not a property — so
  // reflection would buy nothing here but a layer.
  //////////////////////////////////////////////////////////

  // Last hour COMMANDED from the SKY page. The sky clock lives in the sim's python
  // subinterpreter and has no host-readable channel, so this is a write-side mirror: it
  // reads back what the page last SET, not what the free-running clock has reached.
  float sky_hour_cmd = 12.0f;

  // The DIRECT DIFFUSE LOBE the analytic lights shade with, as its ordinal (which is
  // also the POST row's index into its own label list). Write-side mirror of a notify,
  // like the hour — but re-seeded at bind from the scene's own diffuse_brdf param when
  // it declares one, so what the row shows at launch is what the scene actually asked
  // for rather than the engine default.
  int diffuse_brdf_cmd = int(pbr::DiffuseBrdfModel::OREN_NAYAR);

  // The CLOUD DECKS' state. Write-side mirrors like the hour — the deck applier lives in
  // the scene's python system and answers no reads — but NOT invented ones: both are
  // re-seeded at bind from the launch state the deck library publishes on the scenegraph
  // params (CloudCover / CloudTile), so the baseline these rows are saved against is the
  // scene's own declaration. The values below are only what a scene that publishes
  // nothing would have had anyway (the deck library's ORK_CLOUDGAUGE defaults).
  float cloud_cover_cmd = 0.45f;
  float cloud_tile_cmd  = 1.0f;
  // The decks' ASL LIFT ("cloud base", metres) and RADIANCE GAIN. Unlike cover/tile these
  // two have a MATERIAL half as well as a script half, and both halves are read from here
  // every draw by generators bound onto the deck materials at bind (see the CLOUDS page) —
  // which is why they are plain floats in the host's own frame rather than anything the
  // render thread has to be handed. Base is re-seeded from the scene's CloudAltOffset; gain
  // is a MULTIPLIER on whatever radiance colors the scene baked, so 1.0 is "as authored"
  // for every scene and there is no default to get wrong.
  float cloud_base_cmd = 0.0f;
  float cloud_gain_cmd = 1.0f;
  // Sends the deck state ONCE at startup, so the scene's own launch state (or the saved
  // one over it) is what the deck script holds from the first tick — its own numbers come
  // from env knobs and would otherwise disagree with the sky until the first nudge, which
  // is a jump waiting to happen. Assigned only when the scene actually has decks.
  std::function<void()> push_cloud_state;
  // Binds the deck MATERIALS' live band + radiance for a given scene and its materialized
  // artifact set — the material half of the base/gain rows (the script half is CloudSet).
  // Re-callable because the Cmd+R round-trip materializes a whole new set of materials:
  // the generators bound here would otherwise be driving discarded ones, which is the same
  // staleness the atmosphere/postfx instances are carried across for.
  std::function<void(scenedata_ptr_t, varmap::varmap_ptr_t)> bind_cloud_materials;

  // FOLIAGE — the selected member of the scene's "foliage:" visgroup set (an index into
  // foliage_states below). Same shape as push_cloud_state: the set has to be re-asserted
  // on every simulation, because a fresh simulation stages every member at its DECLARED
  // launch visibility and would otherwise show the scene's default while the row reads
  // whatever the user (or the saved editor state) last chose.
  std::vector<std::string> foliage_states; // group suffixes, sorted; row labels
  int foliage_mode_cmd = 0;
  std::function<void()> push_foliage_state;

  //////////////////////////////////////////////////////////
  // SAVED EDITOR VALUES (editor_state.h). The scene's SOURCE .py — reflected on SceneData
  // as "ScriptFile", written by whoever composed the .ecs in PATH-TOKEN form — is the
  // identity everything here keys on. It is expanded against the LIVE workspace, because
  // the token is exactly what lets one .ecs be correct on a mac checkout and a linux one.
  //
  // No source (a hand-composed .ecs, a scene from before composers set it) is not an
  // error: it means this run has nothing to key state to, and the SETTINGS row says so
  // rather than pretending to save.
  //////////////////////////////////////////////////////////
  std::string      editor_state_path;   // the sidecar, or empty when there is no source
  std::string      editor_state_scene;  // basename of the source .py, for the file's own record
  EditorValueMap   editor_baselines;    // tier 2, captured at bind
  std::atomic<int> save_row_state{0};   // indexes the SETTINGS row's state vocabulary
  bool             vr_presentation = false; // the MODE a save/load is keyed to
  enum { SAVE_READY = 0, SAVE_SAVED = 1, SAVE_REFUSED = 2, SAVE_NOSCENE = 3, SAVE_FAILED = 4 };
  std::function<void()> do_save_editor_state;

  //////////////////////////////////////////////////////////
  // WHERE THE WALKER STARTS, as four saved editor values. The spawn is SCENE DATA — the
  // SpawnData transform in the .ecs — so restoring it is not a mid-run teleport: the saved
  // position is written into that transform in gpuInit, BEFORE the simulation is created,
  // and the character is simply BORN there. Nothing fights physics, and the controller
  // needs no teleport vocabulary it does not already have.
  //
  // The four rows are ordinary FLOATs and ride the ordinary save/restore: their tier-2
  // baseline is the spawn the scene authored, so moving the scene's spawn wins over a
  // stale saved one exactly as it does for any other row.
  //
  // HEADING IS NOT PART OF THAT TRANSFORM. A walker's capsule is rotation-locked
  // (angularFactor 0,0,0) and its facing is CONTROLLER state (_heading), which starts at
  // zero for a fresh character and never reads the spawn rotation — so a yaw written into
  // the transform would be a value that looks saved and does nothing. The restored yaw is
  // re-issued instead as the TurnStep the input scripts already use, once, at start,
  // before any input can have moved the heading off zero.
  //////////////////////////////////////////////////////////
  spawndata_ptr_t walker_spawn;      // the character's SpawnData, or null when there is no walker
  float spawn_x = 0.0f, spawn_y = 0.0f, spawn_z = 0.0f; // the rows: WORLD position, capsule centre
  float spawn_yaw          = 0.0f;   // the row: heading in radians (0 = -Z, the walker's own zero)
  float spawn_eye_height   = 1.7f;   // read off the character data — the ray starts at the EYE
  float spawn_cam_distance = 0.0f;   // ... and, in follow mode, that many metres behind
  float spawn_half_capsule = 0.0f;   // feet -> the transform's own currency (the capsule centre)
  float spawn_heading_send = 0.0f;   // nonzero: a restored heading still owed to the character
  std::atomic<int>  spawn_row_state{0}; // the ACTION row's state vocabulary
  std::atomic<bool> spawn_probe_want{false}; // the row fires on any thread; the update thread asks
  enum { SPAWN_READY = 0, SPAWN_SAMPLING = 1, SPAWN_SET = 2, SPAWN_NOWALKER = 3, SPAWN_NOANSWER = 4 };
  // A world coordinate is what these rows hold, so the range is a world, not a knob. The
  // step is 1m: hand-editing a spawn is a nudge, the ACTION row does the real placing.
  static constexpr float kSpawnXZLimit  = 65536.0f;
  static constexpr float kSpawnYLimit   = 65536.0f;
  static constexpr float kSpawnYawLimit = 3.14159265f; // heading wraps at ±pi

  // Applied spawns are lifted this far and fall the last stretch. The character's own
  // ground-snap normally owns Y outright (it raycasts down at spawn XZ and places the feet
  // SpawnAboveGround above the hit), but a scene with no terrain floor has no snap — there
  // the lift is what keeps a re-baked, slightly higher landscape from swallowing the spawn.
  static constexpr float kSpawnApplyLift = 2.0f;
  // How long the ACTION row waits for the character's answer before saying it never came.
  // The update thread ticks at ~480Hz and the request drains on the very next one, so this
  // is a second of grace, not a budget.
  static constexpr int kSpawnProbeTicks = 480;

  // Hand the character its restored facing. Called once per simulation START (a restart
  // builds a fresh character, whose heading is zero again), and a no-op when nothing was
  // restored — this must not touch a scene that saved no spawn.
  auto send_spawn_heading = [&](controller_ptr_t c, sys_ref_t chsys) {
    if (not c or spawn_heading_send == 0.0f)
      return;
    auto tab               = std::make_shared<DataTable>();
    (*tab)["radians"_tok]  = float(spawn_heading_send);
    c->systemNotify(chsys, CharacterControllerSystem::TurnStep._token, tab);
    deco::printf(fvec3::Green(), "ork.ecs.player: walker heading restored to %.3f rad\n",
                 double(spawn_heading_send));
  };

  // The SETTINGS row's command. Every refusal is a STATE the row shows, never a silent
  // no-op: the owner presses the button and the row says what happened.
  do_save_editor_state = [&]() {
    if (editor_state_path.empty()) {
      save_row_state.store(SAVE_NOSCENE);
      deco::printf(fvec3::Yellow(),
                   "ork.ecs.player: no scene source on this .ecs — nowhere to save editor values\n");
      return;
    }
    if (not save_allowed) {
      save_row_state.store(SAVE_REFUSED);
      deco::printf(fvec3::Yellow(),
                   "ork.ecs.player: save REFUSED — a scripted/offscreen run must not rewrite a scene's saved look\n");
      return;
    }
    auto rep = EditorStateIO::save(
        editor_state_path, vr_presentation, editor_state_scene, perfhud._pages, editor_baselines);
    for (const auto& l : rep._lines)
      deco::printf(fvec3::Yellow(), "ork.ecs.player: %s\n", l.c_str());
    save_row_state.store(rep._ok ? SAVE_SAVED : SAVE_FAILED);
    deco::printf(rep._ok ? fvec3::Green() : fvec3::Red(),
                 "ork.ecs.player: editor values %s (%s)\n",
                 rep._ok ? "SAVED" : "NOT saved", rep._summary.c_str());
  };

  auto register_editor_pages = [&]() {
    pbr::skyatmospheredata_ptr_t    atmo;
    std::shared_ptr<PostFxNodeACES> aces;
    std::shared_ptr<PostFxNodeHSVG> hsvg;
    bool has_pysys = false;
    bool has_sgsys = false;
    for (const auto& it : scenedata->getSystemDatas()) {
      if (std::dynamic_pointer_cast<PythonSystemData>(it.second))
        has_pysys = true;
      auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second);
      if (not sgd)
        continue;
      has_sgsys = true;
      const auto& uparams = sgd->userSceneParams();
      // The scene's own diffuse lobe, if it declared one — same resolver the
      // engine uses, so an unknown spelling is refused here too rather than
      // showing a row that disagrees with the render.
      auto di = uparams.find("diffuse_brdf");
      if (di != uparams.end() and di->second.isA<std::string>()) {
        const auto& given = di->second.get<std::string>();
        pbr::DiffuseBrdfModel model;
        if (pbr::diffuseBrdfModelFromName(given, model))
          diffuse_brdf_cmd = int(model);
        else
          deco::printf(fvec3::Red(),
                       "ork.ecs.player: scene declares diffuse_brdf<%s> - unknown; valid: %s\n",
                       given.c_str(), pbr::diffuseBrdfModelValidSet().c_str());
      }
      auto        ai      = uparams.find("SkyAtmosphere");
      if (ai != uparams.end() and ai->second.isA<pbr::skyatmospheredata_ptr_t>())
        atmo = ai->second.get<pbr::skyatmospheredata_ptr_t>();
      // SELF-DEFEND. A scene that selects the procedural sky but authors no medium knob
      // publishes NO SkyAtmosphereData (the sky library only emits one when something is
      // declared) — yet it still renders a sky, because the forward prologue fabricates a
      // default one at first frame (fwdnode_impl_sub.cpp, "procedural sky source with no
      // SkyAtmosphereData attached"). That object is born AFTER this bind-time read, so
      // the SKY page would come up with only its time-of-day row and no way to say why.
      // Attach the very same default HERE instead: identical construction, so the frame
      // is unchanged and the prologue's fallback simply never fires — it finds ours.
      // Gated on the PROCEDURAL source, exactly like the prologue's own fallback, so a
      // baked-sky scene is never silently armed with an atmosphere it did not ask for.
      if (not atmo) {
        auto si = uparams.find("SkySource");
        bool procedural = (si != uparams.end()) and si->second.isA<std::string>() and
                          (si->second.get<std::string>() == "procedural");
        if (procedural) {
          atmo = std::make_shared<pbr::SkyAtmosphereData>();
          sgd->setUserSceneParam("SkyAtmosphere", atmo);
          deco::printf(fvec3::Yellow(),
                       "ork.ecs.player: scene declares a procedural sky with no SkyAtmosphere — "
                       "attached the engine default so the HUD SKY page can edit it\n");
        }
      }
      auto pi = sgd->_postfx_nodes.find("aces");
      if (pi != sgd->_postfx_nodes.end())
        aces = std::dynamic_pointer_cast<PostFxNodeACES>(pi->second);
      pi = sgd->_postfx_nodes.find("hsvg");
      if (pi != sgd->_postfx_nodes.end())
        hsvg = std::dynamic_pointer_cast<PostFxNodeHSVG>(pi->second);
    }
    hud_atmosphere = atmo; // held for the round-trip re-point (see the Cmd+R clone below)
    hud_aces       = aces;
    hud_hsvg       = hsvg;

    ////////////////////////////////////////////////////////
    // SKY
    ////////////////////////////////////////////////////////
    std::vector<HudEditProp> sky;   // SKY-MAIN: the sky itself + the celestial bodies
    std::vector<HudEditProp> haze;  // SKY-HAZE: the artist layer stacked on the medium
    if (has_pysys) {
      sky.push_back(hudFloatProp(
          "time of day", 0.0f, 24.0f, 0.05f,
          [&sky_hour_cmd]() { return sky_hour_cmd; },
          [&](float h) {
            sky_hour_cmd = h;
            controller_ptr_t c;
            sys_ref_t        pys;
            {
              std::lock_guard<std::mutex> lock(ctl_mutex);
              c   = controller;
              pys = pysystem;
            }
            if (c) {
              auto tab           = std::make_shared<DataTable>();
              (*tab)["hour"_tok] = float(h);
              c->systemNotify(pys, "SkyTimeSet"_tok, tab);
            }
          }));
    }
    if (atmo) {
      // Ranges are the knob's own working span (sky_atmosphere.h documents each); the
      // step is ~1/60th of it, so a held trigger crosses the range in about a second.
      sky.push_back(hudFloatProp("sky exposure", 0.0f, 16.0f, 0.25f,
                                 [atmo]() { return atmo->_skyExposure; },
                                 [atmo](float v) { atmo->_skyExposure = v; }));
      sky.push_back(hudFloatProp("sun disc", 0.0f, 400.0f, 5.0f,
                                 [atmo]() { return atmo->_sunDiscIntensity; },
                                 [atmo](float v) { atmo->_sunDiscIntensity = v; }));
      sky.push_back(hudFloatProp("moon disc", 0.0f, 2.0f, 0.02f,
                                 [atmo]() { return atmo->_moonDiscIntensity; },
                                 [atmo](float v) { atmo->_moonDiscIntensity = v; }));
      // the moon's whole-dome scatter — what "moonlight" actually means for the night sky
      sky.push_back(hudFloatProp("moonlight", 0.0f, 0.04f, 5.0e-4f,
                                 [atmo]() { return atmo->_moonRayleighStrength; },
                                 [atmo](float v) { atmo->_moonRayleighStrength = v; }));
      sky.push_back(hudFloatProp("starlight", 0.0f, 5.0e-6f, 5.0e-8f,
                                 [atmo]() { return atmo->_starlightIntensity; },
                                 [atmo](float v) { atmo->_starlightIntensity = v; }));
      sky.push_back(hudFloatProp("airglow", 0.0f, 5.0e-5f, 5.0e-7f,
                                 [atmo]() { return atmo->_airglowIntensity; },
                                 [atmo](float v) { atmo->_airglowIntensity = v; }));
      // THE HAZE BLOCK — the artist layer stacked on the physical medium. All of it is
      // presentation tier (nothing joins mediumHash), which is what makes it live-editable
      // here. The two TINTS are fvec3 and have no descriptor kind yet, so they are not on
      // the page; every scalar knob is.
      sky.push_back(hudColorProp("moon albedo", [atmo]() {
        return HudRGB{atmo->_moonAlbedoColor.x, atmo->_moonAlbedoColor.y, atmo->_moonAlbedoColor.z};
      }, [atmo](const HudRGB& c) { atmo->_moonAlbedoColor = fvec3(c[0], c[1], c[2]); }));
      haze.push_back(hudEnumProp("aerial persp", {"off", "on"},
                                [atmo]() { return atmo->_aerialPerspectiveEnable ? 1.0f : 0.0f; },
                                [atmo](float v) { atmo->_aerialPerspectiveEnable = (v >= 0.5f); }));
      haze.push_back(hudFloatProp("haze density", 0.0f, 1.0f, 0.01f,
                                 [atmo]() { return atmo->_hazeDensity; },
                                 [atmo](float v) { atmo->_hazeDensity = v; }));
      haze.push_back(hudFloatProp("haze height km", 0.0f, 4.0f, 0.05f,
                                 [atmo]() { return atmo->_hazeScaleHeight; },
                                 [atmo](float v) { atmo->_hazeScaleHeight = v; }));
      // HG asymmetry: -1 back-scatter .. +1 forward-scatter, 0 = isotropic
      haze.push_back(hudFloatProp("haze phase g", -0.95f, 0.95f, 0.02f,
                                 [atmo]() { return atmo->_hazePhaseG; },
                                 [atmo](float v) { atmo->_hazePhaseG = v; }));
      haze.push_back(hudFloatProp("haze max km", 1.0f, 320.0f, 4.0f,
                                 [atmo]() { return atmo->_hazeMaxDistanceKm; },
                                 [atmo](float v) { atmo->_hazeMaxDistanceKm = v; }));
      // COLOR rows — one line each, H/S/V edited in place once opened. fvec3 <-> HudRGB
      // is the whole adapter, which is what makes the sub-editor reusable for any future
      // color on any page. Only PRESENTATION-tier colors are here: _sunIlluminance and
      // _groundAlbedo are medium terms (mediumHash), so editing them would force a LUT
      // re-bake rather than a refilter — a different tier of edit, not this page's.
      haze.push_back(hudColorProp("haze scatter", [atmo]() {
        return HudRGB{atmo->_hazeScatterTint.x, atmo->_hazeScatterTint.y, atmo->_hazeScatterTint.z};
      }, [atmo](const HudRGB& c) { atmo->_hazeScatterTint = fvec3(c[0], c[1], c[2]); }));
      haze.push_back(hudColorProp("haze inscatter", [atmo]() {
        return HudRGB{atmo->_hazeInscatterTint.x, atmo->_hazeInscatterTint.y, atmo->_hazeInscatterTint.z};
      }, [atmo](const HudRGB& c) { atmo->_hazeInscatterTint = fvec3(c[0], c[1], c[2]); }));
      // WHERE the shadowed aerial-perspective march samples the cascades — three
      // discrete modes (off / per-fragment inline / quarter-res + composite), so an
      // ENUM row. The value IS the mode (SkyAtmosphereData::_hazeSunShadow), snapped
      // on both sides so a partial editor value can never land between two modes.
      haze.push_back(hudEnumProp("haze in shadow", {"off", "on", "1/4res"},
                                [atmo]() { return float(int(atmo->_hazeSunShadow + 0.5f)); },
                                [atmo](float v) {
                                  int m = int(v + 0.5f);
                                  m     = (m < 0) ? 0 : ((m > 2) ? 2 : m);
                                  atmo->_hazeSunShadow = float(m);
                                }));
      // ACCENTUATION of the shafts, honoured identically by both shaft modes (the
      // remap lives in the shared per-step tap), so switching the row above with
      // this one held is a fair A/B. 1.0 = physical.
      haze.push_back(hudFloatProp("shaft gain", 0.0f, 4.0f, 0.1f,
                                 [atmo]() { return atmo->_hazeSunShadowGain; },
                                 [atmo](float v) { atmo->_hazeSunShadowGain = v; }));
      // THE MEDIUM COLORS — the other tier of sky color, and the reason they are here
      // rather than deliberately absent: an edit to any of them moves mediumHash(), and
      // BOTH consumers already watch that hash (HillaireSky::bakeStaticLuts re-bakes the
      // transmittance/multi-scatter chain, the forward prologue's IBL feed starts a
      // refilter cycle). So these rows land on screen exactly like the presentation ones,
      // paying a LUT re-bake per edit instead of nothing — the cost of editing the medium.
      //
      // THE TWO COEFFICIENT COLORS are edited in SCALED units, named in the label. They
      // are per-channel extinction in 1/km — rayleigh runs 5.8e-3..3.3e-2, ozone ~1e-3 —
      // and the color sub-editor's value channel steps in hundredths, so an unscaled row
      // could not express them at all. A row that cannot reach its own value is a row that
      // lies, so the scale is part of the row.
      sky.push_back(hudColorProp("ground albedo", [atmo]() {
        return HudRGB{atmo->_groundAlbedo.x, atmo->_groundAlbedo.y, atmo->_groundAlbedo.z};
      }, [atmo](const HudRGB& c) { atmo->_groundAlbedo = fvec3(c[0], c[1], c[2]); }));
      sky.push_back(hudColorProp("sun illuminance", [atmo]() {
        return HudRGB{atmo->_sunIlluminance.x, atmo->_sunIlluminance.y, atmo->_sunIlluminance.z};
      }, [atmo](const HudRGB& c) { atmo->_sunIlluminance = fvec3(c[0], c[1], c[2]); }));
      constexpr float kRayleighUnit = 1.0e-2f; // 1/km
      sky.push_back(hudColorProp("rayleigh e-2", [atmo]() {
        return HudRGB{atmo->_rayleighScattering.x / kRayleighUnit,
                      atmo->_rayleighScattering.y / kRayleighUnit,
                      atmo->_rayleighScattering.z / kRayleighUnit};
      }, [atmo](const HudRGB& c) {
        atmo->_rayleighScattering = fvec3(c[0], c[1], c[2]) * kRayleighUnit;
      }));
      constexpr float kOzoneUnit = 1.0e-3f; // 1/km
      sky.push_back(hudColorProp("ozone abs e-3", [atmo]() {
        return HudRGB{atmo->_ozoneAbsorption.x / kOzoneUnit,
                      atmo->_ozoneAbsorption.y / kOzoneUnit,
                      atmo->_ozoneAbsorption.z / kOzoneUnit};
      }, [atmo](const HudRGB& c) {
        atmo->_ozoneAbsorption = fvec3(c[0], c[1], c[2]) * kOzoneUnit;
      }));
    }
    // ONE SUBJECT PER PAGE. The sky outgrew a single page the moment the medium colors
    // joined it, and a page taller than the panel is a page you scroll in your head. The
    // split is presentation only: saved values are keyed by row id, not by page, so rows
    // can be re-homed without touching a scene's state file.
    if (not sky.empty())
      perfhud.registerEditorPage("SKY-MAIN", sky);
    else
      deco::printf(fvec3::Yellow(),
                   "ork.ecs.player: no SkyAtmosphere / PythonSystem declared — no HUD SKY pages\n");
    if (not haze.empty())
      perfhud.registerEditorPage("SKY-HAZE", haze);

    ////////////////////////////////////////////////////////
    // CLOUDS — the deck state, driven the way the deck state is ACTUALLY driven: the
    // decks' runtime control bus is the plane ENTITY TRANSFORMS (coverage is encoded in
    // deck altitude, tile size in uniform scale — _cloud_deck.py's "CONTROL BUS" note),
    // and the one writer of those transforms is the scene's own deck script. So these rows
    // send the script the same state its sticks move, through the CloudSet message, rather
    // than opening a second path to the transforms and racing it.
    //
    // REGISTERED WHENEVER THE SCENE HAS DECKS, detected from the deck entity names the
    // library declares — which, since every procedural sky now carries them, is every
    // procedural-sky scene. They start EMPTY (cover 0 parks every plane), so the page is
    // how weather is raised at all, not merely how it is tuned.
    //
    // THE ROWS COME UP HOLDING WHAT THE SCENE DECLARED. The decks' runtime currency is the
    // deck TRANSFORM (coverage encoded as altitude), which no reader can invert back into
    // a cover — so the launch state rides the scenegraph params next to SkyAtmosphere, and
    // these rows read it there. That is what makes a saved cover a diff against the SCENE
    // (and so re-authoring-proof) rather than against a constant the player made up.
    ////////////////////////////////////////////////////////
    int  num_cloud_rows = 0;
    bool has_decks = false;
    for (const auto& kv : scenedata->GetSceneObjects()) {
      std::string n = kv.first.c_str();
      if (n.rfind("cloud_", 0) == 0)
        has_decks = true;
    }
    float cloud_alt_offset  = 0.0f;
    bool  cloud_state_known = false;
    for (const auto& it : scenedata->getSystemDatas()) {
      auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second);
      if (not sgd)
        continue;
      const auto& up = sgd->userSceneParams();
      auto        rd = [&up](const char* key, float& out) -> bool {
        auto i = up.find(key);
        if (i == up.end() or not i->second.isA<float>())
          return false;
        out = i->second.get<float>();
        return true;
      };
      if (rd("CloudCover", cloud_cover_cmd))
        cloud_state_known = true;
      rd("CloudTile", cloud_tile_cmd);
      rd("CloudAltOffset", cloud_alt_offset);
    }
    if (has_decks and has_pysys) {
      // the base row comes up holding the lift the SCENE declared, so moving it is a diff
      // against the scene and the launch frame is the authored one.
      cloud_base_cmd = cloud_alt_offset;
      const float cloud_base_launch = cloud_alt_offset;
      auto send_clouds = [&](float cover, float tile) {
        controller_ptr_t c;
        sys_ref_t        pys;
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          c   = controller;
          pys = pysystem;
        }
        if (not c)
          return;
        auto tab            = std::make_shared<DataTable>();
        (*tab)["cover"_tok] = float(cover);
        (*tab)["tile"_tok]  = float(tile);
        // The decks' lift travels with every command: the script cannot read scene params,
        // and without it a live cover change would place the shells off the very band their
        // material builds (the author-time transform adds it, the applier had no way to know
        // it). LIVE, not the launch value — this is the "cloud base" row's channel, and the
        // transform is the half of it the script owns.
        (*tab)["alt_offset"_tok] = float(cloud_base_cmd);
        c->systemNotify(pys, "CloudSet"_tok, tab);
      };
      ////////////////////////////////////////////////////////
      // THE MATERIAL HALF OF THE BASE. Deck altitude IS the coverage encoding: the shader
      // decodes t = (apex_y - CgAltLo) * CgInvSweep, and CgAltLo was baked at author time
      // from the deck's spec altitude plus the scene's lift. Move the shells without moving
      // that band and t shifts by (metres / sweep) — the sweep is 600 m, so a 300 m nudge
      // alone would swing coverage across half the band and a kilometre would peg it. The
      // two halves therefore move TOGETHER: the transform through CloudSet above, the band
      // by rebinding CgAltLo here.
      //
      // WHY THE HOST OWNS THIS HALF: the deck script runs in the sim sub-interpreter, whose
      // whole surface is entities / components / datatables — there is no lev2 in it at all,
      // by design, so a material is not reachable from the one place that writes the
      // transforms. The host holds the materialized artifacts, so the host closes the loop.
      //
      // BOUND AS A GENERATOR, once, on this thread: the rows are moved on the UPDATE thread,
      // and a bindParam there would mutate the material's _bound_params map while the render
      // thread is overlaying it into pipelines. A generator bound now means the update thread
      // only ever writes a float, and the draw reads it — including the sun-cookie pass, so
      // the cloud shadows track the same band as the sky.
      ////////////////////////////////////////////////////////
      bind_cloud_materials = [&, cloud_base_launch](scenedata_ptr_t scn, varmap::varmap_ptr_t artifacts) {
        if (not scn or not artifacts)
          return;
        int n_band = 0, n_rad = 0;
        for (const auto& it : scn->getSystemDatas()) {
          auto asd = std::dynamic_pointer_cast<AssetSystemData>(it.second);
          if (not asd)
            continue;
          for (auto gen : asd->_gens) {
            auto mgen = std::dynamic_pointer_cast<lev2::PbrMaterialGenData>(gen);
            if (not mgen or not mgen->_shader_params)
              continue;
            if (mgen->_asset_name.rfind("cloud_", 0) != 0)
              continue;
            lev2::pbrmaterial_ptr_t mtl;
            if (auto m = artifacts->typedValueForKey<lev2::pbrmaterial_ptr_t>(mgen->_asset_name))
              mtl = m.value();
            if (not mtl or not mtl->_as_freestyle)
              continue;
            auto fs = mtl->_as_freestyle;
            // the BAKED values are read back off the gen, never recomputed: the per-deck
            // altitude table and the scene's radiance colors live on the scene side, and a
            // second copy of either here is a copy that goes stale on the next re-author.
            if (auto baked = mgen->_shader_params->typedValueForKey<float>("CgAltLo")) {
              float band = baked.value();
              if (auto par = fs->param("CgAltLo")) {
                mtl->bindParam(par, FxPipeline::varval_t(FxPipeline::varval_generator_t(
                    [&cloud_base_cmd, band, cloud_base_launch]() -> FxPipeline::varval_t {
                      return FxPipeline::varval_t(band + (cloud_base_cmd - cloud_base_launch));
                    })));
                n_band++;
              }
            }
            // RADIANCE: one gain over the pair of colors the deck material lights with, so
            // the decks can be re-exposed against a frame without re-tuning either color.
            for (const char* colname : {"CgLitCol", "CgShadCol"}) {
              auto baked = mgen->_shader_params->typedValueForKey<fvec3>(colname);
              if (not baked)
                continue;
              auto par = fs->param(colname);
              if (not par)
                continue;
              fvec3 color = baked.value();
              mtl->bindParam(par, FxPipeline::varval_t(FxPipeline::varval_generator_t(
                  [&cloud_gain_cmd, color]() -> FxPipeline::varval_t {
                    return FxPipeline::varval_t(color * cloud_gain_cmd);
                  })));
              n_rad++;
            }
          }
        }
        // LOUD EITHER WAY. A base row whose material half did not bind is a row that
        // corrupts coverage as it moves, so it says which half is missing rather than
        // waiting to be discovered as a coverage bug.
        deco::printf(n_band ? fvec3::Green() : fvec3::Red(),
                     "ork.ecs.player: cloud deck materials: band rebind on %d, radiance rebind on %d%s\n",
                     n_band, n_rad,
                     n_band ? "" : " — the CLOUDS base row would move the shells OFF their own "
                                   "coverage band (no CgAltLo found on any cloud_* material)");
      };
      bind_cloud_materials(scenedata, scene_artifacts);
      std::vector<HudEditProp> clouds;
      // sky-cover fraction, the currency a scene author declares in (Scene.cloud_decks)
      clouds.push_back(hudFloatProp(
          "cloud cover", 0.0f, 1.0f, 0.01f,
          [&cloud_cover_cmd]() { return cloud_cover_cmd; },
          [&, send_clouds](float v) {
            cloud_cover_cmd = v;
            send_clouds(cloud_cover_cmd, cloud_tile_cmd);
          }));
      // tile-size multiplier; the script clamps to the gauge's own 0.35..2.8 band
      clouds.push_back(hudFloatProp(
          "cloud tile", 0.35f, 2.8f, 0.02f,
          [&cloud_tile_cmd]() { return cloud_tile_cmd; },
          [&, send_clouds](float v) {
            cloud_tile_cmd = v;
            send_clouds(cloud_cover_cmd, cloud_tile_cmd);
          }));
      // DECK BASE in metres ASL — the AGL spec altitudes' lift, the number a scene declares
      // as cloud_decks(alt_offset_m=). The band range covers every scene the deck library
      // serves (sea-level gauges through the alpine terrain's 3 km lift) and the 50 m step
      // is a twelfth of the coverage sweep, so a single press is a visible move and not a
      // coverage jump. The transform goes out with the message; the material band is already
      // reading this same float per draw (see the generator above), so the two never part.
      clouds.push_back(hudFloatProp(
          "cloud base", 0.0f, 6000.0f, 50.0f,
          [&cloud_base_cmd]() { return cloud_base_cmd; },
          [&, send_clouds](float v) {
            cloud_base_cmd = v;
            send_clouds(cloud_cover_cmd, cloud_tile_cmd);
          }));
      // DECK RADIANCE GAIN over the scene's own lit/shadow colors — 1.0 is exactly as
      // authored, which is why this row needs no scene param to come up honest. Pure
      // material side: no message, the generators re-read it every draw.
      clouds.push_back(hudFloatProp(
          "cloud gain", 0.0f, 3.0f, 0.05f,
          [&cloud_gain_cmd]() { return cloud_gain_cmd; },
          [&cloud_gain_cmd](float v) { cloud_gain_cmd = v; }));
      num_cloud_rows = int(clouds.size());
      perfhud.registerEditorPage("CLOUDS", clouds);
      // ONLY WHEN THE SCENE SAID SO. An .ecs composed before the deck library published
      // its launch state carries none, and the rows above are then the library's generic
      // defaults — pushing those would move decks the scene placed deliberately (and with
      // no CloudAltOffset to go with them, place them off their own band). Silent there:
      // the script keeps the author-time transforms, exactly as it does with no host.
      if (cloud_state_known)
        push_cloud_state = [&, send_clouds]() { send_clouds(cloud_cover_cmd, cloud_tile_cmd); };
      else
        deco::printf(fvec3::Yellow(),
                     "ork.ecs.player: scene publishes no cloud launch state (composed before the "
                     "decks published one) — CLOUDS rows start from library defaults and the decks "
                     "are left exactly as authored until a row is moved\n");
    }

    ////////////////////////////////////////////////////////
    // FOLIAGE — the scene's mutually-exclusive hypermesh presentation sets, one row.
    //
    // A scene declares alternative treatments of the same scatter as co-resident entity
    // sets tagged with a visgroup ("foliage:<state>"; HypermeshComponentData::_visgroup),
    // exactly one of which launches visible. Switching is therefore a VISIBILITY message
    // to the hosting HypermeshSystem, not a material edit on the render thread — the
    // sets do not share a drawable, so nothing here can race a draw.
    //
    // The STATES ARE THE SCENE'S, not this player's: the row's choices are whatever
    // suffixes the .ecs declares, sorted so the ring is stable across runs, and the row
    // comes up on the one the scene launched. A player with a name table here would have
    // to be edited every time a scene grew a treatment.
    //
    // COST OF THE UNSELECTED SETS IS ZERO until they are asked for: a member that stages
    // with its node disabled is skipped by both the per-frame gpu update and the per-view
    // pre-render, so it never materializes a mesh, never allocates an impostor atlas and
    // never starts a section bake. The first switch to a set pays that build in one hitch.
    ////////////////////////////////////////////////////////
    {
      static const char* kFoliagePrefix = "foliage:";
      std::string foliage_launch;
      for (const auto& kv : scenedata->GetSceneObjects()) {
        auto sd = std::dynamic_pointer_cast<SpawnData>(kv.second);
        if (not sd)
          continue;
        auto hcd = sd->typedComponent<HypermeshComponentData>();
        if (not hcd or hcd->_visgroup.rfind(kFoliagePrefix, 0) != 0)
          continue;
        auto state = hcd->_visgroup.substr(strlen(kFoliagePrefix));
        if (std::find(foliage_states.begin(), foliage_states.end(), state) == foliage_states.end())
          foliage_states.push_back(state);
        if (hcd->_visible)
          foliage_launch = state;
      }
      std::sort(foliage_states.begin(), foliage_states.end());
      if (foliage_states.size() > 1) {
        auto it = std::find(foliage_states.begin(), foliage_states.end(), foliage_launch);
        foliage_mode_cmd = (it == foliage_states.end()) ? 0 : int(it - foliage_states.begin());
        auto send_foliage = [&]() {
          controller_ptr_t c;
          sys_ref_t        hms;
          {
            std::lock_guard<std::mutex> lock(ctl_mutex);
            c   = controller;
            hms = hypermeshsystem;
          }
          if (not c)
            return;
          // EVERY group every time, the losers explicitly off. A message that only turned
          // the winner on would leave the previous set drawn as well — two barks on one
          // trunk — and the receiving system has no idea which sets are peers.
          for (size_t i = 0; i < foliage_states.size(); i++) {
            auto tab             = std::make_shared<DataTable>();
            (*tab)["group"_tok]  = std::string(kFoliagePrefix) + foliage_states[i];
            (*tab)["enable"_tok] = int(i == size_t(foliage_mode_cmd) ? 1 : 0);
            c->systemNotify(hms, HypermeshSystem::SET_VISGROUP, tab);
          }
        };
        std::vector<HudEditProp> foliage;
        foliage.push_back(hudEnumProp(
            "bark", foliage_states,
            [&foliage_mode_cmd]() { return float(foliage_mode_cmd); },
            [&, send_foliage](float v) {
              foliage_mode_cmd = int(std::lround(v));
              send_foliage();
            }));
        perfhud.registerEditorPage("FOLIAGE", foliage);
        push_foliage_state = [send_foliage]() { send_foliage(); };
        deco::printf(fvec3::Green(),
                     "ork.ecs.player: foliage visgroups: %zu states, scene launches <%s>\n",
                     foliage_states.size(),
                     foliage_launch.empty() ? "none visible" : foliage_launch.c_str());
      }
    }

    ////////////////////////////////////////////////////////
    // POST
    ////////////////////////////////////////////////////////
    std::vector<HudEditProp> post;
    if (hsvg) {
      post.push_back(hudFloatProp("hue", -1.0f, 1.0f, 0.02f,
                                  [hsvg]() { return hsvg->_hue; },
                                  [hsvg](float v) { hsvg->_hue = v; }));
      post.push_back(hudFloatProp("saturation", 0.0f, 2.0f, 0.02f,
                                  [hsvg]() { return hsvg->_saturation; },
                                  [hsvg](float v) { hsvg->_saturation = v; }));
      post.push_back(hudFloatProp("value", 0.0f, 2.0f, 0.02f,
                                  [hsvg]() { return hsvg->_value; },
                                  [hsvg](float v) { hsvg->_value = v; }));
      post.push_back(hudFloatProp("gamma", 0.25f, 3.0f, 0.02f,
                                  [hsvg]() { return hsvg->_gamma; },
                                  [hsvg](float v) { hsvg->_gamma = v; }));
    }
    if (aces) {
      post.push_back(hudFloatProp("exposure", 0.0f, 4.0f, 0.02f,
                                  [aces]() { return aces->_exposure; },
                                  [aces](float v) { aces->_exposure = v; }));
      // the three scene-adaptation gains (PostFxNodeACES documents the anchor ladder);
      // the floor is the dead-of-night display gain, hence the wide range.
      post.push_back(hudFloatProp("adapt day", 0.0f, 4.0f, 0.02f,
                                  [aces]() { return aces->_adaptDay; },
                                  [aces](float v) { aces->_adaptDay = v; }));
      post.push_back(hudFloatProp("adapt twilight", 0.0f, 4.0f, 0.02f,
                                  [aces]() { return aces->_adaptTwilight; },
                                  [aces](float v) { aces->_adaptTwilight = v; }));
      post.push_back(hudFloatProp("adapt floor", 0.0f, 1024.0f, 8.0f,
                                  [aces]() { return aces->_adaptFloor; },
                                  [aces](float v) { aces->_adaptFloor = v; }));
    }
    // WHICH DIFFUSE LOBE the analytic lights use. Label order is the ordinal order of
    // pbr::DiffuseBrdfModel, so the row's index IS the model — one less mapping to get
    // wrong — while what crosses the wire is the model's crc (SceneGraphSystem's
    // UpdatePbrCommon), because a saved state must survive a renumbering. Unlike the
    // sky/postfx rows this one has no host-side object to poke: the pbr common block is
    // built by the compositor, so it is reached the way time-of-day is, by notify.
    if (has_sgsys) {
      post.push_back(hudEnumProp(
          "diffuse brdf", {"lambert", "oren-nayar", "burley"},
          [&diffuse_brdf_cmd]() { return float(diffuse_brdf_cmd); },
          [&](float v) {
            diffuse_brdf_cmd = int(v);
            controller_ptr_t c;
            sys_ref_t        sgs;
            {
              std::lock_guard<std::mutex> lock(ctl_mutex);
              c   = controller;
              sgs = sgsystem;
            }
            if (c) {
              auto model = pbr::DiffuseBrdfModel(diffuse_brdf_cmd);
              auto tab   = std::make_shared<DataTable>();
              (*tab)["DiffuseBrdfModel"_tok] =
                  uint64_t(CrcString(pbr::diffuseBrdfModelName(model)).hashed());
              c->systemNotify(sgs, "UpdatePbrCommon"_tok, tab);
            }
          }));
    }
    if (not post.empty())
      perfhud.registerEditorPage("POST", post);
    else
      deco::printf(fvec3::Yellow(),
                   "ork.ecs.player: scene declares no aces/hsvg postfx node — no HUD POST page\n");

    ////////////////////////////////////////////////////////
    // SETTINGS — the page of COMMANDS, registered last so the value pages keep their
    // places in the ring. The two commands first: write every edited value on every page
    // to the scene's sidecar (see editor_state.h) for THIS presentation mode, and take the
    // spawn from where the character is standing right now. Then the spawn itself, as four
    // ordinary value rows the same save carries.
    //
    // They are rows and not keys because the pad has to reach them: a headset has no
    // keyboard, and a control that only exists on the desktop is a control the owner
    // cannot use where the grading actually happens.
    ////////////////////////////////////////////////////////
    std::vector<HudEditProp> settings;
    settings.push_back(hudActionProp(
        "save all",
        {"ready", "saved", "refused", "no scene", "failed"},
        [&save_row_state]() { return float(save_row_state.load()); },
        [&]() { do_save_editor_state(); }));
    // ONLY WHEN THERE IS A WALKER. On a scene with no character these rows would have
    // nothing to place and nowhere to read a vantage from — a row that cannot bite.
    if (walker_spawn) {
      settings.push_back(hudActionProp(
          "set spawn here",
          {"ready", "sampling", "set", "no walker", "no answer"},
          [&spawn_row_state]() { return float(spawn_row_state.load()); },
          [&]() {
            // The update thread owns the conversation with the character (it is the thread
            // the request drains on); this only raises the flag, so the row fires the same
            // from the '\' key and from CROSS.
            spawn_row_state.store(SPAWN_SAMPLING);
            spawn_probe_want.store(true);
          }));
      settings.push_back(hudFloatProp("spawn x", -kSpawnXZLimit, kSpawnXZLimit, 1.0f,
                                      [&spawn_x]() { return spawn_x; },
                                      [&spawn_x](float v) { spawn_x = v; }));
      settings.push_back(hudFloatProp("spawn y", -kSpawnYLimit, kSpawnYLimit, 1.0f,
                                      [&spawn_y]() { return spawn_y; },
                                      [&spawn_y](float v) { spawn_y = v; }));
      settings.push_back(hudFloatProp("spawn z", -kSpawnXZLimit, kSpawnXZLimit, 1.0f,
                                      [&spawn_z]() { return spawn_z; },
                                      [&spawn_z](float v) { spawn_z = v; }));
      settings.push_back(hudFloatProp("spawn yaw", -kSpawnYawLimit, kSpawnYawLimit, 0.05f,
                                      [&spawn_yaw]() { return spawn_yaw; },
                                      [&spawn_yaw](float v) { spawn_yaw = v; }));
    }
    perfhud.registerEditorPage("SETTINGS", settings);

    deco::printf(fvec3::Green(),
                 "ork.ecs.player: HUD editor pages: SKY-MAIN<%zu rows> SKY-HAZE<%zu rows> "
                 "CLOUDS<%d rows> POST<%zu rows> SETTINGS<%zu rows> (page ring is now %d long)\n",
                 sky.size(), haze.size(), num_cloud_rows, post.size(), settings.size(),
                 perfhud._pages.numPages());

    // TIER-2 BASELINE, captured the moment every page exists and BEFORE a saved value is
    // applied: this is the scene-authored truth a save measures its diffs against and a
    // load checks for re-authoring.
    //
    // THE COMMAND-MIRROR ROWS (time of day, cloud cover / tile / base) ARE INCLUDED, and the
    // baseline is what makes that safe. Their getters mirror the last command rather than
    // reading the system that owns the value, so the baseline is a fixed, deterministic
    // starting number — which means a row only ever DIFFERS from it after the user moved
    // it, and only a moved row is written. What is saved is therefore always a value the
    // owner actually set, and replaying it is re-issuing that same command. The one thing
    // this cannot do is notice that the SCENE's authored hour changed since the save: with
    // no readback there is nothing to compare, so a saved time of day wins until the row
    // can read the clock instead of remembering it.
    std::vector<std::string> baseline_anomalies;
    editor_baselines = EditorStateIO::capture(perfhud._pages, {}, &baseline_anomalies);
    for (const auto& l : baseline_anomalies)
      deco::printf(fvec3::Red(), "ork.ecs.player: %s\n", l.c_str());
  };

  // --editscript: parse "LABEL:FRAME,..." into scripted HUD EDITOR input fired on the
  // update thread when the tick reaches FRAME. Drives the very calls the cursor keys and
  // the pad make, so a scripted edit and a hand-made one cannot diverge.
  struct EditorScriptEvent {
    enum Action { PAGE, SELECT, ADJUST, COLOR };
    Action      action = PAGE;
    int         arg    = 0; // SELECT/ADJUST: -1 or +1
    int         frame  = 0;
    bool        fired  = false;
    std::string label;
  };
  std::vector<EditorScriptEvent> editor_events;
  if (not editor_script.empty()) {
    std::stringstream ss(editor_script);
    std::string item;
    while (std::getline(ss, item, ',')) {
      auto colon = item.find(':');
      if (colon == std::string::npos)
        continue;
      EditorScriptEvent ese;
      ese.label = item.substr(0, colon);
      ese.frame = atoi(item.substr(colon + 1).c_str());
      for (auto& c : ese.label)
        c = char(std::toupper((unsigned char)c));
      if (ese.label == "PAGE")
        ese.action = EditorScriptEvent::PAGE;
      else if (ese.label == "UP" or ese.label == "DOWN") {
        ese.action = EditorScriptEvent::SELECT;
        ese.arg    = (ese.label == "UP") ? -1 : +1;
      } else if (ese.label == "COLOR") {
        ese.action = EditorScriptEvent::COLOR;
      } else if (ese.label == "LEFT" or ese.label == "RIGHT") {
        ese.action = EditorScriptEvent::ADJUST;
        ese.arg    = (ese.label == "RIGHT") ? +1 : -1;
      } else {
        deco::printf(fvec3::Red(), "ork.ecs.player: --editscript unknown label <%s> (PAGE/UP/DOWN/LEFT/RIGHT)\n",
                     ese.label.c_str());
        continue;
      }
      editor_events.push_back(ese);
    }
    deco::printf(fvec3::Yellow(), "ork.ecs.player: --editscript parsed %zu event(s)\n", editor_events.size());
  }
  int editor_tick = 0;

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
    auto artifacts  = materializeAndWireScene(scenedata, ctx);
    scene_artifacts = artifacts; // the editor pages resolve live materials out of it (CLOUDS)
    if (want_ssaa > 1) { // host display preference -> the SG screen node (link copies userparams)
      for (const auto& it : scenedata->getSystemDatas())
        if (auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second))
          sgd->setUserSceneParam("ssaa", want_ssaa);
      deco::printf(fvec3::Yellow(), "ork.ecs.player: ssaa<%d>\n", want_ssaa);
    }
    ////////////////////////////////////////////
    // --vr: select the VR render model. The preset string FWDPBRVRDM resolves to the
    // single-pass stereo output node (presetForwardPBRSPVR); a device without multiview
    // throws there rather than degrading. The XR device was selected pre-Vulkan from
    // ORKID_VR_DRIVER and, by now (post graphics-init), is _active iff its session came up.
    // Forcing the preset here (a user scene param, like ssaa above) routes BOTH the
    // compositor and the SceneGraphSystem VR-device wiring.
    //
    // The runtime check picks the DEVICE, not the render model: with a live runtime the
    // active XR device drives the HMD; with none, SceneGraphSystem registers a NoVr device
    // and the output node's desktop-mirror blit IS the presentation (side-by-side stereo).
    // --vr therefore always means stereo — a request for VR is never answered with mono.
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
        //  both eyes. The pad bumpers step its page ring, R1 forward / L1 back (see onUpdate).
        perfhud._vrmode = true;
        deco::printf(fvec3::Green(),
                     "ork.ecs.player: --vr ACTIVE — XR runtime up, preset FWDPBRVRDM (single-pass stereo node)\n");
      } else {
        // NO RUNTIME -> NoVR STEREO. Same render model, same preset param; SceneGraphSystem
        //  sees no device that ownsHmdPresentation and registers a NoVrDevice, whose
        //  __compositeStereo is a no-op — the output node's desktop mirror presents both eyes.
        //  Routing through the preset param (rather than an output node reached by hand) keeps
        //  the resolver the single place the VR output node is chosen.
        for (const auto& it : scenedata->getSystemDatas())
          if (auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second))
            sgd->setUserSceneParam("preset", std::string("FWDPBRVRDM"));
        perfhud._vrmode = true;
        deco::printf(fvec3::Yellow(),
                     "ork.ecs.player: --vr: no XR runtime, NoVR stereo — preset FWDPBRVRDM (single-pass stereo node) on a NoVr device\n");
      }
    }
    deco::printf(
        fvec3::Green(),
        "ork.ecs.player: materialized + wired (%zu artifacts)\n",
        artifacts->_themap.size());
    ////////////////////////////////////////////
    // 2b. POST CHAIN SELF-DEFENSE. The HUD POST page edits the reflected "aces" / "hsvg"
    // nodes, and it is now the ONLY tone/grade surface the player has. Most scenes
    // declare one of the pair and not the other (Scene.sky() attaches the tone stage;
    // nothing attaches a grade), which would bring the page up with half its rows and no
    // way to say why. Attach the MISSING half here, default-constructed: HSVG's defaults
    // are an identity grade and ACES' authored exposure is the 1.0 the scene library
    // ships, so an unedited frame is the frame it was. SCENE-AUTHORED NODES WIN — only an
    // absent name is filled, and both calls are idempotent besides.
    // ORDER is append, so a filled-in grade lands AFTER the tone stage. Not a preference:
    // ps_hsvg clamps value to 1, so an identity grade ahead of the tonemap would clip the
    // HDR frame instead of passing it through.
    // BEFORE bind (the reflected _postfx_nodes/_postfx_order are consumed at _onLink),
    // and additive with any scene-declared chain (e.g. "ssss"). NodeCompositor gpuInits
    // registered nodes for us — we never gpuInit these ourselves.
    ////////////////////////////////////////////
    for (const auto& it : scenedata->getSystemDatas()) {
      auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second);
      if (not sgd)
        continue;
      std::string attached;
      auto attach_if_absent = [&](const std::string& name, lev2::compositorpostnode_ptr_t node) {
        if (sgd->_postfx_nodes.count(name))
          return;
        sgd->addPostFxNode(name, node);
        sgd->appendPostFxOrder(name);
        attached += attached.empty() ? name : ("+" + name);
      };
      attach_if_absent("aces", std::make_shared<PostFxNodeACES>());
      attach_if_absent("hsvg", std::make_shared<PostFxNodeHSVG>());
      if (not attached.empty())
        deco::printf(fvec3::Yellow(),
                     "ork.ecs.player: scene declares no <%s> postfx node — "
                     "attached the engine default so the HUD POST page can edit it\n",
                     attached.c_str());
    }
    ////////////////////////////////////////////
    // 2b-2. SCENE IDENTITY + PRESENTATION MODE — what the saved editor values are keyed
    // to. The source is a path TOKEN (<ork_data>/scenes/scn_x.py), so it expands against
    // THIS machine's workspace; the raw token would answer a filesystem test falsely, so
    // nothing may probe it before expandPaths.
    //
    // The MODE is the render model, read back from the preset the arms above resolved —
    // not from want_vr — so a scene that names a VR preset itself lands in the same bucket
    // as --vr does. Both VR preset strings route to the single-pass stereo node.
    ////////////////////////////////////////////
    for (const auto& it : scenedata->getSystemDatas())
      if (auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second)) {
        const auto& up = sgd->userSceneParams();
        auto        pi = up.find("preset");
        if (pi != up.end() and pi->second.isA<std::string>() and
            (pi->second.get<std::string>() == "FWDPBRVRDM" or
             pi->second.get<std::string>() == "FWDPBRSPVR"))
          vr_presentation = true;
      }
    {
      std::string raw = scenedata->_sceneScriptPath.c_str();
      if (raw.empty()) {
        deco::printf(fvec3::Yellow(),
                     "ork.ecs.player: this .ecs carries no scene source — editor values cannot be "
                     "saved or loaded for it (the composer sets SceneData.scene_script_path)\n");
      } else {
        std::string abs = ork::file::expandPaths(raw);
        if (not std::filesystem::exists(abs)) {
          deco::printf(fvec3::Yellow(),
                       "ork.ecs.player: scene source <%s> does not resolve on this machine (<%s>) — "
                       "editor values are not persisted this run\n",
                       raw.c_str(), abs.c_str());
        } else {
          editor_state_path  = EditorStateIO::sidecarPath(abs);
          auto slash         = abs.find_last_of('/');
          editor_state_scene = (slash == std::string::npos) ? abs : abs.substr(slash + 1);
          deco::printf(fvec3::Green(),
                       "ork.ecs.player: scene source <%s> — editor values <%s> mode<%s>\n",
                       editor_state_scene.c_str(), editor_state_path.c_str(),
                       editorStateModeKey(vr_presentation));
        }
      }
    }
    ////////////////////////////////////////////
    // 2b-3. THE WALKER'S SPAWN — the SpawnData whose archetype carries a character
    // controller. Found BEFORE the pages are registered, because the spawn rows seed
    // themselves from the transform it holds: that authored value is the rows' tier 2, and
    // therefore what a stale saved spawn is measured against.
    //
    // The eye height, follow distance and capsule size come from the same archetype: the
    // character answers a camera RAY (an eye and a look), and turning that back into the
    // spawn's own currency — the capsule CENTRE, which is what the transform holds — needs
    // all three.
    ////////////////////////////////////////////
    for (const auto& kv : scenedata->GetSceneObjects()) {
      auto sd = std::dynamic_pointer_cast<SpawnData>(kv.second);
      if (not sd)
        continue;
      auto ccd = sd->typedComponent<CharacterControllerComponentData>();
      if (not ccd)
        continue;
      walker_spawn       = sd;
      spawn_eye_height   = ccd->_eyeHeight;
      spawn_cam_distance = ccd->_camDistance;
      if (auto bod = sd->typedComponent<BulletObjectComponentData>())
        if (auto cap = std::dynamic_pointer_cast<BulletShapeCapsuleData>(bod->_shapedata))
          spawn_half_capsule = 0.5f * cap->mfExtent + cap->mfRadius; // btCapsuleShape total height
      auto xf   = sd->transform();
      spawn_x   = xf->_translation.x;
      spawn_y   = xf->_translation.y;
      spawn_z   = xf->_translation.z;
      auto fwd  = xf->_rotation.transform(fvec3(0, 0, -1)); // the walker's zero heading is -Z
      spawn_yaw = atan2f(fwd.x, -fwd.z);
      deco::printf(fvec3::Green(),
                   "ork.ecs.player: walker <%s> spawns at (%.1f %.1f %.1f) yaw %.3f — the spawn is editable\n",
                   kv.first.c_str(), double(spawn_x), double(spawn_y), double(spawn_z), double(spawn_yaw));
      break;
    }
    ////////////////////////////////////////////
    // 2c. HUD EDITOR PAGES — see register_editor_pages(). AFTER the post-chain fill-in,
    // so the POST page binds whichever chain the frame will actually run.
    ////////////////////////////////////////////
    register_editor_pages();
    ////////////////////////////////////////////
    // 2d. THE SAVED SPAWN, applied BEFORE THE SIMULATION EXISTS. This is the whole reason
    // the spawn is persisted as scene data rather than replayed as a message: written here
    // it is simply where the character is born — no teleport, no physics to fight, no first
    // second of the run spent somewhere the owner did not choose.
    //
    // The file's own anomalies are NOT printed here; the load in updateInit reads the same
    // file and reports every one of them, and saying it twice would only make a duplicated
    // line look like a duplicated fault. What this block reports is its own act.
    ////////////////////////////////////////////
    if (walker_spawn and not editor_state_path.empty()) {
      EditorStateReport rep;
      auto saved = EditorStateIO::readSaved(
          editor_state_path, vr_presentation, perfhud._pages, editor_baselines, rep);
      auto pick = [&saved](const char* id, float& out) -> bool {
        auto it = saved.find(id);
        if (it == saved.end() or it->second._kind != HudEditProp::FLOAT)
          return false;
        out = it->second._f;
        return true;
      };
      float sx = spawn_x, sy = spawn_y, sz = spawn_z, syaw = spawn_yaw;
      bool  moved = pick("spawn x", sx);
      moved       = pick("spawn y", sy) or moved;
      moved       = pick("spawn z", sz) or moved;
      bool turned = pick("spawn yaw", syaw);
      if (moved or turned) {
        auto xf          = walker_spawn->transform();
        xf->_translation = fvec3(sx, sy + kSpawnApplyLift, sz);
        spawn_heading_send = turned ? syaw : 0.0f;
        deco::printf(fvec3::Green(),
                     "ork.ecs.player: saved spawn applied before the simulation — (%.1f %.1f %.1f) "
                     "+%.1fm lift, yaw %.3f (scene authored %.1f %.1f %.1f)\n",
                     double(sx), double(sy), double(sz), double(kSpawnApplyLift), double(syaw),
                     double(spawn_x), double(spawn_y), double(spawn_z));
      }
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
    // the FOLIAGE row's notify target; only ever resolved for a scene that declared
    // visgroups, which is also the only case in which the page exists.
    if (push_foliage_state)
      hypermeshsystem = controller->findSystemWithClassName("HypermeshSystem");
    if (physics_debug) {
      if (bullet_mode) {
        // flag-driven startup enable: the TOGGLE_DEBUG_DRAW path, fired
        // once before the first tick drains — wireframe is up by first-lit (snapshot-gateable).
        controller->systemNotify(bulletsystem, "TOGGLE_DEBUG_DRAW"_tok, std::make_shared<DataTable>());
        deco::printf(fvec3::Green(), "ork.ecs.player: --physics-debug ON (Bullet debug wireframe)\n");
      } else {
        deco::printf(fvec3::Yellow(), "ork.ecs.player: --physics-debug requested but the scene declares no BulletSystem\n");
      }
    }
    ////////////////////////////////////////////
    // SAVED EDITOR VALUES — applied on the simulation's POST-ACTIVATE edge, NOT here.
    //
    // Here is too early, and silently so. A row that writes a live object (the atmosphere,
    // the postfx nodes) takes at any time, but a row that drives a SYSTEM — time of day and
    // the cloud decks, which message the scene's PythonSystem — would reach a system whose
    // SCRIPT DOES NOT EXIST YET: that script's state is built in the python system's
    // CONSTRUCTOR, which the transport FSM runs in the COMPOSE phase, ticks after
    // startSimulation() returns. The message landed on a system with no state to change and
    // the script's own init then adopted the scene's authored clock — the saved hour loaded,
    // reported, and did nothing.
    //
    // POST-ACTIVATE is the first moment every system, script and component in the scene is
    // live and no tick has run yet, so it is the first moment at which "apply the saved
    // values" means ALL of them rather than most of them. One shot per simulation.
    ////////////////////////////////////////////
    {
      auto applied = std::make_shared<bool>(false);
      controller->_onUpdPostActivate.push_back([&, applied](Simulation*) {
        if (*applied)
          return;
        *applied = true;
        if (not editor_state_path.empty()) {
          auto rep = EditorStateIO::load(editor_state_path, vr_presentation, perfhud._pages, editor_baselines);
          for (const auto& l : rep._lines)
            deco::printf(fvec3::Yellow(), "ork.ecs.player: %s\n", l.c_str());
          deco::printf(rep._ok ? fvec3::Green() : fvec3::Red(),
                       "ork.ecs.player: editor values <%s>: %s\n",
                       editorStateModeKey(vr_presentation), rep._summary.c_str());
        }
        // THE RESTORED HEADING, owed to the character since 2d, and owed the same edge: a
        // TurnStep only lands once the character COMPONENT is staged. A fresh character's
        // heading is exactly zero — the capsule is rotation-locked and the controller
        // ignores the spawn rotation — so one discrete step IS the absolute facing. Sent
        // only when a saved yaw survived the re-authoring check, so a scene with no saved
        // spawn behaves as if none of this existed.
        controller_ptr_t c;
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          c = controller;
        }
        send_spawn_heading(c, charsystem);
        // and hand the deck script the state the sky is actually in — the rows hold the
        // scene's launch state with any saved value already applied over it.
        if (push_cloud_state)
          push_cloud_state();
        // and re-assert the chosen foliage set: a fresh simulation staged every member at
        // its DECLARED launch visibility, so without this the scene's default is what is
        // drawn while the row (restored from the saved editor state) claims otherwise.
        if (push_foliage_state)
          push_foliage_state();
      });
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
  // HUD pad controls: host-side rising edges on the two BUMPERS (R1 forward, L1 back —
  //  the pad analogue of ` and SHIFT-`) and on R3 (right stick click, forward). Page 0 in
  //  the ring IS hidden, so there is no separate show/hide. Independent of the pad->python
  //  forwarding below (the bits still forward as GamepadButtons; no scene binds them).
  bool gp_l1_hud_prev = false;
  bool gp_r1_hud_prev = false;
  bool gp_r3_hud_prev = false;
  // HUD EDITOR input (see the key/pad registries at the top of this file). The cursor
  //  LEFT/RIGHT hold state is written on the MAIN thread and read on the update thread,
  //  which is the one that ticks the auto-repeat — hence the atomic. Selection (cursor
  //  up/down, DPAD up/down) is a per-press edge and needs no hold state.
  std::atomic<int> edit_key_dir{0};
  bool gp_dpu_prev = false;
  bool gp_dpd_prev = false;
  bool gp_cross_prev = false;
  double prev_upd_time = -1.0;
  // The bits the HUD EDITOR OWNS while a page is up, masked out of the python forward
  //  (see below). DPAD_UP/DOWN move the selection; CROSS is the activate gesture — and
  //  it is masked for the WHOLE edit window, not only on rows it can activate: a press
  //  that opens a color and also fires the scene's weapon is one button doing two jobs.
  //  Page 0 (no editor page) forwards everything, so fire is untouched where it lives.
  uint32_t gp_hud_mask = 0;
  for (size_t i = 0; i < kNumGamepadButtons; i++)
    if (kGamepadButtonOrder[i] == GamepadButtonId::DPAD_UP or
        kGamepadButtonOrder[i] == GamepadButtonId::DPAD_DOWN or
        kGamepadButtonOrder[i] == GamepadButtonId::CROSS)
      gp_hud_mask |= (1u << i);
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
  // "set spawn here" — the character's camera ray, asked and read on THIS thread (the one
  //  the request drains on, so the answer needs no locking). Update-thread state only.
  response_ref_t spawn_probe_ref;
  bool           spawn_probe_active = false;
  int            spawn_probe_ticks  = 0;
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
        if (not ay_down and abstime >= 1.0) {
          ay_down = true;
          send_key(KEY_CURSOR_RIGHT, 1); // the code the walk input script maps to TurnInput
          deco::printf(fvec3::Yellow(), "ork.ecs.player: AUTOYAW begin\n");
        }
        if (ay_down and not ay_up and abstime >= 1.0 + auto_yaw) {
          ay_up = true;
          send_key(KEY_CURSOR_RIGHT, 0);
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
    //  the L1 toggle. On a keyboardless VR rig this is the only way to raise the HUD.
    bool hud_pad  = perfhud._vrmode.load();
    bool hud_edit = perfhud.editorActive(); // an editor page claims the dpad + triggers
    int  pad_adj_dir = 0;
    if (pysys_mode or hud_pad or hud_edit) {
      if (not gamepad)
        gamepad = GamepadDevice::instance(); // linux: spins up the joydev reader thread
      GamepadState gp = gamepad->sample();
      // HUD EDITOR PAGES: DPAD up/down move the selection (per press edge), L2/R2 adjust
      //  the selected property (held; PerfHud::editAdjustHold owns the repeat). Consumed
      //  host-side ONLY while an editor page is up. The two dpad bits are masked out of
      //  the python forward for exactly that window, so the script sees a clean release
      //  edge on the way in and a fresh press edge on the way out — never a stuck hold.
      //  The triggers need no masking: no scene script binds them (walk_input_system.py's
      //  fire moved to CROSS).
      if (hud_edit and gp.connected) {
        bool du = gp.buttonDown(GamepadButtonId::DPAD_UP);
        bool dd = gp.buttonDown(GamepadButtonId::DPAD_DOWN);
        if (du and not gp_dpu_prev)
          perfhud.editSelect(-1);
        if (dd and not gp_dpd_prev)
          perfhud.editSelect(+1);
        gp_dpu_prev = du;
        gp_dpd_prev = dd;
        // CROSS (PS4 X) is the ACTIVATE gesture: it opens/closes a COLOR row's sub-editor
        //  and FIRES an ACTION row (editSelectedIsActivatable covers both). It never
        //  reaches the scene while a page is up — see gp_hud_mask — so the same press
        //  cannot both work the HUD and shoot.
        bool cx = gp.buttonDown(GamepadButtonId::CROSS);
        if (cx and not gp_cross_prev and perfhud.editSelectedIsActivatable())
          perfhud.editActivate();
        gp_cross_prev = cx;
        if (gp.r2 > 0.35f)
          pad_adj_dir += 1;
        if (gp.l2 > 0.35f)
          pad_adj_dir -= 1;
      } else {
        gp_dpu_prev   = false;
        gp_dpd_prev   = false;
        gp_cross_prev = false;
      }
      // HUD page ring: R3 (right stick click) rising edge, the pad analogue of ` ~ — same
      //  ring, desktop and VR. Consumed HERE; the bit ALSO forwards to python as a
      //  GamepadButton (the right stick is unbound scene-side) — no input conflict.
      {
        bool r3 = gp.connected and gp.buttonDown(GamepadButtonId::R3);
        if (r3 and not gp_r3_hud_prev) {
          perfhud.cyclePage();
          deco::printf(fvec3::Cyan(), "ork.ecs.player: [pad R3] perf HUD page %d\n",
                       perfhud._page.load());
        }
        gp_r3_hud_prev = r3;
      }
      // THE BUMPERS ARE THE PAGE RING, one per direction: R1 steps FORWARD, L1 steps BACK
      //  (the pad twins of ` and SHIFT-`), wrapping through the hidden page 0 the same way
      //  in both directions. Desktop AND VR — in the headset this is the only way to reach
      //  a page. Both are DEDICATED host buttons: no scene script may bind them (jump lives
      //  on TRIANGLE), so a press here has exactly one meaning.
      {
        bool r1 = gp.connected and gp.buttonDown(GamepadButtonId::R1);
        if (r1 and not gp_r1_hud_prev) {
          perfhud.cyclePage(+1);
          deco::printf(fvec3::Cyan(), "ork.ecs.player: [pad R1] perf HUD page<%d>\n",
                       perfhud._page.load());
        }
        gp_r1_hud_prev = r1;
        bool l1 = gp.connected and gp.buttonDown(GamepadButtonId::L1);
        if (l1 and not gp_l1_hud_prev) {
          perfhud.cyclePage(-1);
          deco::printf(fvec3::Cyan(), "ork.ecs.player: [pad L1] perf HUD page<%d>\n",
                       perfhud._page.load());
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
          // button edge transitions (on disconnect gp.buttons==0 releases everything held).
          // While an editor page owns the dpad, its up/down bits read as RELEASED here.
          uint32_t fwd_buttons = hud_edit ? (gp.buttons & ~gp_hud_mask) : gp.buttons;
          uint32_t changed     = fwd_buttons ^ gp_prev_buttons;
          for (size_t i = 0; i < kNumGamepadButtons; i++) {
            uint32_t bit = (1u << i);
            if (not(changed & bit))
              continue;
            auto btntab             = std::make_shared<DataTable>();
            (*btntab)["button"_tok] = std::make_shared<CrcString>(uint64_t(kGamepadButtonOrder[i]));
            (*btntab)["down"_tok]   = int((fwd_buttons & bit) ? 1 : 0);
            c->systemNotify(pysystem, "GamepadButton"_tok, btntab);
            gp_fwd_btn++;
          }
          gp_prev_buttons = fwd_buttons;
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
    // HUD EDITOR held-adjust tick — ONE repeat machine for both sources (cursor keys set
    // edit_key_dir on the main thread, the pad sets pad_adj_dir just above); the keyboard
    // wins a tie because it is the more deliberate input. A slider repeats while held, an
    // enum steps once per press; PerfHud::editAdjustHold owns both policies.
    {
      double dt     = (prev_upd_time < 0.0) ? 0.0 : std::max(0.0, abstime - prev_upd_time);
      prev_upd_time = abstime;
      int dir       = edit_key_dir.load();
      if (dir == 0)
        dir = pad_adj_dir;
      perfhud.editAdjustHold(dir, dt);
    }
    // --editscript (test hook): scripted HUD editor input at update-tick FRAME, through
    // the SAME calls a key or a pad press makes.
    if (not editor_events.empty()) {
      for (auto& ev : editor_events) {
        if (ev.fired or editor_tick < ev.frame)
          continue;
        ev.fired = true;
        switch (ev.action) {
          case EditorScriptEvent::PAGE:
            perfhud.cyclePage();
            break;
          case EditorScriptEvent::SELECT:
            perfhud.editSelect(ev.arg);
            break;
          case EditorScriptEvent::ADJUST:
            perfhud._pages.editAdjust(perfhud._page.load(), ev.arg);
            break;
          case EditorScriptEvent::COLOR:
            perfhud.editActivate();
            break;
        }
        deco::printf(fvec3::Cyan(), "ork.ecs.player: [editscript] %s @ tick %d -> page %d\n",
                     ev.label.c_str(), editor_tick, perfhud._page.load());
      }
      editor_tick++;
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
        // the FOLIAGE row's target moves with the controller like every other system ref;
        // the chosen set is then re-asserted on the fresh simulation's post-activate edge
        // (push_foliage_state), the same place the clone's other host-driven state lands.
        sys_ref_t fresh_hm;
        if (push_foliage_state)
          fresh_hm = fresh->findSystemWithClassName("HypermeshSystem");
        {
          std::lock_guard<std::mutex> lock(ctl_mutex);
          dead_controllers.push_back(controller);
          controller = fresh;
          sgsystem   = fresh_sgsys;
          bulletsystem = fresh_bullet;
          hypermeshsystem = fresh_hm;
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
        // the fresh character's heading is zero again — re-issue the restored facing on the
        // same POST-ACTIVATE edge the first start used (before then there is no staged
        // component to step). A no-op when nothing was restored. The saved VALUES are not
        // re-loaded here: they are already live, and a reload would throw away every edit
        // made since the run began.
        {
          auto sent = std::make_shared<bool>(false);
          fresh->_onUpdPostActivate.push_back([&, sent](Simulation*) {
            if (*sent)
              return;
            *sent = true;
            controller_ptr_t cc;
            {
              std::lock_guard<std::mutex> lock(ctl_mutex);
              cc = controller;
            }
            send_spawn_heading(cc, charsystem);
            // same edge, same reason: the clone staged the foliage sets at their declared
            // launch visibility, so the row's choice has to be re-sent onto it.
            if (push_foliage_state)
              push_foliage_state();
          });
        }
        spawn_probe_active = false; // any in-flight probe belonged to the dead simulation
        deco::printf(fvec3::Green(), "ork.ecs.player: simulation RESTARTED\n");
        break;
      }
      default:
        break;
    }
    ////////////////////////////////////////////
    // "SET SPAWN HERE" — ask the character where it is, then fill the four spawn rows from
    // the answer. Both halves live here because this is the thread the request drains on:
    // the row (fired from the key on the main thread or CROSS on this one) only raises the
    // flag, the ask goes out on the next tick, and the answer is read on the one after.
    // A character that never answers is NAMED — the row would otherwise sit on its last
    // value looking freshly set.
    ////////////////////////////////////////////
    if (spawn_probe_want.exchange(false)) {
      if (walk_mode) {
        spawn_probe_ref    = c->systemRequest(charsystem, CharacterControllerSystem::CameraRay._token,
                                              std::make_shared<DataTable>());
        spawn_probe_ticks  = 0;
        spawn_probe_active = true;
      } else {
        spawn_row_state.store(SPAWN_NOWALKER);
        deco::printf(fvec3::Yellow(), "ork.ecs.player: set spawn here — this scene has no walker to stand anywhere\n");
      }
    }
    if (spawn_probe_active) {
      auto answer = c->systemResponseTable(spawn_probe_ref);
      if (answer) {
        const auto& tab = *answer;
        const fvec3 eye = tab["pos"_tok].get<fvec3>();
        const fvec3 dir = tab["dir"_tok].get<fvec3>();
        const fvec3 up(0, 1, 0);
        // THE RAY IS A CAMERA, THE ROW IS A SPAWN. Step forward along the ray by the follow
        // distance (which puts the horizontal position exactly on the character in both
        // camera modes — the fwd terms cancel), drop the eye height to the feet, then rise
        // by half the capsule to reach the transform's own origin. First person (every
        // terrain walker) is exact in all three axes; a follow camera's Y carries the
        // pitch's share of the orbit, which the ground-snap owns anyway.
        const fvec3 feet   = eye + dir * spawn_cam_distance - up * spawn_eye_height;
        auto        clampf = [](float v, float lim) { return std::min(lim, std::max(-lim, v)); };
        spawn_x   = clampf(feet.x, kSpawnXZLimit);
        spawn_y   = clampf(feet.y + spawn_half_capsule, kSpawnYLimit);
        spawn_z   = clampf(feet.z, kSpawnXZLimit);
        spawn_yaw = clampf(atan2f(dir.x, -dir.z), kSpawnYawLimit);
        spawn_probe_active = false;
        spawn_row_state.store(SPAWN_SET);
        deco::printf(fvec3::Green(),
                     "ork.ecs.player: spawn set to (%.1f %.1f %.1f) yaw %.3f — save all to keep it\n",
                     double(spawn_x), double(spawn_y), double(spawn_z), double(spawn_yaw));
      } else if (++spawn_probe_ticks > kSpawnProbeTicks) {
        spawn_probe_active = false;
        spawn_row_state.store(SPAWN_NOANSWER);
        deco::printf(fvec3::Red(),
                     "ork.ecs.player: set spawn here — the character never answered (no camera ray); "
                     "the spawn rows are unchanged\n");
      }
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
    // '-' / '=' RELEASE always ends the HUD editor's held-adjust — and is never consumed,
    // so the sky clock's own key-up bookkeeping stays whole even if the page opened while
    // the key was already down (its scrub map pops an unheld key safely).
    if (ev->_eventcode == ui::EventCode::KEY_UP and
        (ev->miKeyCode == KEY_EDIT_DEC or ev->miKeyCode == KEY_EDIT_INC))
      edit_key_dir = 0;
    if (ev->_eventcode == ui::EventCode::KEY_DOWN) {
      // '~' / '`' (grave) steps the perf HUD page ring: OFF -> 1 FRAME -> 2 GPU ->
      // 3 PASSES -> 4 CULL -> 5 SYSTEMS -> OFF. Handle it BEFORE the walk/PythonSystem key-forwarding
      // below so the scene can't swallow it.
      if (ev->miKeyCode == '`' or ev->miKeyCode == '~') {
        // SHIFT steps the ring BACKWARD (the same ring, wrapping the same way), so a
        // long ring is reachable from either end without cycling all the way round.
        perfhud.cyclePage(ev->mbSHIFT ? -1 : +1);
        return ui::HandlerResult();
      }
      // HUD EDITOR PAGES (desktop), MODAL — consumed ONLY while an editor page is up.
      // '[' / ']' move the selection; '-' / '=' start the held-adjust the update thread
      // ticks (a repeat here would be the OS's, not ours). THE CURSOR KEYS ARE NOT HERE:
      // they stay the scene's rotation (owner aug08), so you can keep steering while a
      // page is open.
      if (perfhud.editorActive() and not ev->mbSUPER) {
        int kc = ev->miKeyCode;
        if (kc == KEY_EDIT_PREV or kc == KEY_EDIT_NEXT) {
          perfhud.editSelect(kc == KEY_EDIT_PREV ? -1 : +1);
          return ui::HandlerResult();
        }
        if (kc == KEY_EDIT_DEC or kc == KEY_EDIT_INC) {
          edit_key_dir = (kc == KEY_EDIT_INC) ? +1 : -1;
          return ui::HandlerResult();
        }
        // '\' opens/closes the selected COLOR row's H/S/V sub-editor — the desktop echo of
        // the pad's CROSS. Consumed ONLY on a color row, so off one it still reaches the
        // scene's sky-clock pause, like the other four editor keys.
        if (kc == KEY_EDIT_COLOR and perfhud.editSelectedIsActivatable()) {
          perfhud.editActivate();
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
          // STALENESS FIX for the HUD editor pages: the clone deserialized FRESH
          // atmosphere / aces / hsvg instances (all three round-trip through JSON), while
          // the page closures hold the objects resolved at the FIRST bind — so the clone
          // has to run on those instances or every row would silently drive a scene that
          // is no longer rendering. (Byte-compare above already ran on the pristine clone,
          // so this perturbs no audit.)
          for (const auto& it : fresh->getSystemDatas())
            if (auto sgd = std::dynamic_pointer_cast<SceneGraphSystemData>(it.second)) {
              if (hud_atmosphere and sgd->hasUserSceneParam("SkyAtmosphere"))
                sgd->setUserSceneParam("SkyAtmosphere", hud_atmosphere);
              if (hud_aces and sgd->_postfx_nodes.count("aces"))
                sgd->_postfx_nodes["aces"] = hud_aces;
              if (hud_hsvg and sgd->_postfx_nodes.count("hsvg"))
                sgd->_postfx_nodes["hsvg"] = hud_hsvg;
            }
          auto fresh_artifacts = materializeAndWireScene(fresh, ctx);
          scene_artifacts      = fresh_artifacts;
          // the clone's deck materials are NEW objects — re-bind the CLOUDS page's band /
          // radiance generators onto them, or the base row would move the clone's shells
          // while the coverage band it compensates stayed on the discarded material.
          if (bind_cloud_materials)
            bind_cloud_materials(fresh, fresh_artifacts);
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
