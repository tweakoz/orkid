////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/shadman.h>
#include <vector>

namespace ork::ui {

struct PrimCanvas;

////////////////////////////////////////////////////////////////////
// QuadData: Per-quad instance data stored in SSBO
// This struct must match the GLSL layout (4 vec4s = 64 bytes per quad)
////////////////////////////////////////////////////////////////////

struct QuadData {
  fvec4 pos_size;    // xy = position (top-left), zw = size (width, height)
  fvec4 uv_rect;     // xy = uv_min, zw = uv_max
  fvec4 color;       // rgba
  fvec4 extra;       // x = rotation (radians), y = corner_radius, zw = reserved

  QuadData()
      : pos_size(0, 0, 100, 100)
      , uv_rect(0, 0, 1, 1)
      , color(1, 1, 1, 1)
      , extra(0, 0, 0, 0) {}
};
using quaddata_ptr_t = std::shared_ptr<QuadData>;

////////////////////////////////////////////////////////////////////
// VertexData: Per-vertex data for triangle primitives
// Must match GLSL layout (4 vec4s = 64 bytes per vertex)
////////////////////////////////////////////////////////////////////

struct VertexData {
  fvec4 position;    // xy = position, zw = reserved
  fvec4 uv;          // xy = uv, zw = reserved
  fvec4 color;       // rgba
  fvec4 extra;       // reserved

  VertexData()
      : position(0, 0, 0, 1)
      , uv(0, 0, 0, 0)
      , color(1, 1, 1, 1)
      , extra(0, 0, 0, 0) {}
};
using vertexdata_ptr_t = std::shared_ptr<VertexData>;

////////////////////////////////////////////////////////////////////
// Primitive: Base class for renderable primitives
////////////////////////////////////////////////////////////////////

struct Primitive {
  virtual ~Primitive() = default;
  virtual void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd) = 0;
  virtual size_t ssboQuadCount() const { return 0; }
  virtual void gatherQuadData(std::vector<QuadData>& out) const {}

  size_t _ssbo_offset = 0;  // Set by PrimCanvas during SSBO layout
};
using primitive_ptr_t = std::shared_ptr<Primitive>;

////////////////////////////////////////////////////////////////////
// QuadPrimitive: A batch of quads sharing pipeline/texture state
////////////////////////////////////////////////////////////////////

struct QuadPrimitive : Primitive {
  QuadPrimitive(lev2::fxpipeline_ptr_t pipeline, lev2::texture_ptr_t texture = nullptr);

  lev2::fxpipeline_ptr_t _pipeline;
  lev2::texture_ptr_t _texture;
  std::vector<quaddata_ptr_t> _quads;

  void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd) override;
  size_t ssboQuadCount() const override { return _quads.size(); }
  void gatherQuadData(std::vector<QuadData>& out) const override;
};
using quadprimitive_ptr_t = std::shared_ptr<QuadPrimitive>;

////////////////////////////////////////////////////////////////////
// TriStripPrimitive: Triangle strip sharing pipeline/texture state
////////////////////////////////////////////////////////////////////

struct TriStripPrimitive : Primitive {
  TriStripPrimitive(lev2::fxpipeline_ptr_t pipeline, lev2::texture_ptr_t texture = nullptr);

  lev2::fxpipeline_ptr_t _pipeline;
  lev2::texture_ptr_t _texture;
  std::vector<vertexdata_ptr_t> _vertices;

  void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd) override;
  size_t ssboQuadCount() const override { return _vertices.size(); }
  void gatherQuadData(std::vector<QuadData>& out) const override;
};
using tristripprimitive_ptr_t = std::shared_ptr<TriStripPrimitive>;

////////////////////////////////////////////////////////////////////
// TriListPrimitive: Triangle list sharing pipeline/texture state
////////////////////////////////////////////////////////////////////

struct TriListPrimitive : Primitive {
  TriListPrimitive(lev2::fxpipeline_ptr_t pipeline, lev2::texture_ptr_t texture = nullptr);

  lev2::fxpipeline_ptr_t _pipeline;
  lev2::texture_ptr_t _texture;
  std::vector<vertexdata_ptr_t> _vertices;

  void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd) override;
  size_t ssboQuadCount() const override { return _vertices.size(); }
  void gatherQuadData(std::vector<QuadData>& out) const override;
};
using trilistprimitive_ptr_t = std::shared_ptr<TriListPrimitive>;

