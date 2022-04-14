////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

/*OrkAssert(img._format == dds::EFMT_BGR8);
ork::image::Vec4Image v4img;
v4img._width  = img._width;
v4img._height = img._height;
v4img._pixels.resize(img._width * img._height);
constexpr float pixelnorm = 1.0f / 255.0f;
auto dds_pixels           = (const uint8_t*)img._imagedata;
for (int y = 0; y < img._height; y++) {
  for (int x = 0; x < img._width; x++) {
    int ddsbase = (y * img._width + x) * 3;
    uint8_t r   = dds_pixels[ddsbase + 0];
    uint8_t g   = dds_pixels[ddsbase + 1];
    uint8_t b   = dds_pixels[ddsbase + 1];
    v4img.pixelWrite(x, y, fvec4(r, g, b, 255) * pixelnorm);
  }
}
*/
