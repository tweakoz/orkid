////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/image_renderer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void pyinit_gfx_image_renderer(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // ImageSampler
  /////////////////////////////////////////////////////////////////////////////////
  auto imagesampler_type = //
      py::class_<ImageSampler, image_sampler_ptr_t>(module_lev2, "ImageSampler")
          .def(py::init<>())
          .def(
              "sample",
              [](image_sampler_ptr_t sampler, image_ptr_t img, fvec2 coord) -> fvec4 {
                return sampler->sample(img, coord);
              })
          .def_property(
              "wrap_mode",
              [](image_sampler_ptr_t sampler) -> crcstring_ptr_t {
                return std::make_shared<CrcString>(uint64_t(sampler->_wrap_mode));
              },
              [](image_sampler_ptr_t sampler, crcstring_ptr_t mode) {
                sampler->_wrap_mode = ImageSampler::WrapMode(mode->hashed());
              });
  type_codec->registerStdCodec<image_sampler_ptr_t>(imagesampler_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ImageBrush
  /////////////////////////////////////////////////////////////////////////////////
  auto imagebrush_type = //
      py::class_<ImageBrush, image_brush_ptr_t>(module_lev2, "ImageBrush")
          .def(py::init<>())
          .def(py::init<fvec4>())
          .def(py::init<image_ptr_t, fmtx3>())
          .def_property(
              "solid_color",
              [](image_brush_ptr_t brush) -> fvec4 { return brush->_solid_color; },
              [](image_brush_ptr_t brush, fvec4 color) { brush->_solid_color = color; })
          .def_property(
              "texture",
              [](image_brush_ptr_t brush) -> image_ptr_t { return brush->_texture; },
              [](image_brush_ptr_t brush, image_ptr_t tex) {
                brush->_texture = tex;
                brush->_use_texture = (tex != nullptr);
              })
          .def_property(
              "texture_matrix",
              [](image_brush_ptr_t brush) -> fmtx3 { return brush->_texture_matrix; },
              [](image_brush_ptr_t brush, fmtx3 mtx) { brush->_texture_matrix = mtx; })
          .def_property(
              "use_texture",
              [](image_brush_ptr_t brush) -> bool { return brush->_use_texture; },
              [](image_brush_ptr_t brush, bool use) { brush->_use_texture = use; })
          .def_property(
              "sampler",
              [](image_brush_ptr_t brush) -> image_sampler_ptr_t { return brush->_sampler; },
              [](image_brush_ptr_t brush, image_sampler_ptr_t sampler) { brush->_sampler = sampler; });
  type_codec->registerStdCodec<image_brush_ptr_t>(imagebrush_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ImagePen
  /////////////////////////////////////////////////////////////////////////////////
  auto imagepen_type = //
      py::class_<ImagePen, image_pen_ptr_t>(module_lev2, "ImagePen")
          .def(py::init<>())
          .def(py::init<fvec4, float>())
          .def_property(
              "color",
              [](image_pen_ptr_t pen) -> fvec4 { return pen->_color; },
              [](image_pen_ptr_t pen, fvec4 color) { pen->_color = color; })
          .def_property(
              "width",
              [](image_pen_ptr_t pen) -> float { return pen->_width; },
              [](image_pen_ptr_t pen, float width) { pen->_width = width; });
  type_codec->registerStdCodec<image_pen_ptr_t>(imagepen_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ImageRenderer
  /////////////////////////////////////////////////////////////////////////////////
  auto imagerenderer_type = //
      py::class_<ImageRenderer, image_renderer_ptr_t>(module_lev2, "ImageRenderer")
          .def(py::init<int, int>())
          .def(py::init<image_ptr_t>())
          .def("resize", &ImageRenderer::resize)
          .def("clear", &ImageRenderer::clear, py::arg("color") = fvec4(0, 0, 0, 0))
          .def("clearDistance", &ImageRenderer::clearDistance, py::arg("distance") = 1e10f)
          .def_property_readonly(
              "color_buffer",
              [](image_renderer_ptr_t renderer) -> image_ptr_t { return renderer->colorBuffer(); })
          .def_property_readonly(
              "distance_buffer",
              [](image_renderer_ptr_t renderer) -> image_ptr_t { return renderer->distanceBuffer(); })
          // Transform stack
          .def("pushTransform", &ImageRenderer::pushTransform)
          .def("popTransform", &ImageRenderer::popTransform)
          .def("currentTransform", &ImageRenderer::currentTransform)
          // Filled primitives
          .def(
              "fillBox",
              &ImageRenderer::fillBox,
              py::arg("center"),
              py::arg("size"),
              py::arg("brush"),
              py::arg("corner_radius") = 0.0f)
          .def("fillCircle", &ImageRenderer::fillCircle)
          .def("fillArc", &ImageRenderer::fillArc)
          // Stroked primitives
          .def("strokeLine", &ImageRenderer::strokeLine)
          .def(
              "strokeBox",
              &ImageRenderer::strokeBox,
              py::arg("center"),
              py::arg("size"),
              py::arg("pen"),
              py::arg("corner_radius") = 0.0f)
          .def("strokeCircle", &ImageRenderer::strokeCircle)
          .def("strokeArc", &ImageRenderer::strokeArc)
          // Distance field
          .def("exportDistanceField", &ImageRenderer::exportDistanceField);
  type_codec->registerStdCodec<image_renderer_ptr_t>(imagerenderer_type);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
