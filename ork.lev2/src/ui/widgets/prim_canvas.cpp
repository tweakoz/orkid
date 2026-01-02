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
// QuadPrimitive implementation
////////////////////////////////////////////////////////////////

QuadPrimitive::QuadPrimitive(lev2::fxpipeline_ptr_t pipeline, lev2::texture_ptr_t texture)
    : _pipeline(pipeline)
    , _texture(texture) {
}

void QuadPrimitive::gatherQuadData(std::vector<QuadData>& out) const {
  for (auto& qd : _quads) {
    out.push_back(*qd);
  }
}

void QuadPrimitive::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd) {
  if (_quads.empty()) {
    return;
  }

  auto FXI = ctx->FXI();
  auto GBI = ctx->GBI();

  // Bind SSBO
  FXI->bindStorageBuffer(canvas->ssboBlock(), canvas->ssboGpu());

  // Set uniforms
  fvec2 canvas_size(canvas->width(), canvas->height());
  _pipeline->bindParam(canvas->paramCanvasSize(), canvas_size);
  _pipeline->bindParam(canvas->paramSsboBase(), (int)_ssbo_offset);

  if (_texture && canvas->paramColorMap()) {
    _pipeline->bindParam(canvas->paramColorMap(), _texture.get());
  }

  // Draw using SSBO (6 vertices per quad = 2 triangles)
  lev2::RenderContextInstData rcid(rcfd);
  _pipeline->_rasterstate->_priority = 1 << 20;
  FXI->pushRasterState(_pipeline->_rasterstate);
  _pipeline->wrappedDrawCall(rcid, [&]() {
    GBI->DrawPrimitiveEML(
        canvas->ssboGpu(),
        lev2::PrimitiveType::TRIANGLES,
        0,
        _quads.size() * 6);
    FXI->reset();
  });
  FXI->popRasterState();
}

////////////////////////////////////////////////////////////////
// SpritePrimitive implementation
////////////////////////////////////////////////////////////////

SpritePrimitive::SpritePrimitive(lev2::fxpipeline_ptr_t pipeline, lev2::texture_ptr_t texture)
    : _pipeline(pipeline)
    , _texture(texture) {
}

void SpritePrimitive::gatherQuadData(std::vector<QuadData>& out) const {
  for (auto& qd : _quads) {
    out.push_back(*qd);
  }
}

void SpritePrimitive::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd) {
  // Sprites are templates - they only contribute SSBO data
  // Rendering is done via SpriteInstance which calls drawInstanced()
}

void SpritePrimitive::drawInstanced(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
                                     const fmtx4& transform, const fvec4& tint) {
  if (_quads.empty()) {
    return;
  }

  auto FXI = ctx->FXI();
  auto GBI = ctx->GBI();

  // Use sprite-specific pipeline
  auto pipeline = _texture ? canvas->pipelineSpriteTextured() : canvas->pipelineSpriteSolid();

  // Bind SSBO
  FXI->bindStorageBuffer(canvas->ssboBlock(), canvas->ssboGpu());

  // Set uniforms using sprite-specific params
  fvec2 canvas_size(canvas->width(), canvas->height());
  pipeline->bindParam(canvas->paramSpriteCanvasSize(), canvas_size);
  pipeline->bindParam(canvas->paramSpriteSsboBase(), (int)_ssbo_offset);
  pipeline->bindParam(canvas->paramSpriteInstanceTransform(), transform);
  pipeline->bindParam(canvas->paramSpriteInstanceTint(), tint);

  if (_texture && canvas->paramSpriteColorMap()) {
    pipeline->bindParam(canvas->paramSpriteColorMap(), _texture.get());
  }

  // Draw using SSBO (6 vertices per quad = 2 triangles)
  lev2::RenderContextInstData rcid(rcfd);
  pipeline->_rasterstate->_priority = 1 << 20;
  FXI->pushRasterState(pipeline->_rasterstate);
  pipeline->wrappedDrawCall(rcid, [&]() {
    GBI->DrawPrimitiveEML(
        canvas->ssboGpu(),
        lev2::PrimitiveType::TRIANGLES,
        0,
        _quads.size() * 6);
    FXI->reset();
  });
  FXI->popRasterState();
}

////////////////////////////////////////////////////////////////
// SpriteInstance implementation
////////////////////////////////////////////////////////////////

