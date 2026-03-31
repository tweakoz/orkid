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

void QuadPrimitive::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
                         primcanvaslayer_ptr_t layer) {
  if (_quads.empty()) {
    return;
  }

  auto FXI = ctx->FXI();
  auto GBI = ctx->GBI();

  // Get material from pipeline, fall back to canvas's default
  auto material = _pipeline->sharedMaterialAs<lev2::FreestyleMaterial>();
  if (!material) {
    material = canvas->material();
  }
  OrkAssert(material);

  // Get storage block and params from material
  auto ssbo_block = material->storageBlock("storage_quads");
  auto param_canvas_size = material->param("canvas_size");
  auto param_ssbo_base = material->param("ssbo_base");
  auto param_layer_transform = material->param("layer_transform");
  auto param_colormap = material->param("ColorMap");

  // Set uniforms and SSBO binding on pipeline BEFORE wrappedDrawCall
  // so beginBlock applies them when building the descriptor set
  fvec2 canvas_size(canvas->width(), canvas->height());
  _pipeline->bindParam(param_canvas_size, canvas_size);
  _pipeline->bindParam(param_ssbo_base, (int)_ssbo_offset);
  _pipeline->bindParam(param_layer_transform, layer->transform());
  _pipeline->bindStorage(ssbo_block, canvas->ssboGpu());

  if (_texture && param_colormap) {
    _pipeline->bindParam(param_colormap, _texture.get());
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

void SpritePrimitive::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
                           primcanvaslayer_ptr_t layer) {
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

  auto ssbo_block = canvas->ssboBlock();

  // Set uniforms using sprite-specific params
  fvec2 canvas_size(canvas->width(), canvas->height());
  pipeline->bindParam(canvas->paramSpriteCanvasSize(), canvas_size);
  pipeline->bindParam(canvas->paramSpriteSsboBase(), (int)_ssbo_offset);
  pipeline->bindParam(canvas->paramSpriteInstanceTransform(), transform);
  pipeline->bindParam(canvas->paramSpriteInstanceTint(), tint);
  pipeline->bindStorage(ssbo_block, canvas->ssboGpu());

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

void SpriteInstance::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
                          primcanvaslayer_ptr_t layer) {
  if (!_visible || !_sprite) {
    return;
  }
  // Compose layer transform with instance transform
  fmtx4 combined = layer->transform() * _transform;
  _sprite->drawInstanced(canvas, ctx, rcfd, combined, _tint);
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

void TriStripPrimitive::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
                             primcanvaslayer_ptr_t layer) {
  if (_vertices.size() < 3) {
    return;
  }

  auto FXI = ctx->FXI();
  auto GBI = ctx->GBI();

  // Get params from pipeline's material if set, otherwise use canvas's material
  auto material = _pipeline->sharedMaterialAs<lev2::FreestyleMaterial>();
  if (!material) {
    material = canvas->material();  // Fall back to canvas's default material
  }
  OrkAssert(material);

  auto ssbo_block = material->storageBlock("storage_quads");
  auto param_canvas_size = material->param("canvas_size");
  auto param_ssbo_base = material->param("ssbo_base");
  auto param_layer_transform = material->param("layer_transform");
  auto param_colormap = material->param("ColorMap");

  fvec2 canvas_size(canvas->width(), canvas->height());
  _pipeline->bindParam(param_canvas_size, canvas_size);
  _pipeline->bindParam(param_ssbo_base, (int)_ssbo_offset);
  _pipeline->bindParam(param_layer_transform, layer->transform());
  _pipeline->bindStorage(ssbo_block, canvas->ssboGpu());

  if (_texture && param_colormap) {
    _pipeline->bindParam(param_colormap, _texture.get());
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

void TriListPrimitive::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
                            primcanvaslayer_ptr_t layer) {
  if (_vertices.size() < 3) {
    return;
  }

  auto FXI = ctx->FXI();
  auto GBI = ctx->GBI();

  // Get params from pipeline's material if set, otherwise use canvas's material
  auto material = _pipeline->sharedMaterialAs<lev2::FreestyleMaterial>();
  if (!material) {
    material = canvas->material();  // Fall back to canvas's default material
  }
  OrkAssert(material);

  auto ssbo_block = material->storageBlock("storage_quads");
  auto param_canvas_size = material->param("canvas_size");
  auto param_ssbo_base = material->param("ssbo_base");
  auto param_layer_transform = material->param("layer_transform");
  auto param_colormap = material->param("ColorMap");

  fvec2 canvas_size(canvas->width(), canvas->height());
  _pipeline->bindParam(param_canvas_size, canvas_size);
  _pipeline->bindParam(param_ssbo_base, (int)_ssbo_offset);
  _pipeline->bindParam(param_layer_transform, layer->transform());
  _pipeline->bindStorage(ssbo_block, canvas->ssboGpu());

  if (_texture && param_colormap) {
    _pipeline->bindParam(param_colormap, _texture.get());
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

void TextPrimitive::draw(PrimCanvas* canvas, lev2::Context* ctx, lev2::rcfd_ptr_t rcfd,
                         primcanvaslayer_ptr_t layer) {
  // TextPrimitive intentionally ignores layer transform
  // Text stays screen-fixed for HUD/UI purposes
  if (_items.empty() || !_font) {
    return;
  }

  auto mtxi = ctx->MTXI();

  // Use widget dimensions (not RTG) for the UI matrix — with SSAA the RTG
  // is larger, so using widget dims causes the projection to scale up glyphs
  // to match the higher-res surface. Positions are canvas-local either way.
  int uiw = canvas->width();
  int uih = canvas->height();

  lev2::FontMan::PushFont(_font);
  ctx->PushModColor(_color);
  mtxi->PushUIMatrix(uiw, uih);
  {
    for (const auto& item : _items) {
      int text_x = int(item.position.x);
      int text_y = int(item.position.y);

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
// PrimCanvasLayer implementation
////////////////////////////////////////////////////////////////

PrimCanvasLayer::PrimCanvasLayer(const std::string& name)
    : _name(name) {
  _transform.setToIdentity();
}

void PrimCanvasLayer::clear() {
  _primitives.clear();
}

void PrimCanvasLayer::addPrimitive(primitive_ptr_t prim) {
  _primitives.push_back(prim);
}

void PrimCanvasLayer::removePrimitive(primitive_ptr_t prim) {
  auto it = std::find(_primitives.begin(), _primitives.end(), prim);
  if (it != _primitives.end()) {
    _primitives.erase(it);
  }
}

primitive_ptr_t PrimCanvasLayer::primitive(size_t index) const {
  if (index >= _primitives.size()) {
    return nullptr;
  }
  return _primitives[index];
}

size_t PrimCanvasLayer::ssboQuadCount() const {
  size_t count = 0;
  for (const auto& prim : _primitives) {
    count += prim->ssboQuadCount();
  }
  return count;
}

void PrimCanvasLayer::gatherQuadData(std::vector<QuadData>& out) const {
  for (const auto& prim : _primitives) {
    prim->gatherQuadData(out);
  }
}

////////////////////////////////////////////////////////////////
// PrimCanvas implementation
////////////////////////////////////////////////////////////////

PrimCanvas::PrimCanvas(const std::string& name, int x, int y, int w, int h)
    : Surface(name, x, y, w, h, fcolor4(0.1f, 0.1f, 0.1f, 1.0f), 1.0f) {
  _alwaysRepaint = true;
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

  // Ensure Surface RTG is created
  if (!_rtgroup) {
    Surface::_doGpuInit(ctx);
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
  _pipeline_textured->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
  _pipeline_textured->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);

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
  _param_layer_transform = _material->param("layer_transform");

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
// Layer management
////////////////////////////////////////////////////////////////

primcanvaslayer_ptr_t PrimCanvas::createLayer(const std::string& name) {
  auto layer = std::make_shared<PrimCanvasLayer>(name);
  _layers.push_back(layer);
  _ssbo_dirty = true;
  return layer;
}

void PrimCanvas::addLayer(primcanvaslayer_ptr_t layer) {
  _layers.push_back(layer);
  _ssbo_dirty = true;
}

void PrimCanvas::removeLayer(primcanvaslayer_ptr_t layer) {
  auto it = std::find(_layers.begin(), _layers.end(), layer);
  if (it != _layers.end()) {
    _layers.erase(it);
    _ssbo_dirty = true;
  }
}

void PrimCanvas::clearLayers() {
  _layers.clear();
  _ssbo_cpu_data.clear();
  _ssbo_dirty = true;
}

primcanvaslayer_ptr_t PrimCanvas::layer(size_t index) const {
  if (index >= _layers.size()) {
    return nullptr;
  }
  return _layers[index];
}

primcanvaslayer_ptr_t PrimCanvas::layerByName(const std::string& name) const {
  for (const auto& layer : _layers) {
    if (layer->_name == name) {
      return layer;
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////

size_t PrimCanvas::totalPrimitiveCount() const {
  size_t count = 0;
  for (const auto& layer : _layers) {
    count += layer->primitiveCount();
  }
  return count;
}

size_t PrimCanvas::totalQuadCount() const {
  size_t count = 0;
  for (const auto& layer : _layers) {
    count += layer->ssboQuadCount();
  }
  return count;
}

////////////////////////////////////////////////////////////////

void PrimCanvas::_rebuildSsbo(lev2::Context* ctx) {
  if (!_ssbo_dirty) {
    return;
  }

  // Gather all quad data from all layers and assign offsets
  _ssbo_cpu_data.clear();

  for (auto& layer : _layers) {
    for (auto& prim : layer->_primitives) {
      size_t offset = _ssbo_cpu_data.size();
      prim->_ssbo_offset = offset;
      prim->gatherQuadData(_ssbo_cpu_data);
    }
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
  _ssbo_rebuild_count++;
}

////////////////////////////////////////////////////////////////

void PrimCanvas::renderLayers(lev2::Context* ctx) {
  gpuInit(ctx);
  _rebuildSsbo(ctx);

  if (_ssbo_cpu_data.empty()) {
    return;
  }

  auto rcfd = ctx->topRenderContextFrameData();

  for (auto& layer : _layers) {
    if (!layer->_enabled) {
      continue;
    }
    for (auto& prim : layer->_primitives) {
      prim->draw(this, ctx, rcfd, layer);
    }
  }
}

////////////////////////////////////////////////////////////////

void PrimCanvas::DoRePaintSurface(drawevent_constptr_t drwev) {
  auto ctx = drwev->GetTarget();
  auto FBI = ctx->FBI();

  // Initialize GPU resources on first draw
  gpuInit(ctx);

  // Set clear color from canvas bg
  _clearColor = _bg_color;

  // Call pre-render callback (for Python widgets to update primitives)
  if (_onPreRender) {
    _onPreRender();
  }

  // Render into the Surface's RTG (already pushed by Surface::DoDraw)
  int rtw = _rtgroup->width();
  int rth = _rtgroup->height();
  if (rtw < 1 || rth < 1) return;

  lev2::ViewportRect vp(0, 0, rtw, rth);
  FBI->pushViewport(vp);
  FBI->pushScissor(vp);
  renderLayers(ctx);
  FBI->popScissor();
  FBI->popViewport();

  // SVG export (separate pass)
  if (!_svg_export_path.empty()) {
    _doSvgExport();
    _svg_export_path.clear();
  }
}

////////////////////////////////////////////////////////////////

void PrimCanvas::_doSvgExport() {
  int w = width();
  int h = height();
  if (w < 1 || h < 1) return;

  FILE* fp = fopen(_svg_export_path.c_str(), "w");
  if (!fp) return;

  fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  fprintf(fp, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n", w, h, w, h);

  // Background
  if (_draw_background) {
    fprintf(fp, "  <rect width=\"%d\" height=\"%d\" fill=\"rgb(%d,%d,%d)\" fill-opacity=\"%.2f\"/>\n",
            w, h,
            int(_bg_color.x * 255), int(_bg_color.y * 255), int(_bg_color.z * 255), _bg_color.w);
  }

  for (auto& layer : _layers) {
    if (!layer->_enabled) continue;
    fprintf(fp, "  <g id=\"%s\">\n", layer->_name.c_str());

    for (auto& prim : layer->_primitives) {
      // QuadPrimitive
      if (auto qp = dynamic_cast<QuadPrimitive*>(prim.get())) {
        for (auto& qd : qp->_quads) {
          float x = qd->pos_size.x;
          float y_up = qd->pos_size.y;  // Y-up canvas coords
          float qw = qd->pos_size.z;
          float qh = qd->pos_size.w;
          float y = h - y_up - qh;  // convert to Y-down SVG coords
          float r = qd->extra.y;    // corner radius
          float rot = qd->extra.x;  // rotation in radians
          auto& c = qd->color;

          if (rot != 0.0f) {
            float cx = x + qw * 0.5f;
            float cy = y + qh * 0.5f;
            float deg = rot * -180.0f / M_PI;  // negate for SVG Y-down
            fprintf(fp, "    <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" rx=\"%.1f\" ry=\"%.1f\" "
                    "fill=\"rgb(%d,%d,%d)\" fill-opacity=\"%.2f\" transform=\"rotate(%.1f,%.1f,%.1f)\"/>\n",
                    x, y, qw, qh, r, r,
                    int(c.x*255), int(c.y*255), int(c.z*255), c.w, deg, cx, cy);
          } else {
            fprintf(fp, "    <rect x=\"%.1f\" y=\"%.1f\" width=\"%.1f\" height=\"%.1f\" rx=\"%.1f\" ry=\"%.1f\" "
                    "fill=\"rgb(%d,%d,%d)\" fill-opacity=\"%.2f\"/>\n",
                    x, y, qw, qh, r, r,
                    int(c.x*255), int(c.y*255), int(c.z*255), c.w);
          }
        }
      }
      // TriListPrimitive
      else if (auto tp = dynamic_cast<TriListPrimitive*>(prim.get())) {
        size_t nv = tp->_vertices.size();
        for (size_t i = 0; i + 2 < nv; i += 3) {
          auto& v0 = tp->_vertices[i];
          auto& v1 = tp->_vertices[i+1];
          auto& v2 = tp->_vertices[i+2];
          auto& c = v0->color;
          // Convert Y-up to Y-down
          fprintf(fp, "    <polygon points=\"%.1f,%.1f %.1f,%.1f %.1f,%.1f\" "
                  "fill=\"rgb(%d,%d,%d)\" fill-opacity=\"%.2f\" stroke=\"none\"/>\n",
                  v0->position.x, h - v0->position.y,
                  v1->position.x, h - v1->position.y,
                  v2->position.x, h - v2->position.y,
                  int(c.x*255), int(c.y*255), int(c.z*255), c.w);
        }
      }
      // TextPrimitive
      else if (auto txp = dynamic_cast<TextPrimitive*>(prim.get())) {
        auto& c = txp->_color;
        for (auto& item : txp->_items) {
          // Text positions are in screen coords (Y-down already)
          fprintf(fp, "    <text x=\"%.1f\" y=\"%.1f\" font-family=\"monospace\" font-size=\"14\" "
                  "fill=\"rgb(%d,%d,%d)\" fill-opacity=\"%.2f\">%s</text>\n",
                  item.position.x, item.position.y + 12,  // +12 for baseline offset
                  int(c.x*255), int(c.y*255), int(c.z*255), c.w,
                  item.text.c_str());
        }
      }
    }
    fprintf(fp, "  </g>\n");
  }
  fprintf(fp, "</svg>\n");
  fclose(fp);
  printf("[PrimCanvas] SVG exported to: %s\n", _svg_export_path.c_str());
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
