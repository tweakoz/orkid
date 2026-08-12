#pragma once
////////////////////////////////////////////////////////////////
// PerfHudPages — the player perf HUD's SLOT REGISTRY and page templates.
//
// Every row is a NAMED SLOT at a fixed position in a page template, and EVERY slot of
// the active page renders every frame. A slot whose sink published nothing this frame
// keeps its last value and gains an age suffix:
//
//     sun-cascade-fit  1.82 @14f
//
// The cadenced rows (sun-cascade-fit / sun-cascade-flip / sky-ibl-snap / hypermesh-gen
// only record on the frames their job actually runs) therefore stay VISIBLE as an age
// instead of disappearing — the cadence is still legible, but the block never re-flows
// under the reader. A slot that has never published reads `--`.
//
// LAYOUT CONSTANCY is the point of the rewrite: a page's line count is fixed (the ECS
// block is fixed the moment the simulation's systems are known), so the desktop
// bottom-anchor and the VR panel RT geometry never move while pages are cycled.
//
// AGE IS COUNTED IN PUBLISHED FRAMES — the frame counter only advances when the HUD is
// actually consuming the sinks (page on, or the stdout dump enabled), so a slot does
// not accumulate age while the HUD is hidden.
//
// EDITOR PAGES ride the same ring behind the fixed ones. A host registers a page as a
// NAME plus an ordered list of property descriptors (label, kind, get/set); the layout
// is derived, so a new page is zero layout code. Their rows are formatted straight from
// the getters in pageLines() rather than through the slot table: an editor row is a LIVE
// value, and an age suffix on one would be a lie. Everything a selection or a value
// change could move is fixed-width — a 2-char cursor column, a %9.4g value field and a
// constant-width slider bar — so the same layout-constancy law the fixed pages hold to
// survives editing. That extends to the COLOR kind, whose H/S/V sub-editor lives ON THE
// ONE ROW: opening it swaps a fixed-width marker and moves a caret, never a line.
//
// Deliberately free of gfx / ezapp includes: the page formatting is a pure function of
// a HudInputs snapshot, unit-tested off-device by ork.ecs/tests/perfhud_pages.cpp.
////////////////////////////////////////////////////////////////

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <ork/lev2/gfx/renderphasestats.h> // CullCounts (the GPU-cull result funnels)
#include <ork/lev2/gfx/gpupassstats.h>     // GpuPassSnapshot (the per-pass GPU timestamps)
#include <ork/lev2/gfx/nvmlstats.h>        // NvmlSnapshot (NVIDIA clocks / power / throttle)
#include <ork/util/crc.h>                  // enum entries carry the engine's crc id (CrcString)

