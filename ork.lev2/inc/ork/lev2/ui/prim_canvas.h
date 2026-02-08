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
struct PrimCanvasLayer;
using primcanvaslayer_ptr_t = std::shared_ptr<PrimCanvasLayer>;

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
  virtual void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
                    primcanvaslayer_ptr_t layer) = 0;
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

  void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
            primcanvaslayer_ptr_t layer) override;
  size_t ssboQuadCount() const override { return _quads.size(); }
  void gatherQuadData(std::vector<QuadData>& out) const override;
};
using quadprimitive_ptr_t = std::shared_ptr<QuadPrimitive>;

////////////////////////////////////////////////////////////////////
// SpritePrimitive: Static sprite template with quads in local space
// Designed for instancing - geometry defined once, rendered many times
////////////////////////////////////////////////////////////////////

struct SpritePrimitive;
struct SpriteInstance;
using spriteprimitive_ptr_t = std::shared_ptr<SpritePrimitive>;
using spriteinstance_ptr_t = std::shared_ptr<SpriteInstance>;

struct SpritePrimitive : Primitive {
  SpritePrimitive(lev2::fxpipeline_ptr_t pipeline, lev2::texture_ptr_t texture = nullptr);

  lev2::fxpipeline_ptr_t _pipeline;
  lev2::texture_ptr_t _texture;
  std::vector<quaddata_ptr_t> _quads;  // Quads in local space (centered at origin)

  void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
            primcanvaslayer_ptr_t layer) override;
  size_t ssboQuadCount() const override { return _quads.size(); }
  void gatherQuadData(std::vector<QuadData>& out) const override;

  // Draw with combined transform (called by SpriteInstance)
  void drawInstanced(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
                     const fmtx4& combined_transform, const fvec4& tint);
};

////////////////////////////////////////////////////////////////////
// SpriteInstance: Lightweight instance referencing a SpritePrimitive
// Contains transform and tint, no geometry data
////////////////////////////////////////////////////////////////////

struct SpriteInstance : Primitive {
  SpriteInstance(spriteprimitive_ptr_t sprite = nullptr);

  spriteprimitive_ptr_t _sprite;       // Template geometry
  fmtx4 _transform;                    // 2D transform embedded in 4x4 matrix
  fvec4 _tint = fvec4(1, 1, 1, 1);     // Color modulation
  bool _visible = true;

  // Convenience transform setters
  void setPosition(float x, float y);
  void setRotation(float radians);
  void setScale(float sx, float sy);
  void setScale(float uniform_scale);
  void setTransform(float x, float y, float rotation, float scale);

  void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
            primcanvaslayer_ptr_t layer) override;

  // Instances don't contribute to SSBO - they reuse sprite's data
  size_t ssboQuadCount() const override { return 0; }
  void gatherQuadData(std::vector<QuadData>& out) const override {}
};

////////////////////////////////////////////////////////////////////
// TriStripPrimitive: Triangle strip sharing pipeline/texture state
////////////////////////////////////////////////////////////////////

struct TriStripPrimitive : Primitive {
  TriStripPrimitive(lev2::fxpipeline_ptr_t pipeline, lev2::texture_ptr_t texture = nullptr);

  lev2::fxpipeline_ptr_t _pipeline;
  lev2::texture_ptr_t _texture;
  std::vector<vertexdata_ptr_t> _vertices;

  void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
            primcanvaslayer_ptr_t layer) override;
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

  void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
            primcanvaslayer_ptr_t layer) override;
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

  // TextPrimitive ignores layer transform - text stays screen-fixed
  void draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
            primcanvaslayer_ptr_t layer) override;
};
using textprimitive_ptr_t = std::shared_ptr<TextPrimitive>;

////////////////////////////////////////////////////////////////////
// PrimCanvasLayer: A layer containing primitives
// Layers are rendered in order, and can be enabled/disabled
////////////////////////////////////////////////////////////////////

struct PrimCanvasLayer {
  PrimCanvasLayer(const std::string& name = "layer");

  std::string _name;
  bool _enabled = true;
  std::vector<primitive_ptr_t> _primitives;
  fmtx4 _transform;  // Layer transform (identity by default)

  void clear();
  void addPrimitive(primitive_ptr_t prim);
  void removePrimitive(primitive_ptr_t prim);
  size_t primitiveCount() const { return _primitives.size(); }
  primitive_ptr_t primitive(size_t index) const;

  // Transform accessors
  void setTransform(const fmtx4& mtx) { _transform = mtx; }
  const fmtx4& transform() const { return _transform; }

  // SSBO helpers
  size_t ssboQuadCount() const;
  void gatherQuadData(std::vector<QuadData>& out) const;
};

////////////////////////////////////////////////////////////////////
// PrimCanvas: GPU-accelerated canvas widget
// - Ordered list of layers, each containing primitives
// - Rendered in layer order (painter's algorithm)
// - Quad data in SSBO, mappable from Python
// - All input events routed to Python callbacks
////////////////////////////////////////////////////////////////////

