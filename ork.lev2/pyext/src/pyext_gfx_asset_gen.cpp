////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/asset_gen.h>
#include <ork/lev2/gfx/material_pbr.inl> // D.1: materialize() returns a live PBRMaterial
#include <ork/dataflow/all.h> // graphdata_ptr_t for HeightFieldGenData.graph
#include <ork/lev2/gfx/hypermesh/hmdflow.h> // D.3: LiveHypermesh (HypermeshGenData.materialize)

// Pyext bindings for the HYPERECS M2b reflected asset-gen DATA classes.
//
// Only ImplicitSdfGenData is reflected (in addition to the abstract
// AssetGenData base). Domain-specific Python wrappers
// (ThickSaddleSdf, HollowFunnelSdf, etc.) compose an
// ImplicitSdfGenData with the right shader + params and store it as
// their underlying gendata — no per-shape reflected types needed.
//
// Object base methods (serializeJson / deserializeJson) come from the
// existing Object binding in pyext_reflection.cpp.

namespace ork::lev2 {

void pyinit_gfx_asset_gen(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();

  ///////////////////////////////////////////////////////////////////////////
  // AssetGenData (abstract base) — just the type slot + the shared
  // asset_name field. Not constructible from Python.
  ///////////////////////////////////////////////////////////////////////////
  auto agd_type =
      py::class_<AssetGenData, ork::Object, assetgendata_ptr_t>(module_lev2, "AssetGenData")
          .def_property(
              "asset_name",
              [](assetgendata_ptr_t d) -> std::string { return d->_asset_name; },
              [](assetgendata_ptr_t d, std::string v) { d->_asset_name = v; });
  type_codec->registerStdCodec<assetgendata_ptr_t>(agd_type);

  ///////////////////////////////////////////////////////////////////////////
  // ImplicitSdfGenData — reflected fields for AX-shaded SDF generation.
  // Params dict from Python is split by value type: float values go to
  // _float_params, int values go to _int_params (matches the two
  // typed orklut fields the reflection system can serialize).
  ///////////////////////////////////////////////////////////////////////////
  auto isdf_type =
      py::class_<ImplicitSdfGenData, AssetGenData, implicit_sdf_gendata_ptr_t>(
          module_lev2, "ImplicitSdfGenData")
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<ImplicitSdfGenData>();
            if (kwargs.contains("shader"))     d->_shader     = kwargs["shader"].cast<std::string>();
            if (kwargs.contains("bbox_min"))   d->_bbox_min   = kwargs["bbox_min"].cast<fvec3>();
            if (kwargs.contains("bbox_max"))   d->_bbox_max   = kwargs["bbox_max"].cast<fvec3>();
            if (kwargs.contains("voxel_size")) d->_voxel_size = kwargs["voxel_size"].cast<float>();
            if (kwargs.contains("background")) d->_background = kwargs["background"].cast<float>();
            if (kwargs.contains("grid_name"))  d->_grid_name  = kwargs["grid_name"].cast<std::string>();
            if (kwargs.contains("asset_name")) d->_asset_name = kwargs["asset_name"].cast<std::string>();
            if (kwargs.contains("params")) {
              auto params = kwargs["params"].cast<py::dict>();
              if (params.size() > 0) {
                if (not d->_params) d->_params = std::make_shared<varmap::VarMap>();
                for (auto item : params) {
                  auto k = item.first.cast<std::string>();
                  auto v = py::reinterpret_borrow<py::object>(item.second);
                  // Route by Python type → variant payload type. Check
                  // int before float because py::float_ excludes int
                  // but the reverse holds for some duck-typed callers.
                  // Vec types fall through to try-cast since they aren't
                  // py::isinstance<py::float_>.
                  if (py::isinstance<py::int_>(v) && !py::isinstance<py::float_>(v)) {
                    d->_params->set<int>(k, v.cast<int>());
                  } else if (py::isinstance<py::float_>(v)) {
                    d->_params->set<float>(k, v.cast<float>());
                  } else {
                    bool stored = false;
                    try { d->_params->set<fvec4>(k, v.cast<fvec4>()); stored = true; } catch (...) {}
                    if (!stored) { try { d->_params->set<fvec3>(k, v.cast<fvec3>()); stored = true; } catch (...) {} }
                    if (!stored) { try { d->_params->set<fvec2>(k, v.cast<fvec2>()); stored = true; } catch (...) {} }
                    if (!stored) d->_params->set<float>(k, v.cast<float>());
                  }
                }
              }
            }
            return d;
          }))
          .def_property(
              "shader",
              [](implicit_sdf_gendata_ptr_t d) -> std::string { return d->_shader; },
              [](implicit_sdf_gendata_ptr_t d, std::string v) { d->_shader = v; })
          .def_property(
              "bbox_min",
              [](implicit_sdf_gendata_ptr_t d) -> fvec3 { return d->_bbox_min; },
              [](implicit_sdf_gendata_ptr_t d, fvec3 v) { d->_bbox_min = v; })
          .def_property(
              "bbox_max",
              [](implicit_sdf_gendata_ptr_t d) -> fvec3 { return d->_bbox_max; },
              [](implicit_sdf_gendata_ptr_t d, fvec3 v) { d->_bbox_max = v; })
          .def_property(
              "voxel_size",
              [](implicit_sdf_gendata_ptr_t d) -> float { return d->_voxel_size; },
              [](implicit_sdf_gendata_ptr_t d, float v) { d->_voxel_size = v; })
          .def_property(
              "background",
              [](implicit_sdf_gendata_ptr_t d) -> float { return d->_background; },
              [](implicit_sdf_gendata_ptr_t d, float v) { d->_background = v; })
          .def_property(
              "grid_name",
              [](implicit_sdf_gendata_ptr_t d) -> std::string { return d->_grid_name; },
              [](implicit_sdf_gendata_ptr_t d, std::string v) { d->_grid_name = v; })
          // Read-back accessor: return the underlying VarMap directly
          // (already bound in pyext_varmap.inl — Python sees a VarMap
          // object supporting keys(), __getitem__, __contains__, etc.).
          // Lazy-allocate on first read so callers can always treat
          // .params as a non-null VarMap.
          .def_property_readonly(
              "params",
              [](implicit_sdf_gendata_ptr_t d) -> varmap::varmap_ptr_t {
                if (not d->_params) {
                  d->_params = std::make_shared<varmap::VarMap>();
                }
                return d->_params;
              });
  type_codec->registerStdCodec<implicit_sdf_gendata_ptr_t>(isdf_type);

  ///////////////////////////////////////////////////////////////////////////
  // VdbGridToDrawableGenData — reflected fields for one-shot SDF →
  // drawable conversion. grid_asset_name references another reflected
  // gen by name (M2b.3 cross-asset reference convention).
  ///////////////////////////////////////////////////////////////////////////
  auto mtd_type =
      py::class_<VdbGridToDrawableGenData, AssetGenData, vdb_grid_to_drawable_gendata_ptr_t>(
          module_lev2, "VdbGridToDrawableGenData")
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<VdbGridToDrawableGenData>();
            if (kwargs.contains("grid_asset_name"))     d->_grid_asset_name     = kwargs["grid_asset_name"].cast<std::string>();
            if (kwargs.contains("material_asset_name")) d->_material_asset_name = kwargs["material_asset_name"].cast<std::string>();
            if (kwargs.contains("iso"))                 d->_iso                 = kwargs["iso"].cast<float>();
            if (kwargs.contains("adaptivity"))          d->_adaptivity          = kwargs["adaptivity"].cast<float>();
            if (kwargs.contains("flip_windings"))       d->_flip_windings       = kwargs["flip_windings"].cast<bool>();
            if (kwargs.contains("asset_name"))          d->_asset_name          = kwargs["asset_name"].cast<std::string>();
            return d;
          }))
          .def_property(
              "grid_asset_name",
              [](vdb_grid_to_drawable_gendata_ptr_t d) -> std::string { return d->_grid_asset_name; },
              [](vdb_grid_to_drawable_gendata_ptr_t d, std::string v) { d->_grid_asset_name = v; })
          .def_property(
              "material_asset_name",
              [](vdb_grid_to_drawable_gendata_ptr_t d) -> std::string { return d->_material_asset_name; },
              [](vdb_grid_to_drawable_gendata_ptr_t d, std::string v) { d->_material_asset_name = v; })
          .def_property(
              "iso",
              [](vdb_grid_to_drawable_gendata_ptr_t d) -> float { return d->_iso; },
              [](vdb_grid_to_drawable_gendata_ptr_t d, float v) { d->_iso = v; })
          .def_property(
              "adaptivity",
              [](vdb_grid_to_drawable_gendata_ptr_t d) -> float { return d->_adaptivity; },
              [](vdb_grid_to_drawable_gendata_ptr_t d, float v) { d->_adaptivity = v; })
          .def_property(
              "flip_windings",
              [](vdb_grid_to_drawable_gendata_ptr_t d) -> bool { return d->_flip_windings; },
              [](vdb_grid_to_drawable_gendata_ptr_t d, bool v) { d->_flip_windings = v; });
  type_codec->registerStdCodec<vdb_grid_to_drawable_gendata_ptr_t>(mtd_type);

  ///////////////////////////////////////////////////////////////////////////
  // PbrMaterialGenData — reflected recipe for a PBRMaterial.
  ///////////////////////////////////////////////////////////////////////////
  auto pbr_type =
      py::class_<PbrMaterialGenData, AssetGenData, pbr_material_gendata_ptr_t>(
          module_lev2, "PbrMaterialGenData")
          .def( // D.1: the C++ materializer (the Python wrapper delegates here)
              "materialize",
              [](pbr_material_gendata_ptr_t g, ctx_t c) -> pbrmaterial_ptr_t { return g->materialize(c.get()); })
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<PbrMaterialGenData>();
            if (kwargs.contains("base_color"))  d->_base_color  = kwargs["base_color"].cast<fvec4>();
            if (kwargs.contains("metallic"))    d->_metallic    = kwargs["metallic"].cast<float>();
            if (kwargs.contains("roughness"))   d->_roughness   = kwargs["roughness"].cast<float>();
            if (kwargs.contains("color_path"))  d->_color_path  = kwargs["color_path"].cast<std::string>();
            if (kwargs.contains("normal_path")) d->_normal_path = kwargs["normal_path"].cast<std::string>();
            if (kwargs.contains("mtlruf_path")) d->_mtlruf_path = kwargs["mtlruf_path"].cast<std::string>();
            if (kwargs.contains("shaderpath"))  d->_shaderpath  = kwargs["shaderpath"].cast<std::string>();
            if (kwargs.contains("shader_params")) {
              auto params = kwargs["shader_params"].cast<py::dict>();
              if (params.size() > 0) {
                if (not d->_shader_params) d->_shader_params = std::make_shared<varmap::VarMap>();
                for (auto item : params) {
                  auto k = item.first.cast<std::string>();
                  auto v = py::reinterpret_borrow<py::object>(item.second);
                  // a CrcString value = a named fx-pipeline PROVIDER token (e.g. tokens.RCFD_TIME):
                  // the engine supplies the per-frame value (clock, etc.). Store as crcstring_ptr_t;
                  // fx_pipeline resolves it each frame (and the varmap codec round-trips it by hash).
                  bool stored_crc = false;
                  try { d->_shader_params->set<crcstring_ptr_t>(k, v.cast<crcstring_ptr_t>()); stored_crc = true; } catch (...) {}
                  if (stored_crc) continue;
                  if (py::isinstance<py::float_>(v) ||
                      (py::isinstance<py::int_>(v) && !py::isinstance<py::bool_>(v))) {
                    d->_shader_params->set<float>(k, v.cast<float>());
                  } else {
                    bool stored = false;
                    try { d->_shader_params->set<fvec4>(k, v.cast<fvec4>()); stored = true; } catch (...) {}
                    if (!stored) { try { d->_shader_params->set<fvec3>(k, v.cast<fvec3>()); stored = true; } catch (...) {} }
                    if (!stored) { try { d->_shader_params->set<fvec2>(k, v.cast<fvec2>()); stored = true; } catch (...) {} }
                    if (!stored) d->_shader_params->set<float>(k, v.cast<float>());
                  }
                }
              }
            }
            if (kwargs.contains("asset_name"))  d->_asset_name  = kwargs["asset_name"].cast<std::string>();
            if (kwargs.contains("sampler_textures")) // D.4: sampler uniform -> texture image path
              d->_sampler_textures = kwargs["sampler_textures"].cast<std::map<std::string, std::string>>();
            // PBR2 Phase 2 — 8 glTF KHR-extension lobes. Passing any
            // <lobe>_factor / <lobe>_color kwarg implicitly enables the
            // lobe (sets has_<lobe>=true), so authors don't need to set
            // both `has_<lobe>=True` AND `<lobe>_factor=`. Pass explicit
            // `has_<lobe>=` to force-disable an enabled-by-color lobe.
            auto get_f = [&](const char* k, float fallback)->float{
              return kwargs.contains(k) ? kwargs[k].cast<float>() : fallback;
            };
            auto get_v = [&](const char* k, fvec3 fallback)->fvec3{
              return kwargs.contains(k) ? kwargs[k].cast<fvec3>() : fallback;
            };
            auto get_b = [&](const char* k, bool fallback)->bool{
              return kwargs.contains(k) ? kwargs[k].cast<bool>() : fallback;
            };
            d->_transmission_factor         = get_f("transmission_factor",        d->_transmission_factor);
            d->_transmission_roughness      = get_f("transmission_roughness",     d->_transmission_roughness);
            d->_ior                         = get_f("ior",                        d->_ior);
            d->_volume_thickness_factor     = get_f("volume_thickness_factor",    d->_volume_thickness_factor);
            d->_diffuse_transmission_factor = get_f("diffuse_transmission_factor", d->_diffuse_transmission_factor);
            d->_specular_factor             = get_f("specular_factor",            d->_specular_factor);
            d->_clearcoat_factor            = get_f("clearcoat_factor",           d->_clearcoat_factor);
            d->_sheen_factor                = get_f("sheen_factor",               d->_sheen_factor);
            d->_iridescence_factor          = get_f("iridescence_factor",         d->_iridescence_factor);
            d->_sheen_color                 = get_v("sheen_color",                d->_sheen_color);
            d->_specular_color              = get_v("specular_color",             d->_specular_color);
            d->_attenuation_color           = get_v("attenuation_color",          d->_attenuation_color);
            d->_diffuse_transmission_color  = get_v("diffuse_transmission_color", d->_diffuse_transmission_color);
            d->_clearcoat_roughness         = get_f("clearcoat_roughness",        d->_clearcoat_roughness);
            d->_sheen_roughness             = get_f("sheen_roughness",            d->_sheen_roughness);
            d->_attenuation_distance        = get_f("attenuation_distance",       d->_attenuation_distance);
            d->_has_transmission            = get_b("has_transmission",
                                                    kwargs.contains("transmission_factor"));
            d->_has_transmission_roughness  = get_b("has_transmission_roughness",
                                                    kwargs.contains("transmission_roughness"));
            d->_has_ior                     = get_b("has_ior",            kwargs.contains("ior"));
            d->_has_volume                  = get_b("has_volume",         kwargs.contains("volume_thickness_factor"));
            d->_has_diffuse_transmission    = get_b("has_diffuse_transmission",
                                                    kwargs.contains("diffuse_transmission_factor") ||
                                                    kwargs.contains("diffuse_transmission_color"));
            d->_has_specular                = get_b("has_specular",       kwargs.contains("specular_factor") ||
                                                                          kwargs.contains("specular_color"));
            d->_has_clearcoat               = get_b("has_clearcoat",      kwargs.contains("clearcoat_factor"));
            d->_has_sheen                   = get_b("has_sheen",          kwargs.contains("sheen_factor") ||
                                                                          kwargs.contains("sheen_color"));
            d->_has_iridescence             = get_b("has_iridescence",    kwargs.contains("iridescence_factor"));
            // PBR2 Phase 3 (P3.D) — subsurface scattering.
            d->_subsurface_color   = get_v("subsurface_color",   d->_subsurface_color);
            d->_subsurface_radius  = get_v("subsurface_radius",  d->_subsurface_radius);
            d->_subsurface_factor  = get_f("subsurface_factor",  d->_subsurface_factor);
            d->_has_subsurface     = get_b("has_subsurface",
                                           kwargs.contains("subsurface_factor") ||
                                           kwargs.contains("subsurface_color")  ||
                                           kwargs.contains("subsurface_radius"));
            return d;
          }))
          .def_property(
              "base_color",
              [](pbr_material_gendata_ptr_t d) -> fvec4 { return d->_base_color; },
              [](pbr_material_gendata_ptr_t d, fvec4 v) { d->_base_color = v; })
          .def_property(
              "metallic",
              [](pbr_material_gendata_ptr_t d) -> float { return d->_metallic; },
              [](pbr_material_gendata_ptr_t d, float v) { d->_metallic = v; })
          .def_property(
              "roughness",
              [](pbr_material_gendata_ptr_t d) -> float { return d->_roughness; },
              [](pbr_material_gendata_ptr_t d, float v) { d->_roughness = v; })
          .def_property(
              "color_path",
              [](pbr_material_gendata_ptr_t d) -> std::string { return d->_color_path; },
              [](pbr_material_gendata_ptr_t d, std::string v) { d->_color_path = v; })
          .def_property(
              "normal_path",
              [](pbr_material_gendata_ptr_t d) -> std::string { return d->_normal_path; },
              [](pbr_material_gendata_ptr_t d, std::string v) { d->_normal_path = v; })
          .def_property(
              "mtlruf_path",
              [](pbr_material_gendata_ptr_t d) -> std::string { return d->_mtlruf_path; },
              [](pbr_material_gendata_ptr_t d, std::string v) { d->_mtlruf_path = v; })
          //////////////////////////////////////////////////////////////////
          // PBR2 Phase 2 — 8 glTF KHR-extension lobe properties.
          //////////////////////////////////////////////////////////////////
