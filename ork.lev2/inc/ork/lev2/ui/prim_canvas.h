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
#include <variant>

namespace ork::ui {

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

////////////////////////////////////////////////////////////////////
// QuadPrimitive: A batch of quads sharing a pipeline
////////////////////////////////////////////////////////////////////

struct QuadPrimitive {
  lev2::fxpipeline_ptr_t pipeline;  // optional custom pipeline (nullptr = use internal)
  lev2::texture_ptr_t texture;      // optional texture
  uint32_t ssbo_offset = 0;         // Offset into the SSBO (in QuadData units)
  uint32_t quad_count = 0;          // Number of quads in this primitive
};

////////////////////////////////////////////////////////////////////
// TextPrimitive: Text rendered with FontMan
////////////////////////////////////////////////////////////////////

struct TextPrimitive {
  lev2::font_ptr_t font;
  std::string text;
  fvec2 position;
  fvec4 color = fvec4(1, 1, 1, 1);
};

////////////////////////////////////////////////////////////////////
// Primitive: Either quads or text
////////////////////////////////////////////////////////////////////

using Primitive = std::variant<QuadPrimitive, TextPrimitive>;

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

  // Add a quad primitive (uses internal pipeline), returns primitive index
  size_t addQuadPrimitive();

  // Add a quad primitive with optional texture, returns primitive index
  size_t addQuadPrimitive(lev2::texture_ptr_t texture);

  // Add a quad primitive with custom pipeline (advanced), returns primitive index
  size_t addQuadPrimitiveWithPipeline(lev2::fxpipeline_ptr_t pipeline);

  // Add a text primitive, returns primitive index
  size_t addTextPrimitive(
      lev2::font_ptr_t font,
      const std::string& text,
      fvec2 position,
      fvec4 color = fvec4(1, 1, 1, 1));

  // Get primitive count
  size_t primitiveCount() const { return _primitives.size(); }

  //////////////////////////////////////////////////////////////
  // Quad data management
  //////////////////////////////////////////////////////////////

  // Reserve space for quads in a primitive (call before setQuads)
  void reserveQuads(size_t prim_index, size_t count);

  // Set quad data for a primitive (copies from provided array)
  void setQuads(size_t prim_index, const QuadData* data, size_t count);

  // Get pointer to quad data for direct manipulation
  // Returns nullptr if not a quad primitive or index out of range
  QuadData* getQuadData(size_t prim_index);
  const QuadData* getQuadData(size_t prim_index) const;

  // Get quad count for a primitive
  size_t getQuadCount(size_t prim_index) const;

  // Mark SSBO as dirty (needs upload to GPU)
  void markDirty() { _ssbo_dirty = true; }

  //////////////////////////////////////////////////////////////
  // Direct SSBO access for Python/numpy mapping
  //////////////////////////////////////////////////////////////

  // Get raw pointer to SSBO data (entire buffer)
  void* ssboData() { return _ssbo_cpu_data.data(); }
  const void* ssboData() const { return _ssbo_cpu_data.data(); }
  size_t ssboSize() const { return _ssbo_cpu_data.size() * sizeof(QuadData); }
  size_t ssboCapacity() const { return _ssbo_cpu_data.capacity(); }

  //////////////////////////////////////////////////////////////
  // Event callbacks (set from Python)
  //////////////////////////////////////////////////////////////

  std::function<HandlerResult(event_constptr_t)> _onUiEvent;

  //////////////////////////////////////////////////////////////
  // Appearance
  //////////////////////////////////////////////////////////////

  fvec4 _bg_color = fvec4(0.1f, 0.1f, 0.1f, 1.0f);
  bool _draw_background = true;

protected:
  void DoDraw(drawevent_constptr_t drwev) override;
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

private:
  void _gpuInit(lev2::Context* ctx);
  void _uploadSsbo(lev2::Context* ctx);
  void _drawQuadPrimitive(lev2::Context* ctx, lev2::rcfd_ptr_t rcfd, const QuadPrimitive& prim);
  void _drawTextPrimitive(lev2::Context* ctx, const TextPrimitive& prim);

  std::vector<Primitive> _primitives;
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
  lev2::fxparam_constptr_t _param_canvas_size;
  lev2::fxparam_constptr_t _param_ssbo_base;
  lev2::fxparam_constptr_t _param_colormap;
};

using prim_canvas_ptr_t = std::shared_ptr<PrimCanvas>;

} // namespace ork::ui
