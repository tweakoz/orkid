#include <ork/pch.h>
#include <ork/lev2/gfx/image_renderer.h>
#include <ork/math/misc_math.h>
#include <ork/kernel/opq.h>
#include <cmath>

namespace ork::lev2 {

// Chunk size for parallelized rendering
constexpr size_t IMG_RENDER_CHUNK_SIZE = 64;

///////////////////////////////////////////////////////////////////////////////
// ImageSampler
///////////////////////////////////////////////////////////////////////////////

ImageSampler::ImageSampler() {
}

fvec4 ImageSampler::sample(image_ptr_t img, fvec2 coord) {
  if (!img || img->_format != EBufferFormat::RGBA32F) {
    return fvec4(0, 0, 0, 0);
  }

  float u = coord.x;
  float v = coord.y;

  // Apply wrap mode
  switch (_wrap_mode) {
    case ImageSampler::WrapMode::CLAMP:
      u = std::clamp(u, 0.0f, 1.0f);
      v = std::clamp(v, 0.0f, 1.0f);
      break;
    case ImageSampler::WrapMode::REPEAT:
      u = u - std::floor(u);
      v = v - std::floor(v);
      break;
    case ImageSampler::WrapMode::MIRROR:
      u = u - std::floor(u);
      v = v - std::floor(v);
      if (int(std::floor(u * 0.5f)) % 2) u = 1.0f - u;
      if (int(std::floor(v * 0.5f)) % 2) v = 1.0f - v;
      break;
  }

  // Convert to pixel coordinates
  int x = int(u * (img->_width - 1));
  int y = int(v * (img->_height - 1));

  // Clamp to image bounds
  x = std::clamp(x, 0, int(img->_width - 1));
  y = std::clamp(y, 0, int(img->_height - 1));

  // Sample pixel (RGBA32F = 4 floats per pixel)
  const float* pixel = reinterpret_cast<const float*>(img->_data->data()) + (y * img->_width + x) * 4;
  return fvec4(pixel[0], pixel[1], pixel[2], pixel[3]);
}

///////////////////////////////////////////////////////////////////////////////
// ImageBrush
///////////////////////////////////////////////////////////////////////////////

ImageBrush::ImageBrush() {
  _sampler = std::make_shared<ImageSampler>();
}

ImageBrush::ImageBrush(fvec4 solid_color)
  : _solid_color(solid_color) {
  _sampler = std::make_shared<ImageSampler>();
}

ImageBrush::ImageBrush(image_ptr_t texture, fmtx3 texture_matrix)
  : _texture(texture)
  , _texture_matrix(texture_matrix)
  , _use_texture(true) {
  _sampler = std::make_shared<ImageSampler>();
}

///////////////////////////////////////////////////////////////////////////////
// ImagePen
///////////////////////////////////////////////////////////////////////////////

ImagePen::ImagePen() {
}

ImagePen::ImagePen(fvec4 color, float width)
  : _color(color)
  , _width(width) {
}

///////////////////////////////////////////////////////////////////////////////
// ImageRenderer
///////////////////////////////////////////////////////////////////////////////

ImageRenderer::ImageRenderer(int width, int height) {
  _color_buffer = std::make_shared<Image>();
  _color_buffer->initWithFormat(width, height, EBufferFormat::RGBA32F);

  _distance_buffer = std::make_shared<Image>();
  _distance_buffer->initWithFormat(width, height, EBufferFormat::RGBA32F);

  _transform_stack.push_back(fmtx3());  // Identity matrix

  clear();
  clearDistance();
}

ImageRenderer::ImageRenderer(image_ptr_t target)
  : _color_buffer(target) {

  OrkAssert(target->_format == EBufferFormat::RGBA32F);

  _distance_buffer = std::make_shared<Image>();
  _distance_buffer->initWithFormat(target->_width, target->_height, EBufferFormat::RGBA32F);

  _transform_stack.push_back(fmtx3());  // Identity matrix

  clearDistance();
}

void ImageRenderer::resize(int width, int height) {
  _color_buffer->initWithFormat(width, height, EBufferFormat::RGBA32F);
  _distance_buffer->initWithFormat(width, height, EBufferFormat::RGBA32F);
  clear();
  clearDistance();
}

void ImageRenderer::clear(fvec4 color) {
  float* pixels = const_cast<float*>(reinterpret_cast<const float*>(_color_buffer->_data->data()));
  size_t num_pixels = _color_buffer->_width * _color_buffer->_height;

  // Parallelize clear by chunking pixels
  size_t pixels_per_chunk = IMG_RENDER_CHUNK_SIZE * _color_buffer->_width;
  size_t num_chunks = (num_pixels + pixels_per_chunk - 1) / pixels_per_chunk;
  std::atomic<int> chunkcounter = num_chunks;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, pixels, num_pixels, pixels_per_chunk, color, &chunkcounter]() {
      size_t start = chunk * pixels_per_chunk;
      size_t end = std::min(start + pixels_per_chunk, num_pixels);

      for (size_t i = start; i < end; i++) {
        pixels[i * 4 + 0] = color.x;
        pixels[i * 4 + 1] = color.y;
        pixels[i * 4 + 2] = color.z;
        pixels[i * 4 + 3] = color.w;
      }
      chunkcounter.fetch_sub(1);
    };
    opq::concurrentQueue()->enqueue(op);
  }

  while(chunkcounter.load() > 0) {
    std::this_thread::yield();
  }
}

