////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
// LAYOUT-CONSTANCY gate for the player perf HUD's slot registry (perfhud_pages.h).
//
// The HUD it replaced concatenated whatever the sinks happened to publish, so the block
// changed height and rows changed position every time a cadenced phase (sun-cascade-fit,
// sky-ibl-snap, hypermesh-gen) ran or didn't. These tests pin the properties that fix:
//   * a page's line count is the SAME whether every sink published or none did,
//   * a silent slot keeps its last value and gains an age suffix (never disappears),
//   * a never-published slot renders `--` in its own fixed position,
//   * slot ORDER is stable across publish patterns,
//   * the PAGE order is the owner's, and the ring steps both ways through it,
//   * the ECS row set freezes at the first non-empty systems snapshot,
//   * the GPU page's row set only GROWS (a pass first measured later gets a row) and its
//     order is fixed at each growth, never re-sorted per frame under the reader.
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <utpp/UnitTest++.h>

#include "../examples/c++/player/perfhud_pages.h"

using namespace ork;
using namespace ork::ecs::player;

namespace {

// every sink reporting: all phases the pages know about, both cull funnels, grass, 2 systems.
HudInputs fullInputs() {
  HudInputs in;
  in._fps           = 59.9;
  in._ups           = 480.1;
  in._frame_ms      = 16.62f;
  in._frame_ms_max  = 18.20f;
  in._cpu_record_ms = 2.31f;
  const char* phases[] = {
      "gpuUpdate",    "present-idle",     "preRender",      "assemble",     "composite",
      "shadow-maps",  "shadowCull",       "sun-cookie",     "sun-cascades", "sun-cascade-fit",
      "sun-cascade-flip", "sky-lut",      "sky-ibl",        "sky-ibl-snap", "env-probes",
      "terrain-cull", "compute-cull",     "hm-cull",        "hm-shadowcull", "hypermesh-gen"};
  double ms = 0.1;
  for (const char* p : phases)
    in._phases[p] = (ms += 0.05);
  in._cull.terrain_valid = true;
  in._cull.t_total       = 1024;
  in._cull.t_frustum     = 300;
  in._cull.t_visible     = 211;
  in._cull.hyper_valid   = true;
  in._cull.h_variants    = 3;
  in._cull.h_total       = 40000;
  in._cull.h_frustum     = 12000;
  in._cull.h_visible     = 9000;
  in._cull.h_occluded    = 3000;
  in._cull.grass_valid   = true;
  in._cull.g_workgroups  = 8123;
  in._systems["SceneGraphSystem"]        = {0.120, 0.340, 0.010};
  in._systems["CharacterControllerSystem"] = {0.220, 0.000, 0.000};
  in._root[1] = 3.0f;
  in._eye[1]  = 4.5f;
  // per-pass DEVICE time (GpuPassStats), as the timestamp readback publishes it
  in._gpu._gpu_frame_ms = 9.75f;
  in._gpu._lag_frames   = 2;
  in._gpu._calibrated   = true;
  in._gpu._passes["rtg:fwd-color"] = {6.10, 6.05, 1};
  in._gpu._passes["rtg:shadow"]    = {2.20, 2.15, 5};
  in._gpu._passes["xr:blit"]       = {0.90, 0.88, 2};
  in._gpu._passes["cull:perview"]  = {0.40, 0.38, 1};
  return in;
}

// the cadenced case: only the every-frame phases report (the sinks a real idle frame has).
HudInputs sparseInputs() {
  HudInputs in;
  in._fps            = 60.0;
  in._ups            = 479.0;
  in._phases["gpuUpdate"]    = 1.0;
  in._phases["present-idle"] = 11.0;
  in._phases["assemble"]     = 4.0;
  return in;
}

// the fixed pages, and the map from a page to its slot in the allPageCounts / rowHeads
// vectors (which start at PAGE_FRAME — page 0 is OFF and has no template). Expectations
// are written against these, never against a literal ring position: reordering the ring
// is one enum edit, and a test that hard-coded "page 2" would pass on the wrong page.
constexpr int PAGE_FRAME   = PerfHudPages::PAGE_FRAME;
constexpr int PAGE_GPU     = PerfHudPages::PAGE_GPU;
constexpr int PAGE_PASSES  = PerfHudPages::PAGE_PASSES;
constexpr int PAGE_CULL    = PerfHudPages::PAGE_CULL;
constexpr int PAGE_SYSTEMS = PerfHudPages::PAGE_SYSTEMS;

constexpr int idx(int page) {
  return page - PerfHudPages::PAGE_FRAME;
}

std::vector<int> allPageCounts(const PerfHudPages& pages) {
  std::vector<int> out;
  for (int p = PerfHudPages::PAGE_FRAME; p < PerfHudPages::NUM_PAGES; p++)
    out.push_back(pages.pageLineCount(p));
  return out;
}

// the leading label token of each row (order stability probe)
std::vector<std::string> rowHeads(const PerfHudPages& pages, int page) {
  std::vector<std::string> out;
  for (const auto& l : pages.pageLines(page)) {
    auto sp = l.find_first_of(' ');
    out.push_back(sp == std::string::npos ? l : l.substr(0, sp));
  }
  return out;
}

} // namespace

///////////////////////////////////////////////////////////////////////////////

TEST(PerfHudPageLineCountsConstant) {
  PerfHudPages pages;
  auto virgin = allPageCounts(pages);  // nothing ever published
  CHECK_EQUAL(7, virgin[idx(PAGE_FRAME)]);
  CHECK_EQUAL(3, virgin[idx(PAGE_GPU)]);     // header + frame + state; no pass measured yet
  CHECK_EQUAL(14, virgin[idx(PAGE_PASSES)]);
  CHECK_EQUAL(10, virgin[idx(PAGE_CULL)]);   // two stage lines per consumer + grass
  CHECK_EQUAL(5, virgin[idx(PAGE_SYSTEMS)]); // header + column header + root + eye + hypermesh-gen

  pages.publish(fullInputs()); // + the 2 ECS rows, frozen here
  auto bound = allPageCounts(pages);
  CHECK_EQUAL(7, bound[idx(PAGE_FRAME)]);
  CHECK_EQUAL(7, bound[idx(PAGE_GPU)]); // + the 4 measured passes
  CHECK_EQUAL(14, bound[idx(PAGE_PASSES)]);
  CHECK_EQUAL(10, bound[idx(PAGE_CULL)]);
  CHECK_EQUAL(7, bound[idx(PAGE_SYSTEMS)]);

  // 40 frames with only the every-frame sinks reporting: nothing may move.
  for (int i = 0; i < 40; i++)
    pages.publish(sparseInputs());
  auto starved = allPageCounts(pages);
  for (int p = PerfHudPages::PAGE_FRAME; p < PerfHudPages::NUM_PAGES; p++)
    CHECK_EQUAL(size_t(pages.pageLineCount(p)), pages.pageLines(p).size()); // starved page still draws every row
  CHECK_EQUAL(bound[0], starved[0]);
  CHECK_EQUAL(bound[1], starved[1]);
  CHECK_EQUAL(bound[2], starved[2]);
  CHECK_EQUAL(bound[3], starved[3]);
  CHECK_EQUAL(bound[4], starved[4]);

  // and a full frame again does not change the geometry either
  pages.publish(fullInputs());
  auto again = allPageCounts(pages);
  CHECK_EQUAL(bound[1], again[1]);
  CHECK_EQUAL(bound[3], again[3]);

  // every line of every page is non-empty (no blank placeholder rows), and the RENDERED
  // line count matches the count the desktop bottom-anchor is computed from — a page
  // that draws fewer lines than it claims is exactly the anchor drift this replaced.
  for (int p = PerfHudPages::PAGE_FRAME; p < PerfHudPages::NUM_PAGES; p++) {
    auto lines = pages.pageLines(p);
    CHECK_EQUAL(size_t(pages.pageLineCount(p)), lines.size());
    for (const auto& l : lines)
      CHECK(not l.empty());
  }
}

