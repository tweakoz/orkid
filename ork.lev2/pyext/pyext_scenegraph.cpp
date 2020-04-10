#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_scenegraph(py::module& module_lev2) {
  auto sgmodule = module_lev2.def_submodule("scenegraph", "SceneGraph operations");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<scenegraph::Node, scenegraph::node_ptr_t>(sgmodule, "Node");
  //.def("enqueueToRenderer", [](scenegraph::scene_ptr_t SG) { SG->enqueueToRenderer(); })
  //.def("renderOnContext", [](scenegraph::scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<scenegraph::Layer, scenegraph::layer_ptr_t>(sgmodule, "Layer")
      .def("createNode", [](scenegraph::layer_ptr_t layer, std::string named, drawable_ptr_t drawable) -> scenegraph::node_ptr_t {
        return layer->createNode(named, drawable);
      });
  //.def("renderOnContext", [](scenegraph::scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  //.def("renderOnContext", [](scenegraph::scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<scenegraph::Scene, scenegraph::scene_ptr_t>(sgmodule, "Scene")
      .def(py::init<>())
      .def(
          "createLayer",
          [](scenegraph::scene_ptr_t SG, std::string named) -> scenegraph::layer_ptr_t { //
            return SG->createLayer(named);
          })
      .def(
          "enqueueToRenderer",
          [](scenegraph::scene_ptr_t SG) { //
            SG->enqueueToRenderer();
          })
      .def("renderOnContext", [](scenegraph::scene_ptr_t SG, ctx_t context) { //
        SG->renderOnContext(context.get());
      });
}
} // namespace ork::lev2