#define _PBR2_GEN_PROP_BOOL(name, member)                                                          \
  .def_property(name,                                                                              \
                [](pbr_material_gendata_ptr_t d) -> bool { return d->member; },                    \
                [](pbr_material_gendata_ptr_t d, bool v) { d->member = v; })
#define _PBR2_GEN_PROP_FLOAT(name, member)                                                         \
  .def_property(name,                                                                              \
                [](pbr_material_gendata_ptr_t d) -> float { return d->member; },                   \
                [](pbr_material_gendata_ptr_t d, float v) { d->member = v; })
#define _PBR2_GEN_PROP_VEC3(name, member)                                                          \
  .def_property(name,                                                                              \
                [](pbr_material_gendata_ptr_t d) -> fvec3 { return d->member; },                   \
                [](pbr_material_gendata_ptr_t d, fvec3 v) { d->member = v; })
          _PBR2_GEN_PROP_BOOL("has_transmission",             _has_transmission)
          _PBR2_GEN_PROP_FLOAT("transmission_factor",         _transmission_factor)
          _PBR2_GEN_PROP_BOOL("has_transmission_roughness",   _has_transmission_roughness)
          _PBR2_GEN_PROP_FLOAT("transmission_roughness",      _transmission_roughness)
          _PBR2_GEN_PROP_BOOL("has_ior",                      _has_ior)
          _PBR2_GEN_PROP_FLOAT("ior",                         _ior)
          _PBR2_GEN_PROP_BOOL("has_volume",                   _has_volume)
          _PBR2_GEN_PROP_FLOAT("volume_thickness_factor",     _volume_thickness_factor)
          _PBR2_GEN_PROP_BOOL("has_diffuse_transmission",     _has_diffuse_transmission)
          _PBR2_GEN_PROP_FLOAT("diffuse_transmission_factor", _diffuse_transmission_factor)
          _PBR2_GEN_PROP_BOOL("has_specular",                 _has_specular)
          _PBR2_GEN_PROP_FLOAT("specular_factor",             _specular_factor)
          _PBR2_GEN_PROP_BOOL("has_clearcoat",                _has_clearcoat)
          _PBR2_GEN_PROP_FLOAT("clearcoat_factor",            _clearcoat_factor)
          _PBR2_GEN_PROP_BOOL("has_sheen",                    _has_sheen)
          _PBR2_GEN_PROP_FLOAT("sheen_factor",                _sheen_factor)
          _PBR2_GEN_PROP_BOOL("has_iridescence",              _has_iridescence)
          _PBR2_GEN_PROP_FLOAT("iridescence_factor",          _iridescence_factor)
          _PBR2_GEN_PROP_VEC3("sheen_color",                  _sheen_color)
          _PBR2_GEN_PROP_VEC3("specular_color",               _specular_color)
          _PBR2_GEN_PROP_VEC3("attenuation_color",            _attenuation_color)
          _PBR2_GEN_PROP_VEC3("diffuse_transmission_color",   _diffuse_transmission_color)
          _PBR2_GEN_PROP_FLOAT("clearcoat_roughness",         _clearcoat_roughness)
          _PBR2_GEN_PROP_FLOAT("sheen_roughness",             _sheen_roughness)
          _PBR2_GEN_PROP_FLOAT("attenuation_distance",        _attenuation_distance)
          // PBR2 Phase 3 (P3.D) — subsurface.
          _PBR2_GEN_PROP_BOOL("has_subsurface",               _has_subsurface)
          _PBR2_GEN_PROP_VEC3("subsurface_color",             _subsurface_color)
          _PBR2_GEN_PROP_VEC3("subsurface_radius",            _subsurface_radius)
          _PBR2_GEN_PROP_FLOAT("subsurface_factor",           _subsurface_factor)
          .def_property(
              "shaderpath",
              [](pbr_material_gendata_ptr_t d) -> std::string { return d->_shaderpath; },
              [](pbr_material_gendata_ptr_t d, std::string v) { d->_shaderpath = v; })
          .def_property_readonly(
              "shader_params",
              [](pbr_material_gendata_ptr_t d) -> varmap::varmap_ptr_t {
                if (not d->_shader_params)
                  d->_shader_params = std::make_shared<varmap::VarMap>();
                return d->_shader_params;
              })
          .def_property( // D.4: sampler uniform -> texture image path (loaded + bound at materialize)
              "sampler_textures",
              [](pbr_material_gendata_ptr_t d) -> std::map<std::string, std::string> {
                return d->_sampler_textures;
              },
              [](pbr_material_gendata_ptr_t d, std::map<std::string, std::string> v) {
                d->_sampler_textures = std::move(v);
              })
          ;