void ImageRenderer::clearDistance(float distance) {
  float* pixels = const_cast<float*>(reinterpret_cast<const float*>(_distance_buffer->_data->data()));
  size_t num_pixels = _distance_buffer->_width * _distance_buffer->_height;

  // Parallelize clear by chunking pixels
  size_t pixels_per_chunk = IMG_RENDER_CHUNK_SIZE * _distance_buffer->_width;
  size_t num_chunks = (num_pixels + pixels_per_chunk - 1) / pixels_per_chunk;
  std::atomic<int> chunkcounter = num_chunks;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, pixels, num_pixels, pixels_per_chunk, distance, &chunkcounter]() {
      size_t start = chunk * pixels_per_chunk;
      size_t end = std::min(start + pixels_per_chunk, num_pixels);

      for (size_t i = start; i < end; i++) {
        pixels[i * 4 + 0] = distance;  // R channel stores distance
        pixels[i * 4 + 1] = 0.0f;
        pixels[i * 4 + 2] = 0.0f;
        pixels[i * 4 + 3] = 0.0f;
      }
      chunkcounter.fetch_sub(1);
    };
    opq::concurrentQueue()->enqueue(op);
  }

  while(chunkcounter.load() > 0) {
    std::this_thread::yield();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Transform stack
///////////////////////////////////////////////////////////////////////////////

void ImageRenderer::pushTransform(const fmtx3& mtx) {
  fmtx3 current = _transform_stack.back();
  _transform_stack.push_back(current * mtx);
}

void ImageRenderer::popTransform() {
  if (_transform_stack.size() > 1) {
    _transform_stack.pop_back();
  }
}

fmtx3 ImageRenderer::currentTransform() const {
  return _transform_stack.back();
}

///////////////////////////////////////////////////////////////////////////////
// SDF primitive functions
///////////////////////////////////////////////////////////////////////////////

float ImageRenderer::_sdfLine(fvec2 point, fvec2 p0, fvec2 p1) {
  fvec2 pa = point - p0;
  fvec2 ba = p1 - p0;
  float h = std::clamp(pa.dotWith(ba) / ba.dotWith(ba), 0.0f, 1.0f);
  return (pa - ba * h).length();
}

float ImageRenderer::_sdfBox(fvec2 point, fvec2 center, fvec2 size, float corner_radius) {
  fvec2 d = fvec2(std::abs(point.x - center.x), std::abs(point.y - center.y)) - size * 0.5f + fvec2(corner_radius, corner_radius);
  return std::min(std::max(d.x, d.y), 0.0f) + fvec2(std::max(d.x, 0.0f), std::max(d.y, 0.0f)).length() - corner_radius;
}

float ImageRenderer::_sdfCircle(fvec2 point, fvec2 center, float radius) {
  return (point - center).length() - radius;
}

float ImageRenderer::_sdfArc(fvec2 point, fvec2 center, float radius, float start_angle, float end_angle) {
  fvec2 p = point - center;
  float angle = std::atan2(p.y, p.x);

  // Normalize angles to [0, 2π]
  while (start_angle < 0) start_angle += PI2;
  while (end_angle < 0) end_angle += PI2;
  while (angle < 0) angle += PI2;

  // Check if point angle is within arc range
  bool in_arc = false;
  if (end_angle >= start_angle) {
    in_arc = (angle >= start_angle && angle <= end_angle);
  } else {
    in_arc = (angle >= start_angle || angle <= end_angle);
  }

  float dist_to_circle = std::abs(p.length() - radius);

  if (in_arc) {
    return dist_to_circle;
  } else {
    // Distance to closest arc endpoint
    fvec2 p0(std::cos(start_angle) * radius, std::sin(start_angle) * radius);
    fvec2 p1(std::cos(end_angle) * radius, std::sin(end_angle) * radius);
    float d0 = (p - p0).length();
    float d1 = (p - p1).length();
    return std::min(d0, d1);
  }
}

///////////////////////////////////////////////////////////////////////////////
// Coverage calculation for AA
///////////////////////////////////////////////////////////////////////////////

float ImageRenderer::_coverage(float distance, float edge_width) {
  // Smoothstep for antialiasing
  return std::clamp(0.5f - distance / edge_width, 0.0f, 1.0f);
}

///////////////////////////////////////////////////////////////////////////////
// SDFBounds
///////////////////////////////////////////////////////////////////////////////

void SDFBounds::transform(const fmtx3& mtx) {
  // Transform all 4 corners of the bounding box
  fvec3 corners[4] = {
    fvec3(min.x, min.y, 1.0f),
    fvec3(max.x, min.y, 1.0f),
    fvec3(min.x, max.y, 1.0f),
    fvec3(max.x, max.y, 1.0f)
  };

  // Initialize with first corner
  fvec3 tc0 = mtx * corners[0];
  float new_min_x = tc0.x;
  float new_max_x = tc0.x;
  float new_min_y = tc0.y;
  float new_max_y = tc0.y;

  // Expand to include all corners
  for (int i = 1; i < 4; i++) {
    fvec3 tc = mtx * corners[i];
    new_min_x = std::min(new_min_x, tc.x);
    new_max_x = std::max(new_max_x, tc.x);
    new_min_y = std::min(new_min_y, tc.y);
    new_max_y = std::max(new_max_y, tc.y);
  }

  min = fvec2(new_min_x, new_min_y);
  max = fvec2(new_max_x, new_max_y);
}

///////////////////////////////////////////////////////////////////////////////
// Distance field export
///////////////////////////////////////////////////////////////////////////////

void ImageRenderer::exportDistanceField(image_ptr_t dest) {
  OrkAssert(dest->_format == EBufferFormat::RGBA32F);
  OrkAssert(dest->_width == _distance_buffer->_width);
  OrkAssert(dest->_height == _distance_buffer->_height);

  // Copy distance buffer
  std::memcpy(const_cast<uint8_t*>(dest->_data->data()), _distance_buffer->_data->data(), _distance_buffer->_data->length());
}

///////////////////////////////////////////////////////////////////////////////
// Rasterization helpers
///////////////////////////////////////////////////////////////////////////////

void ImageRenderer::_rasterizeFilled(
  std::function<float(fvec2)> sdf_func,
  image_brush_ptr_t brush,
  sdfbounds_ptr_t bounds
) {
  if (!brush || !bounds) return;

  float* color_pixels = const_cast<float*>(reinterpret_cast<const float*>(_color_buffer->_data->data()));
  float* dist_pixels = const_cast<float*>(reinterpret_cast<const float*>(_distance_buffer->_data->data()));

  int width = _color_buffer->_width;
  int height = _color_buffer->_height;

  fmtx3 inv_transform = currentTransform().inverse();

  // Transform bounds to screen space
  bounds->transform(currentTransform());

  // Clamp bounds to screen and convert to pixel indices
  int x_min = std::max(0, int(std::floor(bounds->min.x)));
  int y_min = std::max(0, int(std::floor(bounds->min.y)));
  int x_max = std::min(width, int(std::ceil(bounds->max.x)));
  int y_max = std::min(height, int(std::ceil(bounds->max.y)));

  // Early exit if completely outside screen
  if (x_min >= x_max || y_min >= y_max) return;

  // Parallelize by chunking rows within bounds
  size_t bounded_height = y_max - y_min;
  size_t num_chunks = (bounded_height + IMG_RENDER_CHUNK_SIZE - 1) / IMG_RENDER_CHUNK_SIZE;
  std::atomic<int> chunkcounter = num_chunks;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, this, sdf_func, brush, &chunkcounter,
               color_pixels, dist_pixels, width, height, inv_transform,
               x_min, x_max, y_min, y_max]() {
      size_t y_start = y_min + chunk * IMG_RENDER_CHUNK_SIZE;
      size_t y_end = std::min(y_start + IMG_RENDER_CHUNK_SIZE, size_t(y_max));

      for (size_t y = y_start; y < y_end; y++) {
        for (int x = x_min; x < x_max; x++) {
          // Transform pixel to shape space
          fvec2 pixel_pos(x + 0.5f, y + 0.5f);
          fvec3 transformed = inv_transform.transform(fvec3(pixel_pos.x, pixel_pos.y, 1.0f));
          fvec2 shape_pos(transformed.x, transformed.y);

          // Compute SDF distance
          float distance = sdf_func(shape_pos);

          // Compute coverage for AA
          float coverage = _coverage(distance, 1.0f);

          if (coverage > 0.0f) {
            // Determine fill color
            fvec4 fill_color;
            if (brush->_use_texture && brush->_texture) {
              // Transform shape position to texture space
              fvec3 tex_transformed = brush->_texture_matrix.transform(fvec3(shape_pos.x, shape_pos.y, 1.0f));
              fvec2 tex_coord(tex_transformed.x, tex_transformed.y);
              fill_color = brush->_sampler->sample(brush->_texture, tex_coord);
            } else {
              fill_color = brush->_solid_color;
            }

            // Blend with existing color (over operator)
            int pixel_index = (y * width + x) * 4;
            float alpha = fill_color.w * coverage;

            color_pixels[pixel_index + 0] = fill_color.x * alpha + color_pixels[pixel_index + 0] * (1.0f - alpha);
            color_pixels[pixel_index + 1] = fill_color.y * alpha + color_pixels[pixel_index + 1] * (1.0f - alpha);
            color_pixels[pixel_index + 2] = fill_color.z * alpha + color_pixels[pixel_index + 2] * (1.0f - alpha);
            color_pixels[pixel_index + 3] = alpha + color_pixels[pixel_index + 3] * (1.0f - alpha);

            // Update distance field (min operation)
            dist_pixels[pixel_index + 0] = std::min(dist_pixels[pixel_index + 0], distance);
          }
        }
      }
      chunkcounter.fetch_sub(1);
    };
    opq::concurrentQueue()->enqueue(op);
  }

  while(chunkcounter.load() > 0) {
    std::this_thread::yield();
  }
}