namespace ork::ecs::player {

////////////////////////////////////////////////////////////////
// one frame's worth of everything the pages read, snapshotted by the HUD before it
// touches the slots (so the formatting stays a pure function of this).
////////////////////////////////////////////////////////////////
struct HudInputs {
  double _fps = 0.0;
  double _ups = 0.0;
  float  _frame_ms       = 0.0f;
  float  _frame_ms_max   = 0.0f;
  float  _cpu_record_ms  = 0.0f;
  std::map<std::string, double>               _phases;  // RenderPhaseStats name -> ms (only what ran)
  ork::lev2::CullCounts                       _cull;    // GPU-cull result counts
  std::map<std::string, std::array<double, 3>> _systems; // ECS SystemType -> {update, gpuUpdate, render} ms
  float _root[3] = {0.0f, 0.0f, 0.0f};                   // VR world-root translation
  float _eye[3]  = {0.0f, 0.0f, 0.0f};                   // VR composed center-eye world position
  ork::lev2::GpuPassSnapshot _gpu;                        // per-pass DEVICE time (frame-lagged)
  // Two OPTIONAL blocks on the GPU page. Each carries its own liveness, and a block that
  // is not live contributes NO ROWS at all: XR pacing does not exist in a desktop run and
  // NVIDIA telemetry does not exist on a machine with no NVIDIA driver, so a `--` row
  // there would be an answer to a question nobody asked.
  ork::lev2::VrPacingSnapshot _vr;
  ork::lev2::NvmlSnapshot     _nvml;
};

////////////////////////////////////////////////////////////////

struct HudSlot {
  std::string _text;     // the fully formatted row, WITHOUT the age suffix
  int         _stamp = -1; // publish frame (-1 = never published)
};

// a template entry: a slot reference, an editor property (_edit >= 0), or (empty key,
// _edit < 0) a static line such as a header.
struct HudRow {
  std::string _key;
  std::string _label; // fallback label for a never-published slot / the static text
  bool        _phase = false; // key is a RenderPhaseStats sink name
  int         _edit  = -1;    // index into the owning editor page's property list
};

////////////////////////////////////////////////////////////////
// EDITOR PROPERTY — one live-editable row. The host supplies the accessors; the page
// owns the formatting and the selection. FLOAT is a clamped slider (held = repeat, see
// PerfHud::editAdjustHold); ENUM is an index into _enum, stepped once per press.
////////////////////////////////////////////////////////////////

using HudRGB = std::array<float, 3>; // the color accessors' currency — deliberately NOT
                                     // fvec3, so this header stays math/gfx free

// ONE choice of an ENUM row. The label is what the user reads AND what a saved state
// file writes; the crc is the engine's own identity for that string (ork::CrcString, the
// "..."_crcu machinery), so a reader resolves a saved label through the same hash the
// rest of the engine uses rather than an ad-hoc string compare. The in-memory VALUE of an
// enum row stays the INDEX — that is what the get/set closures traffic in.
struct HudEnumEntry {
  std::string _label;
  uint64_t    _crc = 0;
};

struct HudEditProp {
  // ACTION is a row that DOES something instead of holding a value: it has no slider and
  // no adjust, it fires on the activate gesture ('\' / CROSS — the same one that opens a
  // color), and its _get() reports which of _enum's states to display. That keeps a
  // command (save these values) on the same page ring, in the same fixed-width layout, as
  // the values it acts on.
  enum Kind { FLOAT, ENUM, COLOR, ACTION };
  std::string               _label;
  Kind                      _kind = FLOAT;
  float                     _min = 0.0f, _max = 1.0f, _step = 0.1f; // FLOAT
  std::vector<HudEnumEntry> _enum;              // ENUM choices / ACTION states; value = index
  std::function<float()>     _get;
  std::function<void(float)> _set;
  std::function<HudRGB()>            _getc; // COLOR
  std::function<void(const HudRGB&)> _setc;
  std::function<void()>              _act;  // ACTION
};

inline std::vector<HudEnumEntry> hudEnumEntries(const std::vector<std::string>& labels) {
  std::vector<HudEnumEntry> out;
  out.reserve(labels.size());
  for (const auto& l : labels)
    out.push_back(HudEnumEntry{l, ork::CrcString(l.c_str()).hashed()});
  return out;
}

inline HudEditProp hudFloatProp(
    const char* label,
    float lo,
    float hi,
    float step,
    std::function<float()> get,
    std::function<void(float)> set) {
  HudEditProp p;
  p._label = label;
  p._kind  = HudEditProp::FLOAT;
  p._min   = lo;
  p._max   = hi;
  p._step  = step;
  p._get   = get;
  p._set   = set;
  return p;
}

inline HudEditProp hudEnumProp(
    const char* label,
    std::vector<std::string> labels,
    std::function<float()> get,
    std::function<void(float)> set) {
  HudEditProp p;
  p._label = label;
  p._kind  = HudEditProp::ENUM;
  p._enum  = hudEnumEntries(labels);
  p._get   = get;
  p._set   = set;
  return p;
}

// A COMMAND row. `states` is the vocabulary its _get() indexes (state 0 is what an
// untouched row reads), so the row can say what happened — "saved", "refused" — in the
// same place a value row shows its value, and can never claim to have done something it
// did not do.
inline HudEditProp hudActionProp(
    const char* label,
    std::vector<std::string> states,
    std::function<float()> get,
    std::function<void()> act) {
  HudEditProp p;
  p._label = label;
  p._kind  = HudEditProp::ACTION;
  p._enum  = hudEnumEntries(states);
  p._get   = get;
  p._act   = act;
  return p;
}

inline HudEditProp hudColorProp(
    const char* label,
    std::function<HudRGB()> get,
    std::function<void(const HudRGB&)> set) {
  HudEditProp p;
  p._label = label;
  p._kind  = HudEditProp::COLOR;
  p._getc  = get;
  p._setc  = set;
  return p;
}

////////////////////////////////////////////////////////////////
// HSV <-> RGB, the color rows' working space. Plain math so this header keeps its
// no-dependency property (and so the conversion is unit-testable off-device).
// h in [0,1) and WRAPS; s in [0,1]; v in [0,kHudValueMax].
//
// VALUE IS NOT CAPPED AT 1: the colors these rows edit are scene-referred tints, and a
// real one (scn_forest's haze inscatter) ships at 1.10. Clamping v to 1 would quietly
// darken such a color the moment its row was opened — the editor would be destroying the
// value it exists to show.
////////////////////////////////////////////////////////////////

static constexpr float kHudValueMax = 4.0f;

inline HudRGB hudHSVtoRGB(const HudRGB& hsv) {
  float h = hsv[0] - std::floor(hsv[0]);
  float s = std::min(1.0f, std::max(0.0f, hsv[1]));
  float v = std::min(kHudValueMax, std::max(0.0f, hsv[2]));
  float c = v * s;
  float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
  float m = v - c;
  float r = 0, g = 0, b = 0;
  int   sextant = int(h * 6.0f) % 6;
  switch (sextant) {
    case 0:  r = c; g = x; break;
    case 1:  r = x; g = c; break;
    case 2:  g = c; b = x; break;
    case 3:  g = x; b = c; break;
    case 4:  r = x; b = c; break;
    default: r = c; b = x; break;
  }
  return HudRGB{r + m, g + m, b + m};
}

inline HudRGB hudRGBtoHSV(const HudRGB& rgb) {
  float r = rgb[0], g = rgb[1], b = rgb[2];
  float mx = std::max(r, std::max(g, b));
  float mn = std::min(r, std::min(g, b));
  float d  = mx - mn;
  float h  = 0.0f;
  if (d > 1.0e-8f) {
    if (mx == r)
      h = std::fmod((g - b) / d, 6.0f);
    else if (mx == g)
      h = (b - r) / d + 2.0f;
    else
      h = (r - g) / d + 4.0f;
    h /= 6.0f;
    if (h < 0.0f)
      h += 1.0f;
  }
  return HudRGB{h, (mx > 1.0e-8f) ? (d / mx) : 0.0f, mx};
}

struct HudEditorPage {
  std::string              _name;
  std::vector<HudEditProp> _props;
  int                      _sel = 0; // written by the input threads, read by the draw
  // COLOR sub-editor state. OPEN is an explicit user act (the X button / key) and is what
  //  hands the adjust + select inputs to the H/S/V channels; closed, a color row is inert
  //  and the page navigates as usual. The working HSV is CACHED while open because RGB is
  //  lossy in hue at s=0 or v=0 — round-tripping through the stored color every keystroke
  //  would snap a hue the user is dragging back to zero.
  bool  _open   = false;
  int   _chan   = 0; // 0=H 1=S 2=V
  HudRGB _hsv   = {0.0f, 0.0f, 0.0f};
};

////////////////////////////////////////////////////////////////

struct PerfHudPages {