#undef _PBR2_GEN_PROP_BOOL
#undef _PBR2_GEN_PROP_FLOAT
#undef _PBR2_GEN_PROP_VEC3
  type_codec->registerStdCodec<pbr_material_gendata_ptr_t>(pbr_type);

  ///////////////////////////////////////////////////////////////////////////
  // FreestyleMaterialGenData — reflected recipe for a custom-shader
  // material. Pipeline parameter bindings deferred (dataflow shader
  // story will eventually own that).
  ///////////////////////////////////////////////////////////////////////////
  auto fs_type =
      py::class_<FreestyleMaterialGenData, AssetGenData, freestyle_material_gendata_ptr_t>(
          module_lev2, "FreestyleMaterialGenData")
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<FreestyleMaterialGenData>();
            if (kwargs.contains("shader_text")) d->_shader_text = kwargs["shader_text"].cast<std::string>();
            if (kwargs.contains("shader_file")) d->_shader_file = kwargs["shader_file"].cast<std::string>();
            if (kwargs.contains("shader_name")) d->_shader_name = kwargs["shader_name"].cast<std::string>();
            if (kwargs.contains("technique"))   d->_technique   = kwargs["technique"].cast<std::string>();
            if (kwargs.contains("rendermodel")) d->_rendermodel = kwargs["rendermodel"].cast<std::string>();
            if (kwargs.contains("blending"))    d->_blending    = kwargs["blending"].cast<std::string>();
            if (kwargs.contains("culltest"))    d->_culltest    = kwargs["culltest"].cast<std::string>();
            if (kwargs.contains("depthtest"))   d->_depthtest   = kwargs["depthtest"].cast<std::string>();
            if (kwargs.contains("asset_name"))  d->_asset_name  = kwargs["asset_name"].cast<std::string>();
            return d;
          }))
          .def_property(
              "shader_text",
              [](freestyle_material_gendata_ptr_t d) -> std::string { return d->_shader_text; },
              [](freestyle_material_gendata_ptr_t d, std::string v) { d->_shader_text = v; })
          .def_property(
              "shader_file",
              [](freestyle_material_gendata_ptr_t d) -> std::string { return d->_shader_file; },
              [](freestyle_material_gendata_ptr_t d, std::string v) { d->_shader_file = v; })
          .def_property(
              "shader_name",
              [](freestyle_material_gendata_ptr_t d) -> std::string { return d->_shader_name; },
              [](freestyle_material_gendata_ptr_t d, std::string v) { d->_shader_name = v; })
          .def_property(
              "technique",
              [](freestyle_material_gendata_ptr_t d) -> std::string { return d->_technique; },
              [](freestyle_material_gendata_ptr_t d, std::string v) { d->_technique = v; })
          .def_property(
              "rendermodel",
              [](freestyle_material_gendata_ptr_t d) -> std::string { return d->_rendermodel; },
              [](freestyle_material_gendata_ptr_t d, std::string v) { d->_rendermodel = v; })
          .def_property(
              "blending",
              [](freestyle_material_gendata_ptr_t d) -> std::string { return d->_blending; },
              [](freestyle_material_gendata_ptr_t d, std::string v) { d->_blending = v; })
          .def_property(
              "culltest",
              [](freestyle_material_gendata_ptr_t d) -> std::string { return d->_culltest; },
              [](freestyle_material_gendata_ptr_t d, std::string v) { d->_culltest = v; })
          .def_property(
              "depthtest",
              [](freestyle_material_gendata_ptr_t d) -> std::string { return d->_depthtest; },
              [](freestyle_material_gendata_ptr_t d, std::string v) { d->_depthtest = v; });
  type_codec->registerStdCodec<freestyle_material_gendata_ptr_t>(fs_type);

  ///////////////////////////////////////////////////////////////////////////
  // ParticleSystemGenData — reflected DSL invocation. dsl_file picks
  // the .py to load (same path semantics as ork.particle.viewer.py),
  // params carries scalar kwargs (variant-encoded), and asset_kwargs
  // is a string→string map of ctor-kwarg name → asset name for
  // cross-asset references resolved at materialize time.
  ///////////////////////////////////////////////////////////////////////////
  auto ps_type =
      py::class_<ParticleSystemGenData, AssetGenData, particle_system_gendata_ptr_t>(
          module_lev2, "ParticleSystemGenData")
          .def_property( // D.2 model B: the embedded particle graph (None = legacy model A)
              "graph",
              [](particle_system_gendata_ptr_t d) -> ork::dataflow::graphdata_ptr_t { return d->_graph_data; },
              [](particle_system_gendata_ptr_t d, ork::dataflow::graphdata_ptr_t g) { d->_graph_data = g; })
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<ParticleSystemGenData>();
            if (kwargs.contains("dsl_file"))   d->_dsl_file   = kwargs["dsl_file"].cast<std::string>();
            if (kwargs.contains("dsl_class"))  d->_dsl_class  = kwargs["dsl_class"].cast<std::string>();
            if (kwargs.contains("asset_name")) d->_asset_name = kwargs["asset_name"].cast<std::string>();
            if (kwargs.contains("params")) {
              auto params = kwargs["params"].cast<py::dict>();
              if (params.size() > 0) {
                if (not d->_params) d->_params = std::make_shared<varmap::VarMap>();
                for (auto item : params) {
                  auto k = item.first.cast<std::string>();
                  auto v = py::reinterpret_borrow<py::object>(item.second);
                  // Route by Python type → variant payload. Vec types
                  // fall through to try-cast since they aren't
                  // py::isinstance<py::float_>.
                  if (py::isinstance<py::bool_>(v)) {
                    d->_params->set<bool>(k, v.cast<bool>());
                  } else if (py::isinstance<py::int_>(v) && !py::isinstance<py::float_>(v)) {
                    d->_params->set<int>(k, v.cast<int>());
                  } else if (py::isinstance<py::str>(v)) {
                    d->_params->set<std::string>(k, v.cast<std::string>());
                  } else if (py::isinstance<py::float_>(v)) {
                    d->_params->set<float>(k, v.cast<float>());
                  } else {
                    bool stored = false;
                    try { d->_params->set<fvec4>(k, v.cast<fvec4>()); stored = true; } catch (...) {}
                    if (!stored) { try { d->_params->set<fvec3>(k, v.cast<fvec3>()); stored = true; } catch (...) {} }
                    if (!stored) { try { d->_params->set<fvec2>(k, v.cast<fvec2>()); stored = true; } catch (...) {} }
                    if (!stored) d->_params->set<float>(k, v.cast<float>());
                  }
                }
              }
            }
            if (kwargs.contains("asset_kwargs")) {
              auto m = kwargs["asset_kwargs"].cast<py::dict>();
              for (auto item : m) {
                d->_asset_kwargs[item.first.cast<std::string>()] =
                    item.second.cast<std::string>();
              }
            }
            return d;
          }))
          .def_property(
              "dsl_file",
              [](particle_system_gendata_ptr_t d) -> std::string { return d->_dsl_file; },
              [](particle_system_gendata_ptr_t d, std::string v) { d->_dsl_file = v; })
          .def_property(
              "dsl_class",
              [](particle_system_gendata_ptr_t d) -> std::string { return d->_dsl_class; },
              [](particle_system_gendata_ptr_t d, std::string v) { d->_dsl_class = v; })
          .def_property_readonly(
              "params",
              [](particle_system_gendata_ptr_t d) -> varmap::varmap_ptr_t {
                if (not d->_params) {
                  d->_params = std::make_shared<varmap::VarMap>();
                }
                return d->_params;
              })
          .def_property_readonly(
              "asset_kwargs",
              [](particle_system_gendata_ptr_t d) -> py::dict {
                py::dict out;
                for (auto& [k, v] : d->_asset_kwargs) {
                  out[py::str(k)] = py::str(v);
                }
                return out;
              })
          .def("set_asset_kwarg",
              [](particle_system_gendata_ptr_t d, std::string key, std::string value) {
                d->_asset_kwargs[key] = value;
              })
          .def_property(
              "probe_entity_name",
              [](particle_system_gendata_ptr_t d) -> std::string { return d->_probe_entity_name; },
              [](particle_system_gendata_ptr_t d, std::string v) { d->_probe_entity_name = v; })
          .def_property(
              "emitter_intensity",
              [](particle_system_gendata_ptr_t d) -> float { return d->_emitter_intensity; },
              [](particle_system_gendata_ptr_t d, float v) { d->_emitter_intensity = v; })
          .def_property(
              "emitter_radius",
              [](particle_system_gendata_ptr_t d) -> float { return d->_emitter_radius; },
              [](particle_system_gendata_ptr_t d, float v) { d->_emitter_radius = v; });
  type_codec->registerStdCodec<particle_system_gendata_ptr_t>(ps_type);

  ///////////////////////////////////////////////////////////////////////////
  // HeightFieldGenData — terrain heightfield asset. EMBEDS the serialized
  // terrain compute graph (.graph) + bake dimension. The Python wrapper
  // (HeightField in scene/assets.py) runs the DSL once at authoring to fill
  // .graph; on reload the graph deserializes inline and bakes with no Python.
  ///////////////////////////////////////////////////////////////////////////
  auto hf_type = //
      py::class_<HeightFieldGenData, AssetGenData, heightfield_gendata_ptr_t>(
          module_lev2, "HeightFieldGenData")
          .def( // D.1: the C++ materializer (paths + bake + manifest); returns the manifest path
              "materialize",
              [](heightfield_gendata_ptr_t g, ctx_t c, std::string ext) -> std::string {
                return g->materialize(c.get(), ext);
              },
              py::arg("ctx"),
              py::arg("ext") = "exr")
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<HeightFieldGenData>();
            if (kwargs.contains("asset_name"))
              d->_asset_name = kwargs["asset_name"].cast<std::string>();
            if (kwargs.contains("dimension"))
              d->_dimension = kwargs["dimension"].cast<int>();
            if (kwargs.contains("extent_m"))
              d->_extent_m = kwargs["extent_m"].cast<float>();
            if (kwargs.contains("height_scale_m"))
              d->_height_scale_m = kwargs["height_scale_m"].cast<float>();
            if (kwargs.contains("graph"))
              d->_graph_data = kwargs["graph"].cast<ork::dataflow::graphdata_ptr_t>();
            // E.6/2.20 — the terrain↔material contract (see asset_gen.h)
            if (kwargs.contains("material"))
              d->_material_asset = kwargs["material"].cast<std::string>();
            if (kwargs.contains("channel_samplers"))
              d->_channel_samplers = kwargs["channel_samplers"].cast<std::map<std::string, std::string>>();
            return d;
          }))
          .def_property(
              "dimension",
              [](heightfield_gendata_ptr_t d) -> int { return d->_dimension; },
              [](heightfield_gendata_ptr_t d, int v) { d->_dimension = v; })
          .def_property(
              "extent_m",
              [](heightfield_gendata_ptr_t d) -> float { return d->_extent_m; },
              [](heightfield_gendata_ptr_t d, float v) { d->_extent_m = v; })
          .def_property(
              "height_scale_m",
              [](heightfield_gendata_ptr_t d) -> float { return d->_height_scale_m; },
              [](heightfield_gendata_ptr_t d, float v) { d->_height_scale_m = v; })
          .def_property(
              "graph",
              [](heightfield_gendata_ptr_t d) -> ork::dataflow::graphdata_ptr_t { return d->_graph_data; },
              [](heightfield_gendata_ptr_t d, ork::dataflow::graphdata_ptr_t g) { d->_graph_data = g; })
          .def_property( // D.4: the reflected scatter sinks (placement contract + type bindings)
              "scatters",
              [](heightfield_gendata_ptr_t d) -> std::vector<scattersink_data_ptr_t> { return d->_scatters; },
              [](heightfield_gendata_ptr_t d, std::vector<scattersink_data_ptr_t> v) { d->_scatters = std::move(v); })
          .def_property( // E.6/2.20 — the terrain↔material contract
              "material_asset",
              [](heightfield_gendata_ptr_t d) -> std::string { return d->_material_asset; },
              [](heightfield_gendata_ptr_t d, std::string v) { d->_material_asset = std::move(v); })
          .def_property(
              "channel_samplers",
              [](heightfield_gendata_ptr_t d) -> std::map<std::string, std::string> { return d->_channel_samplers; },
              [](heightfield_gendata_ptr_t d, std::map<std::string, std::string> v) { d->_channel_samplers = std::move(v); });
  type_codec->registerStdCodec<heightfield_gendata_ptr_t>(hf_type);

  ///////////////////////////////////////////////////////////////////////////
  // ScatterSinkData (D.4) — the reflected scatter contract (see asset_gen.h).
  // Mirrors the authoring ScatterSpec 1:1; type_id = index into type_names.
  ///////////////////////////////////////////////////////////////////////////
  auto scat_type = //
      py::class_<ScatterSinkData, ork::Object, scattersink_data_ptr_t>(module_lev2, "ScatterSinkData")
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<ScatterSinkData>();
            if (kwargs.contains("name"))       d->_name       = kwargs["name"].cast<std::string>();
            if (kwargs.contains("density"))    d->_density    = kwargs["density"].cast<float>();
            if (kwargs.contains("count"))      d->_count      = kwargs["count"].cast<int>();
            if (kwargs.contains("seed"))       d->_seed       = kwargs["seed"].cast<int>();
            if (kwargs.contains("align"))      d->_align      = kwargs["align"].cast<std::string>();
            if (kwargs.contains("yaw_lo"))     d->_yaw_lo     = kwargs["yaw_lo"].cast<float>();
            if (kwargs.contains("yaw_hi"))     d->_yaw_hi     = kwargs["yaw_hi"].cast<float>();
            if (kwargs.contains("scale_lo"))   d->_scale_lo   = kwargs["scale_lo"].cast<float>();
            if (kwargs.contains("scale_hi"))   d->_scale_hi   = kwargs["scale_hi"].cast<float>();
            if (kwargs.contains("cutoff"))     d->_cutoff     = kwargs["cutoff"].cast<float>();
            if (kwargs.contains("jitter"))     d->_jitter     = kwargs["jitter"].cast<float>();
            if (kwargs.contains("max_points")) d->_max_points = kwargs["max_points"].cast<int>();
            if (kwargs.contains("lift"))       d->_lift       = kwargs["lift"].cast<float>();
            if (kwargs.contains("type_names"))
              d->_type_names = kwargs["type_names"].cast<std::vector<std::string>>();
            if (kwargs.contains("type_channels"))
              d->_type_channels = kwargs["type_channels"].cast<std::vector<std::string>>();
            if (kwargs.contains("type_assets"))
              d->_type_assets = kwargs["type_assets"].cast<std::map<std::string, std::string>>();
            if (kwargs.contains("type_materials"))
              d->_type_materials = kwargs["type_materials"].cast<std::map<std::string, std::string>>();
            if (kwargs.contains("type_colliders"))
              d->_type_colliders = kwargs["type_colliders"].cast<std::map<std::string, std::string>>();
            return d;
          }))
          .def_property(
              "name",
              [](scattersink_data_ptr_t d) -> std::string { return d->_name; },
              [](scattersink_data_ptr_t d, std::string v) { d->_name = v; })
          .def_property(
              "density",
              [](scattersink_data_ptr_t d) -> float { return d->_density; },
              [](scattersink_data_ptr_t d, float v) { d->_density = v; })
          .def_property(
              "count",
              [](scattersink_data_ptr_t d) -> int { return d->_count; },
              [](scattersink_data_ptr_t d, int v) { d->_count = v; })
          .def_property(
              "seed",
              [](scattersink_data_ptr_t d) -> int { return d->_seed; },
              [](scattersink_data_ptr_t d, int v) { d->_seed = v; })
          .def_property(
              "align",
              [](scattersink_data_ptr_t d) -> std::string { return d->_align; },
              [](scattersink_data_ptr_t d, std::string v) { d->_align = v; })
          .def_property(
              "yaw_lo",
              [](scattersink_data_ptr_t d) -> float { return d->_yaw_lo; },
              [](scattersink_data_ptr_t d, float v) { d->_yaw_lo = v; })
          .def_property(
              "yaw_hi",
              [](scattersink_data_ptr_t d) -> float { return d->_yaw_hi; },
              [](scattersink_data_ptr_t d, float v) { d->_yaw_hi = v; })
          .def_property(
              "scale_lo",
              [](scattersink_data_ptr_t d) -> float { return d->_scale_lo; },
              [](scattersink_data_ptr_t d, float v) { d->_scale_lo = v; })
          .def_property(
              "scale_hi",
              [](scattersink_data_ptr_t d) -> float { return d->_scale_hi; },
              [](scattersink_data_ptr_t d, float v) { d->_scale_hi = v; })
          .def_property(
              "cutoff",
              [](scattersink_data_ptr_t d) -> float { return d->_cutoff; },
              [](scattersink_data_ptr_t d, float v) { d->_cutoff = v; })
          .def_property(
              "jitter",
              [](scattersink_data_ptr_t d) -> float { return d->_jitter; },
              [](scattersink_data_ptr_t d, float v) { d->_jitter = v; })
          .def_property(
              "max_points",
              [](scattersink_data_ptr_t d) -> int { return d->_max_points; },
              [](scattersink_data_ptr_t d, int v) { d->_max_points = v; })
          .def_property(
              "lift",
              [](scattersink_data_ptr_t d) -> float { return d->_lift; },
              [](scattersink_data_ptr_t d, float v) { d->_lift = v; })
          .def_property(
              "type_names",
              [](scattersink_data_ptr_t d) -> std::vector<std::string> { return d->_type_names; },
              [](scattersink_data_ptr_t d, std::vector<std::string> v) { d->_type_names = std::move(v); })
          .def_property(
              "type_channels",
              [](scattersink_data_ptr_t d) -> std::vector<std::string> { return d->_type_channels; },
              [](scattersink_data_ptr_t d, std::vector<std::string> v) { d->_type_channels = std::move(v); })
          .def_property(
              "type_assets",
              [](scattersink_data_ptr_t d) -> std::map<std::string, std::string> { return d->_type_assets; },
              [](scattersink_data_ptr_t d, std::map<std::string, std::string> v) { d->_type_assets = std::move(v); })
          .def_property(
              "type_materials",
              [](scattersink_data_ptr_t d) -> std::map<std::string, std::string> { return d->_type_materials; },
              [](scattersink_data_ptr_t d, std::map<std::string, std::string> v) { d->_type_materials = std::move(v); })
          .def_property(
              "type_colliders",
              [](scattersink_data_ptr_t d) -> std::map<std::string, std::string> { return d->_type_colliders; },
              [](scattersink_data_ptr_t d, std::map<std::string, std::string> v) { d->_type_colliders = std::move(v); });
  type_codec->registerStdCodec<scattersink_data_ptr_t>(scat_type);

  ///////////////////////////////////////////////////////////////////////////
  // HypermeshGenData (D.3) — GPU mesh-graph asset. EMBEDS the serialized
  // hypermesh compute graph (.graph), model B from day one. materialize()
  // = hypermesh::materializeLive (the persistent GraphInst + GPU mesh).
  ///////////////////////////////////////////////////////////////////////////
  auto hmgen_type = //
      py::class_<HypermeshGenData, AssetGenData, hypermesh_gendata_ptr_t>(
          module_lev2, "HypermeshGenData")
          .def(
              "materialize",
              [](hypermesh_gendata_ptr_t g, ctx_t c) -> hypermesh::livehypermesh_ptr_t {
                return g->materialize(c.get());
              })
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<HypermeshGenData>();
            if (kwargs.contains("asset_name"))
              d->_asset_name = kwargs["asset_name"].cast<std::string>();
            if (kwargs.contains("dsl_file"))
              d->_dsl_file = kwargs["dsl_file"].cast<std::string>();
            if (kwargs.contains("vtx_budget"))
              d->_vtx_budget = kwargs["vtx_budget"].cast<int>();
            if (kwargs.contains("graph"))
              d->_graph_data = kwargs["graph"].cast<ork::dataflow::graphdata_ptr_t>();
            return d;
          }))
          .def_property(
              "dsl_file",
              [](hypermesh_gendata_ptr_t d) -> std::string { return d->_dsl_file; },
              [](hypermesh_gendata_ptr_t d, std::string v) { d->_dsl_file = v; })
          .def_property(
              "vtx_budget",
              [](hypermesh_gendata_ptr_t d) -> int { return d->_vtx_budget; },
              [](hypermesh_gendata_ptr_t d, int v) { d->_vtx_budget = v; })
          .def_property(
              "graph",
              [](hypermesh_gendata_ptr_t d) -> ork::dataflow::graphdata_ptr_t { return d->_graph_data; },
              [](hypermesh_gendata_ptr_t d, ork::dataflow::graphdata_ptr_t g) { d->_graph_data = g; });
  type_codec->registerStdCodec<hypermesh_gendata_ptr_t>(hmgen_type);

  ///////////////////////////////////////////////////////////////////////////
  // HdriToXirGenData — reflected recipe for a static HDR→XIR conversion.
  // The Python wrapper (HdriToXir in scene/assets.py) calls .build() at
  // materialize time, drives EnvMapProcessor::processToXIRDataBlockAsync,
  // writes the resulting .xir to a deterministic path, and hands the path
  // string back so downstream consumers (SceneGraph SkyboxTexPathStr) can
  // use it.
  ///////////////////////////////////////////////////////////////////////////
  auto hdri_type =
      py::class_<HdriToXirGenData, AssetGenData, hdri_to_xir_gendata_ptr_t>(
          module_lev2, "HdriToXirGenData")
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<HdriToXirGenData>();
            if (kwargs.contains("source_path")) d->_source_path = kwargs["source_path"].cast<std::string>();
            if (kwargs.contains("levels"))      d->_levels      = kwargs["levels"].cast<int>();
            if (kwargs.contains("samples"))     d->_samples     = kwargs["samples"].cast<int>();
            if (kwargs.contains("scale"))       d->_scale       = kwargs["scale"].cast<float>();
            if (kwargs.contains("clamp"))       d->_clamp       = kwargs["clamp"].cast<float>();
            if (kwargs.contains("asset_name"))  d->_asset_name  = kwargs["asset_name"].cast<std::string>();
            return d;
          }))
          .def_property(
              "source_path",
              [](hdri_to_xir_gendata_ptr_t d) -> std::string { return d->_source_path; },
              [](hdri_to_xir_gendata_ptr_t d, std::string v) { d->_source_path = v; })
          .def_property(
              "levels",
              [](hdri_to_xir_gendata_ptr_t d) -> int { return d->_levels; },
              [](hdri_to_xir_gendata_ptr_t d, int v) { d->_levels = v; })
          .def_property(
              "samples",
              [](hdri_to_xir_gendata_ptr_t d) -> int { return d->_samples; },
              [](hdri_to_xir_gendata_ptr_t d, int v) { d->_samples = v; })
          .def_property(
              "scale",
              [](hdri_to_xir_gendata_ptr_t d) -> float { return d->_scale; },
              [](hdri_to_xir_gendata_ptr_t d, float v) { d->_scale = v; })
          .def_property(
              "clamp",
              [](hdri_to_xir_gendata_ptr_t d) -> float { return d->_clamp; },
              [](hdri_to_xir_gendata_ptr_t d, float v) { d->_clamp = v; });
  type_codec->registerStdCodec<hdri_to_xir_gendata_ptr_t>(hdri_type);

  ///////////////////////////////////////////////////////////////////////////
  // VdbFileSdfGenData — pre-baked .vdb file. Materialize loads via
  // openvdb::io::File::readGrid(_grid_name).
  ///////////////////////////////////////////////////////////////////////////
  auto vdbfile_type =
      py::class_<VdbFileSdfGenData, AssetGenData, vdb_file_sdf_gendata_ptr_t>(
          module_lev2, "VdbFileSdfGenData")
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<VdbFileSdfGenData>();
            if (kwargs.contains("vdb_path"))   d->_vdb_path   = kwargs["vdb_path"].cast<std::string>();
            if (kwargs.contains("grid_name"))  d->_grid_name  = kwargs["grid_name"].cast<std::string>();
            if (kwargs.contains("asset_name")) d->_asset_name = kwargs["asset_name"].cast<std::string>();
            return d;
          }))
          .def_property(
              "vdb_path",
              [](vdb_file_sdf_gendata_ptr_t d) -> std::string { return d->_vdb_path; },
              [](vdb_file_sdf_gendata_ptr_t d, std::string v) { d->_vdb_path = v; })
          .def_property(
              "grid_name",
              [](vdb_file_sdf_gendata_ptr_t d) -> std::string { return d->_grid_name; },
              [](vdb_file_sdf_gendata_ptr_t d, std::string v) { d->_grid_name = v; });
  type_codec->registerStdCodec<vdb_file_sdf_gendata_ptr_t>(vdbfile_type);

  ///////////////////////////////////////////////////////////////////////////
  // MeshSdfGenData — voxelize a triangle mesh loaded from a path.
  // Materialize loads mesh via Assimp + runs meshToLevelSet.
  ///////////////////////////////////////////////////////////////////////////
  auto meshsdf_type =
      py::class_<MeshSdfGenData, AssetGenData, mesh_sdf_gendata_ptr_t>(
          module_lev2, "MeshSdfGenData")
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<MeshSdfGenData>();
            if (kwargs.contains("mesh_path"))  d->_mesh_path  = kwargs["mesh_path"].cast<std::string>();
            if (kwargs.contains("voxel_size")) d->_voxel_size = kwargs["voxel_size"].cast<float>();
            if (kwargs.contains("half_width")) d->_half_width = kwargs["half_width"].cast<float>();
            if (kwargs.contains("grid_name"))  d->_grid_name  = kwargs["grid_name"].cast<std::string>();
            if (kwargs.contains("asset_name")) d->_asset_name = kwargs["asset_name"].cast<std::string>();
            return d;
          }))
          .def_property(
              "mesh_path",
              [](mesh_sdf_gendata_ptr_t d) -> std::string { return d->_mesh_path; },
              [](mesh_sdf_gendata_ptr_t d, std::string v) { d->_mesh_path = v; })
          .def_property(
              "voxel_size",
              [](mesh_sdf_gendata_ptr_t d) -> float { return d->_voxel_size; },
              [](mesh_sdf_gendata_ptr_t d, float v) { d->_voxel_size = v; })
          .def_property(
              "half_width",
              [](mesh_sdf_gendata_ptr_t d) -> float { return d->_half_width; },
              [](mesh_sdf_gendata_ptr_t d, float v) { d->_half_width = v; })
          .def_property(
              "grid_name",
              [](mesh_sdf_gendata_ptr_t d) -> std::string { return d->_grid_name; },
              [](mesh_sdf_gendata_ptr_t d, std::string v) { d->_grid_name = v; });
  type_codec->registerStdCodec<mesh_sdf_gendata_ptr_t>(meshsdf_type);

  ///////////////////////////////////////////////////////////////////////////
  // MeshGenData — baked triangle geometry. The geometry itself lives in a
  // sidecar .ogeo chunkfile (referenced by path, not inlined into JSON);
  // material is referenced by name. Materialize reads the chunkfile back into
  // a Geometry -> MicroMesh -> RigidPrimitive drawable.
  ///////////////////////////////////////////////////////////////////////////
  auto meshgen_type =
      py::class_<MeshGenData, AssetGenData, mesh_gendata_ptr_t>(
          module_lev2, "MeshGenData")
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<MeshGenData>();
            if (kwargs.contains("geometry_path"))       d->_geometry_path       = kwargs["geometry_path"].cast<std::string>();
            if (kwargs.contains("material_asset_name")) d->_material_asset_name = kwargs["material_asset_name"].cast<std::string>();
            if (kwargs.contains("asset_name"))          d->_asset_name          = kwargs["asset_name"].cast<std::string>();
            return d;
          }))
          .def_property(
              "geometry_path",
              [](mesh_gendata_ptr_t d) -> std::string { return d->_geometry_path; },
              [](mesh_gendata_ptr_t d, std::string v) { d->_geometry_path = v; })
          .def_property(
              "material_asset_name",
              [](mesh_gendata_ptr_t d) -> std::string { return d->_material_asset_name; },
              [](mesh_gendata_ptr_t d, std::string v) { d->_material_asset_name = v; });
  type_codec->registerStdCodec<mesh_gendata_ptr_t>(meshgen_type);
}

} // namespace ork::lev2
