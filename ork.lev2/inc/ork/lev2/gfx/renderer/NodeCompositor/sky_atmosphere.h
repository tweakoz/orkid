////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/material_freestyle.h>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::pbr {
////////////////////////////////////////////////////////////////////////////////
// SkyAtmosphereData — reflected Hillaire-2020 medium description (SKYLIGHT L4).
//
// Every value here is an artist knob and therefore reflected + UBO-bound (A8);
// nothing in skytools.i2 bakes any of it into shader text. Units are the ones
// the shader math uses directly: KILOMETERS and per-kilometer coefficients.
//
// Presence of one of these on pbr::CommonStuff::_atmosphere is what ARMS the
// procedural sky path — with none attached the compositor prologue's LUT step
// is a no-op and the renderer is bit-for-bit unchanged.
////////////////////////////////////////////////////////////////////////////////

struct SkyAtmosphereData : public ork::Object {
  DeclareConcreteX(SkyAtmosphereData, ork::Object);

  // Change stamp for the baked (view-independent) LUTs — transmittance and
  // multi-scatter only depend on the medium, so they re-bake only when this moves.
  uint64_t mediumHash() const;

  // Change stamp for the PRESENTATION tier the IBL snapshot bakes in — the
  // artist haze layer, which the snapshot now overlays on the sky it captures.
  // Deliberately a SECOND hash: haze must never re-bake the transmittance /
  // multi-scatter LUTs (it is not in the medium), but a live haze edit does have
  // to reach reflections and SH ambient, and the only route there is a new
  // refilter cycle. So the IBL feed's trigger reads this one alongside
  // mediumHash(); the LUT bake reads only mediumHash().
  uint64_t hazePresentationHash() const;

  float topRadius() const {
    return _groundRadius + _atmosphereThickness;
  }

  float _groundRadius        = 6360.0f; // km, earth-like
  float _atmosphereThickness = 100.0f;  // km above the ground sphere

  fvec3 _rayleighScattering  = fvec3(5.802e-3f, 13.558e-3f, 33.100e-3f); // 1/km
  float _rayleighScaleHeight = 8.0f;                                     // km

  float _mieScattering  = 3.996e-3f; // 1/km
  float _mieExtinction  = 4.440e-3f; // 1/km (scattering + absorption)
  float _mieScaleHeight = 1.2f;      // km
  float _miePhaseG      = 0.8f;      // Henyey-Greenstein asymmetry

  fvec3 _ozoneAbsorption     = fvec3(0.650e-3f, 1.881e-3f, 0.085e-3f); // 1/km
  float _ozoneCenterAltitude = 25.0f;                                  // km
  float _ozoneTentHalfWidth  = 15.0f;                                  // km

  fvec3 _groundAlbedo   = fvec3(0.3f, 0.3f, 0.3f);
  fvec3 _sunIlluminance = fvec3(1.0f, 1.0f, 1.0f); // at the top of the atmosphere

  // World-space plumbing. Orkid world units are meters by convention, so the
  // default converts meters -> km. The floor keeps the sky-view camera inside
  // the medium when the eye sits exactly on the ground plane.
  float _kilometersPerWorldUnit = 1.0e-3f;
  float _minViewAltitude        = 5.0e-4f; // km (0.5 m)

  // VISIBLE-SKY presentation (FWD_SKYBOX_PROC). Deliberately OUTSIDE
  // mediumHash(): none of these change the LUT chain, so editing them must not
  // trigger a re-bake.
  //
  // OUTPUT SCALE: the sky-view LUT stores radiance scaled by _sunIlluminance,
  // which at the default (1,1,1) puts mid-sky around 2e-2 and the near-sun
  // horizon around 5e-1. _skyExposure maps that into the renderer's
  // scene-referred range (default: mid-sky -> ~0.17, i.e. a sky the ACES chain
  // has room to work with; the near-sun horizon deliberately runs hot). The
  // baked path's SkyboxLevel governs ONLY the baked technique — procedural mode
  // never reads it, so the two sources are levelled independently.
  //
  // DISC INTENSITY is a multiplier on _sunIlluminance, NOT the physical
  // radiance ratio (solid angles put the real disc ~1.5e4 x the illuminance):
  // the default is tamed to keep the disc the brightest thing on screen without
  // flooding bloom/tonemap. Raise it toward the physical value for HDR work.
  float _sunDiscAngularRadius = 0.265f; // degrees; the physical sun is ~0.265
  float _sunDiscIntensity     = 100.0f; // x _sunIlluminance x view transmittance
  float _sunDiscLimbSoftness  = 0.25f;  // edge ramp width, as a fraction of the radius
  float _skyExposure          = 4.0f;   // procedural-sky output scale