  enum {
    PAGE_OFF     = 0,
    PAGE_FRAME   = 1,
    PAGE_GPU     = 2, // FRAME then GPU: the two halves of the frame budget sit adjacent,
                      // one step apart from OFF (owner call)
    PAGE_PASSES  = 3,
    PAGE_CULL    = 4,
    PAGE_SYSTEMS = 5,
    NUM_PAGES    = 6, // the BUILT-IN ring (0 = off, so 5 fixed pages). Registered editor
                      // pages extend the ring past this — numPages() is the live length.
  };

  static constexpr int kLabelW    = 16;   // label column — wide enough for "sun-cascade-flip"
  static constexpr int kGpuLabelW = 22;   // the GPU page's own, wider column: pass names are
                                          // rtgroup names ("rtg:spvr.eyeExtractL/R") and two
                                          // distinct passes truncated to the same 16 chars read
                                          // as one row duplicated
  static constexpr int kPanelCols = 52;   // FIXED VR panel width in chars — see maxLineCols()
  static constexpr int kMaxAge    = 9999; // age display clamp
  static constexpr int kBarW      = 12;   // slider bar width, in chars (fixed — see below)
  // The desktop key legend an editor page carries as its last line. Keep it in step with
  // the KEY_EDIT_* constants in the player's main.cpp — this line IS the user's registry.
  // Fits inside pageNominalCols() for an editor page (46 chars), so it cannot widen a panel.
  static constexpr const char* kEditLegend = "` HUD pages  [ ] pick  - = edit  \\ color";

  PerfHudPages() {
    _pages.resize(NUM_PAGES);
    _rebuild();
  }

  //////////////////////////////////////////////////////////////
  // EDITOR PAGE REGISTRATION. Appends a page to the ring and returns its page index.
  // Call before the first draw (the host does it at scene bind, on the render thread,
  // while the update thread is not yet running).
  //////////////////////////////////////////////////////////////

  int registerEditorPage(const std::string& name, std::vector<HudEditProp> props) {
    HudEditorPage ep;
    ep._name  = name;
    ep._props = std::move(props);
    _editors.push_back(std::move(ep));
    _pages.resize(numPages());
    _rebuild(); // page COUNT moved: every header carries it
    return NUM_PAGES + int(_editors.size()) - 1;
  }

  int numPages() const {
    return NUM_PAGES + int(_editors.size());
  }
  bool isEditorPage(int page) const {
    return page >= NUM_PAGES and page < numPages();
  }
  // ONE ring-step rule for every page input (the '~' key, SHIFT-'~', the pad): the ring
  //  wraps through PAGE_OFF in BOTH directions, over the live length (editor pages included).
  int stepPage(int page, int dir) const {
    int n = numPages();
    return ((page + dir) % n + n) % n;
  }
  // page index of the editor page registered under `name`, or -1.
  int findEditorPage(const std::string& name) const {
    for (size_t i = 0; i < _editors.size(); i++)
      if (_editors[i]._name == name)
        return NUM_PAGES + int(i);
    return -1;
  }
  HudEditorPage* editorPage(int page) {
    return isEditorPage(page) ? &_editors[page - NUM_PAGES] : nullptr;
  }
  const HudEditorPage* editorPage(int page) const {
    return isEditorPage(page) ? &_editors[page - NUM_PAGES] : nullptr;
  }

  // move the selection cursor (wraps). Layout does not move: the cursor is its own
  // fixed-width column. With a COLOR row OPEN this walks the H/S/V channels instead —
  // the sub-editor owns up/down until it is closed again.
  void editSelect(int page, int delta) {
    auto* ep = editorPage(page);
    if (not ep or ep->_props.empty())
      return;
    if (ep->_open and ep->_props[ep->_sel]._kind == HudEditProp::COLOR) {
      ep->_chan = ((ep->_chan + delta) % 3 + 3) % 3;
      return;
    }
    int n    = int(ep->_props.size());
    ep->_sel = ((ep->_sel + delta) % n + n) % n;
    ep->_open = false; // leaving a color row closes its sub-editor
  }

  // is the selected property a slider? (the auto-repeat gate: sliders repeat while
  // held, enums step once per press). An OPEN color channel is a slider too.
  bool editSelectedIsSlider(int page) const {
    auto* ep = editorPage(page);
    if (not ep or ep->_props.empty())
      return false;
    auto k = ep->_props[ep->_sel]._kind;
    return k == HudEditProp::FLOAT or (k == HudEditProp::COLOR and ep->_open);
  }

