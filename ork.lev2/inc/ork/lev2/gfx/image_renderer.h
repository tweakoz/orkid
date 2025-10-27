////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/image.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <vector>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
// Forward declarations
///////////////////////////////////////////////////////////////////////////////

struct ImageBrush;
struct ImagePen;
struct ImageSampler;
struct ImageRenderer;
struct SDFBounds;

using image_brush_ptr_t = std::shared_ptr<ImageBrush>;
using image_pen_ptr_t = std::shared_ptr<ImagePen>;
using image_sampler_ptr_t = std::shared_ptr<ImageSampler>;
using image_renderer_ptr_t = std::shared_ptr<ImageRenderer>;
using sdfbounds_ptr_t = std::shared_ptr<SDFBounds>;

///////////////////////////////////////////////////////////////////////////////
// ImageSampler - texture sampling with wrapping modes
///////////////////////////////////////////////////////////////////////////////

struct ImageSampler {

  enum class WrapMode : crc_enum_t {
    CrcEnum(CLAMP),
    CrcEnum(REPEAT),
    CrcEnum(MIRROR)
  };

  ImageSampler();

  // Sample from RGBA32F image with wrapping
  fvec4 sample(image_ptr_t img, fvec2 coord);

  WrapMode _wrap_mode = WrapMode::REPEAT;
};

///////////////////////////////////////////////////////////////////////////////
// ImageBrush - for filled drawing operations
///////////////////////////////////////////////////////////////////////////////

struct ImageBrush {

  ImageBrush();
  ImageBrush(fvec4 solid_color);
  ImageBrush(image_ptr_t texture, fmtx3 texture_matrix = fmtx3());

  // Solid color fill
  fvec4 _solid_color = fvec4(1.0f, 1.0f, 1.0f, 1.0f);

  // Texture fill
  image_ptr_t _texture;
  fmtx3 _texture_matrix;  // Transform pixel coords -> texture coords
  bool _use_texture = false;

  // Sampler for texture lookups
  image_sampler_ptr_t _sampler;
};

///////////////////////////////////////////////////////////////////////////////
// ImagePen - for stroked drawing operations
///////////////////////////////////////////////////////////////////////////////

struct ImagePen {

  ImagePen();
  ImagePen(fvec4 color, float width = 1.0f);

  fvec4 _color = fvec4(1.0f, 1.0f, 1.0f, 1.0f);
  float _width = 1.0f;

  // Future: cap style, join style, dash patterns
};

///////////////////////////////////////////////////////////////////////////////
// SDFBounds - Axis-aligned bounding box for spatial culling
///////////////////////////////////////////////////////////////////////////////

struct SDFBounds {
  fvec2 min;
  fvec2 max;

  SDFBounds() : min(0, 0), max(0, 0) {}
  SDFBounds(fvec2 center, float radius)
    : min(center.x - radius, center.y - radius)
    , max(center.x + radius, center.y + radius) {}
  SDFBounds(fvec2 min_, fvec2 max_) : min(min_), max(max_) {}

  void expand(float margin) {
    min.x -= margin;
    min.y -= margin;
    max.x += margin;
    max.y += margin;
  }

  void transform(const fmtx3& mtx);
};

///////////////////////////////////////////////////////////////////////////////
// ImageRenderer - CPU-based AA drawing with SDF
// Operates on RGBA32F images only
// Maintains both color and distance field buffers
///////////////////////////////////////////////////////////////////////////////

struct ImageRenderer {

  ImageRenderer(int width, int height);
  ImageRenderer(image_ptr_t target);

  ///////////////////////////////////////////
  // Buffer management
  ///////////////////////////////////////////

  void resize(int width, int height);
  void clear(fvec4 color = fvec4(0, 0, 0, 0));
  void clearDistance(float distance = 1e10f);

  image_ptr_t colorBuffer() const { return _color_buffer; }
  image_ptr_t distanceBuffer() const { return _distance_buffer; }

  ///////////////////////////////////////////
  // Transform stack
  ///////////////////////////////////////////

  void pushTransform(const fmtx3& mtx);
  void popTransform();
  fmtx3 currentTransform() const;

  ///////////////////////////////////////////
  // Filled primitives (use ImageBrush)
  ///////////////////////////////////////////

  void fillBox(fvec2 center, fvec2 size, image_brush_ptr_t brush, float corner_radius = 0.0f);
  void fillCircle(fvec2 center, float radius, image_brush_ptr_t brush);
  void fillArc(fvec2 center, float radius, float start_angle, float end_angle, image_brush_ptr_t brush);
  void fillQuadraticBezier(fvec2 A, fvec2 B, fvec2 C, image_brush_ptr_t brush);

  ///////////////////////////////////////////
  // Stroked primitives (use ImagePen)
  ///////////////////////////////////////////

  void strokeLine(fvec2 p0, fvec2 p1, image_pen_ptr_t pen);
  void strokeBox(fvec2 center, fvec2 size, image_pen_ptr_t pen, float corner_radius = 0.0f);
  void strokeCircle(fvec2 center, float radius, image_pen_ptr_t pen);
  void strokeArc(fvec2 center, float radius, float start_angle, float end_angle, image_pen_ptr_t pen);
  void strokeQuadraticBezier(fvec2 A, fvec2 B, fvec2 C, image_pen_ptr_t pen);

  ///////////////////////////////////////////
  // Distance field operations
  ///////////////////////////////////////////

  // Get current distance field
  void exportDistanceField(image_ptr_t dest);

  // CSG operations (future)
  // void unionShape(...)
  // void intersectShape(...)
  // void subtractShape(...)

private:

  image_ptr_t _color_buffer;     // RGBA32F
  image_ptr_t _distance_buffer;  // R32F (stored as RGBA32F, using R channel)

  std::vector<fmtx3> _transform_stack;

  ///////////////////////////////////////////
  // Internal drawing helpers
  ///////////////////////////////////////////

  void _rasterizeFilled(
    std::function<float(fvec2)> sdf_func,
    image_brush_ptr_t brush,
    sdfbounds_ptr_t bounds
  );

  void _rasterizeStroked(
    std::function<float(fvec2)> sdf_func,
    image_pen_ptr_t pen,
    sdfbounds_ptr_t bounds
  );

  // SDF primitive functions (return signed distance)
  float _sdfLine(fvec2 point, fvec2 p0, fvec2 p1);
  float _sdfBox(fvec2 point, fvec2 center, fvec2 size, float corner_radius);
  float _sdfCircle(fvec2 point, fvec2 center, float radius);
  float _sdfArc(fvec2 point, fvec2 center, float radius, float start_angle, float end_angle);
  float _sdfQuadraticBezier(fvec2 point, fvec2 A, fvec2 B, fvec2 C);

  // AA coverage from distance
  float _coverage(float distance, float edge_width = 1.0f);
};

} // namespace ork::lev2
