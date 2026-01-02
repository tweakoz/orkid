////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/ui/prim_canvas.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>

namespace ork::ui {

////////////////////////////////////////////////////////////////

PrimCanvas::PrimCanvas(const std::string& name, int x, int y, int w, int h)
    : Widget(name, x, y, w, h) {
}

////////////////////////////////////////////////////////////////

PrimCanvas::~PrimCanvas() {
  if (_ssbo_gpu) {
    delete _ssbo_gpu;
    _ssbo_gpu = nullptr;
  }
}

////////////////////////////////////////////////////////////////

void PrimCanvas::_gpuInit(lev2::Context* ctx) {
  if (_gpu_initialized) {
    return;
  }

  auto FXI = ctx->FXI();

  // Create material and load shader
  _material = std::make_shared<lev2::FreestyleMaterial>();
  _material->gpuInit(ctx, "orkshader://prim_canvas");

  // Get techniques
  auto tek_solid = _material->technique("tek_canvas_solid");
  auto tek_tex = _material->technique("tek_canvas_tex");

  // Create pipelines using permutations
  auto pipeline_cache = _material->pipelineCache();

  lev2::FxPipelinePermutation permu_solid;
  permu_solid._forced_technique = tek_solid;
  _pipeline_solid = pipeline_cache->findPipeline(permu_solid);

  lev2::FxPipelinePermutation permu_tex;
  permu_tex._forced_technique = tek_tex;
  _pipeline_textured = pipeline_cache->findPipeline(permu_tex);

  // Get shader parameters
  _param_canvas_size = _material->param("canvas_size");
  _param_ssbo_base = _material->param("ssbo_base");
  _param_colormap = _material->param("ColorMap");

  // Get storage block
  _ssbo_block = const_cast<lev2::FxShaderStorageBlock*>(_material->storageBlock("storage_quads"));

  // Create SSBO (start with 1MB, can grow)
  _ssbo_gpu = FXI->createStorageBuffer(1 << 20);

  _gpu_initialized = true;
}

////////////////////////////////////////////////////////////////

void PrimCanvas::clear() {
  _primitives.clear();
  _ssbo_cpu_data.clear();
  _ssbo_dirty = true;
}

////////////////////////////////////////////////////////////////

size_t PrimCanvas::addQuadPrimitive() {
  QuadPrimitive prim;
  prim.pipeline = nullptr;  // use internal
  prim.texture = nullptr;
  prim.ssbo_offset = 0;
  prim.quad_count = 0;
  _primitives.push_back(prim);
  return _primitives.size() - 1;
}

////////////////////////////////////////////////////////////////

size_t PrimCanvas::addQuadPrimitive(lev2::texture_ptr_t texture) {
  QuadPrimitive prim;
  prim.pipeline = nullptr;  // use internal
  prim.texture = texture;
  prim.ssbo_offset = 0;
  prim.quad_count = 0;
  _primitives.push_back(prim);
  return _primitives.size() - 1;
}

////////////////////////////////////////////////////////////////

size_t PrimCanvas::addQuadPrimitiveWithPipeline(lev2::fxpipeline_ptr_t pipeline) {
  QuadPrimitive prim;
  prim.pipeline = pipeline;
  prim.texture = nullptr;
  prim.ssbo_offset = 0;
  prim.quad_count = 0;
  _primitives.push_back(prim);
  return _primitives.size() - 1;
}

////////////////////////////////////////////////////////////////

size_t PrimCanvas::addTextPrimitive(
    lev2::font_ptr_t font,
    const std::string& text,
    fvec2 position,
    fvec4 color) {
  TextPrimitive prim;
  prim.font = font;
  prim.text = text;
  prim.position = position;
  prim.color = color;
  _primitives.push_back(prim);
  return _primitives.size() - 1;
}

////////////////////////////////////////////////////////////////

void PrimCanvas::reserveQuads(size_t prim_index, size_t count) {
  if (prim_index >= _primitives.size()) {
    return;
  }

  auto* quad_prim = std::get_if<QuadPrimitive>(&_primitives[prim_index]);
  if (!quad_prim) {
    return;
  }

  // Calculate new offset at end of current data
  quad_prim->ssbo_offset = _ssbo_cpu_data.size();
  quad_prim->quad_count = count;

  // Resize to accommodate new quads
  _ssbo_cpu_data.resize(_ssbo_cpu_data.size() + count);
  _ssbo_dirty = true;
}

////////////////////////////////////////////////////////////////

void PrimCanvas::setQuads(size_t prim_index, const QuadData* data, size_t count) {
  if (prim_index >= _primitives.size()) {
    return;
  }

  auto* quad_prim = std::get_if<QuadPrimitive>(&_primitives[prim_index]);
  if (!quad_prim) {
    return;
  }

  // If count differs from reserved, re-reserve
  if (count != quad_prim->quad_count) {
    reserveQuads(prim_index, count);
  }

  // Copy data
  size_t offset = quad_prim->ssbo_offset;
  if (offset + count <= _ssbo_cpu_data.size()) {
    memcpy(&_ssbo_cpu_data[offset], data, count * sizeof(QuadData));
    _ssbo_dirty = true;
  }
}

////////////////////////////////////////////////////////////////

QuadData* PrimCanvas::getQuadData(size_t prim_index) {
  if (prim_index >= _primitives.size()) {
    return nullptr;
  }

  auto* quad_prim = std::get_if<QuadPrimitive>(&_primitives[prim_index]);
  if (!quad_prim || quad_prim->quad_count == 0) {
    return nullptr;
  }

  return &_ssbo_cpu_data[quad_prim->ssbo_offset];
}

////////////////////////////////////////////////////////////////

const QuadData* PrimCanvas::getQuadData(size_t prim_index) const {
  if (prim_index >= _primitives.size()) {
    return nullptr;
  }

  auto* quad_prim = std::get_if<QuadPrimitive>(&_primitives[prim_index]);
  if (!quad_prim || quad_prim->quad_count == 0) {
    return nullptr;
  }

  return &_ssbo_cpu_data[quad_prim->ssbo_offset];
}

////////////////////////////////////////////////////////////////

size_t PrimCanvas::getQuadCount(size_t prim_index) const {
  if (prim_index >= _primitives.size()) {
    return 0;
  }

  auto* quad_prim = std::get_if<QuadPrimitive>(&_primitives[prim_index]);
  if (!quad_prim) {
    return 0;
  }

  return quad_prim->quad_count;
}

////////////////////////////////////////////////////////////////

void PrimCanvas::_uploadSsbo(lev2::Context* ctx) {
  if (!_ssbo_dirty || _ssbo_cpu_data.empty()) {
    return;
  }

  auto FXI = ctx->FXI();
  size_t required_size = _ssbo_cpu_data.size() * sizeof(QuadData);

  // Resize GPU SSBO if needed
  if (_ssbo_gpu->_length < required_size) {
    delete _ssbo_gpu;
    _ssbo_gpu = FXI->createStorageBuffer(required_size * 2);  // 2x for growth
  }

  // Map and copy data
  auto mapped = FXI->mapStorageBuffer(_ssbo_gpu, 0, required_size, lev2::BufferMapAccess::WRITE_ONLY);
  memcpy(mapped->_mappedaddr, _ssbo_cpu_data.data(), required_size);
  FXI->unmapStorageBuffer(mapped.get());

  _ssbo_dirty = false;
}

////////////////////////////////////////////////////////////////

void PrimCanvas::_drawQuadPrimitive(lev2::Context* ctx, lev2::rcfd_ptr_t rcfd, const QuadPrimitive& prim) {
  if (prim.quad_count == 0) {
    return;
  }

  auto FXI = ctx->FXI();
  auto GBI = ctx->GBI();

  // Choose pipeline
  lev2::fxpipeline_ptr_t pipeline = _pipeline_solid;
  if (prim.pipeline) {
    pipeline = prim.pipeline;
  } else if (prim.texture) {
    pipeline = _pipeline_textured;
  }

  // Bind SSBO
  FXI->bindStorageBuffer(_ssbo_block, _ssbo_gpu);

  // Set uniforms
  fvec2 canvas_size(_geometry._w, _geometry._h);
  pipeline->bindParam(_param_canvas_size, canvas_size);
  pipeline->bindParam(_param_ssbo_base, (int)prim.ssbo_offset);

  if (prim.texture && _param_colormap) {
    pipeline->bindParam(_param_colormap, prim.texture.get());
  }

  // Create RCID for this draw
  lev2::RenderContextInstData rcid(rcfd);

  // Draw using SSBO (6 vertices per quad = 2 triangles)
  pipeline->wrappedDrawCall(rcid, [&]() {
    GBI->DrawPrimitiveEML(
        _ssbo_gpu,
        lev2::PrimitiveType::TRIANGLES,
        0,                      // base vertex
        prim.quad_count * 6);   // vertex count
    FXI->reset();
  });
}

////////////////////////////////////////////////////////////////

void PrimCanvas::_drawTextPrimitive(lev2::Context* ctx, const TextPrimitive& prim) {
  if (prim.text.empty() || !prim.font) {
    return;
  }

  auto mtxi = ctx->MTXI();

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);