///////////////////////////////////////////////////////////////////////////////
// THE RING: which page each step lands on. The order is the owner's (FRAME then GPU —
// the two halves of the frame budget one step apart from OFF), and stepPage is the ONE
// rule behind every input that pages: '~', SHIFT-'~' and the pad bumpers. Pinned here:
//   * the fixed pages sit at ring positions 1..5, by NAME,
//   * stepping forward from OFF visits all of them and wraps back to OFF,
//   * stepping BACKWARD is the same ring reversed — it wraps through OFF the other way,
//     so page 1 back is OFF and OFF back is the LAST page (an editor page when one is
//     registered), never a clamp and never a skipped page.
///////////////////////////////////////////////////////////////////////////////

TEST(PerfHudPageRingOrder) {
  PerfHudPages pages;

  auto pageName = [&](int p) {
    std::string h = pages.pageLines(p)[0]; // "[n/N] NAME"
    auto        sp = h.find_first_of(' ');
    return h.substr(sp + 1);
  };
  CHECK_EQUAL(std::string("FRAME"), pageName(1));
  CHECK_EQUAL(std::string("GPU"), pageName(2));
  CHECK_EQUAL(std::string("PASSES"), pageName(3));
  CHECK_EQUAL(std::string("CULL"), pageName(4));
  CHECK_EQUAL(std::string("SYSTEMS"), pageName(5));

  // forward from OFF: every page once, then back to OFF
  int p = PerfHudPages::PAGE_OFF;
  for (int i = 1; i < PerfHudPages::NUM_PAGES; i++) {
    p = pages.stepPage(p, +1);
    CHECK_EQUAL(i, p);
  }
  CHECK_EQUAL(int(PerfHudPages::PAGE_OFF), pages.stepPage(p, +1));

  // backward from OFF: the same ring, reversed
  p = PerfHudPages::PAGE_OFF;
  for (int i = PerfHudPages::NUM_PAGES - 1; i >= 1; i--) {
    p = pages.stepPage(p, -1);
    CHECK_EQUAL(i, p);
  }
  CHECK_EQUAL(int(PerfHudPages::PAGE_OFF), pages.stepPage(p, -1));

  // forward then backward from any page is a no-op
  for (int q = 0; q < PerfHudPages::NUM_PAGES; q++)
    CHECK_EQUAL(q, pages.stepPage(pages.stepPage(q, +1), -1));

  // a registered editor page joins the ring at BOTH ends: it is the last page forward
  // and the FIRST page backward out of OFF.
  int sky = pages.registerEditorPage("SKY", {});
  CHECK_EQUAL(sky, pages.stepPage(PerfHudPages::PAGE_OFF, -1));
  CHECK_EQUAL(int(PerfHudPages::PAGE_OFF), pages.stepPage(sky, +1));
  CHECK_EQUAL(int(PerfHudPages::PAGE_SYSTEMS), pages.stepPage(sky, -1));
}

///////////////////////////////////////////////////////////////////////////////

TEST(PerfHudSlotAgeSuffix) {
  PerfHudPages pages;

  // never published -> the row is present and reads `--`
  auto lines = pages.pageLines(PerfHudPages::PAGE_PASSES);
  CHECK(lines[8].find("sun-cascade-fit") == 0);
  CHECK(lines[8].find("--") != std::string::npos);

  pages.publish(fullInputs());
  lines = pages.pageLines(PerfHudPages::PAGE_PASSES);
  CHECK(lines[8].find("sun-cascade-fit") == 0);
  CHECK(lines[8].find("--") == std::string::npos);
  CHECK(lines[8].find("@") == std::string::npos); // fresh: no age suffix

  for (int i = 0; i < 14; i++)
    pages.publish(sparseInputs()); // the cadenced phase stays silent
  lines = pages.pageLines(PerfHudPages::PAGE_PASSES);
  CHECK(lines[8].find("sun-cascade-fit") == 0);
  CHECK(lines[8].find("@14f") != std::string::npos); // last value + age in frames

  // an every-frame phase on the same page never wears an age
  CHECK(lines[2].find("assemble") == 0);
  CHECK(lines[2].find("@") == std::string::npos);

  // cull + grass slots follow the same rule on the CULL page. Two stage lines per
  // consumer, ONE key dialect everywhere (in<>/out<>); HYPM's frus line alone
  // carries var<>. header + 4 phase rows + 2 TERR + 2 HYPM + grass = 10 lines.
  auto cull = pages.pageLines(PerfHudPages::PAGE_CULL);
  CHECK_EQUAL(size_t(10), cull.size());
  const int iTF = 5, iTO = 6, iHF = 7, iHO = 8, iGW = 9;
  CHECK(cull[iTF].find("TERR frus") == 0);
  CHECK(cull[iTO].find("TERR occl") == 0);
  CHECK(cull[iHF].find("HYPM frus") == 0);
  CHECK(cull[iHO].find("HYPM occl") == 0);
  for (int i : {iTF, iTO, iHF, iHO}) { // every stage line speaks the same dialect
    CHECK(cull[i].find("in<") != std::string::npos);
    CHECK(cull[i].find("out<") != std::string::npos);
  }
  CHECK(cull[iHF].find("var<") != std::string::npos);
  CHECK(cull[iHO].find("var<") == std::string::npos);
  CHECK(cull[iGW].find("grass") == 0);
  CHECK(cull[iGW].find("wg<") != std::string::npos);
  CHECK(cull[iGW].find("@14f") != std::string::npos);

  pages.publish(fullInputs()); // publishing again clears the age
  lines = pages.pageLines(PerfHudPages::PAGE_PASSES);
  CHECK(lines[8].find("@") == std::string::npos);
}

///////////////////////////////////////////////////////////////////////////////

