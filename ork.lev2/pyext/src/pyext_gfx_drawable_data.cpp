////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/terrain/terrain_chunk_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/scenegraph/sgnode_grid.h>
#include <ork/lev2/gfx/scenegraph/sgnode_imposter.h>
#include <ork/lev2/gfx/scenegraph/sgnode_billboard.h>
#include <ork/lev2/gfx/scenegraph/sgnode_groundplane.h>
#include <ork/lev2/gfx/scenegraph/sgnode_projectedgrid.h>
#include <ork/lev2/gfx/scenegraph/sgnode_geoclipmap.h>
#include <ork/lev2/gfx/scenegraph/sgnode_uisurface.h>
#include <ork/lev2/gfx/scenegraph/sgnode_cursor.h>
#include <ork/lev2/gfx/scenegraph/sgnode_curvepath.h>
#include <ork/lev2/gfx/scenegraph/sgnode_manipgizmo.h>
#include <ork/lev2/gfx/particle/drawable_data.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/compute_drawable.h>
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
              [](drawabledata_ptr_t data, fvec4 c) { data->_modcolor = c; })
          .def_readwrite("environmentMapPath", &DrawableData::_environmentMapPath);
  type_codec->registerStdCodec<drawabledata_ptr_t>(drawabledata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto cbdrawabledata_type = //
      py::class_<CallbackDrawableData, DrawableData, callback_drawabledata_ptr_t>(module_lev2, "CallbackDrawableData");
  type_codec->registerStdCodec<callback_drawabledata_ptr_t>(cbdrawabledata_type);
  /////////////////////////////////////////////////////////////////////////////////
  // ComputeDrawableData — GPU-driven geometry: 1+ compute passes (each with its own SSBO bindings)
  // run in the drawable's onPreRender, then an indirect draw of the compute-written buffers. All
  // shaders/buffers/material are python-supplied. (See compute_drawable.h.)
  auto computedrawabledata_type = //
      py::class_<ComputeDrawableData, DrawableData, computedrawabledata_ptr_t>(module_lev2, "ComputeDrawableData")
          .def(py::init<>([]() { return std::make_shared<ComputeDrawableData>(); }))
          .def(
              "addComputePass",
              [](computedrawabledata_ptr_t d,
                 pyfxcomputeshader_ptr_t shader,
                 py::list bindings, // list of (storage_block:FxShaderStorageBlock, ssbo:FxShaderStorageBuffer)
                 uint32_t gx,
                 uint32_t gy,
                 uint32_t gz) {
                // bind by storage-block HANDLE — the binding index is auto-resolved from the
                // block's reflected SPIR-V binding within the shader (no hardcoded index).
                std::vector<std::pair<const FxShaderStorageBlock*, FxShaderStorageBuffer*>> binds;
                for (auto item : bindings) {
                  auto tup  = item.cast<py::tuple>();
                  auto blk  = tup[0].cast<pyfxstorage_ptr_t>();
                  auto ssbo = tup[1].cast<fxshaderstoragebuffer_ptr_t>();
                  binds.push_back({blk.get(), ssbo.get()});
                }
                d->addComputePass(shader.get(), binds, gx, gy, gz);
              },
              py::arg("shader"),
              py::arg("bindings"),
              py::arg("gx"),
              py::arg("gy") = 1,
              py::arg("gz") = 1)
          .def(
              "setCameraParams",
              [](computedrawabledata_ptr_t d, fxshaderstoragebuffer_ptr_t ssbo, size_t offset) {
                d->setCameraParams(ssbo.get(), offset);
              },
              py::arg("ssbo"),
              py::arg("offset") = 0)
          .def(
              "addGraphicsStorage", // bind a storage block (the vertex-source SSBO) onto the render pipeline
              [](computedrawabledata_ptr_t d, pyfxstorage_ptr_t block, fxshaderstoragebuffer_ptr_t ssbo) {
                d->addGraphicsStorage(block.get(), ssbo.get());
              },
              py::arg("block"),
              py::arg("ssbo"))
          .def(
              "setIndirect",
              [](computedrawabledata_ptr_t d,
                 fxshaderstoragebuffer_ptr_t args,
                 size_t args_offset,
                 fxshaderstoragebuffer_ptr_t index,
                 crcstring_ptr_t primtype,
                 int index_size) {
                auto pt = primtype ? PrimitiveType(primtype->hashed()) : PrimitiveType::TRIANGLES;
                d->setIndirect(args.get(), args_offset, index ? index.get() : nullptr, pt, index_size);
              },
              py::arg("args"),
              py::arg("args_offset") = 0,
              py::arg("index")     = fxshaderstoragebuffer_ptr_t(nullptr),
              py::arg("primtype")  = crcstring_ptr_t(nullptr),
              py::arg("index_size") = 4)
          .def_property(
              "pipeline", // explicit pipeline (overrides material auto-selection)
              [](computedrawabledata_ptr_t d) -> fxpipeline_ptr_t { return d->_pipeline; },
              [](computedrawabledata_ptr_t d, fxpipeline_ptr_t p) { d->_pipeline = p; })
          .def_property(
              "material", // auto-selects the pipeline per rendering model (unless pipeline is set)
              [](computedrawabledata_ptr_t d) -> material_ptr_t { return d->_material; },
              [](computedrawabledata_ptr_t d, material_ptr_t m) { d->_material = m; })
          .def_property(
              "overlay_material", // the OPTIONAL second (overlay) draw's material, e.g. a wireframe lines material
              [](computedrawabledata_ptr_t d) -> material_ptr_t { return d->_overlayMaterial; },
              [](computedrawabledata_ptr_t d, material_ptr_t m) { d->_overlayMaterial = m; })
          .def_property(
              "instanced", // True -> RCID._isInstanced -> the material's INSTANCED variant (FWD_SSBO_CUSTOM_INSTANCED)
              [](computedrawabledata_ptr_t d) -> bool { return d->_instanced; },
              [](computedrawabledata_ptr_t d, bool v) { d->_instanced = v; })
          .def(
              "addOverlayGraphicsStorage", // bind a storage block onto the overlay draw's pipeline (overlay pull-VS)
              [](computedrawabledata_ptr_t d, pyfxstorage_ptr_t block, fxshaderstoragebuffer_ptr_t ssbo) {
                d->addOverlayGraphicsStorage(block.get(), ssbo.get());
              },
              py::arg("block"),
              py::arg("ssbo"));
  type_codec->registerStdCodec<computedrawabledata_ptr_t>(computedrawabledata_type);
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
              "minor_fade_begin",
              [](griddrawabledataptr_t drw) -> float { return drw->_minorFadeBegin; },
              [](griddrawabledataptr_t drw, float val) { drw->_minorFadeBegin = val; })
          .def_property(
              "minor_fade_end",
              [](griddrawabledataptr_t drw) -> float { return drw->_minorFadeEnd; },
              [](griddrawabledataptr_t drw, float val) { drw->_minorFadeEnd = val; })
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
              "imagePath",
              [](billboarddrawabledataptr_t drw) -> file::Path { return drw->_imagePath; },
              [](billboarddrawabledataptr_t drw, file::Path val) { drw->_imagePath = val; })
          .def_property(
              "image",
              [](billboarddrawabledataptr_t drw) -> image_ptr_t { return drw->_image; },
              [](billboarddrawabledataptr_t drw, image_ptr_t val) { drw->_image = val; })
          .def_property(
              "alpha",
              [](billboarddrawabledataptr_t drw) -> float { return drw->_alpha; },
              [](billboarddrawabledataptr_t drw, float val) { drw->_alpha = val; })
          .def_property(
              "screenSize",
              [](billboarddrawabledataptr_t drw) -> float { return drw->_screenSize; },
              [](billboarddrawabledataptr_t drw, float val) { drw->_screenSize = val; });
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
  auto projgriddrawdata_type = //
      py::class_<ProjectedGridDrawableData, DrawableData, projectedgrid_drawabledataptr_t>(module_lev2, "ProjectedGridDrawableData")
          .def(py::init<>())
          .def_property(
              "pbrmaterial",
              [](projectedgrid_drawabledataptr_t drw) -> pbrmaterial_ptr_t { return drw->_material; },
              [](projectedgrid_drawabledataptr_t drw, pbrmaterial_ptr_t mtl) { drw->_material = mtl; })
          .def_property(
              "pipeline",
              [](projectedgrid_drawabledataptr_t drw) -> fxpipeline_ptr_t { return drw->_pipeline_color; },
              [](projectedgrid_drawabledataptr_t drw, fxpipeline_ptr_t pipe) { drw->_pipeline_color = pipe; })
          .def_property(
              "lod0_cell_size",
              [](projectedgrid_drawabledataptr_t drw) -> float { return drw->_lod0_cell_size; },
              [](projectedgrid_drawabledataptr_t drw, float val) { drw->_lod0_cell_size = val; })
          .def_property(
              "lod0_cells",
              [](projectedgrid_drawabledataptr_t drw) -> int { return drw->_lod0_cells; },
              [](projectedgrid_drawabledataptr_t drw, int val) { drw->_lod0_cells = val; })
          .def_property(
              "lod_count",
              [](projectedgrid_drawabledataptr_t drw) -> int { return drw->_lod_count; },
              [](projectedgrid_drawabledataptr_t drw, int val) { drw->_lod_count = val; });
  type_codec->registerStdCodec<projectedgrid_drawabledataptr_t>(projgriddrawdata_type);
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
          .def_property( // normalized corner anchor: (0,0)=top-left (default), (0,1)=bottom-left, ...
              "anchor",  // pos2D becomes the offset from the anchored corner (block-height compensated on y)
              [](string_drawabledata_ptr_t drw) -> fvec2 { return drw->_anchor; },
              [](string_drawabledata_ptr_t drw, fvec2 val) { drw->_anchor = val; })
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
              auto vpID = DB->getUserPropertyAs<uint64_t>("vpID"_crcu);
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
              [](particles_drawable_data_ptr_t drw, float radius) { drw->_emitterRadius = radius; })
          // When true, the drawable's internal enqueue lambda skips its own
          // graphinst->compute() — an external driver (ECS
          // ParticlesGlobalSystem) is expected to advance compute instead.
          // See drawable_data.h for full rationale.
          .def_property(
              "external_compute",
              [](particles_drawable_data_ptr_t drw) -> bool { return drw->_externalCompute; },
              [](particles_drawable_data_ptr_t drw, bool ec) { drw->_externalCompute = ec; })
          .def_property(
              "probeEntityName",
              [](particles_drawable_data_ptr_t drw) -> std::string { return drw->_probeEntityName; },
              [](particles_drawable_data_ptr_t drw, std::string n) { drw->_probeEntityName = n; });
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
  // TerrainChunkDrawableData (E.6/D.5) — the reflected GPU chunked-terrain render
  // description; assets referenced BY NAME, resolved by the load-side wire step.
  auto tchunk_type = //
      py::class_<terrain::TerrainChunkDrawableData, DrawableData, terrain::terrain_chunk_drawable_data_ptr_t>(
          module_lev2, "TerrainChunkDrawableData")
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<terrain::TerrainChunkDrawableData>();
            // ops-self-defend: an unknown kwarg RAISES instead of vanishing — a silently
            // dropped field here desyncs the shader-text layout from the buffer layout
            // (the layout_dim_cap incident: mesh discontinuities with green gates).
            static const std::set<std::string> known = {
                "hf_asset", "material_asset", "debug_material_assets", "chunk",
                "layout_dim_cap", "render_dimension", "capture_mode", "capture_res",
                "capture_dir", "capture_targets"};
            for (auto item : kwargs) {
              auto key = item.first.cast<std::string>();
              if (known.find(key) == known.end())
                throw std::invalid_argument("TerrainChunkDrawableData: unknown kwarg '" + key + "'");
            }
            if (kwargs.contains("hf_asset"))
              d->_hf_asset_name = kwargs["hf_asset"].cast<std::string>();
            if (kwargs.contains("material_asset"))
              d->_material_asset_name = kwargs["material_asset"].cast<std::string>();
            if (kwargs.contains("debug_material_assets"))
              d->_debug_material_assets = kwargs["debug_material_assets"].cast<std::vector<std::string>>();
            if (kwargs.contains("chunk"))
              d->_chunk = kwargs["chunk"].cast<int>();
            if (kwargs.contains("layout_dim_cap"))
              d->_layout_dim_cap = kwargs["layout_dim_cap"].cast<int>();
            if (kwargs.contains("render_dimension"))
              d->_render_dimension = kwargs["render_dimension"].cast<int>();
            if (kwargs.contains("capture_mode"))
              d->_capture_mode = kwargs["capture_mode"].cast<std::string>();
            if (kwargs.contains("capture_res"))
              d->_capture_res = kwargs["capture_res"].cast<int>();
            if (kwargs.contains("capture_dir"))
              d->_capture_dir = kwargs["capture_dir"].cast<std::string>();
            if (kwargs.contains("capture_targets"))
              d->_capture_targets = kwargs["capture_targets"].cast<std::vector<std::string>>();
            return d;
          }))
          .def_property(
              "hf_asset",
              [](terrain::terrain_chunk_drawable_data_ptr_t d) -> std::string { return d->_hf_asset_name; },
              [](terrain::terrain_chunk_drawable_data_ptr_t d, std::string v) { d->_hf_asset_name = v; })
          .def_property(
              "material_asset",
              [](terrain::terrain_chunk_drawable_data_ptr_t d) -> std::string { return d->_material_asset_name; },
              [](terrain::terrain_chunk_drawable_data_ptr_t d, std::string v) { d->_material_asset_name = v; })
          .def_property(
              "chunk",
              [](terrain::terrain_chunk_drawable_data_ptr_t d) -> int { return d->_chunk; },
              [](terrain::terrain_chunk_drawable_data_ptr_t d, int v) { d->_chunk = v; });
  type_codec->registerStdCodec<terrain::terrain_chunk_drawable_data_ptr_t>(tchunk_type);
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
              auto vpID = DB->getUserPropertyAs<uint64_t>("vpID"_crcu);
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
              "max_samples_per_axis",
              [](uisurfaceprimitivedata_ptr_t data) -> float { return data->_maxSamplesPerAxis; },
              [](uisurfaceprimitivedata_ptr_t data, float s) { data->_maxSamplesPerAxis = s; });
  type_codec->registerStdCodec<uisurfaceprimitivedata_ptr_t>(uisurface_primdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  // ManipGizmoDrawableData - 3D manipulation gizmo for transform manipulation
  /////////////////////////////////////////////////////////////////////////////////
  auto manipgizmodrawdata_type = //
      py::class_<ManipGizmoDrawableData, DrawableData, manipgizmodrawabledata_ptr_t>(module_lev2, "ManipGizmoDrawableData")
          .def(py::init<>())
          .def_property(
              "controller",
              [](manipgizmodrawabledata_ptr_t drw) -> editor::manipcontroller_ptr_t { return drw->_controller; },
              [](manipgizmodrawabledata_ptr_t drw, editor::manipcontroller_ptr_t val) { drw->_controller = val; })
          .def_property(
              "colorX",
              [](manipgizmodrawabledata_ptr_t drw) -> fvec4 { return drw->_colorX; },
              [](manipgizmodrawabledata_ptr_t drw, fvec4 val) { drw->_colorX = val; })
          .def_property(
              "colorY",
              [](manipgizmodrawabledata_ptr_t drw) -> fvec4 { return drw->_colorY; },
              [](manipgizmodrawabledata_ptr_t drw, fvec4 val) { drw->_colorY = val; })
          .def_property(
              "colorZ",
              [](manipgizmodrawabledata_ptr_t drw) -> fvec4 { return drw->_colorZ; },
              [](manipgizmodrawabledata_ptr_t drw, fvec4 val) { drw->_colorZ = val; })
          .def_property(
              "colorHighlight",
              [](manipgizmodrawabledata_ptr_t drw) -> fvec4 { return drw->_colorHighlight; },
              [](manipgizmodrawabledata_ptr_t drw, fvec4 val) { drw->_colorHighlight = val; })
          .def_property(
              "colorActive",
              [](manipgizmodrawabledata_ptr_t drw) -> fvec4 { return drw->_colorActive; },
              [](manipgizmodrawabledata_ptr_t drw, fvec4 val) { drw->_colorActive = val; });
  type_codec->registerStdCodec<manipgizmodrawabledata_ptr_t>(manipgizmodrawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto curvepathdrawdata_type = //
      py::class_<CurvePathDrawableData, DrawableData, curvepath_drawabledata_ptr_t>(module_lev2, "CurvePathDrawableData")
          .def(py::init<>())
          .def(
              "createDrawable",
              [](curvepath_drawabledata_ptr_t data) -> drawable_ptr_t { //
                return data->createDrawable();
              })
          .def(
              "createControlPointDrawable",
              [](curvepath_drawabledata_ptr_t data) -> drawable_ptr_t { //
                return data->createControlPointDrawable();
              })
          .def(
              "updateControlPoints",
              [](curvepath_drawabledata_ptr_t data) { //
                data->updateControlPoints();
              })
          .def_property(
              "curve",
              [](curvepath_drawabledata_ptr_t drw) -> math::transformcurve_ptr_t { return drw->_curve; },
              [](curvepath_drawabledata_ptr_t drw, math::transformcurve_ptr_t val) { drw->_curve = val; })
          .def_property(
              "lineColor",
              [](curvepath_drawabledata_ptr_t drw) -> fvec4 { return drw->_lineColor; },
              [](curvepath_drawabledata_ptr_t drw, fvec4 val) { drw->_lineColor = val; })
          .def_property(
              "cpColor",
              [](curvepath_drawabledata_ptr_t drw) -> fvec4 { return drw->_cpColor; },
              [](curvepath_drawabledata_ptr_t drw, fvec4 val) { drw->_cpColor = val; })
          .def_property(
              "cpSelectedColor",
              [](curvepath_drawabledata_ptr_t drw) -> fvec4 { return drw->_cpSelectedColor; },
              [](curvepath_drawabledata_ptr_t drw, fvec4 val) { drw->_cpSelectedColor = val; })
          .def_property(
              "controlPointScale",
              [](curvepath_drawabledata_ptr_t drw) -> float { return drw->_controlPointScale; },
              [](curvepath_drawabledata_ptr_t drw, float val) { drw->_controlPointScale = val; })
          .def_property(
              "selectedPointIndex",
              [](curvepath_drawabledata_ptr_t drw) -> int { return drw->_selectedPointIndex; },
              [](curvepath_drawabledata_ptr_t drw, int val) { drw->_selectedPointIndex = val; })
          .def_property(
              "lineSubdivisions",
              [](curvepath_drawabledata_ptr_t drw) -> int { return drw->_lineSubdivisions; },
              [](curvepath_drawabledata_ptr_t drw, int val) { drw->_lineSubdivisions = val; })
          .def(
              "hitTestScreenCoord",
              [](curvepath_drawabledata_ptr_t data, fmtx4 vpMtx, fvec2 screenPos,
                 int vpW, int vpH, float hitRadius) -> int {
                return data->hitTestScreenCoord(vpMtx, screenPos, vpW, vpH, hitRadius);
              },
              py::arg("vpMatrix"), py::arg("screenPos"),
              py::arg("vpW"), py::arg("vpH"), py::arg("hitRadius") = 12.0f);
  type_codec->registerStdCodec<curvepath_drawabledata_ptr_t>(curvepathdrawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  // Free function — extract the live graphinst from a particles drawable
  // so Python callers can call graphinst.reset() / read .vars / etc. The
  // C++ accessor is defined in sgnode_particles.cpp and traverses the
  // drawable's CallbackDrawable user vars. Returns nullptr if the drawable
  // wasn't produced by ParticlesDrawableData::createDrawable().
  module_lev2.def(
      "particles_drawable_graphinst",
      [](drawable_ptr_t drw) -> dflow::graphinst_ptr_t {
        return particles_drawable_graphinst(drw);
      },
      py::arg("drawable"));
}
/////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