void ImageRenderer::_rasterizeStroked(
  std::function<float(fvec2)> sdf_func,
  image_pen_ptr_t pen,
  sdfbounds_ptr_t bounds
) {
  if (!pen || !bounds) return;

  float* color_pixels = const_cast<float*>(reinterpret_cast<const float*>(_color_buffer->_data->data()));
  float* dist_pixels = const_cast<float*>(reinterpret_cast<const float*>(_distance_buffer->_data->data()));

  int width = _color_buffer->_width;
  int height = _color_buffer->_height;

  fmtx3 inv_transform = currentTransform().inverse();
  float half_width = pen->_width * 0.5f;

  // Transform bounds to screen space
  bounds->transform(currentTransform());

  // Clamp bounds to screen and convert to pixel indices
  int x_min = std::max(0, int(std::floor(bounds->min.x)));
  int y_min = std::max(0, int(std::floor(bounds->min.y)));
  int x_max = std::min(width, int(std::ceil(bounds->max.x)));
  int y_max = std::min(height, int(std::ceil(bounds->max.y)));

  // Early exit if completely outside screen
  if (x_min >= x_max || y_min >= y_max) return;

  // Parallelize by chunking rows within bounds
  size_t bounded_height = y_max - y_min;
  size_t num_chunks = (bounded_height + IMG_RENDER_CHUNK_SIZE - 1) / IMG_RENDER_CHUNK_SIZE;
  std::atomic<int> chunkcounter = num_chunks;

  for (size_t chunk = 0; chunk < num_chunks; chunk++) {
    auto op = [chunk, this, sdf_func, pen, &chunkcounter,
               color_pixels, dist_pixels, width, height, inv_transform, half_width,
               x_min, x_max, y_min, y_max]() {
      size_t y_start = y_min + chunk * IMG_RENDER_CHUNK_SIZE;
      size_t y_end = std::min(y_start + IMG_RENDER_CHUNK_SIZE, size_t(y_max));

      for (size_t y = y_start; y < y_end; y++) {
        for (int x = x_min; x < x_max; x++) {
          // Transform pixel to shape space
          fvec2 pixel_pos(x + 0.5f, y + 0.5f);
          fvec3 transformed = inv_transform.transform(fvec3(pixel_pos.x, pixel_pos.y, 1.0f));
          fvec2 shape_pos(transformed.x, transformed.y);

          // Compute SDF distance to shape
          float shape_distance = sdf_func(shape_pos);

          // Distance to stroke (distance to shape edge)
          float stroke_distance = std::abs(shape_distance) - half_width;

          // Compute coverage for AA
          float coverage = _coverage(stroke_distance, 1.0f);

          if (coverage > 0.0f) {
            // Blend with existing color (over operator)
            int pixel_index = (y * width + x) * 4;
            float alpha = pen->_color.w * coverage;

            color_pixels[pixel_index + 0] = pen->_color.x * alpha + color_pixels[pixel_index + 0] * (1.0f - alpha);
            color_pixels[pixel_index + 1] = pen->_color.y * alpha + color_pixels[pixel_index + 1] * (1.0f - alpha);
            color_pixels[pixel_index + 2] = pen->_color.z * alpha + color_pixels[pixel_index + 2] * (1.0f - alpha);
            color_pixels[pixel_index + 3] = alpha + color_pixels[pixel_index + 3] * (1.0f - alpha);

            // Update distance field (min operation)
            dist_pixels[pixel_index + 0] = std::min(dist_pixels[pixel_index + 0], stroke_distance);
          }
        }
      }
      chunkcounter.fetch_sub(1);
    };
    opq::concurrentQueue()->enqueue(op);
  }

  while(chunkcounter.load() > 0) {
    std::this_thread::yield();
  }
}

