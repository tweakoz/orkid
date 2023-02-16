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
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_renderer(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto rcfd_type_t = py::class_<RenderContextFrameData, rcfd_ptr_t>(
                         module_lev2,                                 //
                         "RenderContextFrameData")                    //
                         .def(py::init([](ctx_t& ctx) -> rcfd_ptr_t { //
                           return std::make_shared<RenderContextFrameData>(ctx.get());
                         }))
                         .def_property(
                             "cimpl",
                             [](rcfd_ptr_t the_rcfd) -> compositorimpl_ptr_t { return the_rcfd->_cimpl; },
                             [](rcfd_ptr_t the_rcfd, compositorimpl_ptr_t c) { the_rcfd->_cimpl = c; })
                         .def("setRenderingModel", [](rcfd_ptr_t the_rcfd, std::string rendermodel) { //
                           auto as_crc               = CrcString(rendermodel.c_str());
                           the_rcfd->_renderingmodel = (uint32_t)as_crc._hashed;
                         });
  type_codec->registerStdCodec<rcfd_ptr_t>(rcfd_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto rcid_type_t = py::class_<RenderContextInstData, rcid_ptr_t>(
                         module_lev2,                                          //
                         "RenderContextInstData")                              //
                         .def(py::init([](rcfd_ptr_t the_rcfd) -> rcid_ptr_t { //
                           return RenderContextInstData::create(the_rcfd);
                         }))
                         .def(
                             "fxinst",                                                         //
                             [](rcid_ptr_t the_rcid, material_ptr_t mtl) -> fxinstance_ptr_t { //
                               auto cache = mtl->fxInstanceCache();
                               return cache->findfxinst(*the_rcid);
                             })
                         .def(
                             "genMatrix",                                 //
                             [](rcid_ptr_t the_rcid, py::object method) { //
                               auto py_callback     = method.cast<pybind11::object>();
                               the_rcid->_genMatrix = [py_callback]() -> fmtx4 {
                                 py::gil_scoped_acquire acquire;
                                 py::object mtx_attempt = py_callback();
                                 printf("YAY..\n");
                                 return mtx_attempt.cast<fmtx4>();
                               };
                             })
                         .def(
                             "forceTechnique",                                  //
                             [](rcid_ptr_t the_rcid, pyfxtechnique_ptr_t tek) { //
                               the_rcid->forceTechnique(tek.get());
                             });
  /*.def_property("fxcache",
      [](rcid_ptr_t the_rcid) -> fxinstancecache_constptr_t { //
        return the_rcid->_fx_instance_cache;
      },
      [](rcid_ptr_t the_rcid, fxinstancecache_constptr_t cache) { //
        the_rcid->_fx_instance_cache = cache;
      }
  )*/
  type_codec->registerStdCodec<rcid_ptr_t>(rcid_type_t);

  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Drawable, drawable_ptr_t>(module_lev2, "Drawable");
  auto cbdrawable_type = //
      py::class_<CallbackDrawable, Drawable, callback_drawable_ptr_t>(module_lev2, "CallbackDrawable");
  type_codec->registerStdCodec<callback_drawable_ptr_t>(cbdrawable_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<LightData, lightdata_ptr_t>(module_lev2, "LightData")
      .def_property(
          "color",                                       //
          [](lightdata_ptr_t lightdata) -> fvec3_ptr_t { //
            auto color = std::make_shared<fvec3>(lightdata->mColor);
            return color;
          },
          [](lightdata_ptr_t lightdata, fvec3_ptr_t color) { //
            lightdata->mColor = *color.get();
          });
  py::class_<PointLightData, LightData, pointlightdata_ptr_t>(module_lev2, "PointLightData")
      .def(py::init<>())
      .def(
          "createNode",                      //
          [](pointlightdata_ptr_t lightdata, //
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::lightnode_ptr_t { //
            auto xfgen = []() -> fmtx4 { return fmtx4(); };
            auto light = std::make_shared<PointLight>(xfgen, lightdata.get());
            return layer->createLightNode(named, light);
          });
  py::class_<SpotLightData, LightData, spotlightdata_ptr_t>(module_lev2, "SpotLightData");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Light, light_ptr_t>(module_lev2, "Light")
      .def_property(
          "matrix",                              //
          [](light_ptr_t light) -> fmtx4_ptr_t { //
            auto copy = std::make_shared<fmtx4>(light->worldMatrix());
            return copy;
          },
          [](light_ptr_t light, fmtx4_ptr_t mtx) { //
            light->worldMatrix() = *mtx.get();
          });
  py::class_<PointLight, Light, pointlight_ptr_t>(module_lev2, "PointLight");
  py::class_<SpotLight, Light, spotlight_ptr_t>(module_lev2, "SpotLight");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<XgmModel, model_ptr_t>(module_lev2, "Model") //
      .def(py::init([](const std::string& model_path) -> model_ptr_t {
        auto loadreq    = std::make_shared<asset::LoadRequest>(model_path.c_str());
        auto modl_asset = asset::AssetManager<XgmModelAsset>::load(loadreq);
        return modl_asset->_model.atomicCopy();
      }))
      .def(
          "createNode",         //
          [](model_ptr_t model, //
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::node_ptr_t { //
            auto drw        = std::make_shared<ModelDrawable>(nullptr);
            drw->_modelinst = std::make_shared<XgmModelInst>(model.get());

            auto node = layer->createDrawableNode(named, drw);
            node->_userdata->makeValueForKey<model_ptr_t>("pyext.retain.model", model);
            return node;
          })
      .def(
          "createInstancedNode", //
          [](model_ptr_t model,  //
             int numinstances,
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::drawable_node_ptr_t { //
            auto drw = std::make_shared<InstancedModelDrawable>();
            drw->bindModel(model);
            auto node = layer->createDrawableNode(named, drw);
            drw->resize(numinstances);
            auto instdata = drw->_instancedata;
            for (int i = 0; i < numinstances; i++) {
              instdata->_worldmatrices[i].compose(fvec3(0, 0, 0), fquat(), 0.0f);
            }
            return node;
          });
  /////////////////////////////////////////////////////////////////////////////////
  auto camdattype = //
      py::class_<CameraData, cameradata_ptr_t>(module_lev2, "CameraData")
          .def(py::init([]() -> cameradata_ptr_t { return std::make_shared<CameraData>(); }))
          .def(
              "perspective",                                                    //
              [](cameradata_ptr_t camera, float near, float ffar, float fovy) { //
                camera->Persp(near, ffar, fovy);
              })
          .def(
              "lookAt",                                                        //
              [](cameradata_ptr_t camera, fvec3& eye, fvec3& tgt, fvec3& up) { //
                camera->Lookat(eye, tgt, up);
              })
          .def(
              "copyFrom",                                                        //
              [](cameradata_ptr_t camera, cameradata_ptr_t src_camera) { //
                *camera = *src_camera;
              })
          .def(
              "projectDepthRay",                                              //
              [](cameradata_ptr_t camera, fvec2_ptr_t pos2d) -> fray3_ptr_t { //
                auto cammat = camera->computeMatrices(1280.0 / 720.0);
                auto rval   = std::make_shared<fray3>();
                cammat.projectDepthRay(*pos2d.get(), *rval.get());
                return rval;
              });
  type_codec->registerStdCodec<cameradata_ptr_t>(camdattype);
  /////////////////////////////////////////////////////////////////////////////////
  auto camdatluttype = //
      py::class_<CameraDataLut, cameradatalut_ptr_t>(module_lev2, "CameraDataLut")
          .def(py::init<>())
          .def("addCamera", [](cameradatalut_ptr_t lut, std::string key, cameradata_constptr_t camera) { (*lut)[key] = camera; })
          .def("create", [](cameradatalut_ptr_t lut, std::string key) -> cameradata_ptr_t {
            auto camera = lut->create(key);
            return camera;
          });
  type_codec->registerStdCodec<cameradatalut_ptr_t>(camdatluttype);
  /////////////////////////////////////////////////////////////////////////////////
  auto cammatstype = //
      py::class_<CameraMatrices, cameramatrices_ptr_t>(module_lev2, "CameraMatrices")
          .def(py::init([]() -> cameramatrices_ptr_t { //
            return std::make_shared<CameraMatrices>();
          }))
          .def(
              "setCustomProjection",                           //
              [](cameramatrices_ptr_t cammats, fmtx4 matrix) { //
                cammats->setCustomProjection(matrix);
              })
          .def(
              "setCustomView",                                 //
              [](cameramatrices_ptr_t cammats, fmtx4 matrix) { //
                cammats->setCustomView(matrix);
              });
  type_codec->registerStdCodec<cameramatrices_ptr_t>(cammatstype);
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
  auto terdrawdata_type = //
      py::class_<TerrainDrawableData, terraindrawabledata_ptr_t>(module_lev2, "TerrainDrawableData")
          .def(py::init<>())
          .def_property(
              "rock1",
              [](terraindrawabledata_ptr_t drw) -> fvec3 { return drw->_rock1; },
              [](terraindrawabledata_ptr_t drw, fvec3 val) { drw->_rock1 = val; })
          .def("writeHmapPath", [](terraindrawabledata_ptr_t drw, std::string path) { drw->_writeHmapPath(path); });
  type_codec->registerStdCodec<terraindrawabledata_ptr_t>(terdrawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto griddrawdata_type = //
      py::class_<GridDrawableData, griddrawabledataptr_t>(module_lev2, "GridDrawableData")
          .def(py::init<>())
          .def_property(
              "texturepath",
              [](griddrawabledataptr_t drw) -> std::string { return drw->_colortexpath; },
              [](griddrawabledataptr_t drw, std::string val) { drw->_colortexpath = val; })
          .def_property(
              "extent",
              [](griddrawabledataptr_t drw) -> float { return drw->_extent; },
              [](griddrawabledataptr_t drw, float val) { drw->_extent = val; })
          .def_property(
              "majorTileDim",
              [](griddrawabledataptr_t drw) -> float { return drw->_majorTileDim; },
              [](griddrawabledataptr_t drw, float val) { drw->_majorTileDim = val; })
          .def_property(
              "minorTileDim",
              [](griddrawabledataptr_t drw) -> float { return drw->_minorTileDim; },
              [](griddrawabledataptr_t drw, float val) { drw->_minorTileDim = val; });
  type_codec->registerStdCodec<griddrawabledataptr_t>(griddrawdata_type);
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
}
} // namespace ork::lev2
