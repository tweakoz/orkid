////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/shmtexture.h>
#include <ork/lev2/gfx/image.h>
#include <pybind11/numpy.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
void pyinit_shmtexture(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // ShmTexProducer
  /////////////////////////////////////////////////////////////////////////////////
  auto shmtex_producer_type =
      py::class_<ShmTexProducer, shmtexproducer_ptr_t>(module_lev2, "ShmTexProducer")
          .def_static(
              "create",
              [](const std::string& name, uint32_t width, uint32_t height, crcstring_ptr_t format) -> shmtexproducer_ptr_t {
                ShmTexProducerConfig config;
                config.name = name;
                config.width = width;
                config.height = height;
                config.format = EBufferFormat(format->hashed());
                return ShmTexProducer::create(config);
              },
              py::arg("name"),
              py::arg("width"),
              py::arg("height"),
              py::arg("format"))
          .def(
              "beginWrite",
              [](shmtexproducer_ptr_t self) -> py::tuple {
                auto ctx = self->beginWrite();
                // Return (numpy array view, buffer_index) tuple
                // The numpy array is a view into the shared memory
                std::vector<ssize_t> shape = {static_cast<ssize_t>(ctx.height), static_cast<ssize_t>(ctx.width)};
                std::vector<ssize_t> strides = {static_cast<ssize_t>(ctx.stride)};

                // Determine element size based on format
                EBufferFormat format = self->format();
                if (format == EBufferFormat::R8) {
                  strides.push_back(1);
                } else if (format == EBufferFormat::RGBA8) {
                  shape.push_back(4);
                  strides.push_back(4);
                  strides.push_back(1);
                }

                auto arr = py::array_t<uint8_t>(shape, strides, ctx.pixels, py::cast(self));
                return py::make_tuple(arr, ctx.buffer_index);
              })
          .def(
              "endWrite",
              [](shmtexproducer_ptr_t self, uint64_t timestamp_ns) {
                self->endWrite(timestamp_ns);
              },
              py::arg("timestamp_ns") = 0)
          .def("cancelWrite", &ShmTexProducer::cancelWrite)
          .def_property_readonly("framesWritten", &ShmTexProducer::framesWritten)
          .def_property_readonly("framesDropped", &ShmTexProducer::framesDropped)
          .def_property_readonly("width", &ShmTexProducer::width)
          .def_property_readonly("height", &ShmTexProducer::height)
          .def_property_readonly("stride", &ShmTexProducer::stride)
          .def_property_readonly(
              "format",
              [](shmtexproducer_ptr_t self) -> crcstring_ptr_t {
                return std::make_shared<CrcString>(uint64_t(self->format()));
              })
          .def_property_readonly("name", &ShmTexProducer::name);
  type_codec->registerStdCodec<shmtexproducer_ptr_t>(shmtex_producer_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ShmTexConsumer
  /////////////////////////////////////////////////////////////////////////////////
  auto shmtex_consumer_type =
      py::class_<ShmTexConsumer, shmtexconsumer_ptr_t>(module_lev2, "ShmTexConsumer")
          .def_static(
              "create",
              [](const std::string& name) -> shmtexconsumer_ptr_t {
                ShmTexConsumerConfig config;
                config.name = name;
                return ShmTexConsumer::create(config);
              },
              py::arg("name"))
          .def(
              "update",
              [](shmtexconsumer_ptr_t self, ctx_t ctx) -> bool {
                return self->update(ctx.get());
              },
              py::arg("ctx"))
          .def_property_readonly("texture", &ShmTexConsumer::texture)
          .def_property_readonly(
              "texture_provider",
              [](shmtexconsumer_ptr_t self) -> texture_provider_ptr_t {
                return std::make_shared<LambdaTextureProvider>(
                    [self]() -> texture_ptr_t { return self->texture(); }
                );
              })
          .def_property_readonly("hasNewFrame", &ShmTexConsumer::hasNewFrame)
          .def_property_readonly("isConnected", &ShmTexConsumer::isConnected)
          .def_property_readonly("framesReceived", &ShmTexConsumer::framesReceived)
          .def_property_readonly("framesSkipped", &ShmTexConsumer::framesSkipped)
          .def_property_readonly("averageLatencyMs", &ShmTexConsumer::averageLatencyMs)
          .def_property_readonly("width", &ShmTexConsumer::width)
          .def_property_readonly("height", &ShmTexConsumer::height)
          .def_property_readonly(
              "format",
              [](shmtexconsumer_ptr_t self) -> crcstring_ptr_t {
                return std::make_shared<CrcString>(uint64_t(self->format()));
              })
          .def_property_readonly("name", &ShmTexConsumer::name);
  type_codec->registerStdCodec<shmtexconsumer_ptr_t>(shmtex_consumer_type);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