///////////////////////////////////////////////////////////////////////////////
// Filled primitives
///////////////////////////////////////////////////////////////////////////////

void ImageRenderer::fillBox(fvec2 center, fvec2 size, image_brush_ptr_t brush, float corner_radius) {
  auto sdf = [this, center, size, corner_radius](fvec2 p) {
    return _sdfBox(p, center, size, corner_radius);
  };
  auto bounds = std::make_shared<SDFBounds>(fvec2(center.x - size.x/2 - corner_radius, center.y - size.y/2 - corner_radius),
                                              fvec2(center.x + size.x/2 + corner_radius, center.y + size.y/2 + corner_radius));
  bounds->expand(2.0f);
  _rasterizeFilled(sdf, brush, bounds);
}

void ImageRenderer::fillCircle(fvec2 center, float radius, image_brush_ptr_t brush) {
  auto sdf = [this, center, radius](fvec2 p) {
    return _sdfCircle(p, center, radius);
  };
  auto bounds = std::make_shared<SDFBounds>(center, radius);
  bounds->expand(2.0f);
  _rasterizeFilled(sdf, brush, bounds);
}

void ImageRenderer::fillArc(fvec2 center, float radius, float start_angle, float end_angle, image_brush_ptr_t brush) {
  auto sdf = [this, center, radius, start_angle, end_angle](fvec2 p) {
    return _sdfArc(p, center, radius, start_angle, end_angle);
  };
  auto bounds = std::make_shared<SDFBounds>(center, radius);
  bounds->expand(2.0f);
  _rasterizeFilled(sdf, brush, bounds);
}

