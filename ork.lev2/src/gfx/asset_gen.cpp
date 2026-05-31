////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/asset_gen.h>
#include <ork/dataflow/all.h> // full GraphData type for HeightFieldGenData's embedded-graph property

ImplementReflectionX(ork::lev2::AssetGenData,             "AssetGenData");
ImplementReflectionX(ork::lev2::HeightFieldGenData,       "HeightFieldGenData");
ImplementReflectionX(ork::lev2::ImplicitSdfGenData,       "ImplicitSdfGenData");
ImplementReflectionX(ork::lev2::PbrMaterialGenData,       "PbrMaterialGenData");
ImplementReflectionX(ork::lev2::FreestyleMaterialGenData, "FreestyleMaterialGenData");
ImplementReflectionX(ork::lev2::VdbGridToDrawableGenData, "VdbGridToDrawableGenData");
ImplementReflectionX(ork::lev2::ParticleSystemGenData,    "ParticleSystemGenData");
ImplementReflectionX(ork::lev2::HdriToXirGenData,         "HdriToXirGenData");
ImplementReflectionX(ork::lev2::VdbFileSdfGenData,        "VdbFileSdfGenData");
ImplementReflectionX(ork::lev2::MeshSdfGenData,           "MeshSdfGenData");
ImplementReflectionX(ork::lev2::MeshGenData,              "MeshGenData");

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

void AssetGenData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("asset_name", &AssetGenData::_asset_name);
}

///////////////////////////////////////////////////////////////////////////////

void HeightFieldGenData::describeX(object::ObjectClass* clazz) {
  // the embedded terrain graph serializes inline (owned, NOT a cross-asset ref),
  // so the .ecs is self-contained and loads with no Python / no DSL file.
  clazz->directObjectProperty("graph", &HeightFieldGenData::_graph_data);
  clazz->directProperty("dimension", &HeightFieldGenData::_dimension);
  clazz->directProperty("extent_m", &HeightFieldGenData::_extent_m);
  clazz->directProperty("height_scale_m", &HeightFieldGenData::_height_scale_m);
}

///////////////////////////////////////////////////////////////////////////////

void ImplicitSdfGenData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("shader",     &ImplicitSdfGenData::_shader);
  clazz->directProperty("bbox_min",   &ImplicitSdfGenData::_bbox_min);
  clazz->directProperty("bbox_max",   &ImplicitSdfGenData::_bbox_max);
  clazz->directProperty("voxel_size", &ImplicitSdfGenData::_voxel_size);
  clazz->directProperty("background", &ImplicitSdfGenData::_background);
  clazz->directProperty("grid_name",  &ImplicitSdfGenData::_grid_name);
  clazz->directVarMapProperty("params", &ImplicitSdfGenData::_params);
}

///////////////////////////////////////////////////////////////////////////////

void VdbGridToDrawableGenData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("grid_asset_name",     &VdbGridToDrawableGenData::_grid_asset_name);
  clazz->directProperty("material_asset_name", &VdbGridToDrawableGenData::_material_asset_name);
  clazz->directProperty("iso",                 &VdbGridToDrawableGenData::_iso);
  clazz->directProperty("adaptivity",          &VdbGridToDrawableGenData::_adaptivity);
  clazz->directProperty("flip_windings",       &VdbGridToDrawableGenData::_flip_windings);
}

///////////////////////////////////////////////////////////////////////////////