  // MOON DISC. Presentation only, same tier as the sun disc above: no LUT
  // dependency, so it stays out of mediumHash().
  //
  // INTENSITY is absolute (x view transmittance x _skyExposure), NOT a multiple
  // of _sunIlluminance — the real ratio is ~1/400000 and no tonemap survives it.
  // The default lands the disc clearly above a moonlit sky while staying below
  // the ~2e-2 mid-day sky the LUT produces, so it washes out by day on its own
  // (the v1 policy: no daylight suppression term).
  //
  // ALBEDO carries the color AND the reflectance: the lunar regolith is a warm
  // dark gray, and the phase term multiplies it per pixel.
  float _moonDiscAngularRadius   = 0.265f; // degrees; coincidentally the sun's
  float _moonDiscIntensity       = 0.15f;
  float _moonLimbSoftness        = 0.25f; // outer edge ramp, fraction of the radius
  float _moonTerminatorSoftness  = 0.1f;  // lit/dark edge ramp, fraction of the radius
  fvec3 _moonAlbedoColor         = fvec3(0.86f, 0.83f, 0.78f);

  // NIGHT EMISSION. Presentation tier again — the sky-view LUT bake never reads
  // these, they are added where the LUT is SAMPLED (skytools skyNightEmission),
  // so they stay out of mediumHash() and both the visible sky and the IBL
  // snapshot pick them up from the one shared path.
  //
  // THE PROBLEM THEY SOLVE: the LUT is a sun-only integral. Sun below the
  // horizon, LUT converges to zero, every surface the IBL lights goes black —
  // the owner's finding. A real moonless night is not black, and these are the
  // three sources that keep it above zero. ALWAYS ON, no toggle: they are
  // additive and physically sized, so daylight drowns them by construction.
  //
  // DERIVATION of the shipped values. Radiance here is in the same units the
  // sky-view LUT produces, i.e. RELATIVE TO _sunIlluminance, where the day
  // mid-sky sits near 2e-2. The physical ladder, in cd/m^2 for a clear sky:
  //
  //    day mid-sky        5.0e+3      (engine 2e-2)
  //    full-moon sky      2.0e-2      4e-6 of day
  //    moonless night sky 2.0e-4      4e-8 of day
  //      of which airglow 1.3e-4      ~65% of it
  //      and starlight    1.3e-5      ~1/10th of the airglow
  //
  // Strictly scaled, the moonless floor is 8e-10 engine units — and THAT is the
  // number that cannot be shipped, for two independent reasons. It is 19x under
  // the RGBA16F minimum normal even after the capture pre-scale below (measured
  // headroom is what caps that factor), and the renderer has no scotopic
  // adaptation, so at any fixed exposure that shows a day sky it is exactly the
  // black these knobs exist to remove. The shipped values therefore carry a
  // DISPLAY GAIN of 1e4, applied identically to all three so the ratios above
  // survive intact — night is 7.6 stops under day instead of the physical 25.
  // A scene that has its own exposure ladder sets the physical numbers here;
  // they are data, and the gain is the only thing that is not physical.
  //
  // MOON RAYLEIGH STRENGTH converts the DECLARED moon light's illuminance (the
  // SkyBody==2 directional's phase-scaled color x intensity, which the forward
  // prologue reads into SkyFrameState) into scattered sky radiance, at that
  // same display gain. The scatter is exactly linear in the moon's illuminance
  // AND in this value (test_night_moon_ambient_gate asserts that
  // proportionality as a RATIO, so it is blind to the magnitude chosen here),
  // which makes the calibration below a single multiply.
  //
  // ITS VALUE IS SET BY THE TONEMAP'S LUMINANCE LADDER, not by how the moon
  // looks. PostFxNodeACES grades the MEASURED published environment against
  // three anchors, and its scotopic FLOOR limb exists only BELOW the twilight
  // anchor of 5.0e-4. At the previous 0.032 a moonlit night measured 1.99e-3 —
  // 4x ABOVE that anchor — so it was graded by the twilight limb (~0.5x) at
  // every floor value and displayed ~13x DARKER than a MOONLESS night, which
  // does get the floor gain. The night end of the ladder therefore moves down
  // under the anchor, here, rather than the anchor moving up to meet it.
  // Arithmetic, off the two measured legs of
  // test_scene_adaptation_luminance_gate (moonlit 1.99e-3 at 0.032, moonless
  // 1.78e-5 with the moon down):
  //
  //    moon-independent base    1.78e-5      the trio above, moon down
  //    Rayleigh term at 0.032   1.9722e-3    1.99e-3 - 1.78e-5
  //    per unit strength        6.1631e-2    1.9722e-3 / 0.032
  //    target measurement       2.5e-4       half the twilight anchor
  //    required strength        3.77e-3      (2.5e-4 - 1.78e-5) / 6.1631e-2
  //
  // Shipped at 0.004 — exactly 0.032/8, so the re-anchoring is one clean octave
  // and any re-derivation is checkable by eye. Predicted measurement is
  // 0.004 x 6.1631e-2 + 1.78e-5 = 2.64e-4: 1.9x UNDER the twilight anchor and
  // 14.8x OVER the moonless floor, so moonless << moonlit << twilight holds
  // with margin at both ends and a risen moon is graded on the same floor limb
  // the darkness around it is.
  //
  // THE fp16 COST IS NIL, which is why this is the lever and the trio is not:
  // the dim end of the capture is set by _airglowIntensity (headroom note
  // below), and the moon term at 0.004 still writes 2.5e-4 x 32 = 8.0e-3 into
  // the snapshot, ~130x the RGBA16F minimum normal. MOON_INTENSITY (the moon
  // module's declared brightness) is deliberately NOT the lever: it would dim
  // the disc and the moon's direct lighting along with the dome.
  //
  // AIRGLOW ALTITUDE is a real length, not a curve fit: the OH / O2 emission
  // layer sits at 85-100 km, and the horizon brightening is the van Rhijn
  // slant-path factor through a shell at that radius.
  float _airglowIntensity     = 5.0e-6f;  // 65% of the moonless floor
  float _starlightIntensity   = 5.0e-7f;  // 1/10th of the airglow
  float _moonRayleighStrength = 0.004f;   // 0.032/8; lands moonlit under the twilight anchor
  float _airglowAltitudeKm    = 90.0f;

