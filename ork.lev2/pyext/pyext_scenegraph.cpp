#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
using namespace scenegraph;

void pyinit_scenegraph(py::module& module_lev2) {
  auto sgmodule = module_lev2.def_submodule("scenegraph", "SceneGraph operations");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Node, node_ptr_t>(sgmodule, "Node") //
      .def_property(
          "worldMatrix",                       //
          [](node_ptr_t node) -> fmtx4_ptr_t { //
            return std::make_shared<fmtx4>(node->_transform._worldMatrix);
          },
          [](node_ptr_t node, fmtx4_ptr_t mtx) { //
            node->_transform._worldMatrix = *mtx.get();
          });
  //.def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Layer, layer_ptr_t>(sgmodule, "Layer")
      .def(
          "createNode",
          [](layer_ptr_t layer, //
             std::string named,
             drawable_ptr_t drawable) -> node_ptr_t { //
            return layer->createNode(named, drawable);
          });
  //.def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  //.def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Scene, scene_ptr_t>(sgmodule, "Scene")
      .def(py::init<>())
      .def(
          "createLayer",
          [](scene_ptr_t SG, std::string named) -> layer_ptr_t { //
            return SG->createLayer(named);
          })
      .def(
          "enqueueToRenderer",
          [](scene_ptr_t SG, cameradatalut_ptr_t cameralut) { //
            SG->enqueueToRenderer(cameralut);
          })
      .def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { //
        SG->renderOnContext(context.get());
      });
}
} // namespace ork::lev2