  bool editSelectedIsColor(int page) const {
    auto* ep = editorPage(page);
    if (not ep or ep->_props.empty())
      return false;
    return ep->_props[ep->_sel]._kind == HudEditProp::COLOR;
  }
  bool editColorOpen(int page) const {
    auto* ep = editorPage(page);
    return ep and ep->_open;
  }
  bool editSelectedIsAction(int page) const {
    auto* ep = editorPage(page);
    if (not ep or ep->_props.empty())
      return false;
    return ep->_props[ep->_sel]._kind == HudEditProp::ACTION;
  }
  // does the activate gesture ('\' / CROSS) DO anything on the selected row? The host
  // consumes the key only when this is true, so off such a row it still reaches the scene.
  bool editSelectedIsActivatable(int page) const {
    return editSelectedIsColor(page) or editSelectedIsAction(page);
  }

  // the activate gesture, dispatched by kind: a COLOR row opens/closes its sub-editor, an
  // ACTION row FIRES. One gesture, one meaning per row — and exactly one fire per press
  // (the caller is edge-driven), which is what keeps a command row from repeating under a
  // held key the way a slider deliberately does.
  void editActivate(int page) {
    auto* ep = editorPage(page);
    if (not ep or ep->_props.empty())
      return;
    auto& P = ep->_props[ep->_sel];
    if (P._kind == HudEditProp::ACTION) {
      if (P._act)
        P._act();
      return;
    }
    editToggleColor(page);
  }

  // X button / key: open or close the selected color row's H/S/V sub-editor. A no-op on
  // any other kind — the button simply does not apply there.
  void editToggleColor(int page) {
    auto* ep = editorPage(page);
    if (not ep or ep->_props.empty())
      return;
    auto& P = ep->_props[ep->_sel];
    if (P._kind != HudEditProp::COLOR)
      return;
    ep->_open = not ep->_open;
    if (ep->_open) {
      ep->_chan = 0;
      ep->_hsv  = hudRGBtoHSV(P._getc ? P._getc() : HudRGB{0, 0, 0}); // seed from the live color
    }
  }

  // one adjust step on the selected property, dir = -1 / +1.
  void editAdjust(int page, int dir) {
    auto* ep = editorPage(page);
    if (not ep or ep->_props.empty() or dir == 0)
      return;
    auto& P = ep->_props[ep->_sel];
    if (P._kind == HudEditProp::ACTION)
      return; // a command has no value to step — it fires on activate, or not at all
    if (P._kind == HudEditProp::COLOR) {
      // closed = inert: a color is edited through its sub-editor, never by accident
      if (not ep->_open or not P._setc)
        return;
      constexpr float kColorStep = 0.01f;
      float&          ch         = ep->_hsv[ep->_chan];
      ch += kColorStep * float(dir);
      if (ep->_chan == 0)
        ch -= std::floor(ch); // HUE WRAPS — it is an angle
      else
        ch = std::min((ep->_chan == 2) ? kHudValueMax : 1.0f, std::max(0.0f, ch));
      P._setc(hudHSVtoRGB(ep->_hsv));
      return;
    }
    if (not P._get or not P._set)
      return;
    if (P._kind == HudEditProp::ENUM) {
      int n = int(P._enum.size());
      if (n <= 0)
        return;
      int idx = int(std::lround(P._get()));
      idx     = ((idx + dir) % n + n) % n;
      P._set(float(idx));
    } else {
      float v = P._get() + P._step * float(dir);
      P._set(std::min(P._max, std::max(P._min, v)));
    }
  }

  //////////////////////////////////////////////////////////////
  // page templates. A NEW render phase needs a row HERE to appear on the HUD —
  // unlisted sink names are intentionally dropped (the old alpha-order tail is what
  // made the block re-flow).
  //////////////////////////////////////////////////////////////

