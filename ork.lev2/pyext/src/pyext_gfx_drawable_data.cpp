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
          .def(py::init<>(
              [](std::string modelpath) -> modeldrawabledata_ptr_t { return std::make_shared<ModelDrawableData>(modelpath); }));
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
}
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