  // AERIAL PERSPECTIVE / GROUND HAZE. Presentation tier, ALL of it: nothing
  // here is read by skySampleMedium or any LUT bake — the haze is a SECOND
  // medium component added under the one Beer-Lambert law at the forward
  // lighting composite, never inside the LUT integrals. So none of it joins
  // mediumHash(), and editing haze live must never trigger a re-bake.
  //
  // The geophysical part of aerial perspective (Rayleigh/Mie/ozone) already
  // comes from the medium above; these knobs are the artist layer stacked on
  // top of it. At _hazeDensity 0 the result is pure-physical — that zero is the
  // default so a scene opts INTO a graded look rather than out of one.
  //
  // The scale height is a real length (km), an e-folding of a ground-hugging
  // layer, which is why 0.3 (300 m) reads as valley haze rather than as a
  // uniform fog: it thins out well below the Mie scale height.
  //
  // _aerialPerspectiveEnable false makes forward frames byte-identical to the
  // pre-haze engine (the shader gate goes untaken), so it is a true bypass, not
  // a zeroed multiply.
  bool  _aerialPerspectiveEnable = true;
  float _hazeDensity       = 0.0f;           // 1/km extinction at layer base; 0 = pure-physical
  float _hazeScaleHeight   = 0.3f;           // km, vertical e-folding of the haze layer
  float _hazePhaseG        = 0.55f;          // HG asymmetry of the haze lobe
  fvec3 _hazeScatterTint   = fvec3(1, 1, 1); // single-scatter albedo per channel
  fvec3 _hazeInscatterTint = fvec3(1, 1, 1); // artist grade on the haze's own in-scatter
  float _hazeMaxDistanceKm = 160.0f;         // march clamp; the froxel far plane later
  // TERRAIN-SHADOWED MARCH — a three-valued MODE, not a switch. The forward
  // aerial-perspective march samples the sun cascade at its 8 step centers so
  // air standing in a caster's shadow stops in-scattering the DIRECT sun (the
  // orange-veil-in-front-of-shadowed-terrain defect); this chooses WHERE those
  // taps happen, never whether the shafts exist.
  //
  //   0 = OFF     no cascade taps at all; byte-identical to the pre-shaft march.
  //   1 = INLINE  per forward fragment, full resolution: 8 steps x 4 jittered
  //               sub-taps = 32 cascade fetches on every hazed pixel. The
  //               reference look.
  //   2 = QUARTER the shadow arm is marched once at half width x half height
  //               into its own target and subtracted back off the opaque image
  //               by a screen-space pass. Exact in radiometry (the arm is
  //               linear in the occlusion — see skyAerialPerspectiveShadowLoss),
  //               softer at shaft edges, and it does not reach transparent or
  //               unlit surfaces. See fwdnode_impl_hazeshaft.cpp.
  //
  // Rides SkyHazeInscatterTint.w — no uniform layout change. Scenes that
  // authored 1.0 keep the frame they had.
  float _hazeSunShadow     = 0.0f;
  // the three modes, named where the enum would be if a float were not already
  // the shipped storage and the shader's own encoding.
  static constexpr float kHazeSunShadowOff     = 0.0f;
  static constexpr float kHazeSunShadowInline  = 1.0f;
  static constexpr float kHazeSunShadowQuarter = 2.0f;
  // SHAFT GAIN — how hard the shafts are cut, artistically. 1.0 is the physical
  // answer and is byte-identical to having no knob at all; above it the shafts
  // deepen, below it they wash out, and 0 removes them while leaving the mode
  // armed. It scales the OCCLUSION, never the ambient: the per-step lit fraction
  // is remapped 1 - gain*(1 - s), clamped at zero so a gain over 1 deepens the
  // shadow to fully dark and stops there rather than driving the direct term
  // negative. Because the remap happens ONCE, in the shared per-step tap
  // (lib_sun_air), inline and quarter-res modes cannot disagree about it — which
  // is the whole point of a knob whose job is to be A/B'd across those modes.
  // Rides SkyHazeGeom.w, which was reserved padding; no block grew.
  // Not clamped here beyond non-negative: over-driving is a legitimate look.
  float _hazeSunShadowGain = 1.0f;