  void _rebuild() {
    auto R  = [](const char* key, const char* label) { return HudRow{key, label, false, -1}; };
    auto PH = [](const char* name) { return HudRow{name, name, true, -1}; };
    auto S  = [](const char* text) { return HudRow{"", text, false, -1}; };
    // page header, carrying the LIVE ring length — registering an editor page renumbers
    // every page's "[n/N]" in one place.
    auto HDR = [this](int page, const char* name) {
      char hb[64];
      snprintf(hb, sizeof(hb), "[%d/%d] %s", page, numPages() - 1, name);
      return HudRow{"", hb, false, -1};
    };

    _pages.resize(numPages());

    _pages[PAGE_FRAME] = {
        HDR(PAGE_FRAME, "FRAME"),
        R("fps", "FPS"),
        R("ups", "UPS"),
        R("frame", "frame"),
        R("cpu-record", "cpu-record"),
        PH("gpuUpdate"),
        PH("present-idle"),
    };

    // pipeline order: the shadow-maps..env-probes block is the forward node's frame
    // prologue, which runs INSIDE assemble (a breakdown of that row).
    _pages[PAGE_PASSES] = {
        HDR(PAGE_PASSES, "PASSES"),
        PH("preRender"),
        PH("assemble"),
        PH("composite"),
        PH("shadow-maps"),
        PH("shadowCull"),
        PH("sun-cookie"),
        PH("sun-cascades"),
        PH("sun-cascade-fit"),
        PH("sun-cascade-flip"),
        PH("sky-lut"),
        PH("sky-ibl"),
        PH("sky-ibl-snap"),
        PH("env-probes"),
    };

    _pages[PAGE_CULL] = {
        HDR(PAGE_CULL, "CULL"),
        PH("terrain-cull"),
        PH("compute-cull"),
        PH("hm-cull"),
        PH("hm-shadowcull"),
        R("terr-frus", "TERR frus"),
        R("terr-occl", "TERR occl"),
        R("hypm-frus", "HYPM frus"),
        R("hypm-occl", "HYPM occl"),
        R("grass-wg", "grass"),
    };

    _pages[PAGE_SYSTEMS].clear();
    _pages[PAGE_SYSTEMS].push_back(HDR(PAGE_SYSTEMS, "SYSTEMS"));
    { // same field widths as the rows below so the columns line up
      char hb[64];
      snprintf(hb, sizeof(hb), "%-12.12s %5s %5s %5s (ms)", "ECS", "updat", "gpuup", "rendr");
      _pages[PAGE_SYSTEMS].push_back(S(hb));
    }
    for (const auto& n : _ecs_names)
      _pages[PAGE_SYSTEMS].push_back(HudRow{"ecs:" + n, _ecsLabel(n), false, -1});
    _pages[PAGE_SYSTEMS].push_back(R("root", "root"));
    _pages[PAGE_SYSTEMS].push_back(R("eye", "eye"));
    _pages[PAGE_SYSTEMS].push_back(PH("hypermesh-gen"));

    // DEVICE-measured per-pass time (GpuPassStats). The row SET grows as names first
    // appear (a cadenced pass — env probes, an XR blit before the headset is up — has
    // no measurement to show until it runs once) and is ORDERED, at each such growth,
    // by descending EMA. Ordering is NOT redone per frame on purpose: this page obeys
    // the same layout-constancy law as the others, and a table that re-sorted itself
    // every frame would swap rows under the reader mid-read.
    _pages[PAGE_GPU].clear();
    _pages[PAGE_GPU].push_back(HDR(PAGE_GPU, "GPU"));
    _pages[PAGE_GPU].push_back(R("gpu:frame", "frame"));
    _pages[PAGE_GPU].push_back(R("gpu:state", "state"));
    // the two optional blocks, ABOVE the pass rows and each fixed-size once it appears:
    // the deadline the passes are spent against, then what the device was clocked at
    // while spending them.
    if (_vr_bound) {
      _pages[PAGE_GPU].push_back(R("vr:wait", "vr:wait"));
      _pages[PAGE_GPU].push_back(R("vr:period", "vr:period"));
      _pages[PAGE_GPU].push_back(R("vr:missed", "vr:missed"));
      _pages[PAGE_GPU].push_back(R("vr:head", "vr:head"));
    }
    if (_nvml_bound) {
      _pages[PAGE_GPU].push_back(R("nv:clk", "nv:clk"));
      _pages[PAGE_GPU].push_back(R("nv:util", "nv:util"));
      _pages[PAGE_GPU].push_back(R("nv:pwr", "nv:pwr"));
      _pages[PAGE_GPU].push_back(R("nv:temp", "nv:temp"));
      _pages[PAGE_GPU].push_back(R("nv:throttle", "nv:throttle"));
    }
    for (const auto& n : _gpu_names)
      _pages[PAGE_GPU].push_back(HudRow{"gpu:" + n, n, false, -1});

    // the registered editor pages: header + one row per property, in declared order,
    // closed by the KEY LEGEND — the desktop mapping is modal and borrowed, so the page
    // that borrows the keys is the page that says which ones. A page with no properties
    // takes no keys and gets no legend.
    for (size_t e = 0; e < _editors.size(); e++) {
      int page = NUM_PAGES + int(e);
      _pages[page].clear();
      _pages[page].push_back(HDR(page, _editors[e]._name.c_str()));
      for (size_t i = 0; i < _editors[e]._props.size(); i++)
        _pages[page].push_back(HudRow{"", "", false, int(i)});
      if (not _editors[e]._props.empty())
        _pages[page].push_back(S(kEditLegend));
    }

    _phasekeys.clear();
    for (int p = 1; p < NUM_PAGES; p++)
      for (const auto& r : _pages[p])
        if (r._phase)
          _phasekeys.insert(r._key);
  }

  // strip the common "System" suffix for width (the label column is 12 wide)
  static std::string _ecsLabel(const std::string& sysname) {
    std::string       name = sysname;
    const std::string suf  = "System";
    if (name.size() > suf.size() and name.compare(name.size() - suf.size(), suf.size(), suf) == 0)
      name.resize(name.size() - suf.size());
    return name;
  }

  //////////////////////////////////////////////////////////////
  // refresh the slots from one frame's sinks. Slots whose sink is silent are left
  // alone — pageLines() renders them with their age.
  //////////////////////////////////////////////////////////////