SpriteInstance::SpriteInstance(spriteprimitive_ptr_t sprite)
    : _sprite(sprite) {
  _transform.setToIdentity();
}

void SpriteInstance::setPosition(float x, float y) {
  // Set translation in column 3 of the 4x4 matrix
  // setElemXY(col, row, val)
  _transform.setElemXY(3, 0, x);
  _transform.setElemXY(3, 1, y);
}

void SpriteInstance::setRotation(float radians) {
  // Extract current scale and translation, apply rotation
  // elemXY(col, row)
  float tx = _transform.elemXY(3, 0);
  float ty = _transform.elemXY(3, 1);
  float sx = fvec2(_transform.elemXY(0, 0), _transform.elemXY(0, 1)).length();
  float sy = fvec2(_transform.elemXY(1, 0), _transform.elemXY(1, 1)).length();

  float c = cosf(radians);
  float s = sinf(radians);

  // Column 0 is X axis, column 1 is Y axis
  _transform.setElemXY(0, 0, c * sx);
  _transform.setElemXY(0, 1, s * sx);
  _transform.setElemXY(1, 0, -s * sy);
  _transform.setElemXY(1, 1, c * sy);
  _transform.setElemXY(3, 0, tx);
  _transform.setElemXY(3, 1, ty);
}

void SpriteInstance::setScale(float sx, float sy) {
  // Extract current rotation from columns
  // elemXY(col, row)
  float len0 = fvec2(_transform.elemXY(0, 0), _transform.elemXY(0, 1)).length();
  float len1 = fvec2(_transform.elemXY(1, 0), _transform.elemXY(1, 1)).length();

  if (len0 > 0.0001f) {
    float inv = sx / len0;
    _transform.setElemXY(0, 0, _transform.elemXY(0, 0) * inv);
    _transform.setElemXY(0, 1, _transform.elemXY(0, 1) * inv);
  }
  if (len1 > 0.0001f) {
    float inv = sy / len1;
    _transform.setElemXY(1, 0, _transform.elemXY(1, 0) * inv);
    _transform.setElemXY(1, 1, _transform.elemXY(1, 1) * inv);
  }
}

void SpriteInstance::setScale(float uniform_scale) {
  setScale(uniform_scale, uniform_scale);
}

void SpriteInstance::setTransform(float x, float y, float rotation, float scale) {
  float c = cosf(rotation);
  float s = sinf(rotation);

  // Build a 2D transform embedded in 4x4 matrix
  // setElemXY(col, row, val) - Orkid uses column-major indexing
  // Column 0: scaled rotated X axis
  _transform.setElemXY(0, 0, c * scale);
  _transform.setElemXY(0, 1, s * scale);
  _transform.setElemXY(0, 2, 0.0f);
  _transform.setElemXY(0, 3, 0.0f);
  // Column 1: scaled rotated Y axis
  _transform.setElemXY(1, 0, -s * scale);
  _transform.setElemXY(1, 1, c * scale);
  _transform.setElemXY(1, 2, 0.0f);
  _transform.setElemXY(1, 3, 0.0f);
  // Column 2: Z axis (identity)
  _transform.setElemXY(2, 0, 0.0f);
  _transform.setElemXY(2, 1, 0.0f);
  _transform.setElemXY(2, 2, 1.0f);
  _transform.setElemXY(2, 3, 0.0f);
  // Column 3: translation
  _transform.setElemXY(3, 0, x);
  _transform.setElemXY(3, 1, y);
  _transform.setElemXY(3, 2, 0.0f);
  _transform.setElemXY(3, 3, 1.0f);
}

void SpriteInstance::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd) {
  if (!_visible || !_sprite) {
    return;
  }
  _sprite->drawInstanced(canvas, ctx, rcfd, _transform, _tint);
}

////////////////////////////////////////////////////////////////
// TriStripPrimitive implementation
////////////////////////////////////////////////////////////////

TriStripPrimitive::TriStripPrimitive(lev2::fxpipeline_ptr_t pipeline, lev2::texture_ptr_t texture)
    : _pipeline(pipeline)
    , _texture(texture) {
}

void TriStripPrimitive::gatherQuadData(std::vector<QuadData>& out) const {
  for (auto& vd : _vertices) {
    QuadData qd;
    qd.pos_size = vd->position;
    qd.uv_rect = vd->uv;
    qd.color = vd->color;
    qd.extra = vd->extra;
    out.push_back(qd);
  }
}