  // IBL FEED (slice B3, the LAGGED tier of §2). Also outside mediumHash() — a
  // medium edit is a REFILTER TRIGGER, not one of these knobs.
  //
  // SNAPSHOT EXTENT: the prefilter renders all 10 specular roughness levels at
  // the SOURCE dimensions (no downsampling, radiancemaps_processor.cpp
  // initEnvFilterState), so 4k parity with baked assets would cost ten 4096x2048
  // RGBA32F passes per cycle. Per-slice cost is LINEAR in snapshot area.
  //
  // HISTORY, because the numbers here look like they contradict each other:
  // COMFORT-1 measured a 17-19ms worst slice at 512x256 against an 11ms VR
  // frame and dropped the default to 256x128 to cut the area 4x. That
  // measurement predates COMFORT-2, which cut the cycle into a per-level slice
  // plan drained one slice per frame — the worst slice is now the largest
  // INDIVISIBLE step (a level readback, a level's upload mip chain), not a
  // whole level chain. So 512x256 is no longer the ceiling that verdict made it
  // and is the default; 256x128 stays reachable through this knob for a budget
  // that needs it. Extent moves per-slice COST, barely the slice COUNT: the
  // step plan is per level (tiles are batched to levelBatchCount, 1 by
  // default), so doubling each side adds only the one extra diffuse mip its
  // chain now has — 56 slices at 256x128, 58 at 512x256. Longer cycles at
  // constant per-frame cost is the amortize law working, not a regression.
  //
  // The format is fixed float (RGBA8 would clip the HDR sky before the filter
  // ever samples it). Height is independent, not derived: the equirect
  // convention wants 2:1, but the pair is reflected separately because the
  // filter never assumes the ratio and a test may want it broken.
  //
  // SAMPLE COUNTS: importance samples per filtered texel, the other linear term
  // in slice cost. The BAKED path keeps EnvMapProcessor's bake-time 8192/4096
  // (those bytes are blessed); the procedural feed runs leaner — the extra
  // samples buy detail a sky-view-LUT resample does not carry at any extent
  // this feed runs at.
  //
  // REFILTER ANGLE: how far the sun must move (degrees) before a new cycle is
  // worth its cost. The lag this trades away is the accepted L3 trade; raise it
  // for a cheaper feed, lower it for a tighter-tracking one. This threshold is
  // the CHAINING-OFF policy only (see below).
  //
  // CONTINUOUS CHAINING: with chaining ON the feed stops waiting for a
  // keyframe-sized sun step and instead runs back-to-back cycles — a new one
  // starts the moment the previous fade has finished (never DURING it: two live
  // fades would blend three map sets) and the sun has moved at least
  // _iblChainMinAngleDeg. On a moving sun the fades then abut, which is what
  // turns the feed from 2-degree keyframe jumps into piecewise-linear
  // interpolation. The fade window is auto-sized to the OBSERVED cycle cadence
  // in that mode (SkyIblState::autoFadeFrames), so it tracks whatever the
  // hardware actually delivers instead of a fixed guess. Chaining OFF restores
  // the _iblRefilterAngleDeg threshold + the fixed _iblCrossfadeFrames window.
  //
  // CHAIN CADENCE CEILING: chaining's only pacing is the fade + the angle floor,
  // so on a fast day cycle the feed runs as many cycles per second as the frame
  // loop physically allows (measured ~10/s at 2560x1280 offscreen). This is the
  // rate limiter for that: the maximum rate, in Hz, at which a CHAINED cycle may
  // start, measured start-to-start. The 2.0 default is the MEASURED slowest rate
  // that shows neither lag nor pop at the shipped 512x256: a cycle is 143 frames,
  // so the natural cadence is fps/143, which under perf load on a 5090 measured
  // 1.87-1.90 Hz. 2 Hz therefore reproduces that shipped pacing identically and
  // only binds above ~286 fps, while 1 Hz doubles the per-refresh sun jump and
  // pops; a 90 fps VR run sits at 0.63 Hz natural, far under the ceiling. 0 =
  // uncapped, the pre-knob behavior, now an explicit opt-in.
  // Only the sun-motion trigger is paced — a medium edit and the first-ever
  // snapshot are still immediate (the law above), and the chaining-OFF keyframe
  // policy is untouched.
  //
  // CROSSFADE WINDOW: how many frames the outgoing filtered maps stay blended
  // against the new ones after a cycle publishes. A publish replaces the whole
  // IBL in one frame, which reads as an ambient/specular pop on lit geometry;
  // this is the ramp that hides it. 0 restores the hard swap. The window costs
  // one extra pair of env samples per shaded fragment while it runs, and its
  // upper bound is a taste call, not a correctness one.
  //
  // CROSSFADE MAX SECS: the ramp's WALL-CLOCK bound, and the one that decides
  // how long a fade is perceived to last. Frames are not a duration — the
  // chained window is auto-sized to the measured cycle SPAN in frames
  // (SkyIblState::autoFadeFrames), so at 1300 fps offscreen that span is
  // hundreds of frames and a consumer sampling the lighting shortly after a
  // publish reads a half-faded blend. The fade therefore completes at whichever
  // bound lands FIRST: frames-remaining or this budget. 0 = no wall-clock bound
  // (frames only, the behavior before this knob); _iblCrossfadeFrames = 0 still
  // disables the fade outright, and nothing here can resurrect it.
  //
  // SNAPSHOT INTERVAL: a DECLARED wall-clock cadence, in seconds, and the only
  // knob in this block that REPLACES a policy rather than tuning one. Above zero
  // it supersedes the sun-motion trigger entirely — both the chaining floor
  // (_iblChainMinAngleDeg + _iblChainMaxHz) and the chaining-OFF keyframe
  // threshold (_iblRefilterAngleDeg) — with "at least this many seconds since the
  // last cycle STARTED". 0 = disabled = the sun-angle behavior described above,
  // which stays the default and is what every measurement predating this knob
  // was taken under.
  //
  // WHY a clock rather than an angle: the sun-motion triggers make the rebake
  // RATE a function of the scene's day-cycle rate, so a fast clock buys bursts of
  // full-cost cycles the frame loop never asked for. A scene that would rather
  // pay a known cadence than track the sun tightly declares it here; what it
  // trades away is IBL lag on a moving sun, which is this knob's entire cost.
  //
  // The clock is start-to-start (SkyIblState::_chain_timer, restarted by
  // beginCycle) and the declaration is FLOORED at the measured cycle+fade span
  // (SkyIblState::cadenceFloorSecs), because a cycle starting mid-fade pops. So
  // the ACHIEVED cadence is max(declared, measured floor): above the floor the
  // declared rate holds exactly, below it the feed runs at the floor and logs the
  // shortfall once with both numbers — it never clamps in silence. A medium edit
  // and the first-ever snapshot stay immediate in every mode.
  //
  // IBL GRANULARITY (the last three): how the sliced prefilter CUTS UP its work,
  // never what it produces — the plan filters the same pixels at any grain
  // (determinism law, §5). LEVEL BATCHES splits each level's tile grid into that
  // many GPU submits; SLICES PER FRAME caps how many of this job's slices one
  // frame may run (GpuMicrotask::_maxSlicesPerFrame); MIPCHAIN BUDGET PX bounds
  // one publish-chain slice's CPU downsample, in output pixels. All three are
  // EXTENT-DEPENDENT measurements — which is the whole reason they are knobs —
  // and the shipped 1 / 1 / 12288 were measured at the 512x256 snapshot.
  //
  // 0 = UNSET on all three, and unset is the default: the value then resolves to
  // the ORKID_MT_IBL_* env override, else the built-in default (all three live in
  // radiancemaps_processor.cpp, which is also where the measurements are written
  // down). So the env vars keep working as the fallback, the bake path — which
  // has no atmosphere to read and must not gain one — is untouched, and a
  // property above zero beats both. 0 is not expressible THROUGH those env
  // parsers (each floors at 1), which is what makes it a safe sentinel here.
  // Note _maxSlicesPerFrame's own 0 means "no per-task limit": the sentinel is
  // resolved before that field is assigned, so unbounded is deliberately NOT
  // reachable by setting this property to 0.
  // CAPTURE PRE-SCALE: the gain the equirect snapshot is WRITTEN at, divided
  // back out at every env read (shader lib_env_decode). Both the snapshot and
  // the prefiltered maps are RGBA16F, whose minimum NORMAL is 6.1e-5, and a
  // night sky lands under it — measured, the sun at -12 degrees produced a peak
  // near 1e-4 and prefiltered to EXACTLY zero. The gain lifts that into range;
  // the divide takes it straight back out, so nothing downstream sees a
  // different number.
  //
  // HEADROOM, which is what picks the value: fp16 tops out at 65504, so the
  // ceiling is on the PRODUCT of this and _skyExposure. Measured day peak in
  // the snapshot at the shipped exposure of 4 is 0.82 (sun at 20 degrees), so
  // 32 uses 26 of 65504 — about 2400x of room, enough for the near-horizon sun
  // that is several times brighter still. At the bottom, the shipped airglow
  // (_airglowIntensity 5.0e-6) reaches 1.6e-4 post-scale, 2.6x the 6.1e-5
  // minimum normal — the thinnest margin in the capture, and the reason the
  // night-emission trio is not a free knob. A scene running an extreme
  // _skyExposure has to bring this down by the same factor.
  float _iblCaptureScale   = 32.0f;
  int _iblCrossfadeFrames  = 15;
  float _iblCrossfadeMaxSecs = 0.5f; // wall-clock cap on the fade; 0 = frames only
  int _iblSnapshotWidth    = 512;
  int _iblSnapshotHeight   = 256;
  int _iblSpecularSamples  = 2048;
  float _iblRefilterAngleDeg = 2.0f;
  bool _iblFeedEnable        = true;
  bool _iblContinuousChain   = true;
  float _iblChainMinAngleDeg = 0.5f;
  float _iblChainMaxHz       = 2.0f;
  float _iblSnapshotInterval = 0.0f; // seconds; 0 = the sun-angle triggers above
  int _iblLevelBatches     = 0;      // 0 = unset -> env / built-in default (1)
  int _iblSlicesPerFrame   = 0;      // 0 = unset -> env / built-in default (1)
  int _iblMipChainBudgetPx = 0;      // 0 = unset -> env / built-in default (12288)
};

