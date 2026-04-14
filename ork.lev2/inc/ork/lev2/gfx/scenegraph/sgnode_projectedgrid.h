#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

// ProjectedGridDrawableData
//
// A ground-plane primitive whose vertices live in parameter space [0,1]^2.
// Shaders consume it by bilerping across the 4 world-space frustum/plane
// corners provided by the RCFD_GROUND_FRUSTUM_CORNERS named-param provider,
// giving a screen-uniform tessellation density without wasted triangles.
//
// The vertex buffer is V12T8 (position + uv0). Position stores (u, 0, v),
// uv0 stores (u, v). Normals/tangents must be synthesized in the shader.

struct ProjectedGridDrawableData final : public DrawableData {

  DeclareConcreteX(ProjectedGridDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  ProjectedGridDrawableData();
  ~ProjectedGridDrawableData();

  fxpipeline_ptr_t _pipeline_color;
  pbrmaterial_ptr_t _material;
  int _griddim = 128;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