void TriStripPrimitive::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd) {
  if (_vertices.size() < 3) {
    return;
  }

  auto FXI = ctx->FXI();
  auto GBI = ctx->GBI();

  FXI->bindStorageBuffer(canvas->ssboBlock(), canvas->ssboGpu());

  fvec2 canvas_size(canvas->width(), canvas->height());
  _pipeline->bindParam(canvas->paramCanvasSize(), canvas_size);
  _pipeline->bindParam(canvas->paramSsboBase(), (int)_ssbo_offset);

  if (_texture && canvas->paramColorMap()) {
    _pipeline->bindParam(canvas->paramColorMap(), _texture.get());
  }

  lev2::RenderContextInstData rcid(rcfd);
  _pipeline->_rasterstate->_priority = 1 << 20;
  FXI->pushRasterState(_pipeline->_rasterstate);
  _pipeline->wrappedDrawCall(rcid, [&]() {
    GBI->DrawPrimitiveEML(
        canvas->ssboGpu(),
        lev2::PrimitiveType::TRIANGLESTRIP,
        0,
        _vertices.size());
    FXI->reset();
  });
  FXI->popRasterState();
}

////////////////////////////////////////////////////////////////
// TriListPrimitive implementation
////////////////////////////////////////////////////////////////

TriListPrimitive::TriListPrimitive(lev2::fxpipeline_ptr_t pipeline, lev2::texture_ptr_t texture)
    : _pipeline(pipeline)
    , _texture(texture) {
}

void TriListPrimitive::gatherQuadData(std::vector<QuadData>& out) const {
  for (auto& vd : _vertices) {
    QuadData qd;
    qd.pos_size = vd->position;
    qd.uv_rect = vd->uv;
    qd.color = vd->color;
    qd.extra = vd->extra;
    out.push_back(qd);
  }
}

void TriListPrimitive::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd) {
  if (_vertices.size() < 3) {
    return;
  }

  auto FXI = ctx->FXI();
  auto GBI = ctx->GBI();

  FXI->bindStorageBuffer(canvas->ssboBlock(), canvas->ssboGpu());

  fvec2 canvas_size(canvas->width(), canvas->height());
  _pipeline->bindParam(canvas->paramCanvasSize(), canvas_size);
  _pipeline->bindParam(canvas->paramSsboBase(), (int)_ssbo_offset);

  if (_texture && canvas->paramColorMap()) {
    _pipeline->bindParam(canvas->paramColorMap(), _texture.get());
  }

  lev2::RenderContextInstData rcid(rcfd);
  _pipeline->_rasterstate->_priority = 1 << 20;
  FXI->pushRasterState(_pipeline->_rasterstate);
  _pipeline->wrappedDrawCall(rcid, [&]() {
    GBI->DrawPrimitiveEML(
        canvas->ssboGpu(),
        lev2::PrimitiveType::TRIANGLES,
        0,
        _vertices.size());
    FXI->reset();
  });
  FXI->popRasterState();
}

////////////////////////////////////////////////////////////////
// TextPrimitive implementation
////////////////////////////////////////////////////////////////

TextPrimitive::TextPrimitive(lev2::font_ptr_t font, fvec4 color)
    : _font(font)
    , _color(color) {
}

void TextPrimitive::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd) {
  if (_items.empty() || !_font) {
    return;
  }

  auto mtxi = ctx->MTXI();

  int ix1, iy1;
  canvas->LocalToRoot(0, 0, ix1, iy1);

  lev2::FontMan::PushFont(_font);
  ctx->PushModColor(_color);
  mtxi->PushUIMatrix();
  {
    for (const auto& item : _items) {
      int text_x = ix1 + int(item.position.x);
      int text_y = iy1 + int(item.position.y);

      lev2::FontMan::beginTextBlock(ctx, item.text.length());
      lev2::FontMan::DrawText(ctx, text_x, text_y, item.text.c_str());
      lev2::FontMan::endTextBlock(ctx);
    }
  }
  mtxi->PopUIMatrix();
  ctx->PopModColor();
  lev2::FontMan::PopFont();
}

////////////////////////////////////////////////////////////////
// PrimCanvas implementation
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

