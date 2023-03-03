////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeDecompBlur.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_compositor(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto compositorpassdata_type = //
      py::class_<CompositingPassData, compositingpassdata_ptr_t>(module_lev2, "CompositingPassData")
          .def(py::init<>())
          .def_property("cameramatrices",
            [](compositingpassdata_ptr_t cpd) -> cameramatrices_ptr_t {
              return cpd->_shared_cameraMatrices;
            },
            [](compositingpassdata_ptr_t cpd, cameramatrices_ptr_t m){
              cpd->setSharedCameraMatrices(m);
            }
          )
          .def("__repr__", [](compositingpassdata_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingPassData(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositingpassdata_ptr_t>(compositorpassdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<RenderPresetContext>(module_lev2, "RenderPresetContext");
  /////////////////////////////////////////////////////////////////////////////////
  auto rendernode_type = //
      py::class_<RenderCompositingNode, compositorrendernode_ptr_t>(module_lev2, "RenderCompositingNode")
          .def_property("layers",
            [](compositorrendernode_ptr_t rnode) -> std::string {
              return rnode->_layers;
            },
            [](compositorrendernode_ptr_t rnode, std::string l){
              rnode->_layers = l;
            }
          )
          .def("__repr__", [](compositorrendernode_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("RenderCompositingNode(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositorrendernode_ptr_t>(rendernode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto postnode_type = //
      py::class_<PostCompositingNode, compositorpostnode_ptr_t>(module_lev2, "PostCompositingNode")
          .def("__repr__", [](compositorpostnode_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("PostCompositingNode(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositorpostnode_ptr_t>(postnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto outputnode_type = //
      py::class_<OutputCompositingNode, compositoroutnode_ptr_t>(module_lev2, "OutputCompositingNode")
          .def("__repr__", [](compositoroutnode_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("OutputCompositingNode(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositoroutnode_ptr_t>(outputnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto scene_type = //
      py::class_<CompositingScene, compositingscene_ptr_t>(module_lev2, "CompositingScene")
          .def("createSceneItem",[](compositingscene_ptr_t scene, std::string named) -> compositingsceneitem_ptr_t {
            auto item = std::make_shared<CompositingSceneItem>();
            scene->_items[named] = item;
            auto cdata = scene->_parent;
            cdata->_activeItem = named;
            return item;
          })
          .def("__repr__", [](compositingscene_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingScene(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositingscene_ptr_t>(scene_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto sceneitem_type = //
      py::class_<CompositingSceneItem, compositingsceneitem_ptr_t>(module_lev2, "CompositingSceneItem")
          .def(py::init<>())
          .def_property("technique",[](compositingsceneitem_ptr_t item) -> compositortechnique_ptr_t {
              return item->_technique;
          },
          [](compositingsceneitem_ptr_t item, compositortechnique_ptr_t t){
            item->_technique = t;
          })
          .def("__repr__", [](compositingsceneitem_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingSceneItem(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositingsceneitem_ptr_t>(sceneitem_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto technique_type = //
      py::class_<CompositingTechnique, compositortechnique_ptr_t>(module_lev2, "CompositingTechnique")
          .def("__repr__", [](compositortechnique_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingTechnique(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositortechnique_ptr_t>(technique_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto nodecompositortechnique_type = //
      py::class_<NodeCompositingTechnique, CompositingTechnique, nodecompositortechnique_ptr_t>(module_lev2, "NodeCompositingTechnique")
          .def(py::init<>())
          .def_property("renderNode",[](nodecompositortechnique_ptr_t ntek) -> compositorrendernode_ptr_t {
              return ntek->_renderNode;
          },
          [](nodecompositortechnique_ptr_t ntek, compositorrendernode_ptr_t t){
            ntek->_renderNode = t;
          })
          .def_property("outputNode",[](nodecompositortechnique_ptr_t ntek) -> compositoroutnode_ptr_t {
              return ntek->_outputNode;
          },
          [](nodecompositortechnique_ptr_t ntek, compositoroutnode_ptr_t t){
            ntek->_outputNode = t;
          })
          .def_property("postNode",[](nodecompositortechnique_ptr_t ntek) -> compositorpostnode_ptr_t {
              return ntek->_postfxNode;
          },
          [](nodecompositortechnique_ptr_t ntek, compositorpostnode_ptr_t t){
            ntek->_postfxNode = t;
          })
          .def("__repr__", [](nodecompositortechnique_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("NodeCompositingTechnique(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<nodecompositortechnique_ptr_t>(nodecompositortechnique_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto compositordata_type = //
      py::class_<CompositingData, compositordata_ptr_t>(module_lev2, "CompositingData")
          .def(py::init<>())
          .def("createScene",[](compositordata_ptr_t cdata, std::string named) -> compositingscene_ptr_t {
            auto scene = std::make_shared<CompositingScene>();
            cdata->_scenes[named] = scene;
            cdata->_activeScene = named;
            scene->_parent = cdata.get();
            return scene;
          })
          .def("presetDeferredPBR", 
               [](compositordata_ptr_t cdata) {
                cdata->presetDeferredPBR();
          })
          .def("__repr__", [](compositordata_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingData(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositordata_ptr_t>(compositordata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto compositorimpl_type = //
      py::class_<CompositingImpl, compositorimpl_ptr_t>(module_lev2, "CompositingImpl")
          .def(py::init([](compositordata_ptr_t cdata) -> compositorimpl_ptr_t { //
            return std::make_shared<CompositingImpl>(cdata);
          }))
          .def("pushCPD", 
               [](compositorimpl_ptr_t ci, 
                  compositingpassdata_ptr_t cpd) {
                ci->pushCPD(*cpd);
          })
          .def("popCPD", 
               [](compositorimpl_ptr_t ci) {
                ci->popCPD();
          })
          .def("__repr__", [](compositorimpl_ptr_t i) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingImpl(%p)", i.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositorimpl_ptr_t>(compositorimpl_type);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  using unlit_ptr_t = std::shared_ptr<compositor::UnlitNode>;
  auto unlitnode_type = //
      py::class_<compositor::UnlitNode, RenderCompositingNode, unlit_ptr_t>(module_lev2, "UnlitRenderNode")
          .def(py::init([]() -> unlit_ptr_t { //
            return std::make_shared<compositor::UnlitNode>();
          }))
          .def("__repr__", [](unlit_ptr_t i) -> std::string {
            fxstring<64> fxs;
            fxs.format("UnlitRenderNode(%p)", i.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<unlit_ptr_t>(unlitnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  auto defpbrctx_type = //
      py::class_<pbr::deferrednode::DeferredContext, pbr_deferred_context_ptr_t>(module_lev2, "DeferredPbrContext")
      .def_property_readonly("lightingMaterial", [](pbr_deferred_context_ptr_t ctx) -> freestyle_mtl_ptr_t { //
        return ctx->_lightingmtl;
      })
      .def("gpuInit", [](pbr_deferred_context_ptr_t ctx, ctx_t gfx_ctx) { //
        ctx->gpuInit(gfx_ctx.get());
      });
  type_codec->registerStdCodec<pbr_deferred_context_ptr_t>(defpbrctx_type);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  using defpbrnode_ptr_t = std::shared_ptr<pbr::deferrednode::DeferredCompositingNodePbr>;
  auto defpbrnode_type = //
      py::class_<pbr::deferrednode::DeferredCompositingNodePbr, RenderCompositingNode, defpbrnode_ptr_t>(module_lev2, "DeferredPbrRenderNode")
          .def(py::init([]() -> defpbrnode_ptr_t { //
            return std::make_shared<pbr::deferrednode::DeferredCompositingNodePbr>();
          }))
          .def_property_readonly("pbr_common", [](defpbrnode_ptr_t node) -> pbr::commonstuff_ptr_t { //
            return node->_pbrcommon;
          })
          .def_property_readonly("context", [](defpbrnode_ptr_t node) -> pbr_deferred_context_ptr_t { //
            return node->deferredContext();
          })
          .def("overrideShader", [](defpbrnode_ptr_t node, std::string shaderpath)  { //
            return node->overrideShader(shaderpath);
          })
          .def("__repr__", [](defpbrnode_ptr_t node) -> std::string {
            fxstring<64> fxs;
            fxs.format("DeferredPbrRenderNode(%p)", node.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<defpbrnode_ptr_t>(defpbrnode_type);

  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  using scroutnode_ptr_t = std::shared_ptr<ScreenOutputCompositingNode>;
  auto scroutnode_type = //
      py::class_<ScreenOutputCompositingNode, OutputCompositingNode, scroutnode_ptr_t>(module_lev2, "ScreenOutputNode")
          .def(py::init([]() -> scroutnode_ptr_t { //
            return std::make_shared<ScreenOutputCompositingNode>();
          }))
          .def("__repr__", [](scroutnode_ptr_t i) -> std::string {
            fxstring<64> fxs;
            fxs.format("ScreenOutputNode(%p)", i.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<scroutnode_ptr_t>(scroutnode_type);



  /////////////////////////////////////////////////////////////////////////////////

}
} //namespace ork::lev2 {