////////////////////////////////////////////////////////////////////////////////
// SkyFrameState — what the forward prologue's LUT step publishes for the rest
// of the frame (RCFD user property "SKY_FRAME"). It carries the sun direction
// and view altitude the sky-view LUT was ACTUALLY baked with this frame, so the
// consumers (visible skybox now; aerial perspective / IBL snapshot later) can
// never re-derive a sun that disagrees with the LUT they are sampling.
////////////////////////////////////////////////////////////////////////////////

struct SkyFrameState {
  texture_ptr_t _skyViewLUT;
  texture_ptr_t _transmittanceLUT;
  // the multi-scatter LUT the two above were baked against — the aerial
  // perspective march folds it in so haze in shadow does not go black.
  texture_ptr_t _multiScatterLUT;
  fvec3 _dirToSun       = fvec3(0, 1, 0);
  // TOWARD the moon, same convention as _dirToSun. ZERO means the scene declared
  // no moon (LightData SkyBody==2) — the visible disc is gated on it, and there
  // is no fallback direction because an invented moon is a wrong sky.
  fvec3 _dirToMoon      = fvec3(0, 0, 0);
  // The declared moon light's ILLUMINANCE at the top of the atmosphere, in
  // _sunIlluminance units: its color times its intensity, read off the same
  // SkyBody==2 directional _dirToMoon came from. That intensity is where a
  // scene's lunar phase scaling lives, so the moonlight the sky scatters waxes
  // and wanes with the light the scene already drives. Zero with no moon.
  fvec3 _moonIlluminance = fvec3(0, 0, 0);
  float _viewAltitudeKm = 0.0f;
};

