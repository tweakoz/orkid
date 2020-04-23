#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
using namespace scenegraph;

void pyinit_scenegraph(py::module& module_lev2) {
  auto sgmodule   = module_lev2.def_submodule("scenegraph", "SceneGraph operations");
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto node_type =                                   //
      py::class_<Node, node_ptr_t>(sgmodule, "Node") //
          .def_property(
              "worldMatrix",                       //
              [](node_ptr_t node) -> fmtx4_ptr_t { //
                return node->_transform._worldMatrix;
              },
              [](node_ptr_t node, fmtx4_ptr_t mtx) { //
                node->_transform._worldMatrix = mtx;
              })
          .def(
              "__setattr__",                                                          //
              [type_codec](node_ptr_t node, const std::string& key, py::object val) { //
                auto varmap_val = type_codec->decode(val);
                node->_userdata.setValueForKey(key, varmap_val);
              })
          .def(
              "__getattr__",                                                        //
              [type_codec](node_ptr_t node, const std::string& key) -> py::object { //
                auto varmap_val = node->_userdata.valueForKey(key);
                auto python_val = type_codec->encode(varmap_val);
                return python_val;
              });
  type_codec->registerStdCodec<node_ptr_t>(node_type);
  //.def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  auto layer_type = //
      py::class_<Layer, layer_ptr_t>(sgmodule, "Layer")
          .def(
              "createNode",
              [](layer_ptr_t layer, //
                 std::string named,
                 drawable_ptr_t drawable) -> node_ptr_t { //
                return layer->createNode(named, drawable);
              });
  type_codec->registerStdCodec<layer_ptr_t>(layer_type);
  //.def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  //.def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  auto scenegraph_type = //
      py::class_<Scene, scene_ptr_t>(sgmodule, "Scene")
          .def(py::init<>())
          .def(
              "__setattr__",                                                         //
              [type_codec](scene_ptr_t SG, const std::string& key, py::object val) { //
                auto varmap_val = type_codec->decode(val);
                SG->_userdata.setValueForKey(key, varmap_val);
              })
          .def(
              "__getattr__",                                                       //
              [type_codec](scene_ptr_t SG, const std::string& key) -> py::object { //
                auto varmap_val = SG->_userdata.valueForKey(key);
                auto python_val = type_codec->encode(varmap_val);
                return python_val;
              })
          .def(
              "createLayer",
              [](scene_ptr_t SG, std::string named) -> layer_ptr_t { //
                return SG->createLayer(named);
              })
          .def(
              "updateScene",
              [](scene_ptr_t SG, cameradatalut_ptr_t cameralut) { //
                SG->enqueueToRenderer(cameralut);
              })
          .def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { //
            SG->renderOnContext(context.get());
          });
  type_codec->registerStdCodec<scene_ptr_t>(scenegraph_type);
}
} // namespace ork::lev2
