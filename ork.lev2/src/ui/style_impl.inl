////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// NOTE: This file is included INSIDE namespace ork::ui { }
// Do not add namespace declarations here

///////////////////////////////////////////////////////////////////////////////
// ThemeEngine Implementation - shared across style.cpp and style_sdf_prims.cpp
///////////////////////////////////////////////////////////////////////////////

struct ThemeEngineImpl {
  lev2::freestyle_mtl_ptr_t _sdf_material;
  lev2::fxshader_ptr_t _sdf_shader;

  // Cached technique handles
  lev2::fxtechnique_constptr_t _sdf_box_tek;
  lev2::fxtechnique_constptr_t _sdf_box_per_corner_tek;
  lev2::fxtechnique_constptr_t _sdf_circle_tek;
  lev2::fxtechnique_constptr_t _sdf_triangle_tek;
  lev2::fxtechnique_constptr_t _sdf_ring_tek;

  // Cached parameter handles (shared across all techniques)
  lev2::fxparam_constptr_t _param_mvp = nullptr;
  lev2::fxparam_constptr_t _param_modcolor = nullptr;
  lev2::fxparam_constptr_t _param_box_size = nullptr;
  lev2::fxparam_constptr_t _param_box_pos = nullptr;
  lev2::fxparam_constptr_t _param_corner_radius = nullptr;
  lev2::fxparam_constptr_t _param_border_width = nullptr;
  lev2::fxparam_constptr_t _param_fill_color = nullptr;
  lev2::fxparam_constptr_t _param_border_color = nullptr;
  lev2::fxparam_constptr_t _param_corner_radii = nullptr;
  lev2::fxparam_constptr_t _param_shape_param = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
