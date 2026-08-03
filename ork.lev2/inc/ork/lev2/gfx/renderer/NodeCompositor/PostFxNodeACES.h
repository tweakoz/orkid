////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "NodeCompositor.h"

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

class PostFxNodeACES : public PostCompositingNode {
  DeclareConcreteX(PostFxNodeACES, PostCompositingNode);

public:
  PostFxNodeACES();
  ~PostFxNodeACES();

  // THE SCENE'S AUTHORED EXPOSURE. Reflected, serialized, and NEVER written at
  // runtime — the adaptation below composes with it multiplicatively instead of
  // replacing it, so a scene that wrote down an exposure still has it in the
  // frame. (An earlier drive overwrote this field every tick, which is why the
  // rule is stated rather than assumed.)
  float _exposure = 1.0f;

  /////////////////////////////////////////////////////////////////////////////
  // SCENE ADAPTATION — the CONTENT-driven half of exposure, per frame, from the
  // measured available light (CommonStuff::availableLightLuminance). It is not
  // display compensation: that is a property of the hardware, is constant for
  // that hardware, and lives on the OUTPUT node
  // (OutputCompositingNode::_displayCompensation). Dialling one to fix the
  // other is the bug this split exists to make impossible.
  //
  // THREE ANCHORS in log10(luminance), owner-calibrated against rendered
  // frames, joined by smoothsteps so the term is C1 everywhere:
  //   DAY      full daylight            -> 1.0, the identity: a day frame is
  //                                       graded by the tone curve alone
  //   TWILIGHT the early-night reading  -> the compression the owner tuned;
  //                                       night sits BELOW day, it is not
  //                                       lifted
  //   FLOOR    dead of night, when the  -> the SCOTOPIC opening: a large gain,
  //            only light left is the      because the renderer models no dark
  //            night-emission trio         adaptation of its own (see below)
  // Between FLOOR and TWILIGHT the term DECREASES as the scene gets brighter —
  // the sign every adaptation must have, and the one a moonrise exercises.
  // The luminance anchors are the ladder this engine MEASURES, not estimates:
  // sun 60 deg up 9.4e-2, sun 4 deg down 5.5e-4, moonlit night 2.6e-4,
  // moonless floor 1.8e-5 (test_scene_adaptation_luminance_gate prints all four
  // legs). A MOONLIT night sits between the twilight anchor and the floor
  // anchor BY DESIGN, so the whole night — moon up or down — is graded on the
  // FLOOR..TWILIGHT limb; see the note below the anchors.
  //
  // THE FLOOR VALUE IS A SCENE DECISION, and this default deliberately does NOT
  // make it. What is measured and settled:
  //
  //  * a MOONLESS sky reaches this stage at ~4.5e-5 of scene-referred radiance,
  //    and through the ACES curve at a gain near unity that is 0.002 of ONE
  //    8-BIT STEP. The frame quantizes to black and the star field goes with it
  //    (measured on scn_nightcal, no grade: sky mean 0.205 of a step — which is
  //    the output dither, not the sky — and 20 star pixels in 1280x720).
  //  * the RADIANCE-side answers do not work. Raising the night-emission trio
  //    (sky_atmosphere.h) by 32x moved that sky mean by 0.02 of a step, because
  //    the measurement it feeds walks this term DOWN by nearly the same factor,
  //    and it halved the visible star field — the star contrast law thresholds
  //    against that same measured sky.
  //  * the DISPLAY-side answer works, and it is this anchor: at 512 the same
  //    frame reads sky 4.5, terrain 1.9, 75k star pixels — a night sky with a
  //    skyline in it (Scene.sky(tonemap={"adapt_floor": 512}), which is what
  //    scn_nightcal declares).
  //  * it is NOT SAFE AS A GLOBAL DEFAULT YET, which is why this stays where it
  //    was. On a CLOUDED night the decks' radiance is not the sky's: their
  //    moonless-night floor (_cloud_deck.py night_floor = 0.03 of the authored
  //    colors) leaves a night cloud ~300x the night SKY's radiance, so the same
  //    512 that develops a clear night correctly clips a clouded one to white —
  //    measured on one rig, one hour, clouds the only difference: clear sky 4.5,
  //    clouded 164.9, and scn_forest_procsky (all cloud, no visible stars) goes
  //    to a 200-mean white frame. Until night content tracks the sky, the anchor
  //    is authored per scene.
  //
  // IT CANNOT DISTURB THE EARLY NIGHT, by construction, not by tuning: this
  // anchor only exists on the FLOOR..TWILIGHT limb, so every luminance at or
  // above the twilight anchor evaluates through _adaptTwilight/_adaptDay alone.
  // The owner-accepted first half of the night is bit-identical across any value
  // here (proven per-pixel in test_night_display_visibility_gate).
  //
  // IT NOW REACHES A MOONLIT NIGHT, and that took a change on the OTHER side.
  // The moon's whole-dome Rayleigh scatter used to lift the moonlit sky's
  // MEASURED luminance to 1.99e-3, 4x ABOVE the twilight anchor, so a risen
  // moon was graded by the twilight/day limb (~0.5) at EVERY value of this
  // anchor and displayed darker than the moonless night beside it — the one
  // state the floor was for was the one it could not see. The fix was to move
  // the night end of the LUMINANCE ladder back down under twilight
  // (_moonRayleighStrength 0.032 -> 0.004, derived in sky_atmosphere.h;
  // measured moonlit ~2.6e-4, half the anchor), NOT a bigger number here. That
  // doctrine stands: this anchor is a DISPLAY gain, and a display gain cannot
  // fix a scene whose measured luminance lands on the wrong limb. The fp16
  // headroom cost that objection anticipated came out nil — the capture's dim
  // end is set by the airglow constant, which this did not touch.
  float _adaptDayLuminance      = 1.0e-1f;
  float _adaptTwilightLuminance = 5.0e-4f;
  float _adaptFloorLuminance    = 1.8e-5f;
  float _adaptDay               = 1.0f;
  float _adaptTwilight          = 0.5f;
  float _adaptFloor             = 0.65f;

  // Pure, so it is testable without a frame. `luminance` NEGATIVE means nothing
  // has been measured yet; pass sin(sun elevation) as the seed and the warm-up
  // window lands on the DAY or the FLOOR anchor instead of on a black frame.
  float sceneAdaptation(float luminance, float seed_sun_elevation_sin) const;

  void doGpuInit(lev2::Context* pTARG, int w, int h) final;
  void DoRender(CompositorDrawData& drawdata) final;

  lev2::rtbuffer_ptr_t GetOutput() const final;
  lev2::rtgroup_ptr_t GetOutputGroup() const final;
  svar256_t _impl;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