///////////////////////////////////////////////////////////////////////////////
// Stroked primitives
///////////////////////////////////////////////////////////////////////////////

void ImageRenderer::strokeLine(fvec2 p0, fvec2 p1, image_pen_ptr_t pen) {
  auto sdf = [this, p0, p1](fvec2 p) {
    return _sdfLine(p, p0, p1);
  };
  float minx = std::min(p0.x, p1.x);
  float maxx = std::max(p0.x, p1.x);
  float miny = std::min(p0.y, p1.y);
  float maxy = std::max(p0.y, p1.y);
  auto bounds = std::make_shared<SDFBounds>(fvec2(minx, miny), fvec2(maxx, maxy));
  bounds->expand(pen->_width + 2.0f);
  _rasterizeStroked(sdf, pen, bounds);
}

void ImageRenderer::strokeBox(fvec2 center, fvec2 size, image_pen_ptr_t pen, float corner_radius) {
  auto sdf = [this, center, size, corner_radius](fvec2 p) {
    return _sdfBox(p, center, size, corner_radius);
  };
  auto bounds = std::make_shared<SDFBounds>(fvec2(center.x - size.x/2 - corner_radius, center.y - size.y/2 - corner_radius),
                                              fvec2(center.x + size.x/2 + corner_radius, center.y + size.y/2 + corner_radius));
  bounds->expand(pen->_width + 2.0f);
  _rasterizeStroked(sdf, pen, bounds);
}

