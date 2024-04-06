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

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

namespace dflow = dataflow;
void pyinit_gfx_drawables(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto drawabledata_type = //
      py::class_<DrawableData, ork::Object, drawabledata_ptr_t>(module_lev2, "DrawableData")
          .def("createDrawable", [](drawabledata_ptr_t data) -> drawable_ptr_t { return data->createDrawable(); })
          .def("createSGDrawable", [](drawabledata_ptr_t data, scenegraph::scene_ptr_t SG) -> drawable_ptr_t {
            return data->createSGDrawable(SG);
          });
  type_codec->registerStdCodec<drawabledata_ptr_t>(drawabledata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto drawable_type = py::class_<Drawable, drawable_ptr_t>(module_lev2, "Drawable")
                           .def_property(
                               "scenegraph",
                               [](drawable_ptr_t drw) -> scenegraph::scene_ptr_t { return drw->_sg; },
                               [](drawable_ptr_t drw, scenegraph::scene_ptr_t sg) { drw->_sg = sg; });
  type_codec->registerStdCodec<drawable_ptr_t>(drawable_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto cbdrawable_type = //
      py::class_<CallbackDrawable, Drawable, callback_drawable_ptr_t>(module_lev2, "CallbackDrawable");
  type_codec->registerStdCodec<callback_drawable_ptr_t>(cbdrawable_type);
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
  auto griddrawdata_type = //
      py::class_<GridDrawableData, DrawableData, griddrawabledataptr_t>(module_lev2, "GridDrawableData")
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
              [](griddrawabledataptr_t drw, float val) { drw->_minorTileDim = val; })
          .def_property(
              "shader_suffix",
              [](griddrawabledataptr_t drw) -> std::string { return drw->_shader_suffix; },
              [](griddrawabledataptr_t drw, std::string val) { drw->_shader_suffix = val; });
  type_codec->registerStdCodec<griddrawabledataptr_t>(griddrawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto bbdrawdata_type = //
      py::class_<BillboardDrawableData, DrawableData, billboarddrawabledataptr_t>(module_lev2, "BillboardDrawableData")
          .def(py::init<>())
          .def_property(
              "texturepath",
              [](billboarddrawabledataptr_t drw) -> std::string { return drw->_colortexpath; },
              [](billboarddrawabledataptr_t drw, std::string val) { drw->_colortexpath = val; })
          .def_property(
              "alpha",
              [](billboarddrawabledataptr_t drw) -> float { return drw->_alpha; },
              [](billboarddrawabledataptr_t drw, float val) { drw->_alpha = val; });
  type_codec->registerStdCodec<billboarddrawabledataptr_t>(bbdrawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto groundplanedrawdata_type = //
      py::class_<GroundPlaneDrawableData, DrawableData, groundplane_drawabledataptr_t>(module_lev2, "GroundPlaneDrawableData")
          .def(py::init<>())
          .def_property(
              "extent",
              [](groundplane_drawabledataptr_t drw) -> float { return drw->_extent; },
              [](groundplane_drawabledataptr_t drw, float val) { //
                printf("set gpd<%p> extent<%g>\n", (void*)drw.get(), val);
                drw->_extent = val; //
              })
          .def_property(
              "pbrmaterial",
              [](groundplane_drawabledataptr_t drw) -> pbrmaterial_ptr_t { return drw->_material; },
              [](groundplane_drawabledataptr_t drw, pbrmaterial_ptr_t mtl) { drw->_material = mtl; })
          .def_property(
              "pipeline",
              [](groundplane_drawabledataptr_t drw) -> fxpipeline_ptr_t { return drw->_pipeline_color; },
              [](groundplane_drawabledataptr_t drw, fxpipeline_ptr_t pipe) { drw->_pipeline_color = pipe; });
  type_codec->registerStdCodec<groundplane_drawabledataptr_t>(groundplanedrawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto clipmapdrawdata_type = //
      py::class_<ClipMapDrawableData, DrawableData, clipmapdrawabledata_ptr_t>(module_lev2, "GeoClipMapDrawable")
          .def(py::init<>())
          .def_property(
              "pbrmaterial",
              [](clipmapdrawabledata_ptr_t drw) -> pbrmaterial_ptr_t { return drw->_material; },
              [](clipmapdrawabledata_ptr_t drw, pbrmaterial_ptr_t mtl) { drw->_material = mtl; })
          .def_property(
              "numLevels",
              [](clipmapdrawabledata_ptr_t drw) -> int { return drw->_levels; },
              [](clipmapdrawabledata_ptr_t drw, int val) { drw->_levels = val; })
          .def_property(
              "ringSize",
              [](clipmapdrawabledata_ptr_t drw) -> int { return drw->_ringSize; },
              [](clipmapdrawabledata_ptr_t drw, int val) { drw->_ringSize = val; })
          .def_property(
              "baseQuadSize",
              [](clipmapdrawabledata_ptr_t drw) -> float { return drw->_baseQuadSize; },
              [](clipmapdrawabledata_ptr_t drw, float val) { drw->_baseQuadSize = val; })
            .def_property(
              "circle",
              [](clipmapdrawabledata_ptr_t drw) -> bool { return drw->_circle; },
              [](clipmapdrawabledata_ptr_t drw, bool val) { drw->_circle = val; });
  type_codec->registerStdCodec<clipmapdrawabledata_ptr_t>(clipmapdrawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto stringdrawdata_type = //
      py::class_<StringDrawableData, DrawableData, string_drawabledata_ptr_t>(module_lev2, "StringDrawableData")
          .def(py::init<>())
          .def_property(
              "text",
              [](string_drawabledata_ptr_t drw) -> std::string { return drw->_initialString; },
              [](string_drawabledata_ptr_t drw, std::string val) { drw->_initialString = val; })
          .def_property(
              "pos2D",
              [](string_drawabledata_ptr_t drw) -> fvec2 { return drw->_pos2D; },
              [](string_drawabledata_ptr_t drw, fvec2 val) { drw->_pos2D = val; })
          .def_property(
              "scale",
              [](string_drawabledata_ptr_t drw) -> float { return drw->_scale; },
              [](string_drawabledata_ptr_t drw, float val) { drw->_scale = val; })
          .def_property(
              "color",
              [](string_drawabledata_ptr_t drw) -> fvec4 { return drw->_color; },
              [](string_drawabledata_ptr_t drw, fvec4 val) { drw->_color = val; })
          .def_property(
              "font",
              [](string_drawabledata_ptr_t drw) -> std::string { return drw->_font; },
              [](string_drawabledata_ptr_t drw, std::string val) { drw->_font = val; })
          .def("onRender", [](string_drawabledata_ptr_t drw, py::object callback) {
            drw->_onRender = [callback](RenderContextInstData& RCID) {
              auto RCFD = RCID._RCFD;
              auto DB   = RCFD->GetDB();
              auto vpID = DB->getUserProperty("vpID"_crcu).get<uint64_t>();
              py::gil_scoped_acquire acquire;
              callback(int(vpID));
            };
          });
  type_codec->registerStdCodec<string_drawabledata_ptr_t>(stringdrawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto ptcdrawdata_type = //
      py::class_<ParticlesDrawableData, DrawableData, particles_drawable_data_ptr_t>(module_lev2, "ParticlesDrawableData")
          .def(py::init<>())
          .def_property(
              "graphdata",
              [](particles_drawable_data_ptr_t drw) -> dflow::graphdata_ptr_t { return drw->_graphdata; },
              [](particles_drawable_data_ptr_t drw, dflow::graphdata_ptr_t gdata) { drw->_graphdata = gdata; })
          .def_property(
              "emitterIntensity",
              [](particles_drawable_data_ptr_t drw) -> float { return drw->_emitterIntensity; },
              [](particles_drawable_data_ptr_t drw, float intens) { drw->_emitterIntensity = intens; })
          .def_property(
              "emitterRadius",
              [](particles_drawable_data_ptr_t drw) -> float { return drw->_emitterRadius; },
              [](particles_drawable_data_ptr_t drw, float radius) { drw->_emitterRadius = radius; });
  type_codec->registerStdCodec<particles_drawable_data_ptr_t>(ptcdrawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto terdrawdata_type = //
      py::class_<TerrainDrawableData, DrawableData, terraindrawabledata_ptr_t>(module_lev2, "TerrainDrawableData")
          .def(py::init<>())
          .def_property(
              "rock1",
              [](terraindrawabledata_ptr_t drw) -> fvec3 { return drw->_rock1; },
              [](terraindrawabledata_ptr_t drw, fvec3 val) { drw->_rock1 = val; })
          .def("writeHmapPath", [](terraindrawabledata_ptr_t drw, std::string path) { drw->_writeHmapPath(path); });
  type_codec->registerStdCodec<terraindrawabledata_ptr_t>(terdrawdata_type);
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
  auto labeledpoint_drawdata_type = //
      py::class_<LabeledPointDrawableData, DrawableData, labeled_point_drawabledata_ptr_t>(module_lev2, "LabeledPointDrawableData")
          .def(py::init<>())
          .def_property(
              "scale",
              [](labeled_point_drawabledata_ptr_t drw) -> float { return drw->_scale; },
              [](labeled_point_drawabledata_ptr_t drw, float val) { drw->_scale = val; })
          .def_property(
              "color",
              [](labeled_point_drawabledata_ptr_t drw) -> fvec4 { return drw->_color; },
              [](labeled_point_drawabledata_ptr_t drw, fvec4 val) { drw->_color = val; })
          .def_property(
              "font",
              [](labeled_point_drawabledata_ptr_t drw) -> std::string { return drw->_font; },
              [](labeled_point_drawabledata_ptr_t drw, std::string val) { drw->_font = val; })
          .def_property(
              "pointsmesh",
              [](labeled_point_drawabledata_ptr_t drw) -> meshutil::submesh_ptr_t { return drw->_points_only_mesh; },
              [](labeled_point_drawabledata_ptr_t drw, meshutil::submesh_ptr_t val) { drw->_points_only_mesh = val; })
          .def_property(
              "pipeline_points",
              [](labeled_point_drawabledata_ptr_t drw) -> fxpipeline_ptr_t { return drw->_points_pipeline; },
              [](labeled_point_drawabledata_ptr_t drw, fxpipeline_ptr_t val) { drw->_points_pipeline = val; })
          .def_property(
              "pipeline_text",
              [](labeled_point_drawabledata_ptr_t drw) -> fxpipeline_ptr_t { return drw->_text_pipeline; },
              [](labeled_point_drawabledata_ptr_t drw, fxpipeline_ptr_t val) { drw->_text_pipeline = val; })
          .def("onRender", [](labeled_point_drawabledata_ptr_t drw, py::object callback) {
            drw->_onRender = [callback](RenderContextInstData& RCID) {
              auto RCFD = RCID._RCFD;
              auto DB   = RCFD->GetDB();
              auto vpID = DB->getUserProperty("vpID"_crcu).get<uint64_t>();
              py::gil_scoped_acquire acquire;
              callback(int(vpID));
            };
          });
  type_codec->registerStdCodec<labeled_point_drawabledata_ptr_t>(labeledpoint_drawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto billboard_drawdata_type = //
      py::class_<BillboardStringDrawableData, DrawableData, billboard_string_drawabledata_ptr_t>(
          module_lev2, "BillboardStringDrawableData")
          .def(py::init<>())
          .def_property(
              "text",
              [](billboard_string_drawabledata_ptr_t drw) -> std::string { return drw->_initialString; },
              [](billboard_string_drawabledata_ptr_t drw, std::string val) { drw->_initialString = val; })
          .def_property(
              "cameraRelativeOffset",
              [](billboard_string_drawabledata_ptr_t drw) -> bool { return drw->_cameraRelativeOffset; },
              [](billboard_string_drawabledata_ptr_t drw, bool val) { drw->_cameraRelativeOffset = val; })
          .def_property(
              "offset",
              [](billboard_string_drawabledata_ptr_t drw) -> fvec3 { return drw->_offset; },
              [](billboard_string_drawabledata_ptr_t drw, fvec3 val) { drw->_offset = val; })
          .def_property(
              "upvec",
              [](billboard_string_drawabledata_ptr_t drw) -> fvec3 { return drw->_upvec; },
              [](billboard_string_drawabledata_ptr_t drw, fvec3 val) { drw->_upvec = val; })
          .def_property(
              "scale",
              [](billboard_string_drawabledata_ptr_t drw) -> float { return drw->_scale; },
              [](billboard_string_drawabledata_ptr_t drw, float val) { drw->_scale = val; })
          .def_property(
              "color",
              [](billboard_string_drawabledata_ptr_t drw) -> fvec4 { return drw->_color; },
              [](billboard_string_drawabledata_ptr_t drw, fvec4 val) { drw->_color = val; })
          .def_property(
              "blending",
              [](billboard_string_drawabledata_ptr_t drw) -> crcstring_ptr_t { //
                auto crcstr = std::make_shared<CrcString>(uint64_t(drw->_blendmode));
                return crcstr;
              },
              [](billboard_string_drawabledata_ptr_t drw, crcstring_ptr_t ctest) { //
                drw->_blendmode = Blending(ctest->hashed());
              });
  type_codec->registerStdCodec<billboard_string_drawabledata_ptr_t>(billboard_drawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto overlay_drawdata_type = //
      py::class_<OverlayStringDrawableData, DrawableData, overlay_string_drawabledata_ptr_t>(
          module_lev2, "OverlayStringDrawableData")
          .def(py::init<>())
          .def_property(
              "text",
              [](overlay_string_drawabledata_ptr_t drw) -> std::string { return drw->_initialString; },
              [](overlay_string_drawabledata_ptr_t drw, std::string val) { drw->_initialString = val; })
          .def_property(
              "font",
              [](overlay_string_drawabledata_ptr_t drw) -> std::string { return drw->_font; },
              [](overlay_string_drawabledata_ptr_t drw, std::string val) { drw->_font = val; })
          .def_property(
              "position",
              [](overlay_string_drawabledata_ptr_t drw) -> fvec2 { return drw->_position; },
              [](overlay_string_drawabledata_ptr_t drw, fvec2 val) { drw->_position = val; })
          .def_property(
              "scale",
              [](overlay_string_drawabledata_ptr_t drw) -> float { return drw->_scale; },
              [](overlay_string_drawabledata_ptr_t drw, float val) { drw->_scale = val; })
          .def_property(
              "color",
              [](overlay_string_drawabledata_ptr_t drw) -> fvec4 { return drw->_color; },
              [](overlay_string_drawabledata_ptr_t drw, fvec4 val) { drw->_color = val; });
  type_codec->registerStdCodec<overlay_string_drawabledata_ptr_t>(overlay_drawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
}

} // namespace ork::lev2