void PbrMaterialGenData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("base_color",  &PbrMaterialGenData::_base_color);
  clazz->directProperty("metallic",    &PbrMaterialGenData::_metallic);
  clazz->directProperty("roughness",   &PbrMaterialGenData::_roughness);
  clazz->directProperty("color_path",  &PbrMaterialGenData::_color_path);
  clazz->directProperty("normal_path", &PbrMaterialGenData::_normal_path);
  clazz->directProperty("mtlruf_path", &PbrMaterialGenData::_mtlruf_path);
  clazz->directProperty("shaderpath",  &PbrMaterialGenData::_shaderpath);
  clazz->directVarMapProperty("shader_params", &PbrMaterialGenData::_shader_params);
  // PBR2 Phase 2 — 8 glTF KHR-extension lobes.
  clazz->directProperty("has_transmission",            &PbrMaterialGenData::_has_transmission);
  clazz->directProperty("transmission_factor",         &PbrMaterialGenData::_transmission_factor);
  clazz->directProperty("has_transmission_roughness",  &PbrMaterialGenData::_has_transmission_roughness);
  clazz->directProperty("transmission_roughness",      &PbrMaterialGenData::_transmission_roughness);
  clazz->directProperty("has_ior",                     &PbrMaterialGenData::_has_ior);
  clazz->directProperty("ior",                         &PbrMaterialGenData::_ior);
  clazz->directProperty("has_volume",                  &PbrMaterialGenData::_has_volume);
  clazz->directProperty("volume_thickness_factor",     &PbrMaterialGenData::_volume_thickness_factor);
  clazz->directProperty("has_diffuse_transmission",    &PbrMaterialGenData::_has_diffuse_transmission);
  clazz->directProperty("diffuse_transmission_factor", &PbrMaterialGenData::_diffuse_transmission_factor);
  clazz->directProperty("has_specular",                &PbrMaterialGenData::_has_specular);
  clazz->directProperty("specular_factor",             &PbrMaterialGenData::_specular_factor);
  clazz->directProperty("has_clearcoat",               &PbrMaterialGenData::_has_clearcoat);
  clazz->directProperty("clearcoat_factor",            &PbrMaterialGenData::_clearcoat_factor);
  clazz->directProperty("has_sheen",                   &PbrMaterialGenData::_has_sheen);
  clazz->directProperty("sheen_factor",                &PbrMaterialGenData::_sheen_factor);
  clazz->directProperty("has_iridescence",             &PbrMaterialGenData::_has_iridescence);
  clazz->directProperty("iridescence_factor",          &PbrMaterialGenData::_iridescence_factor);
  clazz->directProperty("sheen_color",                 &PbrMaterialGenData::_sheen_color);
  clazz->directProperty("specular_color",              &PbrMaterialGenData::_specular_color);
  clazz->directProperty("attenuation_color",           &PbrMaterialGenData::_attenuation_color);
  clazz->directProperty("diffuse_transmission_color",  &PbrMaterialGenData::_diffuse_transmission_color);
  clazz->directProperty("clearcoat_roughness",         &PbrMaterialGenData::_clearcoat_roughness);
  clazz->directProperty("sheen_roughness",             &PbrMaterialGenData::_sheen_roughness);
  clazz->directProperty("attenuation_distance",        &PbrMaterialGenData::_attenuation_distance);
  // PBR2 Phase 3 (P3.D) — subsurface (KHR_materials_subsurface in-flight)
  clazz->directProperty("has_subsurface",              &PbrMaterialGenData::_has_subsurface);
  clazz->directProperty("subsurface_color",            &PbrMaterialGenData::_subsurface_color);
  clazz->directProperty("subsurface_radius",           &PbrMaterialGenData::_subsurface_radius);
  clazz->directProperty("subsurface_factor",           &PbrMaterialGenData::_subsurface_factor);
}

///////////////////////////////////////////////////////////////////////////////

void ParticleSystemGenData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("dsl_file",  &ParticleSystemGenData::_dsl_file);
  clazz->directProperty("dsl_class", &ParticleSystemGenData::_dsl_class);
  clazz->directVarMapProperty("params", &ParticleSystemGenData::_params);
  clazz->directMapProperty("asset_kwargs", &ParticleSystemGenData::_asset_kwargs);
  clazz->directProperty("probe_entity_name", &ParticleSystemGenData::_probe_entity_name);
}

///////////////////////////////////////////////////////////////////////////////

void FreestyleMaterialGenData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("shader_text", &FreestyleMaterialGenData::_shader_text);
  clazz->directProperty("shader_file", &FreestyleMaterialGenData::_shader_file);
  clazz->directProperty("shader_name", &FreestyleMaterialGenData::_shader_name);
  clazz->directProperty("technique",   &FreestyleMaterialGenData::_technique);
  clazz->directProperty("rendermodel", &FreestyleMaterialGenData::_rendermodel);
  clazz->directProperty("blending",    &FreestyleMaterialGenData::_blending);
  clazz->directProperty("culltest",    &FreestyleMaterialGenData::_culltest);
  clazz->directProperty("depthtest",   &FreestyleMaterialGenData::_depthtest);
}

///////////////////////////////////////////////////////////////////////////////

void HdriToXirGenData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("source_path", &HdriToXirGenData::_source_path);
  clazz->directProperty("levels",      &HdriToXirGenData::_levels);
  clazz->directProperty("samples",     &HdriToXirGenData::_samples);
  clazz->directProperty("scale",       &HdriToXirGenData::_scale);
  clazz->directProperty("clamp",       &HdriToXirGenData::_clamp);
}

///////////////////////////////////////////////////////////////////////////////

void VdbFileSdfGenData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("vdb_path",  &VdbFileSdfGenData::_vdb_path);
  clazz->directProperty("grid_name", &VdbFileSdfGenData::_grid_name);
}

///////////////////////////////////////////////////////////////////////////////

void MeshSdfGenData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("mesh_path",  &MeshSdfGenData::_mesh_path);
  clazz->directProperty("voxel_size", &MeshSdfGenData::_voxel_size);
  clazz->directProperty("half_width", &MeshSdfGenData::_half_width);
  clazz->directProperty("grid_name",  &MeshSdfGenData::_grid_name);
}

///////////////////////////////////////////////////////////////////////////////

void MeshGenData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("geometry_path",       &MeshGenData::_geometry_path);
  clazz->directProperty("material_asset_name", &MeshGenData::_material_asset_name);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
