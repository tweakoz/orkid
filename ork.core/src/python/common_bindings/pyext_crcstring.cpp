#include <ork/python/common_bindings/pyext_crcstring.inl>
namespace py = pybind11;
using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////
namespace ork::python {
void init_crcstring(py::module& module_core, python::pb11_typecodec_ptr_t type_codec) {
  _init_crcstring<pybind11adapter>(module_core, type_codec);
}
} // namespace ork::python