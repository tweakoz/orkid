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
          .def_property_readonly(
              "user",                                       //
              [](node_ptr_t node) -> varmap::varmap_ptr_t { //
                return node->_userdata;
              });
  type_codec->registerStdCodec<node_ptr_t>(node_type);
  /////////////////////////////////////////////////////////////////////////////////
  // auto ibufproxy_type =
  //  py::class_<InstanceBufferProxy, ibufproxy_ptr_t>(sgmodule, "InstanceBufferProxy",))
  /////////////////////////////////////////////////////////////////////////////////
  auto drawablenode_type =                                                         //
      py::class_<DrawableNode, Node, drawablenode_ptr_t>(sgmodule, "DrawableNode") //
          .def_property_readonly(
              "instanceData",
              [](drawablenode_ptr_t drwnode) -> instanceddrawdata_ptr_t {
                auto drw     = drwnode->_drawable;
                auto instdrw = std::dynamic_pointer_cast<InstancedModelDrawable>(drw);
                instanceddrawdata_ptr_t rval;
                if (instdrw) {
                  rval = instdrw->_instancedata;
                } else {
                  OrkAssert(false);
                }
                return rval;
              })
          .def(
              "setInstanceMatrix",                                            //
              [](drawablenode_ptr_t node, int instance, fmtx4_ptr_t matrix) { //
                auto drw     = node->_drawable;
                auto instdrw = std::dynamic_pointer_cast<InstancedModelDrawable>(drw);
                if (instdrw) {
                  auto instdata                      = instdrw->_instancedata;
                  instdata->_worldmatrices[instance] = *matrix.get();
                } else {
                  OrkAssert(false);
                }
                return node->_userdata;
              })
          .def(
              "setInstanceColor",                                            //
              [](drawablenode_ptr_t node, int instance, fvec4_ptr_t color) { //
                auto drw     = node->_drawable;
                auto instdrw = std::dynamic_pointer_cast<InstancedModelDrawable>(drw);
                if (instdrw) {
                  auto instdata                  = instdrw->_instancedata;
                  instdata->_modcolors[instance] = *color.get();
                } else {
                  OrkAssert(false);
                }
                return node->_userdata;
              });
  type_codec->registerStdCodec<drawablenode_ptr_t>(drawablenode_type);
  //.def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  auto lightnode_type = //
      py::class_<LightNode, Node, lightnode_ptr_t>(sgmodule, "LightNode")
          .def(
              "setMatrix",
              [](lightnode_ptr_t lnode, //
                 fmtx4_ptr_t mtx) {     //
                auto mtx_copy          = *mtx.get();
                auto light             = lnode->_light;
                light->_xformgenerator = [=]() -> fmtx4 { return mtx_copy; };
              });
  type_codec->registerStdCodec<lightnode_ptr_t>(lightnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto layer_type = //
      py::class_<Layer, layer_ptr_t>(sgmodule, "Layer")
          .def(
              "createDrawableNode",
              [](layer_ptr_t layer, //
                 std::string named,
                 drawable_ptr_t drawable) -> node_ptr_t { //
                return layer->createDrawableNode(named, drawable);
              })
          .def(
              "createLineNode",
              [](layer_ptr_t layer, //
                 std::string named, //
                 fvec3_ptr_t a,
                 fvec3_ptr_t b,
                 fxinstance_ptr_t fxinst) -> node_ptr_t { //
                auto drawable = std::make_shared<CallbackDrawable>(nullptr);
                drawable->SetRenderCallback([a, b, fxinst](lev2::RenderContextInstData& RCID) { //
                  auto context = RCID.context();
                  fxinst->wrappedDrawCall(RCID, [a, b, context]() {
                    auto& VB = GfxEnv::GetSharedDynamicVB2();
                    VtxWriter<SVtxV12N12B12T8C4> vw;
                    auto v0 = SVtxV12N12B12T8C4(*a.get(), fvec3(), fvec3(), fvec2(), 0xffffffff);
                    auto v1 = SVtxV12N12B12T8C4(*b.get(), fvec3(), fvec3(), fvec2(), 0xffffffff);
                    vw.Lock(context, &VB, 6);
                    vw.AddVertex(v0);
                    vw.AddVertex(v1);
                    vw.UnLock(context);
                    auto GBI = context->GBI();
                    GBI->DrawPrimitiveEML(vw, lev2::PrimitiveType::LINES);
                  });
                });
                auto node = layer->createDrawableNode(named, drawable);
                return node;
              });
  type_codec->registerStdCodec<layer_ptr_t>(layer_type);
  //.def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  //.def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  auto scenegraph_type = //
      py::class_<Scene, scene_ptr_t>(sgmodule, "Scene")
          .def(py::init<>())
          .def(py::init<varmap::varmap_ptr_t>())
          .def_property_readonly(
              "user",                                      //
              [](scene_ptr_t SG) -> varmap::varmap_ptr_t { //
                return SG->_userdata;
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
          .def(
              "renderOnContext",
              [](scene_ptr_t SG, ctx_t context) { //
                SG->renderOnContext(context.get());
              })
          .def(
              "pickWithRay",
              [](scene_ptr_t SG, fray3_ptr_t ray) -> uint64_t { //
                OrkAssert(SG != nullptr);
                OrkAssert(ray != nullptr);
                return SG->pickWithRay(ray);
              })
          .def("pickWithScreenCoord", [](scene_ptr_t SG, cameradata_ptr_t cam, fvec2_ptr_t scoord) -> uint64_t { //
            OrkAssert(SG != nullptr);
            OrkAssert(scoord != nullptr);
            return SG->pickWithScreenCoord(cam, *scoord.get());
          });
  type_codec->registerStdCodec<scene_ptr_t>(scenegraph_type);
}
} // namespace ork::lev2