  void publish(const HudInputs& in) {
    _frame++;
    char b[256];
    auto set = [&](const std::string& key, const char* text) {
      auto& s  = _slots[key];
      s._text  = text;
      s._stamp = _frame;
    };

    snprintf(b, sizeof(b), "%-*s %8.1f", kLabelW, "FPS", in._fps);
    set("fps", b);
    snprintf(b, sizeof(b), "%-*s %8.1f", kLabelW, "UPS", in._ups);
    set("ups", b);
    snprintf(b, sizeof(b), "%-*s %6.2f ms (max %5.2f)", kLabelW, "frame", in._frame_ms, in._frame_ms_max);
    set("frame", b);
    snprintf(b, sizeof(b), "%-*s %6.2f ms", kLabelW, "cpu-record", in._cpu_record_ms);
    set("cpu-record", b);

    for (const auto& kv : in._phases) {
      if (_phasekeys.find(kv.first) == _phasekeys.end())
        continue; // no slot for it — a new phase gets a row in _rebuild()
      snprintf(b, sizeof(b), "%-*s %6.2f", kLabelW, kv.first.c_str(), kv.second);
      set(kv.first, b);
    }

    // GPU-cull result funnels. Two SHORT lines each (the VR lens blurs long lines at
    // TWO lines per cull consumer (short lines beat one wide funnel), with ONE key
    // dialect for every line: in<entered this stage> out<survived it>. The occl
    // line's out<> is what draws; culled amounts are the in-out differences. HYPM's
    // frus line alone carries var<> (its instanced variant count, fixed per scene);
    // grass has no per-tile funnel readback, only its emitted-workgroup total.
    const auto& c = in._cull;
    if (c.terrain_valid) {
      snprintf(b, sizeof(b), "%-*s in<%u> out<%u>", kLabelW, "TERR frus", c.t_total, c.t_frustum);
      set("terr-frus", b);
      snprintf(b, sizeof(b), "%-*s in<%u> out<%u>", kLabelW, "TERR occl", c.t_frustum, c.t_visible);
      set("terr-occl", b);
    }
    if (c.hyper_valid) {
      snprintf(b, sizeof(b), "%-*s in<%llu> out<%llu> var<%d>", kLabelW, "HYPM frus",
               (unsigned long long)c.h_total, (unsigned long long)c.h_frustum, c.h_variants);
      set("hypm-frus", b);
      snprintf(b, sizeof(b), "%-*s in<%llu> out<%llu>", kLabelW, "HYPM occl",
               (unsigned long long)c.h_frustum, (unsigned long long)c.h_visible);
      set("hypm-occl", b);
    }
    if (c.grass_valid) {
      snprintf(b, sizeof(b), "%-*s wg<%u>", kLabelW, "grass", c.g_workgroups);
      set("grass-wg", b);
    }

    // ECS systems: the row SET is frozen the first time the simulation reports any
    // (every system ticks in the update loop, so the first non-empty snapshot names
    // them all); membership never changes after that, so P4's height is fixed.
    if (not _ecs_bound and not in._systems.empty()) {
      for (const auto& kv : in._systems)
        _ecs_names.push_back(kv.first);
      _ecs_bound = true;
      _rebuild();
    }
    for (const auto& n : _ecs_names) {
      auto it = in._systems.find(n);
      if (it == in._systems.end())
        continue;
      const auto& a = it->second;
      // %-12.12s = left-justified, TRUNCATED to exactly 12 so long names like
      // CharacterController can't push the value columns out of alignment.
      snprintf(b, sizeof(b), "%-12.12s %5.3f %5.3f %5.3f", _ecsLabel(n).c_str(), a[0], a[1], a[2]);
      set("ecs:" + n, b);
    }

    // VR camera diagnostics: the world-root translation and the composed center-eye
    // world position the VR node used. root=(0,0,0) is the vrroot-lookup-miss tell;
    // non-VR these read zeros (the VR node is the only writer).
    snprintf(b, sizeof(b), "%-*s %8.1f %8.1f %8.1f", kLabelW, "root", in._root[0], in._root[1], in._root[2]);
    set("root", b);
    snprintf(b, sizeof(b), "%-*s %8.1f %8.1f %8.1f", kLabelW, "eye", in._eye[0], in._eye[1], in._eye[2]);
    set("eye", b);

    // GPU page. gpu:frame is the whole-frame device span (MT0); every other row is one
    // pass name's summed segments. The numbers are LAG frames old by construction (the
    // readback never waits on the GPU) and the state row says so, next to whether the
    // device correlates its clock with the CPU's ("uncal" = it does not) and any slices
    // the frame's slot budget could not hold.
    {
      const auto& g = in._gpu;
      if (g._gpu_frame_ms >= 0.0f)
        snprintf(b, sizeof(b), "%-*s %6.2f", kGpuLabelW, "frame", g._gpu_frame_ms);
      else
        snprintf(b, sizeof(b), "%-*s %6s", kGpuLabelW, "frame", "--");
      set("gpu:frame", b);
      snprintf(b, sizeof(b), "%-*s lag%d %s drop<%d>", kGpuLabelW, "state", g._lag_frames,
               g._calibrated ? "cal" : "uncal", g._dropped);
      set("gpu:state", b);
      // OPTIONAL BLOCKS. Each binds ONCE, the first time its sink reports live, and never
      // unbinds — a headset or a driver that answered once does not stop existing, and a
      // row set that came and went would re-flow the page under the reader.
      const auto& v = in._vr;
      if (v._live and not _vr_bound) {
        _vr_bound = true;
        _rebuild();
      }
      if (_vr_bound) {
        snprintf(b, sizeof(b), "%-*s %6.2f ema %6.2f", kGpuLabelW, "vr:wait", v._wait_ms, v._wait_ms_ema);
        set("vr:wait", b);
        double hz = (v._period_ms > 0.0) ? (1000.0 / v._period_ms) : 0.0;
        snprintf(b, sizeof(b), "%-*s %6.2f ms (%5.1f Hz)", kGpuLabelW, "vr:period", v._period_ms, hz);
        set("vr:period", b);
        snprintf(b, sizeof(b), "%-*s %6d /s", kGpuLabelW, "vr:missed", v._missed_1s);
        set("vr:missed", b);
        // headroom needs a GPU reading to subtract; without one it says so.
        if (v._headroom_valid)
          snprintf(b, sizeof(b), "%-*s %+6.2f ms (tgt %4.1f)", kGpuLabelW, "vr:head", v._headroom_ms, v._target_ms);
        else
          snprintf(b, sizeof(b), "%-*s %6s ms (tgt %4.1f)", kGpuLabelW, "vr:head", "--", v._target_ms);
        set("vr:head", b);
      }
      const auto& nv = in._nvml;
      if (nv._live and not _nvml_bound) {
        _nvml_bound = true;
        _rebuild();
      }
      if (_nvml_bound) {
        snprintf(b, sizeof(b), "%-*s sm<%d> mem<%d> MHz", kGpuLabelW, "nv:clk", nv._sm_mhz, nv._mem_mhz);
        set("nv:clk", b);
        snprintf(b, sizeof(b), "%-*s gpu<%d%%> mem<%d%%>", kGpuLabelW, "nv:util", nv._util_gpu, nv._util_mem);
        set("nv:util", b);
        snprintf(b, sizeof(b), "%-*s %6.1f W", kGpuLabelW, "nv:pwr", nv._power_w);
        set("nv:pwr", b);
        snprintf(b, sizeof(b), "%-*s %6d C", kGpuLabelW, "nv:temp", nv._temp_c);
        set("nv:temp", b);
        // TRUNCATED: a GPU limited by everything at once would otherwise widen the panel
        // (the first tags are the ones that explain a lost clock).
        snprintf(b, sizeof(b), "%-*s %.24s", kGpuLabelW, "nv:throttle", nv._throttle.c_str());
        set("nv:throttle", b);
      }
      // grow (never shrink) the row set, then re-order it by EMA — both only when a NAME
      // appears that has no row yet.
      bool grew = false;
      for (const auto& kv : g._passes)
        if (_gpu_seen.insert(kv.first).second)
          grew = true;
      if (grew) {
        _gpu_names.assign(_gpu_seen.begin(), _gpu_seen.end());
        std::sort(_gpu_names.begin(), _gpu_names.end(), [&](const std::string& a, const std::string& c) {
          auto ita = g._passes.find(a);
          auto itc = g._passes.find(c);
          double ma = (ita != g._passes.end()) ? ita->second._ms_ema : 0.0;
          double mc = (itc != g._passes.end()) ? itc->second._ms_ema : 0.0;
          return (ma != mc) ? (ma > mc) : (a < c);
        });
        _rebuild();
      }
      for (const auto& kv : g._passes) {
        snprintf(b, sizeof(b), "%-*.*s %6.2f ema %6.2f x%d", kGpuLabelW, kGpuLabelW, kv.first.c_str(),
                 kv.second._ms, kv.second._ms_ema, kv.second._segments);
        set("gpu:" + kv.first, b);
      }
    }
  }