void PrimCanvas::gpuInit(lev2::Context* ctx) {
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

  // Vertex-based techniques
  auto tek_vtx_solid = _material->technique("tek_canvas_vtx_solid");
  auto tek_vtx_tex = _material->technique("tek_canvas_vtx_tex");

  lev2::FxPipelinePermutation permu_vtx_solid;
  permu_vtx_solid._forced_technique = tek_vtx_solid;
  _pipeline_vtx_solid = pipeline_cache->findPipeline(permu_vtx_solid);

  lev2::FxPipelinePermutation permu_vtx_tex;
  permu_vtx_tex._forced_technique = tek_vtx_tex;
  _pipeline_vtx_textured = pipeline_cache->findPipeline(permu_vtx_tex);

  // Get shader parameters for standard primitives
  _param_canvas_size = _material->param("canvas_size");
  _param_ssbo_base = _material->param("ssbo_base");
  _param_colormap = _material->param("ColorMap");

  // Sprite-specific techniques and pipelines
  auto tek_sprite_solid = _material->technique("tek_sprite_solid");
  auto tek_sprite_tex = _material->technique("tek_sprite_tex");

  lev2::FxPipelinePermutation permu_sprite_solid;
  permu_sprite_solid._forced_technique = tek_sprite_solid;
  _pipeline_sprite_solid = pipeline_cache->findPipeline(permu_sprite_solid);

  lev2::FxPipelinePermutation permu_sprite_tex;
  permu_sprite_tex._forced_technique = tek_sprite_tex;
  _pipeline_sprite_textured = pipeline_cache->findPipeline(permu_sprite_tex);

  // Sprite params (from the sprite uniform block - unique names to avoid conflicts)
  _param_sprite_canvas_size = _material->param("sprite_canvas_size");
  _param_sprite_ssbo_base = _material->param("sprite_ssbo_base");
  _param_sprite_colormap = _material->param("ColorMap");
  _param_sprite_instance_transform = _material->param("sprite_instance_transform");
  _param_sprite_instance_tint = _material->param("sprite_instance_tint");

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

void PrimCanvas::addPrimitive(primitive_ptr_t prim) {
  _primitives.push_back(prim);
  _ssbo_dirty = true;
}

////////////////////////////////////////////////////////////////

primitive_ptr_t PrimCanvas::primitive(size_t index) const {
  if (index >= _primitives.size()) {
    return nullptr;
  }
  return _primitives[index];
}

////////////////////////////////////////////////////////////////

void PrimCanvas::_rebuildSsbo(lev2::Context* ctx) {
  if (!_ssbo_dirty) {
    return;
  }

  // Gather all quad data from primitives and assign offsets
  _ssbo_cpu_data.clear();

  for (auto& prim : _primitives) {
    prim->_ssbo_offset = _ssbo_cpu_data.size();
    prim->gatherQuadData(_ssbo_cpu_data);
  }

  if (_ssbo_cpu_data.empty()) {
    _ssbo_dirty = false;
    return;
  }

  auto FXI = ctx->FXI();
  size_t required_size = _ssbo_cpu_data.size() * sizeof(QuadData);

  // Resize GPU SSBO if needed
  if (_ssbo_gpu->_length < required_size) {
    delete _ssbo_gpu;
    _ssbo_gpu = FXI->createStorageBuffer(required_size * 2);
  }

  // Map and copy data
  auto mapped = FXI->mapStorageBuffer(_ssbo_gpu, 0, required_size, lev2::BufferMapAccess::WRITE_ONLY);
  memcpy(mapped->_mappedaddr, _ssbo_cpu_data.data(), required_size);
  FXI->unmapStorageBuffer(mapped.get());

  _ssbo_dirty = false;
}

////////////////////////////////////////////////////////////////

void PrimCanvas::DoDraw(drawevent_constptr_t drwev) {
  auto ctx = drwev->GetTarget();

  // Initialize GPU resources on first draw
  gpuInit(ctx);

  // Rebuild SSBO if dirty
  _rebuildSsbo(ctx);

  // Draw background if enabled
  if (_draw_background) {
    _drawColoredBox(drwev, _bg_color, lev2::BlendingMacro::ALPHA);
  }

  // Get RCFD for pipeline draws
  auto rcfd = ctx->topRenderContextFrameData();

  // Draw all primitives in order (painter's algorithm)
  for (auto& prim : _primitives) {
    prim->draw(this, ctx, rcfd);
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