void ImageRenderer::strokeCircle(fvec2 center, float radius, image_pen_ptr_t pen) {
  auto sdf = [this, center, radius](fvec2 p) {
    return _sdfCircle(p, center, radius);
  };
  auto bounds = std::make_shared<SDFBounds>(center, radius);
  bounds->expand(pen->_width + 2.0f);
  _rasterizeStroked(sdf, pen, bounds);
}

void ImageRenderer::strokeArc(fvec2 center, float radius, float start_angle, float end_angle, image_pen_ptr_t pen) {
  auto sdf = [this, center, radius, start_angle, end_angle](fvec2 p) {
    return _sdfArc(p, center, radius, start_angle, end_angle);
  };
  auto bounds = std::make_shared<SDFBounds>(center, radius);
  bounds->expand(pen->_width + 2.0f);
  _rasterizeStroked(sdf, pen, bounds);
}

///////////////////////////////////////////////////////////////////////////////
// SDF Bezier
///////////////////////////////////////////////////////////////////////////////

float ImageRenderer::_sdfQuadraticBezier(fvec2 point, fvec2 A, fvec2 B, fvec2 C) {
  fvec2 a = B - A;
  fvec2 b = A - fvec2(2.0f * B.x, 2.0f * B.y) + C;
  fvec2 c = a * 2.0f;
  fvec2 d = A - point;

  float kk = 1.0f / fvec2(b.x, b.y).dotWith(b);
  float kx = kk * fvec2(a.x, a.y).dotWith(b);
  float ky = kk * (2.0f * fvec2(a.x, a.y).dotWith(a) + fvec2(d.x, d.y).dotWith(b)) / 3.0f;
  float kz = kk * fvec2(d.x, d.y).dotWith(a);

  float p = ky - kx * kx;
  float q = kx * (2.0f * kx * kx - 3.0f * ky) + kz;
  float p3 = p * p * p;
  float q2 = q * q;
  float h = q2 + 4.0f * p3;

  float res;
  if (h >= 0.0f) {
    h = std::sqrt(h);
    fvec2 x = fvec2(h - q, -h - q) / 2.0f;
    fvec2 uv = fvec2(std::copysignf(std::pow(std::abs(x.x), 1.0f / 3.0f), x.x),
                      std::copysignf(std::pow(std::abs(x.y), 1.0f / 3.0f), x.y));
    float t = std::clamp(uv.x + uv.y - kx, 0.0f, 1.0f);
    fvec2 q_val = d + (c + b * t) * t;
    res = q_val.dotWith(q_val);
  } else {
    float z = std::sqrt(-p);
    float v = std::acos(q / (p * z * 2.0f)) / 3.0f;
    float m = std::cos(v);
    float n = std::sin(v) * 1.732050808f;
    fvec3 t = fvec3(m + m, -n - m, n - m) * z - fvec3(kx, kx, kx);
    t = fvec3(std::clamp(t.x, 0.0f, 1.0f), std::clamp(t.y, 0.0f, 1.0f), std::clamp(t.z, 0.0f, 1.0f));
    fvec2 qx = d + (c + b * t.x) * t.x;
    float dx = qx.dotWith(qx);
    fvec2 qy = d + (c + b * t.y) * t.y;
    float dy = qy.dotWith(qy);
    res = std::min(dx, dy);
  }

  return std::sqrt(res);
}