struct PrimCanvas : public Widget {
  PrimCanvas(const std::string& name, int x = 0, int y = 0, int w = 0, int h = 0);
  ~PrimCanvas();

  //////////////////////////////////////////////////////////////
  // Layer management
  //////////////////////////////////////////////////////////////

  primcanvaslayer_ptr_t createLayer(const std::string& name = "layer");
  void addLayer(primcanvaslayer_ptr_t layer);
  void removeLayer(primcanvaslayer_ptr_t layer);
  void clearLayers();
  size_t layerCount() const { return _layers.size(); }
  primcanvaslayer_ptr_t layer(size_t index) const;
  primcanvaslayer_ptr_t layerByName(const std::string& name) const;

  // Mark SSBO as dirty (needs upload to GPU)
  void markDirty() { _ssbo_dirty = true; }

  //////////////////////////////////////////////////////////////
  // Metrics (for leak detection / debugging)
  //////////////////////////////////////////////////////////////

  size_t totalPrimitiveCount() const;
  size_t totalQuadCount() const;
  size_t ssboCpuSize() const { return _ssbo_cpu_data.size(); }
  size_t ssboGpuCapacity() const { return _ssbo_gpu ? _ssbo_gpu->_length : 0; }
  size_t ssboRebuildCount() const { return _ssbo_rebuild_count; }

  //////////////////////////////////////////////////////////////
  // Callbacks (set from Python)
  //////////////////////////////////////////////////////////////

  std::function<HandlerResult(event_constptr_t)> _onUiEvent;
  std::function<void()> _onPreRender;  // Called before each render

  //////////////////////////////////////////////////////////////
  // Appearance
  //////////////////////////////////////////////////////////////

  fvec4 _bg_color = fvec4(0.1f, 0.1f, 0.1f, 1.0f);
  bool _draw_background = true;

  // Desired size for scroll containers (0 = use actual size)
  int _desired_width = 0;
  int _desired_height = 0;
  int desiredWidth() const override { return _desired_width; }
  int desiredHeight() const override { return _desired_height; }

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
  lev2::fxparam_constptr_t paramLayerTransform() const { return _param_layer_transform; }

  // Sprite-specific pipelines and params
  lev2::fxpipeline_ptr_t pipelineSpriteSolid() const { return _pipeline_sprite_solid; }
  lev2::fxpipeline_ptr_t pipelineSpriteTextured() const { return _pipeline_sprite_textured; }
  lev2::fxparam_constptr_t paramSpriteCanvasSize() const { return _param_sprite_canvas_size; }
  lev2::fxparam_constptr_t paramSpriteSsboBase() const { return _param_sprite_ssbo_base; }
  lev2::fxparam_constptr_t paramSpriteColorMap() const { return _param_sprite_colormap; }
  lev2::fxparam_constptr_t paramSpriteInstanceTransform() const { return _param_sprite_instance_transform; }
  lev2::fxparam_constptr_t paramSpriteInstanceTint() const { return _param_sprite_instance_tint; }

  // Material accessor for custom shader support
  lev2::freestyle_mtl_ptr_t material() const { return _material; }

protected:
  void DoDraw(drawevent_constptr_t drwev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

private:
  void _rebuildSsbo(lev2::Context* ctx);

  std::vector<primcanvaslayer_ptr_t> _layers;
  std::vector<QuadData> _ssbo_cpu_data;  // CPU-side SSBO data

  // GPU resources
  lev2::FxShaderStorageBuffer* _ssbo_gpu = nullptr;
  lev2::FxShaderStorageBlock* _ssbo_block = nullptr;
  bool _ssbo_dirty = false;
  bool _gpu_initialized = false;
  size_t _ssbo_rebuild_count = 0;

  // Internal shader/pipeline
  lev2::freestyle_mtl_ptr_t _material;
  lev2::fxpipeline_ptr_t _pipeline_solid;
  lev2::fxpipeline_ptr_t _pipeline_textured;
  lev2::fxpipeline_ptr_t _pipeline_vtx_solid;
  lev2::fxpipeline_ptr_t _pipeline_vtx_textured;
  lev2::fxparam_constptr_t _param_canvas_size;
  lev2::fxparam_constptr_t _param_ssbo_base;
  lev2::fxparam_constptr_t _param_colormap;
  lev2::fxparam_constptr_t _param_layer_transform;

  // Sprite-specific
  lev2::fxpipeline_ptr_t _pipeline_sprite_solid;
  lev2::fxpipeline_ptr_t _pipeline_sprite_textured;
  lev2::fxparam_constptr_t _param_sprite_canvas_size;
  lev2::fxparam_constptr_t _param_sprite_ssbo_base;
  lev2::fxparam_constptr_t _param_sprite_colormap;
  lev2::fxparam_constptr_t _param_sprite_instance_transform;
  lev2::fxparam_constptr_t _param_sprite_instance_tint;
};

using prim_canvas_ptr_t = std::shared_ptr<PrimCanvas>;

} // namespace ork::ui