////////////////////////////////////////////////////////////////////////////////
// HillaireSky — the GPU-side LUT chain. Owns its render targets and the
// `orkshader://sky` material; every pass is a fullscreen quad.
//
// FRAME CONTRACT: bakeStaticLuts / updateSkyView record into the caller's
// command buffer and therefore MUST be called between Context::beginFrame and
// Context::endFrame (the compositor prologue already is; a standalone caller
// opens its own frame, as LightProbe::exportEquirectangular does).
////////////////////////////////////////////////////////////////////////////////

struct HillaireSky {

  static hillairesky_ptr_t create(Context* ctx);

  // Re-bakes transmittance + multi-scatter only when `atmo`'s medium hash moves.
  // Returns true when a bake actually ran this call.
  bool bakeStaticLuts(Context* ctx, rcfd_ptr_t RCFD, skyatmospheredata_ptr_t atmo);

  // Per-frame sky-view LUT. `dir_to_sun` points TOWARD the sun (world, Y-up) —
  // the negation of a DirectionalLight's travel direction. `view_altitude_km`
  // is the CENTER-EYE altitude above the ground sphere.
  void updateSkyView(
      Context* ctx,                    //
      rcfd_ptr_t RCFD,                 //
      skyatmospheredata_ptr_t atmo,    //
      const fvec3& dir_to_sun,         //
      float view_altitude_km);         //

