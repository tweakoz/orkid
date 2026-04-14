#pragma once
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

// ProjectedGridDrawableData
//
// Concentric LOD-ring ground mesh centered on the camera.
//   LOD 0 is a solid _lod0_cells × _lod0_cells square of cells at _lod0_cell_size.
//   LOD k>0 is an annular ring around LOD k-1 with the cell size doubled and
//            the inner (_lod0_cells/2 × _lod0_cells/2) region removed so the
//            rings nest without overlap.
//
// Total visible half-extent ≈ (_lod0_cells * _lod0_cell_size * 2^(_lod_count-1)) / 2.
// _lod0_cells MUST be a multiple of 4 so the hole aligns with cell boundaries.
//
// The vertex buffer is V12T8 in camera-local world xz (y=0). The shader adds
// the camera's world xz (snapped to the coarsest LOD cell size) to follow the
// camera without crawling under sub-snap motion. uv0.x encodes the LOD index
// normalized into [0,1) for debug visualization.

struct ProjectedGridDrawableData final : public DrawableData {

  DeclareConcreteX(ProjectedGridDrawableData, DrawableData);

public:
  drawable_ptr_t createDrawable() const final;
  ProjectedGridDrawableData();
  ~ProjectedGridDrawableData();

  fxpipeline_ptr_t _pipeline_color;
  pbrmaterial_ptr_t _material;

  float _lod0_cell_size = 0.5f;
  int   _lod0_cells     = 32;  // per-axis, must be multiple of 4
  int   _lod_count      = 7;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
