////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/image.h>
#include <ork/kernel/memcpy.inl>
#include <pybind11/numpy.h>
#include <iostream>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void pyinit_gfx_image(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  auto image_type = //
      py::class_<Image, image_ptr_t>(module_lev2, "Image")
      .def(py::init([]() -> image_ptr_t {
        return std::make_shared<Image>();
      }))
      .def("initWithFormat", [](image_ptr_t img, int w, int h, crcstring_ptr_t fmt) {
        img->initWithFormat(w, h, EBufferFormat(fmt->hashed()));
      })
      .def("pixel32f", [](image_ptr_t img, int x, int y) -> py::array_t<float> {
        float* pixel = img->pixel32f(x, y);
        return py::array_t<float>(
          {img->_numcomponents},  // shape
          {sizeof(float)},         // strides
          pixel,                   // data pointer
          py::cast(img)            // parent object to keep alive
        );
      })
      .def_static("createFromFile", [](py::object inpath) -> image_ptr_t {
        auto as_str = py::cast<py::str>(inpath);
        auto datablock = ::ork::File::loadDatablock(as_str.cast<std::string>());
        auto img = std::make_shared<Image>();
        img->initFromDataBlock(datablock);
        return img;
      })
      .def_static("createRGB8FromColor", [](int w, int h, fvec3 color) -> image_ptr_t {
        auto img = std::make_shared<Image>();
        img->initRGB8WithColor(w,h,color);
        return img;
      })
      .def_static("createRGBA8FromColor", [](int w, int h, fvec4 color) -> image_ptr_t {
        auto img = std::make_shared<Image>();
        img->initRGBA8WithColor(w,h,color);
        return img;
      })
      .def_static("createFromBuffer", [](int w, int h, crcstring_ptr_t fmt, py::buffer data) -> image_ptr_t {
         py::buffer_info info  = data.request();
         auto format_code = EBufferFormat(fmt->hashed());
          auto img = std::make_shared<Image>();
         switch(format_code){
          case EBufferFormat::RGB8:{
            OrkAssert(info.format == py::format_descriptor<uint8_t>::format());
            int data_len = info.size;
            auto data_ptr = static_cast<uint8_t*>(info.ptr);
            OrkAssert(data_len == (w*h*3));
            img->init(w,h,3,1);
            img->_format = format_code;
            auto data_out = (void*) img->_data->data();
            memcpy_fast(data_out,data_ptr,data_len);
            //printf( "got good rgb8 bufferdata <%p>\n", data_ptr );
            break;
          }
          case EBufferFormat::RGBA8:{
            OrkAssert(info.format == py::format_descriptor<uint8_t>::format());
            int data_len = info.size;
            auto data_ptr = static_cast<uint8_t*>(info.ptr);
            OrkAssert(data_len == (w*h*4));
            img->init(w,h,4,1);
            img->_format = format_code;
            auto data_out = (void*) img->_data->data();
            memcpy_fast(data_out,data_ptr,data_len);
            //printf( "got good rgb8 bufferdata <%p>\n", data_ptr );
            break;
          }
          default:
            OrkAssert(false);

         }
        return img;
      })
      .def_property_readonly("width", [](image_ptr_t img) -> int { return img->_width; })
      .def_property_readonly("height", [](image_ptr_t img) -> int { return img->_height; })
      .def_property_readonly("depth", [](image_ptr_t img) -> int { return img->_depth; })
      .def_property_readonly("numcomponents", [](image_ptr_t img) -> int { return img->_numcomponents; })
      .def_property_readonly("bytesPerChannel", [](image_ptr_t img) -> int { return img->_bytesPerChannel; })
      .def_property_readonly("format", [](image_ptr_t img) -> int { return int(img->_format); })
      .def_property_readonly("data", [](image_ptr_t img) -> datablock_ptr_t { return img->_data; })
      .def("writeToFile", [](image_ptr_t img, const std::string& outpath) {
        img->writeToFile(file::Path(outpath));
      })
      .def("invert", [](image_ptr_t img, uint8_t channel_mask) {
        img->invert(channel_mask);
      }, py::arg("channel_mask") = 0x0F) // Default: invert all channels (RGBA)
      .def_property_readonly("inverted", [](image_ptr_t img) -> image_ptr_t {
        auto result = std::make_shared<Image>(*img); // Copy constructor
        result->invert(0x0F); // Invert all channels
        return result;
      })
      .def("gamma", [](image_ptr_t img, float gamma_value, uint8_t channel_mask) {
        img->gamma(gamma_value, channel_mask);
      }, py::arg("gamma_value"), py::arg("channel_mask") = 0x0F)
      .def("gammaed", [](image_ptr_t img, float gamma_value, uint8_t channel_mask) -> image_ptr_t {
        auto result = std::make_shared<Image>(*img);
        result->gamma(gamma_value, channel_mask);
        return result;
      }, py::arg("gamma_value"), py::arg("channel_mask") = 0x0F)
      .def("gammaPerChannel", [](image_ptr_t img, float r, float g, float b, float a) {
        img->gammaPerChannel(r, g, b, a);
      }, py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a"))
      .def("gammaedPerChannel", [](image_ptr_t img, float r, float g, float b, float a) -> image_ptr_t {
        auto result = std::make_shared<Image>(*img);
        result->gammaPerChannel(r, g, b, a);
        return result;
      }, py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a"))
      .def("dualThreshold", [](image_ptr_t img, float low_threshold, float set_low, float high_threshold, float set_high, uint8_t channel_mask) {
        img->dualThreshold(low_threshold, set_low, high_threshold, set_high, channel_mask);
      }, py::arg("low_threshold"), py::arg("set_low"), py::arg("high_threshold"), py::arg("set_high"), py::arg("channel_mask") = 0x0F)
      .def("dualThresholded", [](image_ptr_t img, float low_threshold, float set_low, float high_threshold, float set_high, uint8_t channel_mask) -> image_ptr_t {
        auto result = std::make_shared<Image>(*img);
        result->dualThreshold(low_threshold, set_low, high_threshold, set_high, channel_mask);
        return result;
      }, py::arg("low_threshold"), py::arg("set_low"), py::arg("high_threshold"), py::arg("set_high"), py::arg("channel_mask") = 0x0F)
      .def("contrast", [](image_ptr_t img, float contrast_value, float midpoint, uint8_t channel_mask) {
        img->contrast(contrast_value, midpoint, channel_mask);
      }, py::arg("contrast_value"), py::arg("midpoint") = 0.5f, py::arg("channel_mask") = 0x0F)
      .def("contrasted", [](image_ptr_t img, float contrast_value, float midpoint, uint8_t channel_mask) -> image_ptr_t {
        auto result = std::make_shared<Image>(*img);
        result->contrast(contrast_value, midpoint, channel_mask);
        return result;
      }, py::arg("contrast_value"), py::arg("midpoint") = 0.5f, py::arg("channel_mask") = 0x0F)
      .def("combine", [](image_ptr_t img, fmtx4 matrix) {
        img->combine(matrix);
      }, py::arg("matrix"))
      .def("combined", [](image_ptr_t img, fmtx4 matrix) -> image_ptr_t {
        auto result = std::make_shared<Image>(*img);
        result->combine(matrix);
        return result;
      }, py::arg("matrix"))
      .def_property_readonly("rotated90cw", [](image_ptr_t img) -> image_ptr_t {
        return img->rotated90cw();
      })
      .def_property_readonly("rotated90ccw", [](image_ptr_t img) -> image_ptr_t {
        return img->rotated90ccw();
      })
      .def("rotate90cw", [](image_ptr_t img) {
        img->rotate90cw();
      })
      .def("rotate90ccw", [](image_ptr_t img) {
        img->rotate90ccw();
      })
      .def_property_readonly("hFlipped", [](image_ptr_t img) -> image_ptr_t {
        return img->hFlipped();
      })
      .def_property_readonly("vFlipped", [](image_ptr_t img) -> image_ptr_t {
        return img->vFlipped();
      })
      .def("hFlip", [](image_ptr_t img) {
        img->hFlip();
      })
      .def("vFlip", [](image_ptr_t img) {
        img->vFlip();
      })
      .def("separableConvolve", [](image_ptr_t img, const std::vector<float>& kernel, fvec4 threshold) -> image_ptr_t {
        auto output = std::make_shared<Image>();
        img->separableConvolve(*output, kernel, threshold);
        return output;
      }, py::arg("kernel"), py::arg("threshold") = fvec4(0.0f, 0.0f, 0.0f, 0.0f))
      ;
  type_codec->registerStdCodec<image_ptr_t>(image_type);      
  ///////////////////////////////////////////////////////
  auto image_provider_type = //
      py::class_<ImageProvider, image_provider_ptr_t>(module_lev2, "ImageProvider")
      .def("__repr__", [](image_provider_ptr_t ip) {
        return "<lev2.ImageProvider>";
      })
      .def_static("createFromLambda", [](py::function func) -> image_provider_ptr_t {
        auto prov = std::make_shared<ImageProvider>();
        prov->_func = [func]() -> image_ptr_t {
          py::gil_scoped_acquire acquire;
          auto img = func();
          return img.cast<image_ptr_t>();
        };
        return prov;
      });
  type_codec->registerStdCodec<image_provider_ptr_t>(image_provider_type);
  ///////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