  //////////////////////////////////////////////////////////////

  std::vector<std::string> pageLines(int page) const {
    std::vector<std::string> out;
    if (page <= PAGE_OFF or page >= numPages())
      return out;
    char b[256];
    for (const auto& r : _pages[page]) {
      if (r._edit >= 0) { // editor property — formatted LIVE off its getter
        out.push_back(_editLine(page, r._edit));
        continue;
      }
      if (r._key.empty()) { // static line (page header / column header)
        out.push_back(r._label);
        continue;
      }
      auto it = _slots.find(r._key);
      if (it == _slots.end() or it->second._stamp < 0) {
        snprintf(b, sizeof(b), "%-*s %6s", kLabelW, r._label.c_str(), "--");
        out.push_back(b);
        continue;
      }
      int age = _frame - it->second._stamp;
      if (age <= 0)
        out.push_back(it->second._text);
      else {
        snprintf(b, sizeof(b), " @%df", std::min(age, kMaxAge));
        out.push_back(it->second._text + b);
      }
    }
    return out;
  }

  std::string pageText(int page) const {
    std::string out;
    for (const auto& l : pageLines(page)) {
      if (not out.empty())
        out += "\n";
      out += l;
    }
    return out;
  }

  // ONE editor row, cursor column included. EVERY field is fixed-width: the cursor is
  //  2 chars ("> " / "  "), the value is %9.4g and the bar is kBarW between brackets, so
  //  neither moving the selection nor changing a value can shift a character.
  std::string _editLine(int page, int idx) const {
    char        b[256];
    const auto* ep = editorPage(page);
    if (not ep or idx < 0 or idx >= int(ep->_props.size()))
      return std::string();
    const auto& P   = ep->_props[idx];
    const char* cur = (ep->_sel == idx) ? "> " : "  ";
    if (P._kind == HudEditProp::COLOR) {
      // ONE row, always. The sub-editor never adds or removes a line: opening it only
      //  swaps the trailing marker and moves the channel caret, both fixed-width, so a
      //  color row costs exactly what any other row costs.
      bool  live = (ep->_sel == idx) and ep->_open;
      HudRGB hsv = live ? ep->_hsv : hudRGBtoHSV(P._getc ? P._getc() : HudRGB{0, 0, 0});
      auto  caret = [&](int c) { return (live and ep->_chan == c) ? '*' : ' '; };
      snprintf(b, sizeof(b), "%s%-*s H%c%4.2f S%c%4.2f V%c%4.2f %-6s", cur, kLabelW, P._label.c_str(),
               caret(0), hsv[0], caret(1), hsv[1], caret(2), hsv[2], live ? " edit" : "  x");
      return b;
    }
    float v = P._get ? P._get() : 0.0f;
    if (P._kind == HudEditProp::ENUM or P._kind == HudEditProp::ACTION) {
      int n   = int(P._enum.size());
      int sel = n ? std::min(n - 1, std::max(0, int(std::lround(v)))) : 0;
      // An ACTION says how to fire it where an ENUM shows its position in the list — the
      // two are the same width by construction, so a command row costs a value row.
      char tail[8];
      if (P._kind == HudEditProp::ACTION)
        snprintf(tail, sizeof(tail), "  [\\]");
      else
        snprintf(tail, sizeof(tail), "%2d/%-2d", sel + 1, n);
      snprintf(b, sizeof(b), "%s%-*s %-14.14s %-5.5s", cur, kLabelW, P._label.c_str(),
               n ? P._enum[sel]._label.c_str() : "--", tail);
      return b;
    }
    float t = (P._max > P._min) ? (v - P._min) / (P._max - P._min) : 0.0f;
    t       = std::min(1.0f, std::max(0.0f, t));
    char bar[kBarW + 1];
    int  fill = int(std::lround(t * float(kBarW)));
    for (int i = 0; i < kBarW; i++)
      bar[i] = (i < fill) ? '#' : '-';
    bar[kBarW] = 0;
    // WIDTH-BOUNDED value text. "%9.4g" is exactly 9 columns for every value the grading
    // rows hold, but a WORLD COORDINATE (the spawn rows) crosses into exponent form at 1e4
    // and takes a tenth — and one extra column widens the backdrop the moment the value
    // passes 9999, which is precisely the layout drift these pages exist to forbid. Drop a
    // significant digit rather than a column; every value that already fit is unchanged.
    char vt[32];
    snprintf(vt, sizeof(vt), "%.4g", double(v));
    if (strlen(vt) > 9)
      snprintf(vt, sizeof(vt), "%.3g", double(v));
    if (strlen(vt) > 9)
      snprintf(vt, sizeof(vt), "%.2g", double(v));
    snprintf(b, sizeof(b), "%s%-*s %9s [%s]", cur, kLabelW, P._label.c_str(), vt, bar);
    return b;
  }

