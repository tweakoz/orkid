////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/lev2_types.h>
#include <ork/math/cvector3.h>
#include <atomic>

////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
////////////////////////////////////////////////////////////////////////////////
// VrHudOverlay : render-thread bridge that lets a host draw a debug HUD into a VR
//  headset. The host renders the HUD content into an offscreen RT and publishes
//  its texture here; the VR output node (SinglePassStereoVr) reads it and draws ONE
//  head-locked textured quad into EACH eye's final buffer (natural stereo
//  disparity places it at _distance_m in front of the head). Both the XR handoff
//  and the desktop mirror inherit it.
//
//  THREADING: written by the host in its onDraw and read by the VR composite of
//  the following frame — both on the render thread, sequential across frames, so
//  no locking is needed (_enabled is atomic only to be tidy about the host toggle
//  edge that also flips it).
////////////////////////////////////////////////////////////////////////////////
struct VrHudOverlay {
  static VrHudOverlay& instance();

  // THE PANEL SLATE, in one place. The VR output node draws it as a straight-ALPHA quad
  // under the premultiplied text (the split FontMan's glyph blend forces), and the
  // DESKTOP HUD draws the same rgba behind its own text block — so the two presentations
  // are the same panel, and re-tinting it moves both. rgb + a, straight alpha.
  float _slate_rgba[4] = {0.0f, 0.0f, 0.0f, 0.5f};

  std::atomic<bool> _enabled{false}; // false => the VR node draws nothing (zero cost)
  texture_ptr_t     _texture;        // panel content (nullptr => nothing to draw)
  float             _aspect     = 1.0f; // panel content width / height
  float             _distance_m = 3.0f; // head-relative -Z of the panel center (meters)
  float             _width_m    = 2.0f; // panel width in meters (height follows _aspect)
  float             _yoffset_frac = 0.15f; // drop the panel this fraction of the FRAME height
                                           // below center (owner call: HUD sits low, keeps the
                                           // view clear; 0 = centered, negative = raise)

  // VR camera diagnostics (terrain-invisibility hunt): written per-frame by the VR
  //  output node where the matrices exist, read by the host HUD's stats text. Both
  //  on the render thread within a frame, so plain fvec3 (no locking).
  //  _cam_root: the world-root (vrroot) translation actually used this frame — (0,0,0)
  //             is the vrroot-lookup-miss smoking gun.
  //  _cam_eye:  the composed center-eye WORLD position (center inverse-view translation).
  fvec3 _cam_root;
  fvec3 _cam_eye;
};

inline VrHudOverlay& VrHudOverlay::instance() {
  static VrHudOverlay s;
  return s;
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
////////////////////////////////////////////////////////////////////////////////
