#include "pyext_math_la.inl"
namespace py = pybind11;
using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////
namespace ork::python {
void init_crcstring(py::module& module_core, python::typecodec_ptr_t type_codec) {
  /////////////////////////////////////////////////////////////////////////////////
  auto crcstr_type =                                                   //
      py::class_<CrcString, crcstring_ptr_t>(module_core, "CrcString") //
          .def(py::init<>([](std::string str) -> crcstring_ptr_t { return std::make_shared<CrcString>(str.c_str()); }))
          .def_property_readonly(
              "hashed",
              [](crcstring_ptr_t s) -> uint64_t { //
                return s->hashed();
              })
          .def_property_readonly(
              "hashedi",
              [](crcstring_ptr_t s) -> int { //
                return int(s->hashed());
              })
          .def("__repr__", [](crcstring_ptr_t s) -> std::string {
            fxstring<64> fxs;
            fxs.format("CrcString(0x%zx:%zu)", s->hashed(), s->hashed());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<crcstring_ptr_t>(crcstr_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct CrcStringProxy {};
  using crcstrproxy_ptr_t = std::shared_ptr<CrcStringProxy>;
  auto crcstrproxy_type   =                                                        //
      py::class_<CrcStringProxy, crcstrproxy_ptr_t>(module_core, "CrcStringProxy") //
          .def(py::init<>())
          .def(
              "__getattr__",                                                           //
              [](crcstrproxy_ptr_t proxy, const std::string& key) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(key.c_str());
              });
  type_codec->registerStdCodec<crcstrproxy_ptr_t>(crcstrproxy_type);
  /////////////////////////////////////////////////////////////////////////////////
  using crc64_ctx_t     = boost::Crc64;
  using crc64_ctx_ptr_t = std::shared_ptr<crc64_ctx_t>;
  auto crc64_type       =                                                   //
      py::class_<crc64_ctx_t, crc64_ctx_ptr_t>(module_core, "Crc64Context") //
          .def(py::init<>())
          .def("begin", [](crc64_ctx_ptr_t ctx) { ctx->init(); })
          .def("finish", [](crc64_ctx_ptr_t ctx) { ctx->finish(); })
          .def(
              "accum",
              [](crc64_ctx_ptr_t ctx, py::object value) {
                if (py::isinstance<py::str>(value)) {
                  ctx->accumulateString(py::cast<std::string>(value));
                } else if (py::isinstance<py::int_>(value)) {
                  ctx->accumulateItem<int>(py::cast<int>(value));
                } else if (py::isinstance<py::float_>(value)) {
                  ctx->accumulateItem<float>(py::cast<float>(value));
                } else if (py::isinstance<fvec2>(value)) {
                  ctx->accumulateItem<fvec2>(py::cast<fvec2>(value));
                } else if (py::isinstance<fvec3>(value)) {
                  ctx->accumulateItem<fvec3>(py::cast<fvec3>(value));
                } else if (py::isinstance<fvec4>(value)) {
                  ctx->accumulateItem<fvec4>(py::cast<fvec4>(value));
                } else {
                  OrkAssert(false);
                }
              })
          .def_property_readonly("result", [](crc64_ctx_ptr_t ctx) -> uint64_t { return ctx->result(); });
  type_codec->registerStdCodec<crc64_ctx_ptr_t>(crc64_type);
}
} // namespace ork::python