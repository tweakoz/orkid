#include <ork/python/pycodec.inl>
#include <ork/util/crc.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>
// namespace py = pybind11;
// using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////
namespace ork::python {
template <typename ADAPTER>
inline void _init_crcstring(typename ADAPTER::module_t& module_core, typename ADAPTER::codec_ptr_t type_codec) {
  /////////////////////////////////////////////////////////////////////////////////
  auto crcstr_type =                                                       //
      clazz<ADAPTER, CrcString, crcstring_ptr_t>(module_core, "CrcString") //
          .prop_ro(
              "hashed",
              [](crcstring_ptr_t s) -> uint64_t { //
                return s->hashed();
              })
          .prop_ro(
              "hashedi",
              [](crcstring_ptr_t s) -> int { //
                return int(s->hashed());
              })
          .def(initor<ADAPTER>([](std::string str) -> crcstring_ptr_t { return std::make_shared<CrcString>(str.c_str()); }))
          .def(
              "__repr__",
              [](crcstring_ptr_t s) -> std::string {
                fxstring<64> fxs;
                fxs.format("CrcString(0x%zx:%zu)", s->hashed(), s->hashed());
                return fxs.c_str();
              });
  type_codec->template registerStdCodec<crcstring_ptr_t>(crcstr_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct CrcStringProxy {
    CrcStringProxy(){
    }
  };
  using crcstrproxy_ptr_t = std::shared_ptr<CrcStringProxy>;
  auto crcstrproxy_type   =                                                            //
      clazz<ADAPTER, CrcStringProxy, crcstrproxy_ptr_t>(module_core, "CrcStringProxy") //
          .def(initor<ADAPTER>())
          .def(
              "__getattr__",                                                           //
              [type_codec](crcstrproxy_ptr_t proxy, const std::string& key) -> typename ADAPTER::object_t { //
                if(key.find("__")!=std::string::npos){
                  return type_codec->encode(nullptr);
                }
                  if(key=="xxx"){
                      printf( "xxx\n");
                  }
                auto crc = std::make_shared<CrcString>(key.c_str());
                return type_codec->encode(crc);
              });
  type_codec->template registerStdCodec<crcstrproxy_ptr_t>(crcstrproxy_type);
  /////////////////////////////////////////////////////////////////////////////////
  using crc64_ctx_t     = boost::Crc64;
  using crc64_ctx_ptr_t = std::shared_ptr<crc64_ctx_t>;
  auto crc64_type       =                                                       //
      clazz<ADAPTER, crc64_ctx_t, crc64_ctx_ptr_t>(module_core, "Crc64Context") //
          .construct(initor<ADAPTER>())
          .method("begin", [](crc64_ctx_ptr_t ctx) { ctx->init(); })
          .method("finish", [](crc64_ctx_ptr_t ctx) { ctx->finish(); })
          .method(
              "accum",
              [](crc64_ctx_ptr_t ctx, typename ADAPTER::object_t value) {
                if (is_instance_pystr<ADAPTER>(value)) {
                  ctx->accumulateString(cast2ork<ADAPTER, std::string>(value));
                } else if (is_instance_pyint<ADAPTER>(value)) {
                  ctx->accumulateItem<int>(cast2ork<ADAPTER, int>(value));
                } else if (is_instance_pyfloat<ADAPTER>(value)) {
                  ctx->accumulateItem<float>(cast2ork<ADAPTER, float>(value));
                } else if (ADAPTER::template isinstance<fvec2>(value)) {
                  ctx->accumulateItem<fvec2>(cast2ork<ADAPTER, fvec2>(value));
                } else if (ADAPTER::template isinstance<fvec3>(value)) {
                  ctx->accumulateItem<fvec3>(cast2ork<ADAPTER, fvec3>(value));
                } else if (ADAPTER::template isinstance<fvec4>(value)) {
                  ctx->accumulateItem<fvec4>(cast2ork<ADAPTER, fvec4>(value));
                } else {
                  OrkAssert(false);
                }
              })
          .prop_ro("result", [](crc64_ctx_ptr_t ctx) -> uint64_t { return ctx->result(); });
  type_codec->template registerStdCodec<crc64_ctx_ptr_t>(crc64_type);
}
} // namespace ork::python