void ImageRenderer::fillQuadraticBezier(fvec2 A, fvec2 B, fvec2 C, image_brush_ptr_t brush) {
  auto sdf = [this, A, B, C](fvec2 p) {
    return _sdfQuadraticBezier(p, A, B, C);
  };
  float minx = std::min({A.x, B.x, C.x});
  float maxx = std::max({A.x, B.x, C.x});
  float miny = std::min({A.y, B.y, C.y});
  float maxy = std::max({A.y, B.y, C.y});
  auto bounds = std::make_shared<SDFBounds>(fvec2(minx, miny), fvec2(maxx, maxy));
  bounds->expand(2.0f);
  _rasterizeFilled(sdf, brush, bounds);
}

void ImageRenderer::strokeQuadraticBezier(fvec2 A, fvec2 B, fvec2 C, image_pen_ptr_t pen) {
  auto sdf = [this, A, B, C](fvec2 p) {
    return _sdfQuadraticBezier(p, A, B, C);
  };
  float minx = std::min({A.x, B.x, C.x});
  float maxx = std::max({A.x, B.x, C.x});
  float miny = std::min({A.y, B.y, C.y});
  float maxy = std::max({A.y, B.y, C.y});
  auto bounds = std::make_shared<SDFBounds>(fvec2(minx, miny), fvec2(maxx, maxy));
  bounds->expand(pen->_width + 2.0f);
  _rasterizeStroked(sdf, pen, bounds);
}

} // namespace ork::lev2