TEST(PerfHudSlotOrderStable) {
  PerfHudPages pages;
  pages.publish(fullInputs());
  std::vector<std::vector<std::string>> ref;
  for (int p = PerfHudPages::PAGE_FRAME; p < PerfHudPages::NUM_PAGES; p++)
    ref.push_back(rowHeads(pages, p));

  // every page leads with its header, numbered by its OWN ring position, and PASSES is
  // in pipeline order
  CHECK_EQUAL(std::string("[1/5]"), ref[idx(PAGE_FRAME)][0]);
  CHECK_EQUAL(std::string("[2/5]"), ref[idx(PAGE_GPU)][0]);
  CHECK_EQUAL(std::string("[3/5]"), ref[idx(PAGE_PASSES)][0]);
  CHECK_EQUAL(std::string("preRender"), ref[idx(PAGE_PASSES)][1]);
  CHECK_EQUAL(std::string("assemble"), ref[idx(PAGE_PASSES)][2]);
  CHECK_EQUAL(std::string("composite"), ref[idx(PAGE_PASSES)][3]);
  CHECK_EQUAL(std::string("[4/5]"), ref[idx(PAGE_CULL)][0]);
  CHECK_EQUAL(std::string("[5/5]"), ref[idx(PAGE_SYSTEMS)][0]);

  // alternate starved / full frames: the head of every row must never move
  for (int i = 0; i < 8; i++) {
    pages.publish((i & 1) ? fullInputs() : sparseInputs());
    for (int p = PerfHudPages::PAGE_FRAME; p < PerfHudPages::NUM_PAGES; p++) {
      auto heads = rowHeads(pages, p);
      CHECK_EQUAL(ref[idx(p)].size(), heads.size());
      for (size_t r = 0; r < heads.size() and r < ref[idx(p)].size(); r++)
        CHECK_EQUAL(ref[idx(p)][r], heads[r]);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

TEST(PerfHudEcsRowsFrozenAtBind) {
  PerfHudPages pages;
  pages.publish(sparseInputs()); // no systems yet -> no ECS rows
  CHECK_EQUAL(5, pages.pageLineCount(PerfHudPages::PAGE_SYSTEMS));

  pages.publish(fullInputs()); // 2 systems -> frozen at 2 rows
  CHECK_EQUAL(7, pages.pageLineCount(PerfHudPages::PAGE_SYSTEMS));

  // an idle system (all-zero timings) keeps its row — membership is fixed, not thresholded
  HudInputs idle          = fullInputs();
  idle._systems["SceneGraphSystem"] = {0.0, 0.0, 0.0};
  pages.publish(idle);
  CHECK_EQUAL(7, pages.pageLineCount(PerfHudPages::PAGE_SYSTEMS));

  // a system that shows up later does NOT re-flow the page
  HudInputs latecomer = fullInputs();
  latecomer._systems["LateSystem"] = {1.0, 1.0, 1.0};
  pages.publish(latecomer);
  CHECK_EQUAL(7, pages.pageLineCount(PerfHudPages::PAGE_SYSTEMS));

  // the VR panel is sized off the tallest page, which must be PASSES here
  CHECK_EQUAL(14, pages.maxLineCount());
}

///////////////////////////////////////////////////////////////////////////////

TEST(PerfHudGpuPageRowsGrowAndHoldOrder) {
  PerfHudPages pages;

  // nothing measured: the two fixed rows still render, frame as `--`
  auto virgin = pages.pageLines(PerfHudPages::PAGE_GPU);
  CHECK_EQUAL(size_t(3), virgin.size());
  CHECK(virgin[1].find("frame") == 0);
  CHECK(virgin[1].find("--") != std::string::npos);

  pages.publish(fullInputs());
  auto lines = pages.pageLines(PerfHudPages::PAGE_GPU);
  CHECK_EQUAL(size_t(7), lines.size());
  // the state row reports the lag and the calibration honestly
  CHECK(lines[2].find("state") == 0);
  CHECK(lines[2].find("lag2") != std::string::npos);
  CHECK(lines[2].find("cal") != std::string::npos);
  CHECK(lines[2].find("uncal") == std::string::npos);
  // rows ordered by descending EMA, and a pass carries its segment count
  CHECK(lines[3].find("rtg:fwd-color") == 0);
  CHECK(lines[4].find("rtg:shadow") == 0);
  CHECK(lines[5].find("xr:blit") == 0);
  CHECK(lines[6].find("cull:perview") == 0);
  CHECK(lines[4].find("x5") != std::string::npos);

  // a frame where the device published NOTHING must not drop a row — each keeps its
  // last value plus an age, exactly like the cadenced CPU phases.
  for (int i = 0; i < 5; i++)
    pages.publish(sparseInputs());
  lines = pages.pageLines(PerfHudPages::PAGE_GPU);
  CHECK_EQUAL(size_t(7), lines.size());
  CHECK(lines[3].find("@5f") != std::string::npos);

  // an UNCALIBRATED device says so rather than implying a shared timeline, and dropped
  // slices are visible
  HudInputs uncal            = fullInputs();
  uncal._gpu._calibrated     = false;
  uncal._gpu._dropped        = 3;
  uncal._gpu._gpu_frame_ms   = -1.0f;
  pages.publish(uncal);
  lines = pages.pageLines(PerfHudPages::PAGE_GPU);
  CHECK(lines[1].find("--") != std::string::npos);
  CHECK(lines[2].find("uncal") != std::string::npos);
  CHECK(lines[2].find("drop<3>") != std::string::npos);

  // a pass measured for the FIRST time later gets a row; the ones already there keep
  // their order (the page grows at the tail, it does not re-sort under the reader)
  HudInputs late = fullInputs();
  late._gpu._passes["hzb:build"] = {99.0, 99.0, 11};
  pages.publish(late);
  lines = pages.pageLines(PerfHudPages::PAGE_GPU);
  CHECK_EQUAL(size_t(8), lines.size());
  CHECK(lines[3].find("hzb:build") == 0); // the growth re-orders once, by EMA
  CHECK(lines[4].find("rtg:fwd-color") == 0);
  auto heads_before = rowHeads(pages, PerfHudPages::PAGE_GPU);
  for (int i = 0; i < 6; i++) { // steady state: no further movement
    HudInputs jitter = late;
    jitter._gpu._passes["cull:perview"] = {50.0, 50.0, 1}; // a value spike may NOT re-sort
    pages.publish(jitter);
    CHECK(rowHeads(pages, PerfHudPages::PAGE_GPU) == heads_before);
  }
}

///////////////////////////////////////////////////////////////////////////////
// The GPU page's two OPTIONAL blocks — XR frame pacing (vr:) and NVIDIA driver
// telemetry (nv:). Their sinks report LIVENESS, and a sink that is not live must
// contribute NO ROWS: a desktop run has no XR pacing and an AMD/Apple box has no NVML,
// and a `--` row there would answer a question that does not apply. Pinned here:
//   * neither block appears while its sink is silent,
//   * a block appears ONCE, as a fixed-size group ABOVE the pass rows,
//   * the blocks keep their template order whichever one goes live first,
//   * a headroom with no GPU reading behind it says `--` instead of a plausible number,
//   * once bound, nothing moves again.
///////////////////////////////////////////////////////////////////////////////

namespace {

HudInputs nvmlInputs() {
  HudInputs in         = fullInputs();
  in._nvml._live       = true;
  in._nvml._sm_mhz     = 2610;
  in._nvml._mem_mhz    = 10501;
  in._nvml._util_gpu   = 97;
  in._nvml._util_mem   = 41;
  in._nvml._power_w    = 288.4;
  in._nvml._temp_c     = 71;
  in._nvml._throttle   = "hwtherm swpwr";
  return in;
}

HudInputs vrInputs() {
  HudInputs in            = nvmlInputs();
  in._vr._live            = true;
  in._vr._wait_ms         = 4.21;
  in._vr._wait_ms_ema     = 4.05;
  in._vr._period_ms       = 11.111;
  in._vr._missed_1s       = 2;
  in._vr._target_ms       = 8.0;
  in._vr._headroom_ms     = -1.75;
  in._vr._headroom_valid  = true;
  return in;
}

} // namespace

TEST(PerfHudGpuPageOptionalBlocksAbsentUntilLive) {
  PerfHudPages pages;
  pages.publish(fullInputs()); // desktop, no NVIDIA driver
  auto heads = rowHeads(pages, PerfHudPages::PAGE_GPU);
  CHECK_EQUAL(size_t(7), heads.size()); // header + frame + state + the 4 passes only
  for (const auto& h : heads) {
    CHECK(h.compare(0, 3, "vr:") != 0);
    CHECK(h.compare(0, 3, "nv:") != 0);
  }
}

TEST(PerfHudGpuPageNvmlBlock) {
  PerfHudPages pages;
  pages.publish(nvmlInputs());
  auto lines = pages.pageLines(PerfHudPages::PAGE_GPU);
  CHECK_EQUAL(size_t(12), lines.size()); // + 5 telemetry rows, no pacing rows
  CHECK(lines[3].find("nv:clk") == 0);
  CHECK(lines[3].find("sm<2610> mem<10501> MHz") != std::string::npos);
  CHECK(lines[4].find("nv:util") == 0);
  CHECK(lines[4].find("gpu<97%> mem<41%>") != std::string::npos);
  CHECK(lines[5].find("nv:pwr") == 0);
  CHECK(lines[5].find("288.4 W") != std::string::npos);
  CHECK(lines[6].find("nv:temp") == 0);
  CHECK(lines[7].find("nv:throttle") == 0);
  CHECK(lines[7].find("hwtherm swpwr") != std::string::npos);
  CHECK(lines[8].find("rtg:fwd-color") == 0); // the pass rows follow the block
  for (const auto& h : rowHeads(pages, PerfHudPages::PAGE_GPU))
    CHECK(h.compare(0, 3, "vr:") != 0);
}

TEST(PerfHudGpuPageVrBlock) {
  PerfHudPages pages;
  pages.publish(vrInputs());
  auto lines = pages.pageLines(PerfHudPages::PAGE_GPU);
  CHECK_EQUAL(size_t(16), lines.size()); // header + frame + state + 4 vr + 5 nv + 4 passes
  CHECK(lines[3].find("vr:wait") == 0);
  CHECK(lines[3].find("4.21 ema") != std::string::npos);
  CHECK(lines[4].find("vr:period") == 0);
  CHECK(lines[4].find("90.0 Hz") != std::string::npos); // period reported as a rate too
  CHECK(lines[5].find("vr:missed") == 0);
  CHECK(lines[5].find("2 /s") != std::string::npos);
  CHECK(lines[6].find("vr:head") == 0);
  CHECK(lines[6].find("-1.75") != std::string::npos); // OVER budget reads signed
  CHECK(lines[6].find("tgt  8.0") != std::string::npos);
  CHECK(lines[7].find("nv:clk") == 0); // the pacing block sits ABOVE the telemetry one
  CHECK(lines[12].find("rtg:fwd-color") == 0);

  // no GPU reading behind the headroom = say so, do not publish a plausible number
  HudInputs noread          = vrInputs();
  noread._vr._headroom_valid = false;
  pages.publish(noread);
  lines = pages.pageLines(PerfHudPages::PAGE_GPU);
  CHECK(lines[6].find("vr:head") == 0);
  CHECK(lines[6].find("--") != std::string::npos);

  // steady state: values move, rows do not
  auto heads_before = rowHeads(pages, PerfHudPages::PAGE_GPU);
  for (int i = 0; i < 6; i++) {
    HudInputs jitter        = vrInputs();
    jitter._vr._missed_1s   = i;
    jitter._nvml._sm_mhz    = 400 + i;
    jitter._nvml._throttle  = "hwslow hwtherm hwbrake swtherm swpwr sync appclk dispclk idle";
    pages.publish(jitter);
    CHECK(rowHeads(pages, PerfHudPages::PAGE_GPU) == heads_before);
    // even an everything-limited GPU cannot widen the panel
    CHECK(pages.pageLines(PerfHudPages::PAGE_GPU)[11].size() <= size_t(PerfHudPages::maxLineCols()));
  }
}

TEST(PerfHudGpuPageBlockOrderIndependentOfArrival) {
  PerfHudPages pages;
  pages.publish(nvmlInputs()); // telemetry first ...
  CHECK(pages.pageLines(PerfHudPages::PAGE_GPU)[3].find("nv:clk") == 0);
  pages.publish(vrInputs()); // ... a headset joins later
  auto lines = pages.pageLines(PerfHudPages::PAGE_GPU);
  CHECK_EQUAL(size_t(16), lines.size());
  CHECK(lines[3].find("vr:wait") == 0); // template order, not arrival order
  CHECK(lines[7].find("nv:clk") == 0);
}

///////////////////////////////////////////////////////////////////////////////
// EDITOR PAGES — the same layout-constancy law, under EDITING. What is pinned here:
//   * a registered page extends the ring and RENUMBERS every header ("[1/5]" -> "[1/7]"),
//   * moving the selection changes no character except the 2-char cursor column,
//   * changing a value changes no character outside the fixed value + bar fields,
//   * a slider clamps at both ends and an enum wraps,
//   * the VR panel's per-page nominal width is a CONSTANT for an editor page,
//   * a page with no properties is still a legal (header-only) page.
///////////////////////////////////////////////////////////////////////////////

namespace {

struct EditModel {
  float exposure = 1.0f;
  float mode     = 0.0f;
};

std::vector<HudEditProp> editProps(EditModel& m) {
  std::vector<HudEditProp> props;
  props.push_back(hudFloatProp(
      "exposure", 0.0f, 2.0f, 0.5f, [&m]() { return m.exposure; }, [&m](float v) { m.exposure = v; }));
  props.push_back(hudEnumProp(
      "haze in shadow", {"off", "on"}, [&m]() { return m.mode; }, [&m](float v) { m.mode = v; }));
  return props;
}

} // namespace

TEST(PerfHudEditorPageRingAndHeaders) {
  PerfHudPages pages;
  CHECK_EQUAL(PerfHudPages::NUM_PAGES, pages.numPages());
  CHECK_EQUAL(std::string("[1/5] FRAME"), pages.pageLines(PerfHudPages::PAGE_FRAME)[0]);

  EditModel m;
  int sky = pages.registerEditorPage("SKY", editProps(m));
  CHECK_EQUAL(PerfHudPages::NUM_PAGES, sky);
  CHECK_EQUAL(PerfHudPages::NUM_PAGES + 1, pages.numPages());
  CHECK(pages.isEditorPage(sky));
  CHECK_EQUAL(sky, pages.findEditorPage("SKY"));
  CHECK_EQUAL(-1, pages.findEditorPage("POST"));

  // the ring grew, so EVERY header says so
  CHECK_EQUAL(std::string("[1/6] FRAME"), pages.pageLines(PerfHudPages::PAGE_FRAME)[0]);
  CHECK_EQUAL(std::string("[6/6] SKY"), pages.pageLines(sky)[0]);
  CHECK_EQUAL(4, pages.pageLineCount(sky)); // header + 2 properties + the key legend
  // the legend is the page's LAST line, and it is the desktop mapping the player binds
  CHECK_EQUAL(std::string(PerfHudPages::kEditLegend), pages.pageLines(sky)[3]);

  EditModel m2;
  int post = pages.registerEditorPage("POST", editProps(m2));
  CHECK_EQUAL(std::string("[1/7] FRAME"), pages.pageLines(PerfHudPages::PAGE_FRAME)[0]);
  CHECK_EQUAL(std::string("[6/7] SKY"), pages.pageLines(sky)[0]);
  CHECK_EQUAL(std::string("[7/7] POST"), pages.pageLines(post)[0]);

  // an editor page joins the VR panel's height budget like any other
  CHECK(pages.maxLineCount() >= pages.pageLineCount(sky));
  // ... and its nominal width is a constant, not a measurement of the current values
  int nom = pages.pageNominalCols(sky);
  m2.exposure = 1.75f;
  pages.editSelect(sky, +1);
  CHECK_EQUAL(nom, pages.pageNominalCols(sky));
}

TEST(PerfHudEditorLayoutDoesNotMove) {
  PerfHudPages pages;
  EditModel    m;
  int          sky = pages.registerEditorPage("SKY", editProps(m));

  auto lines0 = pages.pageLines(sky);
  CHECK_EQUAL(size_t(4), lines0.size()); // header + 2 properties + legend
  CHECK(lines0[1].find("> ") == 0);   // row 0 selected
  CHECK(lines0[2].find("  ") == 0);

  // MOVING THE SELECTION may only move the cursor column
  pages.editSelect(sky, +1);
  auto lines1 = pages.pageLines(sky);
  CHECK_EQUAL(lines0[1].size(), lines1[1].size());
  CHECK_EQUAL(lines0[2].size(), lines1[2].size());
  CHECK(lines1[1].find("  ") == 0);
  CHECK(lines1[2].find("> ") == 0);
  CHECK_EQUAL(lines0[1].substr(2), lines1[1].substr(2));
  CHECK_EQUAL(lines0[2].substr(2), lines1[2].substr(2));

  // CHANGING A VALUE may not change the line WIDTH (fixed value field + fixed bar)
  pages.editSelect(sky, +1); // back to the slider
  size_t w = pages.pageLines(sky)[1].size();
  for (int i = 0; i < 8; i++) {
    pages.editAdjust(sky, +1);
    CHECK_EQUAL(w, pages.pageLines(sky)[1].size());
    CHECK_EQUAL(size_t(4), pages.pageLines(sky).size());
  }
  for (int i = 0; i < 16; i++) {
    pages.editAdjust(sky, -1);
    CHECK_EQUAL(w, pages.pageLines(sky)[1].size());
  }
}

TEST(PerfHudEditorSliderClampsEnumWraps) {
  PerfHudPages pages;
  EditModel    m;
  int          sky = pages.registerEditorPage("SKY", editProps(m));

  CHECK(pages.editSelectedIsSlider(sky));
  for (int i = 0; i < 20; i++)
    pages.editAdjust(sky, +1);
  CHECK_CLOSE(2.0f, m.exposure, 1e-5f); // clamped at _max, never past it
  for (int i = 0; i < 20; i++)
    pages.editAdjust(sky, -1);
  CHECK_CLOSE(0.0f, m.exposure, 1e-5f); // clamped at _min

  pages.editSelect(sky, +1);
  CHECK(not pages.editSelectedIsSlider(sky)); // the enum row: no auto-repeat
  pages.editAdjust(sky, +1);
  CHECK_CLOSE(1.0f, m.mode, 1e-5f);
  CHECK(pages.pageLines(sky)[2].find("on") != std::string::npos);
  pages.editAdjust(sky, +1); // wraps
  CHECK_CLOSE(0.0f, m.mode, 1e-5f);
  pages.editAdjust(sky, -1); // wraps the other way
  CHECK_CLOSE(1.0f, m.mode, 1e-5f);

  // the selection wraps too, and never leaves the page
  pages.editSelect(sky, +1);
  CHECK_EQUAL(0, pages.editorPage(sky)->_sel);
  pages.editSelect(sky, -1);
  CHECK_EQUAL(1, pages.editorPage(sky)->_sel);
}

TEST(PerfHudEditorEmptyPageIsLegal) {
  PerfHudPages pages;
  int          empty = pages.registerEditorPage("NONE", {});
  CHECK_EQUAL(1, pages.pageLineCount(empty)); // the header alone
  pages.editSelect(empty, +1);                // must not fault or move anything
  pages.editAdjust(empty, +1);
  CHECK_EQUAL(1, pages.pageLineCount(empty));
  CHECK(not pages.editSelectedIsSlider(empty));
  // the fixed pages are untouched by a neighbour's registration
  pages.publish(fullInputs());
  CHECK_EQUAL(14, pages.pageLineCount(PerfHudPages::PAGE_PASSES));
}

///////////////////////////////////////////////////////////////////////////////
// COLOR ROWS — the reusable H/S/V sub-editor. What is pinned here:
//   * HSV <-> RGB round-trips,
//   * a color row is ONE line whether the sub-editor is open or shut (no relayout),
//   * shut, the row is INERT: an adjust cannot change the color by accident,
//   * open, up/down walk H/S/V instead of moving the row selection,
//   * HUE WRAPS and s/v clamp, and the working hue survives a pass through gray
//     (which is exactly what a naive RGB round-trip would destroy).
///////////////////////////////////////////////////////////////////////////////

TEST(PerfHudColorHSVRoundTrip) {
  // the last case is SCENE-REFERRED (a component over 1) — a tint the engine really ships
  const HudRGB cases[] = {{1, 0, 0},          {0, 1, 0},           {0, 0, 1},        {0.86f, 0.83f, 0.78f},
                          {0.2f, 0.4f, 0.6f}, {0, 0, 0},           {1.10f, 0.7f, 0.9f}};
  for (const auto& rgb : cases) {
    auto back = hudHSVtoRGB(hudRGBtoHSV(rgb));
    for (int i = 0; i < 3; i++)
      CHECK_CLOSE(rgb[i], back[i], 1e-4f);
  }
  // hue is an angle: 1.25 and 0.25 are the same color
  auto a = hudHSVtoRGB({0.25f, 1.0f, 1.0f});
  auto b = hudHSVtoRGB({1.25f, 1.0f, 1.0f});
  for (int i = 0; i < 3; i++)
    CHECK_CLOSE(a[i], b[i], 1e-4f);
}

TEST(PerfHudColorRowEditing) {
  PerfHudPages pages;
  HudRGB       tint = {1.0f, 1.0f, 1.0f};
  std::vector<HudEditProp> props;
  props.push_back(hudFloatProp("density", 0.0f, 1.0f, 0.1f, []() { return 0.5f; }, [](float) {}));
  props.push_back(hudColorProp("tint", [&tint]() { return tint; }, [&tint](const HudRGB& c) { tint = c; }));
  int sky = pages.registerEditorPage("SKY", props);

  // select the color row; the page is still 4 lines and stays 4 lines through everything
  pages.editSelect(sky, +1);
  CHECK(pages.editSelectedIsColor(sky));
  CHECK(not pages.editColorOpen(sky));
  size_t w = pages.pageLines(sky)[2].size();
  CHECK_EQUAL(size_t(4), pages.pageLines(sky).size());

  // SHUT: adjust is inert — a color is never changed by a stray trigger
  HudRGB before = tint;
  pages.editAdjust(sky, +1);
  for (int i = 0; i < 3; i++)
    CHECK_CLOSE(before[i], tint[i], 1e-6f);
  CHECK(pages.pageLines(sky)[2].find("x") != std::string::npos); // the "press X" marker

  // OPEN: same line count, same width, and up/down now walk the channels
  pages.editToggleColor(sky);
  CHECK(pages.editColorOpen(sky));
  CHECK_EQUAL(size_t(4), pages.pageLines(sky).size());
  CHECK_EQUAL(w, pages.pageLines(sky)[2].size());
  CHECK(pages.editSelectedIsSlider(sky)); // an open channel repeats like any slider
  CHECK_EQUAL(1, pages.editorPage(sky)->_sel);
  pages.editSelect(sky, +1);
  CHECK_EQUAL(1, pages.editorPage(sky)->_sel);  // the ROW did not move ...
  CHECK_EQUAL(1, pages.editorPage(sky)->_chan); // ... the CHANNEL did

  // channel 1 = saturation: pull it up off white (0.01 per step) and the color must
  // leave gray, and must stop at 1.0 rather than run past it
  for (int i = 0; i < 120; i++)
    pages.editAdjust(sky, +1);
  CHECK_CLOSE(1.0f, pages.editorPage(sky)->_hsv[1], 1e-4f);
  CHECK(std::fabs(tint[0] - tint[2]) > 0.5f); // no longer neutral
  CHECK_EQUAL(w, pages.pageLines(sky)[2].size());

  // hue wraps rather than clamping, and the WORKING hue survives being driven to black
  pages.editSelect(sky, -1); // back to H
  CHECK_EQUAL(0, pages.editorPage(sky)->_chan);
  for (int i = 0; i < 150; i++)
    pages.editAdjust(sky, +1); // 1.5 turns
  float h = pages.editorPage(sky)->_hsv[0];
  CHECK(h >= 0.0f and h < 1.0f);
  pages.editSelect(sky, -1); // V
  CHECK_EQUAL(2, pages.editorPage(sky)->_chan);
  // value is scene-referred: it must be able to go ABOVE 1 (an HDR tint), then to black
  for (int i = 0; i < 60; i++)
    pages.editAdjust(sky, +1);
  CHECK(pages.editorPage(sky)->_hsv[2] > 1.0f);
  for (int i = 0; i < 200; i++)
    pages.editAdjust(sky, -1); // to black
  CHECK_CLOSE(0.0f, pages.editorPage(sky)->_hsv[2], 1e-4f);
  CHECK_CLOSE(h, pages.editorPage(sky)->_hsv[0], 1e-4f); // the hue is still the user's

  // closing hands up/down back to the row selection
  pages.editToggleColor(sky);
  CHECK(not pages.editColorOpen(sky));
  pages.editSelect(sky, +1);
  CHECK_EQUAL(0, pages.editorPage(sky)->_sel);
  // ... and moving off a color row can never leave a sub-editor open behind you
  CHECK(not pages.editColorOpen(sky));
  pages.editToggleColor(sky); // not a color row: a no-op, never a fault
  CHECK(not pages.editColorOpen(sky));
}

///////////////////////////////////////////////////////////////////////////////
// SAVED EDITOR VALUES (editor_state.h) — the model half of per-scene persistence.
// What is pinned here:
//   * an ACTION row is a command: no slider, no adjust, fires once per activate,
//     and it renders in the same fixed-width layout as a value row,
//   * the file is PAGE-AGNOSTIC — a row that moves to another page keeps its value,
//   * a saved value applies over the scene's (tier 3 over tier 2),
//   * a scene RE-AUTHORED since the save keeps ITS value and the file is reported,
//   * an id the ring no longer has is skipped and reported, never fatal,
//   * modes are independent: a vr load never sees desktop values,
//   * enums round-trip as LABELS resolved through the engine's crc.
///////////////////////////////////////////////////////////////////////////////

#include "../examples/c++/player/editor_state.h"
#include <cstdio>

namespace {

struct StateModel {
  float  density   = 0.30f;
  float  exposure  = 1.00f;
  float  enumv     = 0.0f;
  HudRGB tint      = {1.0f, 1.0f, 1.0f};
  int    fired     = 0;
  int    row_state = 0;
};

std::vector<HudEditProp> stateProps(StateModel& m) {
  std::vector<HudEditProp> p;
  p.push_back(hudFloatProp("haze density", 0.0f, 1.0f, 0.1f,
                           [&m]() { return m.density; }, [&m](float v) { m.density = v; }));
  p.push_back(hudEnumProp("aerial persp", {"off", "on"},
                          [&m]() { return m.enumv; }, [&m](float v) { m.enumv = v; }));
  p.push_back(hudColorProp("haze scatter",
                           [&m]() { return m.tint; }, [&m](const HudRGB& c) { m.tint = c; }));
  return p;
}

std::string tmpStatePath(const char* tag) {
  std::string p = "/tmp/orkid_editor_state_";
  p += tag;
  p += ".json";
  remove(p.c_str());
  return p;
}

} // namespace

TEST(PerfHudActionRowIsACommand) {
  PerfHudPages pages;
  StateModel   m;
  auto         props = stateProps(m);
  props.push_back(hudActionProp("save all", {"ready", "saved", "refused"},
                                [&m]() { return float(m.row_state); },
                                [&m]() { m.fired++; m.row_state = 1; }));
  int page = pages.registerEditorPage("SETTINGS", props);

  // row 3 is the ACTION row; select it and prove it is not a value row
  for (int i = 0; i < 3; i++)
    pages.editSelect(page, +1);
  CHECK(pages.editSelectedIsAction(page));
  CHECK(pages.editSelectedIsActivatable(page));
  CHECK(not pages.editSelectedIsSlider(page));
  CHECK(not pages.editSelectedIsColor(page));

  // an adjust cannot fire it, and cannot change anything
  pages.editAdjust(page, +1);
  pages.editAdjust(page, -1);
  CHECK_EQUAL(0, m.fired);

  // activate fires exactly once per press, and the row reports the new state
  size_t w = pages.pageLines(page)[4].size();
  pages.editActivate(page);
  CHECK_EQUAL(1, m.fired);
  CHECK(pages.pageLines(page)[4].find("saved") != std::string::npos);
  CHECK_EQUAL(w, pages.pageLines(page)[4].size()); // a state change may not move the layout
  pages.editActivate(page);
  CHECK_EQUAL(2, m.fired);
}

TEST(PerfHudEditorStateRoundTrip) {
  auto path = tmpStatePath("roundtrip");
  StateModel m;
  {
    PerfHudPages pages;
    auto         base = pages.registerEditorPage("SKY-HAZE", stateProps(m));
    (void)base;
    auto baselines = EditorStateIO::capture(pages);
    // edit all three kinds, then save
    m.density = 0.75f;
    m.enumv   = 1.0f;
    m.tint    = HudRGB{0.5f, 0.25f, 0.125f};
    auto rep  = EditorStateIO::save(path, false, "scn_test.py", pages, baselines);
    CHECK(rep._ok);
    CHECK_EQUAL(3, rep._written);
  }
  // a FRESH run of the same scene: values start at the scene's, load restores the saved
  StateModel m2;
  PerfHudPages pages2;
  pages2.registerEditorPage("SKY-HAZE", stateProps(m2));
  auto baselines2 = EditorStateIO::capture(pages2);
  auto rep2       = EditorStateIO::load(path, false, pages2, baselines2);
  CHECK(rep2._ok);
  CHECK_EQUAL(3, rep2._applied);
  CHECK_EQUAL(0, rep2._skipped);
  CHECK_EQUAL(0, rep2._unknown);
  CHECK_CLOSE(0.75f, m2.density, 1e-5f);
  CHECK_CLOSE(1.0f, m2.enumv, 1e-5f);
  CHECK_CLOSE(0.5f, m2.tint[0], 1e-4f);
  CHECK_CLOSE(0.125f, m2.tint[2], 1e-4f);
  remove(path.c_str());
}

TEST(PerfHudEditorStateIsPageAgnostic) {
  auto path = tmpStatePath("pageagnostic");
  StateModel m;
  {
    PerfHudPages pages;
    pages.registerEditorPage("SKY", stateProps(m)); // saved from ONE page ...
    auto baselines = EditorStateIO::capture(pages);
    m.density      = 0.9f;
    auto rep       = EditorStateIO::save(path, false, "scn_test.py", pages, baselines);
    CHECK(rep._ok);
    CHECK_EQUAL(1, rep._written);
  }
  // ... and reloaded after the rows were re-homed onto differently-named pages
  StateModel   m2;
  PerfHudPages pages2;
  auto         props = stateProps(m2);
  std::vector<HudEditProp> a, b;
  a.push_back(props[0]);
  b.push_back(props[1]);
  b.push_back(props[2]);
  pages2.registerEditorPage("SKY-HAZE", a);
  pages2.registerEditorPage("SKY-MAIN", b);
  auto baselines2 = EditorStateIO::capture(pages2);
  auto rep2       = EditorStateIO::load(path, false, pages2, baselines2);
  CHECK_EQUAL(1, rep2._applied);
  CHECK_EQUAL(0, rep2._unknown);
  CHECK_CLOSE(0.9f, m2.density, 1e-5f);
  remove(path.c_str());
}

TEST(PerfHudEditorStateReAuthoredSceneWins) {
  auto path = tmpStatePath("reauthored");
  StateModel m;
  {
    PerfHudPages pages;
    pages.registerEditorPage("SKY-HAZE", stateProps(m));
    auto baselines = EditorStateIO::capture(pages); // tier 2 = 0.30
    m.density      = 0.75f;
    EditorStateIO::save(path, false, "scn_test.py", pages, baselines);
  }
  // the scene now declares 0.55 — the file's `was` (0.30) no longer matches tier 2
  StateModel m2;
  m2.density = 0.55f;
  PerfHudPages pages2;
  pages2.registerEditorPage("SKY-HAZE", stateProps(m2));
  auto baselines2 = EditorStateIO::capture(pages2);
  auto rep        = EditorStateIO::load(path, false, pages2, baselines2);
  CHECK_EQUAL(0, rep._applied);
  CHECK_EQUAL(1, rep._skipped);
  CHECK_CLOSE(0.55f, m2.density, 1e-5f); // the SCENE's value stands
  CHECK(rep._lines.size() == 1);         // ... and the divergence is reported, not silent
  remove(path.c_str());
}

TEST(PerfHudEditorStateModesAreIndependent) {
  auto path = tmpStatePath("modes");
  StateModel m;
  {
    PerfHudPages pages;
    pages.registerEditorPage("SKY-HAZE", stateProps(m));
    auto baselines = EditorStateIO::capture(pages);
    m.density      = 0.8f;
    EditorStateIO::save(path, false, "scn_test.py", pages, baselines); // DESKTOP only
  }
  // a VR run must not inherit the desktop grade
  StateModel   mv;
  PerfHudPages pv;
  pv.registerEditorPage("SKY-HAZE", stateProps(mv));
  auto bv  = EditorStateIO::capture(pv);
  auto rv  = EditorStateIO::load(path, true, pv, bv);
  CHECK_EQUAL(0, rv._applied);
  CHECK_CLOSE(0.30f, mv.density, 1e-5f);
  CHECK(rv._lines.size() == 1); // says the other mode has values, rather than using them

  // saving VR must PRESERVE the desktop section it did not touch
  mv.density = 0.2f;
  auto sv    = EditorStateIO::save(path, true, "scn_test.py", pv, bv);
  CHECK(sv._ok);
  StateModel   md;
  PerfHudPages pd;
  pd.registerEditorPage("SKY-HAZE", stateProps(md));
  auto bd = EditorStateIO::capture(pd);
  auto rd = EditorStateIO::load(path, false, pd, bd);
  CHECK_EQUAL(1, rd._applied);
  CHECK_CLOSE(0.8f, md.density, 1e-5f);
  remove(path.c_str());
}

TEST(PerfHudEditorStateToleratesDrift) {
  auto path = tmpStatePath("drift");
  StateModel m;
  {
    PerfHudPages pages;
    pages.registerEditorPage("SKY-HAZE", stateProps(m));
    auto baselines = EditorStateIO::capture(pages);
    m.density      = 0.65f;
    m.enumv        = 1.0f;
    EditorStateIO::save(path, false, "scn_test.py", pages, baselines);
  }
  // this scene has only ONE of the saved rows — the rest must be reported, not fatal
  StateModel   m2;
  PerfHudPages pages2;
  std::vector<HudEditProp> only;
  only.push_back(stateProps(m2)[0]);
  pages2.registerEditorPage("SKY-HAZE", only);
  auto baselines2 = EditorStateIO::capture(pages2);
  auto rep        = EditorStateIO::load(path, false, pages2, baselines2);
  CHECK_EQUAL(1, rep._applied);
  CHECK_EQUAL(1, rep._unknown);
  CHECK(rep._ok);
  CHECK_CLOSE(0.65f, m2.density, 1e-5f);

  // a file that does not parse stops the SAVE (never truncate what you could not read)
  FILE* f = fopen(path.c_str(), "wb");
  fputs("{ this is not json", f);
  fclose(f);
  auto bad = EditorStateIO::save(path, false, "scn_test.py", pages2, baselines2);
  CHECK(not bad._ok);
  auto badload = EditorStateIO::load(path, false, pages2, baselines2);
  CHECK(not badload._ok);
  remove(path.c_str());
}

///////////////////////////////////////////////////////////////////////////////
// THE SAVED SPAWN — where the walker starts, carried by four ordinary FLOAT rows and one
// command row. The player writes the restored position into the SceneData BEFORE the
// simulation exists, which is why the read half has to work on its own; what is pinned:
//   * the four rows save and restore like any other value row,
//   * readSaved() hands back exactly what load() would have applied, applying NOTHING —
//     the pre-simulation read cannot depend on a live row to have taken,
//   * a scene that MOVED its spawn keeps its own, and the saved one is reported,
//   * the command row fills the four rows and does not itself persist (it holds no value),
//   * a WORLD COORDINATE cannot widen an editor row (the layout law, at 1e5 metres).
///////////////////////////////////////////////////////////////////////////////

namespace {

struct SpawnModel {
  float x = 0.0f, y = 260.0f, z = 0.0f, yaw = 0.0f;
  int   row_state = 0;
};

std::vector<HudEditProp> spawnProps(SpawnModel& m) {
  std::vector<HudEditProp> p;
  p.push_back(hudActionProp("set spawn here", {"ready", "sampling", "set"},
                            [&m]() { return float(m.row_state); },
                            [&m]() { // the vantage the character answered with
                              m.x = 1200.0f; m.y = 812.0f; m.z = -430.0f; m.yaw = 1.25f;
                              m.row_state = 2;
                            }));
  p.push_back(hudFloatProp("spawn x", -65536.0f, 65536.0f, 1.0f,
                           [&m]() { return m.x; }, [&m](float v) { m.x = v; }));
  p.push_back(hudFloatProp("spawn y", -65536.0f, 65536.0f, 1.0f,
                           [&m]() { return m.y; }, [&m](float v) { m.y = v; }));
  p.push_back(hudFloatProp("spawn z", -65536.0f, 65536.0f, 1.0f,
                           [&m]() { return m.z; }, [&m](float v) { m.z = v; }));
  p.push_back(hudFloatProp("spawn yaw", -3.14159265f, 3.14159265f, 0.05f,
                           [&m]() { return m.yaw; }, [&m](float v) { m.yaw = v; }));
  return p;
}

} // namespace

TEST(PerfHudSpawnRowsPersist) {
  auto path = tmpStatePath("spawn");
  {
    // the scene's authored spawn is the baseline; the command row then places it elsewhere
    SpawnModel   m;
    PerfHudPages pages;
    int          page = pages.registerEditorPage("SETTINGS", spawnProps(m));
    auto         baselines = EditorStateIO::capture(pages);
    CHECK_EQUAL(size_t(4), baselines.size()); // the ACTION row holds no value to save
    CHECK(baselines.count("set spawn here") == 0);
    pages.editActivate(page); // "set spawn here" — row 0 is selected by default
    CHECK_CLOSE(1200.0f, m.x, 1e-3f);
    auto rep = EditorStateIO::save(path, false, "scn_walk.py", pages, baselines);
    CHECK(rep._ok);
    CHECK_EQUAL(4, rep._written);
  }
  // a FRESH run of the same scene: the rows come up authored, the load moves them
  SpawnModel   m2;
  PerfHudPages pages2;
  pages2.registerEditorPage("SETTINGS", spawnProps(m2));
  auto baselines2 = EditorStateIO::capture(pages2);

  // THE PRE-SIMULATION READ: it must produce the values without touching a single row,
  // because the player consumes them before the character it places even exists.
  EditorStateReport rrep;
  auto              saved = EditorStateIO::readSaved(path, false, pages2, baselines2, rrep);
  CHECK_EQUAL(size_t(4), saved.size());
  CHECK_CLOSE(1200.0f, saved["spawn x"]._f, 1e-3f);
  CHECK_CLOSE(-430.0f, saved["spawn z"]._f, 1e-3f);
  CHECK_CLOSE(1.25f, saved["spawn yaw"]._f, 1e-4f);
  CHECK_CLOSE(0.0f, m2.x, 1e-5f);     // ... and nothing was applied
  CHECK_CLOSE(260.0f, m2.y, 1e-5f);

  // and the ordinary load applies exactly the same set
  auto rep2 = EditorStateIO::load(path, false, pages2, baselines2);
  CHECK_EQUAL(4, rep2._applied);
  CHECK_EQUAL(0, rep2._skipped);
  CHECK_CLOSE(1200.0f, m2.x, 1e-3f);
  CHECK_CLOSE(812.0f, m2.y, 1e-3f);
  CHECK_CLOSE(-430.0f, m2.z, 1e-3f);
  CHECK_CLOSE(1.25f, m2.yaw, 1e-4f);
  remove(path.c_str());
}

TEST(PerfHudSpawnReAuthoredSceneWins) {
  auto path = tmpStatePath("spawnreauthor");
  {
    SpawnModel   m;
    PerfHudPages pages;
    int          page = pages.registerEditorPage("SETTINGS", spawnProps(m));
    auto         baselines = EditorStateIO::capture(pages);
    pages.editActivate(page);
    EditorStateIO::save(path, false, "scn_walk.py", pages, baselines);
  }
  // the scene now spawns the walker somewhere else entirely — the author moved it since
  SpawnModel m2;
  m2.x = 4000.0f;
  m2.y = 90.0f;
  PerfHudPages pages2;
  pages2.registerEditorPage("SETTINGS", spawnProps(m2));
  auto              baselines2 = EditorStateIO::capture(pages2);
  EditorStateReport rrep;
  auto              saved = EditorStateIO::readSaved(path, false, pages2, baselines2, rrep);
  CHECK(saved.count("spawn x") == 0); // the pre-simulation read must not place the character
  CHECK(saved.count("spawn y") == 0);
  CHECK(saved.count("spawn z") == 1); // ... on the axes the scene did NOT re-author it does
  CHECK_EQUAL(2, rrep._skipped);
  CHECK(rrep._lines.size() == 2);     // reported by name, never silently dropped
  auto rep = EditorStateIO::load(path, false, pages2, baselines2);
  CHECK_EQUAL(2, rep._skipped);
  CHECK_CLOSE(4000.0f, m2.x, 1e-3f);  // the SCENE's spawn stands
  CHECK_CLOSE(90.0f, m2.y, 1e-3f);
  remove(path.c_str());
}

TEST(PerfHudEditorRowWidthIsMagnitudeProof) {
  // A spawn row holds metres, and a world is bigger than a knob: the value column has to
  // be the same width at 1e5 as at 0.5 or the backdrop moves the moment a walker crosses
  // ten kilometres.
  PerfHudPages pages;
  float        v = 0.0f;
  std::vector<HudEditProp> props;
  props.push_back(hudFloatProp("spawn x", -65536.0f, 65536.0f, 1.0f,
                               [&v]() { return v; }, [&v](float nv) { v = nv; }));
  int page = pages.registerEditorPage("SETTINGS", props);
  const float cases[] = {0.0f, 0.5f, -0.001234f, 999.9f, 9999.5f, 10000.0f,
                         -12345.678f, 65536.0f, -65536.0f};
  size_t w = 0;
  for (float c : cases) {
    v         = c;
    auto line = pages.pageLines(page)[1];
    if (w == 0)
      w = line.size();
    CHECK_EQUAL(w, line.size());
  }
  CHECK(int(w) <= pages.pageNominalCols(page)); // and still inside the VR panel's content box
}

TEST(PerfHudEnumEntriesCarryCrc) {
  auto p = hudEnumProp("aerial persp", {"off", "on"}, []() { return 0.0f; }, [](float) {});
  CHECK_EQUAL(size_t(2), p._enum.size());
  CHECK_EQUAL(std::string("off"), p._enum[0]._label);
  CHECK_EQUAL(ork::CrcString("off").hashed(), p._enum[0]._crc);
  CHECK_EQUAL(ork::CrcString("on").hashed(), p._enum[1]._crc);
  CHECK(p._enum[0]._crc != p._enum[1]._crc);
}