  int pageLineCount(int page) const {
    if (page <= PAGE_OFF or page >= numPages())
      return 0;
    return int(_pages[page].size());
  }

  // the VR panel RT is sized from these ONCE-PER-BIND constants, so cycling pages can
  // never resize the head-locked panel.
  int maxLineCount() const {
    int m = 0;
    for (int p = 1; p < numPages(); p++)
      m = std::max(m, pageLineCount(p));
    return m;
  }
  static int maxLineCols() {
    return kPanelCols;
  }

  // Per-page NOMINAL column width, for centering the block inside the fixed VR panel.
  //  CONSTANTS by design: centering on measured line widths would shift the block as
  //  value digits change ("1.9" -> "12.3", "@9f" -> "@120f") — these are hand-sized to
  //  each page's typical widest line and never move. kPanelCols stays the PANEL's
  //  (allocation) width; this is the CONTENT's.
  int pageNominalCols(int page) const {
    // an editor row is EXACTLY 2 + kLabelW + 1 + 9 + 1 + (kBarW+2) wide by construction,
    // and a COLOR row is 2 + kLabelW + 1 + 20 + 1 + 6 — the wider of the two governs.
    if (isEditorPage(page))
      return 2 + kLabelW + 1 + std::max(9 + 1 + kBarW + 2, 20 + 1 + 6);
    switch (page) {
      case PAGE_FRAME:   return 28; // "frame ms      16.62 (18.20)"
      case PAGE_GPU:     return 48; // "rtg:spvr.eyeExtractL     0.31 ema   0.30 x1"
      case PAGE_PASSES:  return 32; // "sun-cascade-fit   2.28 @999f"
      case PAGE_CULL:    return 44; // "HYPM frus  in<215378> out<5407> var<18>"
      case PAGE_SYSTEMS: return 40; // "SceneGraphSystem  0.120 0.340 0.010 (ms)"
      default:           return kPanelCols;
    }
  }

  //////////////////////////////////////////////////////////////

  int                            _frame      = 0;
  bool                           _ecs_bound  = false;
  bool                           _vr_bound   = false; // XR pacing rows exist (once live)
  bool                           _nvml_bound = false; // NVIDIA telemetry rows exist (once live)
  std::vector<std::string>       _ecs_names; // frozen SystemType list (P4's row set)
  std::set<std::string>          _gpu_seen;  // every GPU pass name ever measured
  std::vector<std::string>       _gpu_names; // the GPU page's row set, in display order
  std::map<std::string, HudSlot> _slots;
  std::set<std::string>          _phasekeys;
  std::vector<std::vector<HudRow>> _pages;   // NUM_PAGES fixed + one per registered editor
  std::vector<HudEditorPage>       _editors;
};

} // namespace ork::ecs::player
