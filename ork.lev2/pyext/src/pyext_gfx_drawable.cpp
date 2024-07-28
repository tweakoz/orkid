////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/scenegraph/sgnode_billboard.h>
#include <ork/lev2/gfx/scenegraph/sgnode_groundplane.h>
#include <ork/lev2/gfx/scenegraph/sgnode_geoclipmap.h>
#include <ork/lev2/gfx/particle/drawable_data.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/gfx/image.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

namespace dflow = dataflow;
void pyinit_gfx_drawables(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto drawable_type = py::class_<Drawable, drawable_ptr_t>(module_lev2, "Drawable")
                           .def_property(
                               "scenegraph",
                               [](drawable_ptr_t drw) -> scenegraph::scene_ptr_t { return drw->_sg; },
                               [](drawable_ptr_t drw, scenegraph::scene_ptr_t sg) { drw->_sg = sg; });
  type_codec->registerStdCodec<drawable_ptr_t>(drawable_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto cbdrawable_type = //
      py::class_<CallbackDrawable, Drawable, callback_drawable_ptr_t>(module_lev2, "CallbackDrawable")
      .def_property_readonly(
          "tryGridImpl",
          [](callback_drawable_ptr_t drw) -> griddrawableimpl_ptr_t {
            griddrawableimpl_ptr_t gridimpl;
            if( auto as_impl = drw->_implA.tryAsShared<GridDrawableImpl>() ){
              gridimpl = as_impl.value();
            }
            return gridimpl;
          });
  type_codec->registerStdCodec<callback_drawable_ptr_t>(cbdrawable_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto mdldrawable_type =
      py::class_<ModelDrawable, Drawable, model_drawable_ptr_t>(module_lev2, "ModelDrawable")
          .def_property_readonly("modelinst", [](model_drawable_ptr_t drw) -> xgmmodelinst_ptr_t { return drw->_modelinst; });
  type_codec->registerStdCodec<model_drawable_ptr_t>(mdldrawable_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto instanced_drawable_type =
      py::class_<InstancedDrawable, Drawable, instanced_drawable_ptr_t>(module_lev2, "InstancedDrawable")
          .def_property(
              "instance_data",
              [](instanced_drawable_ptr_t drw) -> instanceddrawinstancedata_ptr_t { return drw->_instancedata; },
              [](instanced_drawable_ptr_t drw, instanceddrawinstancedata_ptr_t idata) { drw->_instancedata = idata; });
  type_codec->registerStdCodec<instanced_drawable_ptr_t>(instanced_drawable_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct InstanceMatricesProxy {
    instanceddrawinstancedata_ptr_t _instancedata;
  };
  using matrixinstdata_ptr_t = std::shared_ptr<InstanceMatricesProxy>;
  auto matrixinstdata_type   = //
      py::class_<InstanceMatricesProxy, matrixinstdata_ptr_t>(
          module_lev2, //
          "InstancedMatrices",
          pybind11::buffer_protocol())
          .def_buffer([](InstanceMatricesProxy& proxy) -> pybind11::buffer_info {
            auto idata = proxy._instancedata;
            auto data  = idata->_worldmatrices.data(); // Pointer to buffer
            int count  = idata->_worldmatrices.size();
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                3,                                                       // Number of dimensions
                {count, 4, 4},                                           // Buffer dimensions
                {sizeof(float) * 16, sizeof(float) * 4, sizeof(float)}); // Strides (in bytes) for each index
          });
  type_codec->registerStdCodec<matrixinstdata_ptr_t>(matrixinstdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct InstanceColorsProxy {
    instanceddrawinstancedata_ptr_t _instancedata;
  };
  using colorsinstdata_ptr_t = std::shared_ptr<InstanceColorsProxy>;
  auto colorsinstdata_type   = //
      py::class_<InstanceColorsProxy, colorsinstdata_ptr_t>(
          module_lev2, //
          "InstanceColors",
          pybind11::buffer_protocol())
          .def_buffer([](InstanceColorsProxy& proxy) -> pybind11::buffer_info {
            auto idata = proxy._instancedata;
            auto data  = idata->_modcolors.data(); // Pointer to buffer
            int count  = idata->_modcolors.size();
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                2,                                   // Number of dimensions
                {count, 4},                          // Buffer dimensions
                {sizeof(float) * 4, sizeof(float)}); // Strides (in bytes) for each index
          });
  type_codec->registerStdCodec<colorsinstdata_ptr_t>(colorsinstdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto instancedata_type = //
      py::class_<InstancedDrawableInstanceData, instanceddrawinstancedata_ptr_t>(
          module_lev2, //
          "InstancedDrawableInstanceData")
          .def_property_readonly(
              "matrices",
              [](instanceddrawinstancedata_ptr_t idata) -> matrixinstdata_ptr_t {
                auto proxy           = std::make_shared<InstanceMatricesProxy>();
                proxy->_instancedata = idata;
                return proxy;
              })
          .def_property_readonly("colors", [](instanceddrawinstancedata_ptr_t idata) -> colorsinstdata_ptr_t {
            auto proxy           = std::make_shared<InstanceColorsProxy>();
            proxy->_instancedata = idata;
            return proxy;
          });
  type_codec->registerStdCodec<instanceddrawinstancedata_ptr_t>(instancedata_type);
  /////////////////////////////////////////////////////////////////////////////////
  /*auto terdrawinst_type = //
      py::class_<TerrainDrawableInst, terraindrawableinst_ptr_t>(module_lev2, "TerrainDrawableInst")
          .def(py::init<>([](terraindrawabledata_ptr_t data)->terraindrawableinst_ptr_t{
            return std::make_shared<TerrainDrawableInst>(data);
          }))
          // TODO - find shorter registration method for simple properties
          .def_property("worldHeight",
                        [](terraindrawableinst_ptr_t drwi) -> float {
                          return drwi->_worldHeight;
                        },
                        [](terraindrawableinst_ptr_t drwi, float val) {
                          drwi->_worldHeight = val;
                        }
          )
          .def_property("worldSizeXZ",
                        [](terraindrawableinst_ptr_t drwi) -> float {
                          return drwi->_worldSizeXZ;
                        },
                        [](terraindrawableinst_ptr_t drwi, float val) {
                          drwi->_worldSizeXZ = val;
                        }
          )
          .def(
              "createCallbackDrawable",
              [](terraindrawableinst_ptr_t drwi) {
                return drwi->createCallbackDrawable();
              })
  ;
  type_codec->registerStdCodec<terraindrawableinst_ptr_t>(terdrawinst_type);*/
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  auto rprimbase_t =
      py::class_<meshutil::RigidPrimitiveBase, meshutil::rigidprimitive_ptr_t>(module_lev2, "meshutil::RigidPrimitiveBase")
          .def(
              "createNode",
              [](meshutil::rigidprimitive_ptr_t prim,                            //
                 std::string named,                                              //
                 scenegraph::layer_ptr_t layer,                                  //
                 fxpipeline_ptr_t pipeline) -> scenegraph::drawable_node_ptr_t { //
                auto node                                                        //
                    = prim->createNode(named, layer, pipeline);
                // node->_userdata->template makeValueForKey<T>("_primitive") = prim; // hold on to reference
                return node;
              })
          .def(
              "createDrawable",
              [](meshutil::rigidprimitive_ptr_t prim,                          //
                 fxpipeline_ptr_t pipeline) -> lev2::callback_drawable_ptr_t { //
                auto drw = prim->createDrawable(pipeline);
                return drw;
              })
          .def(
              "createDrawable",
              [](meshutil::rigidprimitive_ptr_t prim,                   //
                 material_ptr_t mtl) -> lev2::callback_drawable_ptr_t { //
                auto drw = prim->createDrawable(mtl);
                return drw;
              })
          .def(
              "createDrawableData",
              [](meshutil::rigidprimitive_ptr_t prim,                                    //
                 fxpipeline_ptr_t pipeline) -> meshutil::rigidprimitive_drawdata_ptr_t { //
                auto drwdata        = std::make_shared<meshutil::RigidPrimitiveDrawableData>();
                drwdata->_pipeline  = pipeline;
                drwdata->_primitive = prim;
                return drwdata;
              });
  type_codec->registerStdCodec<meshutil::rigidprimitive_ptr_t>(rprimbase_t);
  /////////////////////////////////////////////////////////////////////////////////
  using rigidprim_t     = meshutil::RigidPrimitive<SVtxV12N12B12T8C4>;
  using rigidprim_ptr_t = std::shared_ptr<rigidprim_t>;
  py::class_<rigidprim_t, meshutil::RigidPrimitiveBase, rigidprim_ptr_t>(module_lev2, "RigidPrimitive")
      .def(py::init<>())
      .def(py::init([](meshutil::submesh_ptr_t submesh, ctx_t context) {
        auto prim = std::make_shared<rigidprim_t>();
        prim->fromSubMesh(*submesh, context.get());
        return prim;
      }))
      .def(
          "fromSubMesh",
          [](rigidprim_ptr_t prim, meshutil::submesh_ptr_t submesh, ctx_t context) { prim->fromSubMesh(*submesh, context.get()); })
      .def("renderEML", [](rigidprim_ptr_t prim, ctx_t context) { prim->renderEML(context.get()); });
  /////////////////////////////////////////////////////////////////////////////////
  auto grid_drawimpl_type = //
      py::class_<GridDrawableImpl, griddrawableimpl_ptr_t>(module_lev2, "GridDrawableImpl")
      .def("setColorImage", [](griddrawableimpl_ptr_t gridimpl, ctx_t context, image_ptr_t img) {
        auto txi = context->TXI();
        auto mtl = gridimpl->_pbrmaterial;
        if(mtl){
          auto tex = mtl->_texArrayCNMREA;
          txi->updateTextureArraySlice(tex.get(), 0, img);
        }
      })
      .def("setMtlRufImage", [](griddrawableimpl_ptr_t gridimpl, ctx_t context, image_ptr_t img) {
        auto txi = context->TXI();
        auto mtl = gridimpl->_pbrmaterial;
        if(mtl){
          auto tex = mtl->_texArrayCNMREA;
          txi->updateTextureArraySlice(tex.get(), 2, img);
        }
      })
      .def_property_readonly("material", [](griddrawableimpl_ptr_t gridimpl) -> pbrmaterial_ptr_t { return gridimpl->_pbrmaterial; });
  type_codec->registerStdCodec<griddrawableimpl_ptr_t>(grid_drawimpl_type);
  /////////////////////////////////////////////////////////////////////////////////
}

} // namespace ork::lev2
