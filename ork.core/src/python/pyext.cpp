#include <ork/kernel/prop.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/fixedstring.hpp>
#include <ork/python/pyext.h>
namespace py = pybind11;
using namespace pybind11::literals;

namespace ork::python {
////////////////////////////////////////////////////////////////////////////////
ORK_PUSH_SYMVIZ_PRIVATE
struct PyCodecImpl {
  ork::varmap::val_t decode(const py::object& val) const;
  std::vector<std::pair<py::object, decoderfn_t>> _decoders;
};
////////////////////////////////////////////////////////////////////////////////
ork::varmap::val_t PyCodecImpl::decode(const py::object& val) const {
  ork::varmap::val_t rval;
  auto type = val.get_type();
  bool done = false;
  for (auto& decoder_item : _decoders) {
    if (type.is(decoder_item.first)) {
      decoder_item.second(val, rval);
      return rval;
    }
  }
  return rval;
}
ORK_POP_SYMVIZ
////////////////////////////////////////////////////////////////////////////////
TypeCodec::TypeCodec() {
  _impl.Make<PyCodecImpl>();
  /////////////////////////////////////////////////
  // builtin decoders
  /////////////////////////////////////////////////
  auto builtins   = py::module::import("builtins");
  auto int_type   = builtins.attr("int");
  auto float_type = builtins.attr("float");
  auto str_type   = builtins.attr("str");
  registerDecoder(int_type, [](const py::object& inpval, ork::varmap::val_t& outval) { //
    outval.Set<int>(inpval.cast<int>());
  });
  registerDecoder(float_type, [](const py::object& inpval, ork::varmap::val_t& outval) { //
    outval.Set<float>(inpval.cast<float>());
  });
  registerDecoder(str_type, [](const py::object& inpval, ork::varmap::val_t& outval) { //
    outval.Set<std::string>(inpval.cast<std::string>());
  });
}
//////////////////////////////////
void TypeCodec::registerDecoder(const py::object& pytype, decoderfn_t dfn) {
  _impl.Get<PyCodecImpl>()._decoders.push_back(std::pair<py::object, decoderfn_t>(pytype, dfn));
}
//////////////////////////////////
ork::varmap::val_t TypeCodec::decode(const py::object& val) const {
  return _impl.Get<PyCodecImpl>().decode(val);
}
//////////////////////////////////
std::shared_ptr<TypeCodec> TypeCodec::instance() { // static
  struct TypeCodecFactory : public TypeCodec {
    TypeCodecFactory()
        : TypeCodec(){};
  };
  static auto _instance = std::make_shared<TypeCodecFactory>();
  return _instance;
}

} // namespace ork::python
