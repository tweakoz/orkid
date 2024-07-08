#include <ork/python/pycodec.inl>
#include <ork/util/crc.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>
//namespace py = pybind11;
//using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////
namespace ork::python {
template <typename ADAPTER>
inline void _init_crcstring(typename ADAPTER::module_t& module_core, typename ADAPTER::codec_ptr_t type_codec) {
  /////////////////////////////////////////////////////////////////////////////////
  auto crcstr_type =                                                   //
      pybind11::class_<CrcString, crcstring_ptr_t>(module_core, "CrcString") //
          .def(ADAPTER::template init<>([](std::string str) -> crcstring_ptr_t { return std::make_shared<CrcString>(str.c_str()); }))
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
  type_codec->template registerStdCodec<crcstring_ptr_t>(crcstr_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct CrcStringProxy {};
  using crcstrproxy_ptr_t = std::shared_ptr<CrcStringProxy>;
  auto crcstrproxy_type   =                                                        //
      pybind11::class_<CrcStringProxy, crcstrproxy_ptr_t>(module_core, "CrcStringProxy") //
          .def(ADAPTER::template init<>())
          .def(
              "__getattr__",                                                           //
              [](crcstrproxy_ptr_t proxy, const std::string& key) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(key.c_str());
              });
  type_codec->template registerStdCodec<crcstrproxy_ptr_t>(crcstrproxy_type);
  /////////////////////////////////////////////////////////////////////////////////
  using crc64_ctx_t     = boost::Crc64;
  using crc64_ctx_ptr_t = std::shared_ptr<crc64_ctx_t>;
  auto crc64_type       =                                                   //
      pybind11::class_<crc64_ctx_t, crc64_ctx_ptr_t>(module_core, "Crc64Context") //
          .def(ADAPTER::template init<>())
          .def("begin", [](crc64_ctx_ptr_t ctx) { ctx->init(); })
          .def("finish", [](crc64_ctx_ptr_t ctx) { ctx->finish(); })
          .def(
              "accum",
              [](crc64_ctx_ptr_t ctx, ADAPTER::object_t value) {
                if (ADAPTER::template isinstance<typename ADAPTER::str_t>(value)) {
                  ctx->accumulateString(ADAPTER::template cast2ork<std::string>(value));
                } else if (ADAPTER::template isinstance<typename ADAPTER::int_t>(value)) {
                  ctx->accumulateItem<int>(ADAPTER::template cast2ork<int>(value));
                } else if (ADAPTER::template isinstance<typename ADAPTER::float_t>(value)) {
                  ctx->accumulateItem<float>(ADAPTER::template cast2ork<float>(value));
                } else if (ADAPTER::template isinstance<fvec2>(value)) {
                  ctx->accumulateItem<fvec2>(ADAPTER::template cast2ork<fvec2>(value));
                } else if (ADAPTER::template isinstance<fvec3>(value)) {
                  ctx->accumulateItem<fvec3>(ADAPTER::template cast2ork<fvec3>(value));
                } else if (ADAPTER::template isinstance<fvec4>(value)) {
                  ctx->accumulateItem<fvec4>(ADAPTER::template cast2ork<fvec4>(value));
                } else {
                  OrkAssert(false);
                }
              })
          .def_property_readonly("result", [](crc64_ctx_ptr_t ctx) -> uint64_t { return ctx->result(); });
  type_codec->template registerStdCodec<crc64_ctx_ptr_t>(crc64_type);
}
} // namespace ork::python