  // SKYLIGHT slice B3 — resample this frame's sky-view LUT into the persistent
  // equirect SNAPSHOT the sliced radiance prefilter consumes. Call ONLY at
  // refilter-cycle start (§2 snapshot law); the caller owns that gate, since
  // only it knows whether a job is still in flight. `dir_to_sun` /
  // `view_altitude_km` must be the SKY_FRAME values the LUT was baked with this
  // frame — re-deriving them here is how the IBL and the visible sky would
  // drift apart. The RTG is (re)allocated whenever the atmosphere's snapshot
  // extent changes, which is safe only because no job can be running.
  //
  // `dir_to_moon` / `moon_illuminance` are the SKY_FRAME moon (zero direction =
  // none declared). They are here rather than derived because the snapshot now
  // carries the night emission, and the moonlight term in it must be the same
  // moon the visible sky scatters.
  void renderEquirectSnapshot(
      Context* ctx,                    //
      rcfd_ptr_t RCFD,                 //
      skyatmospheredata_ptr_t atmo,    //
      const fvec3& dir_to_sun,         //
      float view_altitude_km,          //
      const fvec3& dir_to_moon,        //
      const fvec3& moon_illuminance);  //

  texture_ptr_t transmittanceTexture() const;
  texture_ptr_t multiScatterTexture() const;
  texture_ptr_t skyViewTexture() const;
  texture_ptr_t equirectSnapshotTexture() const;

