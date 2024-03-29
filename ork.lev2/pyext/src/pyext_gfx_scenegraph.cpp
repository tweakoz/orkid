////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/scenegraph/sgnode_billboard.h>
#include <ork/lev2/gfx/scenegraph/sgnode_groundplane.h>
#include <ork/python/pycodec.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
using namespace scenegraph;

void pyinit_scenegraph(py::module& module_lev2) {
  auto sgmodule   = module_lev2.def_submodule("scenegraph", "SceneGraph operations");
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto synchro_type = //
      py::class_<Synchro, synchro_ptr_t>(sgmodule, "Synchro");
  type_codec->registerStdCodec<synchro_ptr_t>(synchro_type);

  /////////////////////////////////////////////////////////////////////////////////
  auto node_type =                                   //
      py::class_<Node, node_ptr_t>(sgmodule, "Node") //
          .def_property(
              "worldTransform",                       //
              [](node_ptr_t node) -> decompxf_ptr_t { //
                return node->_dqxfdata._worldTransform;
              },
              [](node_ptr_t node, decompxf_ptr_t mtx) { //
                node->_dqxfdata._worldTransform = mtx;
              })
          .def_property_readonly(
              "name",
              [](node_ptr_t node) -> std::string { //
                return node->_name;
              })
          .def_property(
              "modcolor",                    //
              [](node_ptr_t node) -> fvec4 { //
                return node->_dqxfdata._modcolor;
              },
              [](node_ptr_t node, fvec4 color) { //
                node->_dqxfdata._modcolor     = color;
                node->_dqxfdata._use_modcolor = true;
              })
          .def_property(
              "enabled",                    //
              [](node_ptr_t node) -> bool { //
                return node->_enabled;
              },
              [](node_ptr_t node, bool ena) { //
                node->_enabled = ena;
              })
          .def_property(
              "pickable",                    //
              [](node_ptr_t node) -> bool { //
                return node->_pickable;
              },
              [](node_ptr_t node, bool ena) { //
                node->_pickable = ena;
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
  auto drawablenode_type =                                                          //
      py::class_<DrawableNode, Node, drawable_node_ptr_t>(sgmodule, "DrawableNode") //
          .def_property(
              "sortkey",
              [](drawable_node_ptr_t node) -> int { //
                return node->_drawable->_sortkey;
              },
              [](drawable_node_ptr_t node, int key) { //
                node->_drawable->_sortkey = key;
              })
          .def_property_readonly(
              "instanceData",
              [](drawable_node_ptr_t drwnode) -> instanceddrawinstancedata_ptr_t {
                auto drw     = drwnode->_drawable;
                auto instdrw = std::dynamic_pointer_cast<InstancedModelDrawable>(drw);
                instanceddrawinstancedata_ptr_t rval;
                if (instdrw) {
                  rval = instdrw->_instancedata;
                } else {
                  OrkAssert(false);
                }
                return rval;
              })
          .def(
              "setInstanceMatrix",                                             //
              [](drawable_node_ptr_t node, int instance, fmtx4 matrix) { //
                auto drw     = node->_drawable;
                auto instdrw = std::dynamic_pointer_cast<InstancedModelDrawable>(drw);
                if (instdrw) {
                  auto instdata                      = instdrw->_instancedata;
                  instdata->_worldmatrices[instance] = matrix;
                } else {
                  OrkAssert(false);
                }
                return node->_userdata;
              })
          .def(
              "setInstanceColor",                                             //
              [](drawable_node_ptr_t node, int instance, fvec4_ptr_t color) { //
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
  type_codec->registerStdCodec<drawable_node_ptr_t>(drawablenode_type);
  //.def("renderOnContext", [](scene_ptr_t SG, ctx_t context) { SG->renderOnContext(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  auto lightnode_type = //
      py::class_<LightNode, Node, lightnode_ptr_t>(sgmodule, "LightNode")
          .def(
              "setMatrix",
              [](lightnode_ptr_t lnode, //
                 fmtx4 mtx) {     //
                auto light             = lnode->_light;
                light->_xformgenerator = [=]() -> fmtx4 { return mtx; };
              });
  type_codec->registerStdCodec<lightnode_ptr_t>(lightnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto layer_type = //
      py::class_<Layer, layer_ptr_t>(sgmodule, "Layer")
          .def_property(
              "sortkey",
              [](layer_ptr_t layer) -> uint32_t { //
                return layer->_sortkey;
              },
              [](layer_ptr_t layer, uint32_t sortkey) { //
                layer->_sortkey = sortkey;
              })
          .def(
              "createDrawableNode",
              [](layer_ptr_t layer, //
                 std::string named,
                 drawable_ptr_t drawable) -> node_ptr_t { //
                return layer->createDrawableNode(named, drawable);
              })
          .def(
              "createDrawableNodeFromData",
              [](layer_ptr_t layer, //
                 std::string named,
                 drawabledata_ptr_t drawable_data) -> node_ptr_t { //
                 auto drawable = drawable_data->createDrawable();
                return layer->createDrawableNode(named, drawable);
              })
          .def(
              "createGridNode",
              [](layer_ptr_t layer, //
                 std::string named,
                 griddrawabledataptr_t data) -> node_ptr_t { //
                if (data->_colortexpath == "")
                  data->_colortexpath = "lev2://textures/gridcell_blue.png";
                auto drawable = data->createDrawable();
                printf("D\n");
                return layer->createDrawableNode(named, drawable);
              })
          .def(
              "createBillboardNode",
              [](layer_ptr_t layer, //
                 std::string named,
                 billboarddrawabledataptr_t data) -> node_ptr_t { //
                if (data->_colortexpath == "")
                  data->_colortexpath = "lev2://textures/gridcell_blue.png";
                auto drawable = data->createDrawable();
                printf("D\n");
                return layer->createDrawableNode(named, drawable);
              })
          .def(
              "createGroundPlaneNode",
              [](layer_ptr_t layer, //
                 std::string named,
                 groundplane_drawabledataptr_t data) -> node_ptr_t { //
                //if (data->_colortexpath == "")
                  //data->_colortexpath = "lev2://textures/gridcell_blue.png";
                auto drawable = data->createDrawable();
                printf("D\n");
                return layer->createDrawableNode(named, drawable);
              })
          .def(
              "createLineNode",
              [](layer_ptr_t layer, //
                 std::string named, //
                 fvec3 a,
                 fvec3 b,
                 fxpipeline_ptr_t pipeline) -> node_ptr_t { //
                auto drawable = std::make_shared<CallbackDrawable>(nullptr);
                drawable->SetRenderCallback([a, b, pipeline](lev2::RenderContextInstData& RCID) { //
                  auto context = RCID.context();
                  pipeline->wrappedDrawCall(RCID, [a, b, context]() {
                    auto& VB = GfxEnv::GetSharedDynamicVB2();
                    VtxWriter<SVtxV12N12B12T8C4> vw;
                    auto v0 = SVtxV12N12B12T8C4(a, fvec3(), fvec3(), fvec2(), 0xffffffff);
                    auto v1 = SVtxV12N12B12T8C4(b, fvec3(), fvec3(), fvec2(), 0xffffffff);
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
              "compositordata",                            //
              [](scene_ptr_t SG) -> compositordata_ptr_t { //
                return SG->_compositorData;
              })
          .def_property_readonly(
              "compositorimpl",                            //
              [](scene_ptr_t SG) -> compositorimpl_ptr_t { //
                return SG->_compositorImpl;
              })
          .def_property_readonly(
              "compositoroutputnode",                         //
              [](scene_ptr_t SG) -> compositoroutnode_ptr_t { //
                return SG->_outputNode;
              })
          .def_property_readonly(
              "compositorrendernode",                            //
              [](scene_ptr_t SG) -> compositorrendernode_ptr_t { //
                return SG->_renderNode;
              })
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
              "enablePickHud",
              [](scene_ptr_t SG) { //
                SG->enablePickHud();
              })
          .def(
              "pickWithRay",
              [type_codec](scene_ptr_t SG, fray3_ptr_t ray, py::object callback) { //
                OrkAssert(SG != nullptr);
                OrkAssert(ray != nullptr);
                SG->_userdata->set<py::object>("pickcallback", callback);
                SG->pickWithRay(ray,[SG,type_codec](pixelfetchctx_ptr_t pfc){
                  py::gil_scoped_acquire acquire_gil;
                  auto try_callback = SG->_userdata->typedValueForKey<py::object>("pickcallback");
                  if(try_callback and try_callback.value()){
                    try_callback.value()(pfc);
                  }
                });
              })
          .def("pickWithScreenCoord", [type_codec](scene_ptr_t SG, 
                                         cameradata_ptr_t cam, 
                                         fvec2 scoord,
                                         py::object callback) { //
            OrkAssert(SG != nullptr);
            SG->_userdata->set<py::object>("pickcallback", callback);
            SG->pickWithScreenCoord(cam, scoord, [SG,type_codec](pixelfetchctx_ptr_t pfc){
                py::gil_scoped_acquire acquire_gil;
                auto try_callback = SG->_userdata->typedValueForKey<py::object>("pickcallback");
                if(try_callback and try_callback.value()){
                  try_callback.value()(pfc);
                }
            });
          })
          .def(
              "enableSynchro",                      //
              [](scene_ptr_t SG) -> synchro_ptr_t { //
                SG->_synchro = std::make_shared<Synchro>();
                return SG->_synchro;
              })
          .def_property(
              "scenetime",                  //
              [](scene_ptr_t SG) -> float { //
                return SG->_currentTime;
              },
              [](scene_ptr_t SG, float time) { //
                SG->_currentTime = time;
              })
          .def_property("pickFormat", 
              [](scene_ptr_t SG) -> int { //
                return SG->_pickFormat;
              },
              [](scene_ptr_t SG, int time) { //
                SG->_pickFormat = int(time);
              })
    ;
  type_codec->registerStdCodec<scene_ptr_t>(scenegraph_type);
}
} // namespace ork::lev2