////////////////////////////////////////////////////////////////////
// TextItem: Single text entry within a TextPrimitive
////////////////////////////////////////////////////////////////////

struct TextItem {
  std::string text;
  fvec2 position;
};

////////////////////////////////////////////////////////////////////
// TextPrimitive: Text rendered with FontMan (collection sharing state)
////////////////////////////////////////////////////////////////////

struct TextPrimitive : Primitive {
  TextPrimitive(lev2::font_ptr_t font, fvec4 color = fvec4(1, 1, 1, 1));

  lev2::font_ptr_t _font;
  fvec4 _color;
  std::vector<TextItem> _items;

  void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd) override;
};
using textprimitive_ptr_t = std::shared_ptr<TextPrimitive>;

////////////////////////////////////////////////////////////////////
// PrimCanvas: GPU-accelerated canvas widget
// - Ordered list of primitives rendered painter's algorithm
// - Quad data in SSBO, mappable from Python
// - All input events routed to Python callbacks
////////////////////////////////////////////////////////////////////

struct PrimCanvas : public Widget {
  PrimCanvas(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~PrimCanvas();

  //////////////////////////////////////////////////////////////
  // Primitive management
  //////////////////////////////////////////////////////////////

  void clear();
  void addPrimitive(primitive_ptr_t prim);
  size_t primitiveCount() const { return _primitives.size(); }
  primitive_ptr_t primitive(size_t index) const;

  // Mark SSBO as dirty (needs upload to GPU)
  void markDirty() { _ssbo_dirty = true; }

  //////////////////////////////////////////////////////////////
  // Event callbacks (set from Python)
  //////////////////////////////////////////////////////////////

  std::function<HandlerResult(event_constptr_t)> _onUiEvent;

  //////////////////////////////////////////////////////////////
  // Appearance
  //////////////////////////////////////////////////////////////

  fvec4 _bg_color = fvec4(0.1f, 0.1f, 0.1f, 1.0f);
  bool _draw_background = true;

  //////////////////////////////////////////////////////////////
  // GPU initialization and pipeline access
  //////////////////////////////////////////////////////////////

  void gpuInit(lev2::Context* ctx);
  lev2::fxpipeline_ptr_t pipelineSolid() const { return _pipeline_solid; }
  lev2::fxpipeline_ptr_t pipelineTextured() const { return _pipeline_textured; }
  lev2::fxpipeline_ptr_t pipelineVtxSolid() const { return _pipeline_vtx_solid; }
  lev2::fxpipeline_ptr_t pipelineVtxTextured() const { return _pipeline_vtx_textured; }
  lev2::FxShaderStorageBuffer* ssboGpu() const { return _ssbo_gpu; }
  lev2::FxShaderStorageBlock* ssboBlock() const { return _ssbo_block; }
  lev2::fxparam_constptr_t paramCanvasSize() const { return _param_canvas_size; }
  lev2::fxparam_constptr_t paramSsboBase() const { return _param_ssbo_base; }
  lev2::fxparam_constptr_t paramColorMap() const { return _param_colormap; }

protected:
  void DoDraw(drawevent_constptr_t drwev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

private:
  void _rebuildSsbo(lev2::Context* ctx);

  std::vector<primitive_ptr_t> _primitives;
  std::vector<QuadData> _ssbo_cpu_data;  // CPU-side SSBO data

  // GPU resources
  lev2::FxShaderStorageBuffer* _ssbo_gpu = nullptr;
  lev2::FxShaderStorageBlock* _ssbo_block = nullptr;
  bool _ssbo_dirty = false;
  bool _gpu_initialized = false;

  // Internal shader/pipeline
  lev2::freestyle_mtl_ptr_t _material;
  lev2::fxpipeline_ptr_t _pipeline_solid;
  lev2::fxpipeline_ptr_t _pipeline_textured;
  lev2::fxpipeline_ptr_t _pipeline_vtx_solid;
  lev2::fxpipeline_ptr_t _pipeline_vtx_textured;
  lev2::fxparam_constptr_t _param_canvas_size;
  lev2::fxparam_constptr_t _param_ssbo_base;
  lev2::fxparam_constptr_t _param_colormap;
};

using prim_canvas_ptr_t = std::shared_ptr<PrimCanvas>;

} // namespace ork::ui