  lev2::FontMan::PushFont(prim.font);
  ctx->PushModColor(prim.color);
  mtxi->PushUIMatrix();
  {
    int text_x = ix1 + int(prim.position.x);
    int text_y = iy1 + int(prim.position.y);

    lev2::FontMan::beginTextBlock(ctx, prim.text.length());
    lev2::FontMan::DrawText(ctx, text_x, text_y, prim.text.c_str());
    lev2::FontMan::endTextBlock(ctx);
  }
  mtxi->PopUIMatrix();
  ctx->PopModColor();
  lev2::FontMan::PopFont();
}

////////////////////////////////////////////////////////////////

void PrimCanvas::DoDraw(drawevent_constptr_t drwev) {
  auto ctx = drwev->GetTarget();
  auto mtxi = ctx->MTXI();

  // Initialize GPU resources on first draw
  _gpuInit(ctx);

  // Upload SSBO data if dirty
  _uploadSsbo(ctx);

  int ix1, iy1;
  LocalToRoot(0, 0, ix1, iy1);

  // Draw background if enabled
  if (_draw_background) {
    _drawColoredBox(drwev, _bg_color, lev2::BlendingMacro::ALPHA);
  }

  // Get RCFD for pipeline draws
  auto rcfd = ctx->topRenderContextFrameData();

  // Draw all primitives in order (painter's algorithm)
  for (const auto& prim : _primitives) {
    std::visit([this, ctx, rcfd](auto&& p) {
      using T = std::decay_t<decltype(p)>;
      if constexpr (std::is_same_v<T, QuadPrimitive>) {
        _drawQuadPrimitive(ctx, rcfd, p);
      } else if constexpr (std::is_same_v<T, TextPrimitive>) {
        _drawTextPrimitive(ctx, p);
      }
    }, prim);
  }
}

////////////////////////////////////////////////////////////////

HandlerResult PrimCanvas::DoOnUiEvent(event_constptr_t ev) {
  if (_onUiEvent) {
    return _onUiEvent(ev);
  }
  return HandlerResult();
}

////////////////////////////////////////////////////////////////

} // namespace ork::ui
