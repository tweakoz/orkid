////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/image.h>
#include <lunasvg/lunasvg.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

image_ptr_t Image::fromSvgString(const std::string& svg, int width, int height) {
  auto doc = lunasvg::Document::loadFromData(svg);
  if (!doc) {
    return nullptr;
  }
  auto bitmap = doc->renderToBitmap(width, height, 0x00000000);
  if (bitmap.isNull()) {
    return nullptr;
  }
  bitmap.convertToRGBA();

  auto image = std::make_shared<Image>();
  image->initWithFormat(bitmap.width(), bitmap.height(), EBufferFormat::RGBA8);
  memcpy(image->pixel8(0, 0), bitmap.data(), bitmap.width() * bitmap.height() * 4);
  image->_debugName = "svg";
  return image;
}

///////////////////////////////////////////////////////////////////////////////

image_ptr_t Image::fromSvgString(const std::string& svg, int size) {
  auto doc = lunasvg::Document::loadFromData(svg);
  if (!doc) {
    return nullptr;
  }
  float iw = doc->width();
  float ih = doc->height();
  if (iw <= 0 || ih <= 0) {
    return nullptr;
  }
  int w, h;
  if (iw >= ih) {
    w = size;
    h = static_cast<int>(size * ih / iw + 0.5f);
  } else {
    h = size;
    w = static_cast<int>(size * iw / ih + 0.5f);
  }
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  return fromSvgString(svg, w, h);
}

///////////////////////////////////////////////////////////////////////////////

fvec2 Image::svgIntrinsicSize(const std::string& svg) {
  auto doc = lunasvg::Document::loadFromData(svg);
  if (!doc) {
    return fvec2(0, 0);
  }
  return fvec2(doc->width(), doc->height());
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