  void _init(Context* ctx);
  void _applyLutSampling(Context* ctx);
  void _bindAtmosphere(skyatmospheredata_ptr_t atmo, int lut_w, int lut_h);
  void _renderLut(
      Context* ctx,                    //
      rcfd_ptr_t RCFD,                 //
      const FxShaderTechnique* tek,    //
      rtgroup_ptr_t rtg,               //
      const void_lambda_t& bind_params);

  static constexpr int kTransmittanceW = 256;
  static constexpr int kTransmittanceH = 64;
  static constexpr int kMultiScatterW  = 32;
  static constexpr int kMultiScatterH  = 32;
  static constexpr int kSkyViewW       = 192;
  static constexpr int kSkyViewH       = 108;

  rtgroup_ptr_t _rtgTransmittance;
  rtgroup_ptr_t _rtgMultiScatter;
  rtgroup_ptr_t _rtgSkyView;
  rtgroup_ptr_t _rtgEquirect; // slice B3 IBL snapshot; null until the first cycle

  freestyle_mtl_ptr_t _material;
  const FxShaderTechnique* _tekTransmittance = nullptr;
  const FxShaderTechnique* _tekMultiScatter  = nullptr;
  const FxShaderTechnique* _tekSkyView       = nullptr;
  const FxShaderTechnique* _tekEquirect      = nullptr;

  fxparam_constptr_t _parRayleighScatter = nullptr;
  fxparam_constptr_t _parMieScatter      = nullptr;
  fxparam_constptr_t _parOzoneAbsorb     = nullptr;
  fxparam_constptr_t _parOzoneTent       = nullptr;
  fxparam_constptr_t _parRadii           = nullptr;
  fxparam_constptr_t _parGroundAlbedo    = nullptr;
  fxparam_constptr_t _parSunIlluminance  = nullptr;
  fxparam_constptr_t _parSunDirection    = nullptr;
  fxparam_constptr_t _parLutDims         = nullptr;
  fxparam_constptr_t _parSunDisc         = nullptr;
  fxparam_constptr_t _parMoonDirection   = nullptr;
  fxparam_constptr_t _parMoonDisc        = nullptr;
  fxparam_constptr_t _parNightEmission   = nullptr;
  fxparam_constptr_t _parMoonIlluminance = nullptr;
  fxparam_constptr_t _parTransmittanceLut = nullptr;
  fxparam_constptr_t _parMultiScatterLut  = nullptr;
  fxparam_constptr_t _parSkyViewLut       = nullptr;
  // the artist haze layer; read by the equirect snapshot pass ALONE (the LUT
  // bakes are the medium, and the haze is not in it)
  fxparam_constptr_t _parHazeDensity       = nullptr;
  fxparam_constptr_t _parHazeScatterTint   = nullptr;
  fxparam_constptr_t _parHazeInscatterTint = nullptr;
  fxparam_constptr_t _parHazeGeom          = nullptr;

  uint64_t _bakedMediumHash = 0;
  bool _staticLutsValid     = false;
  bool _lutSamplingApplied  = false;
  int _snapshotW            = 0; // extent _rtgEquirect was allocated at
  int _snapshotH            = 0;
};

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::pbr
////////////////////////////////////////////////////////////////////////////////
