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
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/scenegraph/sgnode_imposter.h>
#include <ork/lev2/gfx/scenegraph/sgnode_billboard.h>
#include <ork/lev2/gfx/scenegraph/sgnode_groundplane.h>
#include <ork/lev2/gfx/scenegraph/sgnode_geoclipmap.h>
#include <ork/lev2/gfx/scenegraph/sgnode_uisurface.h>
#include <ork/lev2/gfx/scenegraph/sgnode_cursor.h>
#include <ork/lev2/gfx/particle/drawable_data.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/gfx/image.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

namespace dflow = dataflow;
void pyinit_gfx_drawabledatas(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto drawabledata_type = //
      py::class_<DrawableData, ork::Object, drawabledata_ptr_t>(module_lev2, "DrawableData")
          .def("createDrawable", [](drawabledata_ptr_t data) -> drawable_ptr_t { return data->createDrawable(); })
          .def(
              "createSGDrawable",
              [](drawabledata_ptr_t data, scenegraph::scene_ptr_t SG) -> drawable_ptr_t { return data->createSGDrawable(SG); })
          .def_property(
              "modcolor",
              [](drawabledata_ptr_t data) -> fvec4 { return data->_modcolor; },
              [](drawabledata_ptr_t data, fvec4 c) { data->_modcolor = c; });
  type_codec->registerStdCodec<drawabledata_ptr_t>(drawabledata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto cbdrawabledata_type = //
      py::class_<CallbackDrawableData, DrawableData, callback_drawabledata_ptr_t>(module_lev2, "CallbackDrawableData");
  type_codec->registerStdCodec<callback_drawabledata_ptr_t>(cbdrawabledata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto mdldrawabledata_type = //
      py::class_<ModelDrawableData, DrawableData, modeldrawabledata_ptr_t>(module_lev2, "ModelDrawableData")
          .def(py::init<>([](std::string modelpath) -> modeldrawabledata_ptr_t {
            return std::make_shared<ModelDrawableData>(modelpath);
          }));
  type_codec->registerStdCodec<modeldrawabledata_ptr_t>(mdldrawabledata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto instmdldrawabledata_type = //
      py::class_<InstancedModelDrawableData, DrawableData, instancedmodeldrawabledata_ptr_t>(
          module_lev2, "InstancedModelDrawableData")
          .def(py::init<>([](std::string modelpath) -> instancedmodeldrawabledata_ptr_t {
            return std::make_shared<InstancedModelDrawableData>(modelpath);
          }))
          .def("resize", [](instancedmodeldrawabledata_ptr_t d, size_t count) { d->resize(count); });
  type_codec->registerStdCodec<instancedmodeldrawabledata_ptr_t>(instmdldrawabledata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto imppassdata_type = //
      py::class_<ImposterPassData, imposterpassdataptr_t>(module_lev2, "ImposterPassData")
          .def(py::init<>())
          .def_property(
              "pipeline",
              [](imposterpassdataptr_t pass) -> fxpipeline_ptr_t { return pass->_pipeline; },
              [](imposterpassdataptr_t pass, fxpipeline_ptr_t val) { pass->_pipeline = val; })
          .def_property(
              "rtgroup",
              [](imposterpassdataptr_t pass) -> rtgroup_ptr_t { return pass->_rtg; },
              [](imposterpassdataptr_t pass, rtgroup_ptr_t val) { pass->_rtg = val; })
          .def_property(
              "debug_viz",
              [](imposterpassdataptr_t pass) -> bool { return pass->_debug_viz; },
              [](imposterpassdataptr_t pass, bool val) { pass->_debug_viz = val; })
          .def_property(
              "debug_shaderstate",
              [](imposterpassdataptr_t pass) -> bool { return pass->_debug_shaderstate; },
              [](imposterpassdataptr_t pass, bool val) { pass->_debug_shaderstate = val; })
          .def_property_readonly("userdata", [](imposterpassdataptr_t pass) -> varmap::varmap_ptr_t { return pass->_userdata; })
          .def_property(
              "enabled",
              [](imposterpassdataptr_t pass) -> bool { return pass->_enabled; },
              [](imposterpassdataptr_t pass, bool val) { pass->_enabled = val; })
          .def(
              "onPreRender",
              [](imposterpassdataptr_t pass, py::object func) {
                auto mypo          = pass->_userdata->makeSharedForKey<py::object>("_onPreRender");
                (*mypo)            = func;
                pass->_onPreRender = [=]() { //
                  py::gil_scoped_acquire gil;
                  py::function func = py::cast<py::function>(*mypo);
                  func();
                };
                // func(rcid); };
              })
          .def("onPostRender", [](imposterpassdataptr_t pass, py::object func) {
            auto mypo           = pass->_userdata->makeSharedForKey<py::object>("_onPostRender");
            (*mypo)             = func;
            pass->_onPostRender = [=]() { //
              py::gil_scoped_acquire gil;
              py::function func = py::cast<py::function>(*mypo);
              func();
            };
            // func(rcid); };
          });
  type_codec->registerStdCodec<imposterpassdataptr_t>(imppassdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto impdrawdata_type = //
      py::class_<ImposterDrawableData, DrawableData, imposterdrawabledataptr_t>(module_lev2, "ImposterDrawableData")
          .def(py::init<>())
          .def("createDrawable", [](imposterdrawabledataptr_t data) -> drawable_ptr_t { return data->createDrawable(); })
          .def_property_readonly("imp_pass", [](imposterdrawabledataptr_t drw) -> imposterpassdataptr_t { return drw->_imp_pass; })
          .def_property(
              "user_passes",
              [](imposterdrawabledataptr_t drw) -> std::vector<imposterpassdataptr_t> { return drw->_user_passes; },
              [](imposterdrawabledataptr_t drw, std::vector<imposterpassdataptr_t> val) { drw->_user_passes = val; })
          .def_property_readonly(
              "blit_pass", [](imposterdrawabledataptr_t drw) -> imposterpassdataptr_t { return drw->_blit_pass; })
          .def_property(
              "shape",
              [type_codec](imposterdrawabledataptr_t drw) -> py::object { //
                return type_codec->encode64(drw->_shape);
              },
              [type_codec](imposterdrawabledataptr_t drw, py::object val) { //
                drw->_shape = type_codec->decode64(val);
              })
          .def_property(
              "detail",
              [](imposterdrawabledataptr_t drw) -> size_t { return drw->_detail; },
              [](imposterdrawabledataptr_t drw, size_t val) { drw->_detail = val; })
          .def_property(
              "filter_type",
              [](imposterdrawabledataptr_t drw) -> crcstring_ptr_t { //
                return std::make_shared<ork::CrcString>(uint64_t(drw->_filter_type));
              },
              [](imposterdrawabledataptr_t drw, crcstring_ptr_t val) { //
                OrkAssert(val != nullptr);
                drw->_filter_type = EImposterFilterType(val->hashed());
              })
          .def_property(
              "filter_radius",
              [](imposterdrawabledataptr_t drw) -> float { return drw->_filterRadius; },
              [](imposterdrawabledataptr_t drw, float val) { drw->_filterRadius = val; });
  type_codec->registerStdCodec<imposterdrawabledataptr_t>(impdrawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto griddrawdata_type = //
      py::class_<GridDrawableData, DrawableData, griddrawabledataptr_t>(module_lev2, "GridDrawableData")
          .def(py::init<>())
          .def("createDrawable", [](griddrawabledataptr_t data) -> drawable_ptr_t { return data->createDrawable(); })
          .def_property(
              "colorImage",
              [](griddrawabledataptr_t drw) -> image_ptr_t { return drw->_colorImage; },
              [](griddrawabledataptr_t drw, image_ptr_t val) { drw->_colorImage = val; })
          .def_property(
              "normalImage",
              [](griddrawabledataptr_t drw) -> image_ptr_t { return drw->_normalImage; },
              [](griddrawabledataptr_t drw, image_ptr_t val) { drw->_normalImage = val; })
          .def_property(
              "mtlrufImage",
              [](griddrawabledataptr_t drw) -> image_ptr_t { return drw->_mtlrufImage; },
              [](griddrawabledataptr_t drw, image_ptr_t val) { drw->_mtlrufImage = val; })
          .def_property(
              "texturepath",
              [](griddrawabledataptr_t drw) -> std::string { return drw->_colortexpath; },
              [](griddrawabledataptr_t drw, std::string val) { drw->_colortexpath = val; })
          .def_property(
              "modcolor",
              [](griddrawabledataptr_t drw) -> fvec3 { return drw->_modcolor; },
              [](griddrawabledataptr_t drw, fvec3 val) { drw->_modcolor = val; })
          .def_property(
              "intensityA",
              [](griddrawabledataptr_t drw) -> float { return drw->_intensityA; },
              [](griddrawabledataptr_t drw, float val) { drw->_intensityA = val; })
          .def_property(
              "intensityB",
              [](griddrawabledataptr_t drw) -> float { return drw->_intensityB; },
              [](griddrawabledataptr_t drw, float val) { drw->_intensityB = val; })
          .def_property(
              "intensityC",
              [](griddrawabledataptr_t drw) -> float { return drw->_intensityC; },
              [](griddrawabledataptr_t drw, float val) { drw->_intensityC = val; })
          .def_property(
              "intensityD",
              [](griddrawabledataptr_t drw) -> float { return drw->_intensityD; },
              [](griddrawabledataptr_t drw, float val) { drw->_intensityD = val; })
          .def_property(
              "lineWidth",
              [](griddrawabledataptr_t drw) -> float { return drw->_lineWidth; },
              [](griddrawabledataptr_t drw, float val) { drw->_lineWidth = val; })
          .def_property(
              "modcolor",
              [](griddrawabledataptr_t drw) -> fvec3 { return drw->_modcolor; },
              [](griddrawabledataptr_t drw, fvec3 val) { drw->_modcolor = val; })
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
  auto cursordrawdata_type = //
      py::class_<CursorDrawableData, DrawableData, cursordrawabledata_ptr_t>(module_lev2, "CursorDrawableData")
          .def(py::init<>())
          .def_property(
              "color",
              [](cursordrawabledata_ptr_t drw) -> fvec4 { return drw->_color; },
              [](cursordrawabledata_ptr_t drw, fvec4 val) { drw->_color = val; })
          .def_property(
              "size",
              [](cursordrawabledata_ptr_t drw) -> float { return drw->_size; },
              [](cursordrawabledata_ptr_t drw, float val) { drw->_size = val; })
          .def_property(
              "thickness",
              [](cursordrawabledata_ptr_t drw) -> float { return drw->_thickness; },
              [](cursordrawabledata_ptr_t drw, float val) { drw->_thickness = val; })
          .def_property(
              "depth",
              [](cursordrawabledata_ptr_t drw) -> float { return drw->_depth; },
              [](cursordrawabledata_ptr_t drw, float val) { drw->_depth = val; })
          .def_property(
              "autopos",
              [](cursordrawabledata_ptr_t drw) -> bool { return drw->_autopos; },
              [](cursordrawabledata_ptr_t drw, bool val) { drw->_autopos = val; });
  type_codec->registerStdCodec<cursordrawabledata_ptr_t>(cursordrawdata_type);
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
            drw->_vars->makeValueForKey<py::object>("_hold_callback") = callback;
            drw->_onRender                                            = [drw](RenderContextInstData& RCID) {
              auto RCFD = RCID.rcfd();
              auto DB   = RCFD->GetDB();
              auto vpID = DB->getUserProperty("vpID"_crcu).get<uint64_t>();
              py::gil_scoped_acquire acquire;
              auto cb = drw->_vars->typedValueForKey<py::object>("_hold_callback").value();
              cb(int(vpID));
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
            drw->_vars->makeValueForKey<py::object>("_hold_callback") = callback;
            drw->_onRender                                            = [drw](RenderContextInstData& RCID) {
              auto RCFD = RCID.rcfd();
              auto DB   = RCFD->GetDB();
              auto vpID = DB->getUserProperty("vpID"_crcu).get<uint64_t>();
              py::gil_scoped_acquire acquire;
              auto cb = drw->_vars->typedValueForKey<py::object>("_hold_callback").value();
              cb(int(vpID));
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
                drw->_blendmode = BlendingMacro(ctest->hashed());
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
  auto rprimddata_t =
      py::class_<meshutil::RigidPrimitiveDrawableData, lev2::DrawableData, meshutil::rigidprimitive_drawdata_ptr_t>(
          module_lev2, "RigidPrimitiveDrawableData")
          .def(py::init<>())
          .def_property(
              "pipeline",
              [](meshutil::rigidprimitive_drawdata_ptr_t dd) -> fxpipeline_ptr_t { return dd->_pipeline; },
              [](meshutil::rigidprimitive_drawdata_ptr_t dd, fxpipeline_ptr_t pipeline) { dd->_pipeline = pipeline; })
          .def_property(
              "material",
              [](meshutil::rigidprimitive_drawdata_ptr_t dd) -> material_ptr_t { return dd->_material; },
              [](meshutil::rigidprimitive_drawdata_ptr_t dd, material_ptr_t mtl) { dd->_material = mtl; })
          .def_property(
              "primitive",
              [](meshutil::rigidprimitive_drawdata_ptr_t dd) { return dd->_primitive; },
              [](meshutil::rigidprimitive_drawdata_ptr_t dd, meshutil::rigidprimitive_ptr_t prim) { dd->_primitive = prim; });
  type_codec->registerStdCodec<meshutil::rigidprimitive_drawdata_ptr_t>(rprimddata_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto uisurface_primdata_type = //
      py::class_<UISurfacePrimitiveData, DrawableData, uisurfaceprimitivedata_ptr_t>(module_lev2, "UISurfacePrimitiveData")
          .def(py::init<>())
          .def(
              "createDrawable",
              [](uisurfaceprimitivedata_ptr_t data, ui::layoutsurface_ptr_t surface) -> drawable_ptr_t {
                return data->createDrawable(surface);
              },
              py::arg("surface"))
          .def_property(
              "size",
              [](uisurfaceprimitivedata_ptr_t data) -> float { return data->_size; },
              [](uisurfaceprimitivedata_ptr_t data, float s) { data->_size = s; })
          .def_property(
              "blendMode",
              [](uisurfaceprimitivedata_ptr_t data) -> crcstring_ptr_t {
                return std::make_shared<CrcString>(uint64_t(data->_blendMode));
              },
              [](uisurfaceprimitivedata_ptr_t data, crcstring_ptr_t mode) {
                data->_blendMode = BlendingMacro(mode->hashed());
              })
          .def_property(
              "doubleSided",
              [](uisurfaceprimitivedata_ptr_t data) -> bool { return data->_doubleSided; },
              [](uisurfaceprimitivedata_ptr_t data, bool ds) { data->_doubleSided = ds; })
          .def_property(
              "sphere",
              [](uisurfaceprimitivedata_ptr_t data) -> bool { return data->_sphere; },
              [](uisurfaceprimitivedata_ptr_t data, bool v) { data->_sphere = v; })
          .def_property(
              "sphere_radius",
              [](uisurfaceprimitivedata_ptr_t data) -> float { return data->_sphereRadius; },
              [](uisurfaceprimitivedata_ptr_t data, float v) { data->_sphereRadius = v; })
          .def_property(
              "sphere_slices",
              [](uisurfaceprimitivedata_ptr_t data) -> int { return data->_sphereSlices; },
              [](uisurfaceprimitivedata_ptr_t data, int v) { data->_sphereSlices = v; })
          .def_property(
              "sphere_stacks",
              [](uisurfaceprimitivedata_ptr_t data) -> int { return data->_sphereStacks; },
              [](uisurfaceprimitivedata_ptr_t data, int v) { data->_sphereStacks = v; })
          .def_property(
              "uv_xform_l",
              [](uisurfaceprimitivedata_ptr_t data) -> fvec4 { return data->_uvXformL; },
              [](uisurfaceprimitivedata_ptr_t data, const fvec4& v) { data->_uvXformL = v; })
          .def_property(
              "uv_xform_r",
              [](uisurfaceprimitivedata_ptr_t data) -> fvec4 { return data->_uvXformR; },
              [](uisurfaceprimitivedata_ptr_t data, const fvec4& v) { data->_uvXformR = v; })

          .def_property(
              "max_samples_per_axis",
              [](uisurfaceprimitivedata_ptr_t data) -> float { return data->_maxSamplesPerAxis; },
              [](uisurfaceprimitivedata_ptr_t data, float s) { data->_maxSamplesPerAxis = s; });
  type_codec->registerStdCodec<uisurfaceprimitivedata_ptr_t>(uisurface_primdata_type);
}
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
