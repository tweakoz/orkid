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
#include <ork/lev2/gfx/material_pbr.inl>      // D.1: the C++ PbrMaterial materializer
#include <ork/lev2/gfx/material_freestyle.h>  // param() resolution for shader_params pre-binding
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h> // D.1: the C++ HeightField materializer (bake + manifest)
#include <ork/lev2/gfx/terrain/dflow/hfdflow_scatter.h> // E.2: the C++ scatter placer (post-bake sinks)
#include <ork/lev2/gfx/meshutil/geometry.h>             // E.2: ScatterSet .ogeo write
#include <ork/lev2/gfx/hypermesh/hmdflow.h>     // D.3: the C++ Hypermesh materializer (materializeLive)
#include <ork/lev2/gfx/txi.h>                   // D.4: sampler-texture upload (initTextureFromImage)
#include <ork/reflect/properties/DirectTypedVector.hpp> // D.4: ScatterSinkData string vectors
#include <ork/reflect/properties/DirectTypedMap.hpp>    // D.4: type/sampler binding string maps
#include <ork/reflect/properties/ITypedMap.hpp>
#include <filesystem>
#include <sstream>
#include <iomanip>

ImplementReflectionX(ork::lev2::AssetGenData,             "AssetGenData");
ImplementReflectionX(ork::lev2::ScatterSinkData,          "ScatterSinkData");
ImplementReflectionX(ork::lev2::HeightFieldGenData,       "HeightFieldGenData");
ImplementReflectionX(ork::lev2::HypermeshGenData,         "HypermeshGenData");
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
// D.1 — PbrMaterialGenData::materialize: the C++ port of the Python build() (a 1:1 mirror of
// hypergraph ecs/scene/assets.py PbrMaterial.build). Every callee was already C++; the Python
// wrapper now delegates here. Order matters: _shaderpath BEFORE gpuInit (the internal
// FreestyleMaterial loads it there); the bindable uniform-block params resolve only AFTER
// gpuInit, so the round-tripped _shader_params defaults pre-bind post-init (they propagate into
// pipelines at pipeline-creation, before first draw).
///////////////////////////////////////////////////////////////////////////////

pbrmaterial_ptr_t PbrMaterialGenData::materialize(Context* ctx) const {
  auto mat           = std::make_shared<PBRMaterial>();
  mat->mMaterialName = _asset_name.empty() ? "pbr" : _asset_name;
  auto img = [](const std::string& path, fvec3 dflt) -> image_ptr_t {
    if (not path.empty())
      return Image::createFromFile(path);
    auto im = std::make_shared<Image>();  // procedural solid-color default
    im->initRGB8WithColor(64, 64, dflt);  // (matches createPbrMaterialWithColor)
    return im;
  };
  auto img_color  = img(_color_path, fvec3(1, 1, 1));
  auto img_normal = img(_normal_path, fvec3(0.5, 1.0, 0.5));
  auto img_mtlruf = img(_mtlruf_path, fvec3(1, 1, 1));
  mat->assignImages(ctx, img_color, img_normal, img_mtlruf, nullptr, nullptr, true /*doConform*/);
  mat->_metallicFactor  = _metallic;
  mat->_roughnessFactor = _roughness;
  mat->_baseColor       = _base_color;
  // PBR2 lobes — GenData (snake_case) -> live material (camelCase), 1:1 with the Python kwargs list
  mat->_hasTransmission           = _has_transmission;
  mat->_transmissionFactor        = _transmission_factor;
  mat->_hasTransmissionRoughness  = _has_transmission_roughness;
  mat->_transmissionRoughness     = _transmission_roughness;
  mat->_hasIor                    = _has_ior;
  mat->_ior                       = _ior;
  mat->_hasVolume                 = _has_volume;
  mat->_volumeThicknessFactor     = _volume_thickness_factor;
  mat->_hasDiffuseTransmission    = _has_diffuse_transmission;
  mat->_diffuseTransmissionFactor = _diffuse_transmission_factor;
  mat->_hasSpecular               = _has_specular;
  mat->_specularFactor            = _specular_factor;
  mat->_hasClearcoat              = _has_clearcoat;
  mat->_clearcoatFactor           = _clearcoat_factor;
  mat->_hasSheen                  = _has_sheen;
  mat->_sheenFactor               = _sheen_factor;
  mat->_hasIridescence            = _has_iridescence;
  mat->_iridescenceFactor         = _iridescence_factor;
  mat->_sheenColor                = _sheen_color;
  mat->_specularColor             = _specular_color;
  mat->_attenuationColor          = _attenuation_color;
  mat->_diffuseTransmissionColor  = _diffuse_transmission_color;
  mat->_clearcoatRoughness        = _clearcoat_roughness;
  mat->_sheenRoughness            = _sheen_roughness;
  mat->_attenuationDistance       = _attenuation_distance;
  mat->_hasSubsurface             = _has_subsurface;
  mat->_subsurfaceColor           = _subsurface_color;
  mat->_subsurfaceRadius          = _subsurface_radius;
  mat->_subsurfaceFactor          = _subsurface_factor;
  if (not _shaderpath.empty())
    mat->_shaderpath = _shaderpath;       // generated ptex3d fxv2 (<hyperassets> alias, B.5c)
  mat->gpuInit(ctx);
  if (not _shaderpath.empty() and _shader_params) {
    for (const auto& item : _shader_params->_themap) {
      auto par = mat->_as_freestyle ? mat->_as_freestyle->param(item.first) : nullptr;
      if (par)
        mat->bindParam(par, item.second); // varmap value IS bindParam's varval_t
    }
  }
  // D.4 (review 1.11): sampler→texture bindings — load each image, upload to a texture,
  // bind to the generated shader's sampler2D uniform. The bound varval_t keeps the
  // texture_ptr_t alive. Missing files / params skip LOUDLY (AssetSystem declaration
  // order is the dependency order — bake-producing gens must precede this material).
  for (const auto& item : _sampler_textures)
    bindSamplerTexture(mat, ctx, item.first, item.second, "PbrMaterialGenData<" + _asset_name + ">");
  return mat;
}

///////////////////////////////////////////////////////////////////////////////
// D.1 — HeightFieldGenData::materialize: the C++ port of the Python build() (hypergraph
// ecs/scene/assets.py HeightField.build, minus the AUTHORING-only scatter sinks). Derives the
// per-channel output paths onto the graph's CaptureModules, runs the (cook-cache-backed) bake,
// and writes the .terrain.json scale-contract manifest — schema identical to Python's
// TerrainManifest.write (version 1). Returns the manifest path: the manifest is the
// self-describing result contract (channel files + semantics + fit ranges + physical scale).
///////////////////////////////////////////////////////////////////////////////

bool PbrMaterialGenData::bindSamplerTexture(
    pbrmaterial_ptr_t mat, Context* ctx, const std::string& sampler, const std::string& path_in, const std::string& who) {
  std::string path = file::Path::expandPathString(path_in);
  auto par         = mat->_as_freestyle ? mat->_as_freestyle->param(sampler) : nullptr;
  if (not par) {
    printf("%s: shader has no sampler param<%s> — binding skipped\n", who.c_str(), sampler.c_str());
    return false;
  }
  if (not std::filesystem::exists(path)) {
    printf(
        "%s: sampler<%s> texture<%s> MISSING — binding skipped "
        "(is the producing asset declared in the scene?)\n",
        who.c_str(),
        sampler.c_str(),
        path.c_str());
    return false;
  }
  auto img = Image::createFromFile(path);
  auto tex = std::make_shared<Texture>();
  tex->_debugName = sampler;
  ctx->TXI()->initTextureFromImage(tex.get(), img, false /*mipmapped*/, false /*async*/);
  mat->bindParam(par, tex);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

std::string HeightFieldGenData::channelPath(const std::string& channel, const std::string& ext) const {
  std::string name = _asset_name.empty() ? "unnamed" : _asset_name;
  return file::Path::expandPathString("<assetcache>/terrain/" + name + "/" + channel + "." + ext);
}

std::string HeightFieldGenData::materialize(Context* ctx, const std::string& ext) const {
  OrkAssert(ext == "exr" or ext == "png");
  OrkAssert(_graph_data != nullptr);
  std::string name = _asset_name.empty() ? "unnamed" : _asset_name;
  // expandPathString — the SAME expansion Python's _Path.expandPathString uses (a bare
  // toAbsolute() renders without the leading slash and downstream abspath() doubles the CWD on)
  std::string outdir = file::Path::expandPathString("<assetcache>/terrain/" + name);
  std::filesystem::create_directories(outdir);
  // per-channel output paths onto the CaptureModules (single channel -> concrete path;
  // multi "a,b" -> ONE {channel} template the multichannel bake substitutes per channel)
  std::vector<std::string> channels; // flat, in capture/declaration order (stats align to this)
  for (size_t i = 0; i < _graph_data->numModules(); i++) {
    auto cap = std::dynamic_pointer_cast<terrain::CaptureModuleData>(_graph_data->module(i));
    if (not cap)
      continue;
    std::vector<std::string> ch_list;
    std::string src = cap->_channel.empty() ? "height" : cap->_channel;
    std::stringstream ss(src);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
      while (not tok.empty() and tok.front() == ' ') tok.erase(tok.begin());
      while (not tok.empty() and tok.back() == ' ') tok.pop_back();
      if (not tok.empty())
        ch_list.push_back(tok);
    }
    if (ch_list.empty())
      ch_list.push_back("height");
    if (ch_list.size() == 1)
      cap->_path = file::Path((outdir + "/" + ch_list[0] + "." + ext).c_str());
    else
      cap->_path = file::Path((outdir + "/{channel}." + ext).c_str());
    for (const auto& c : ch_list)
      channels.push_back(c);
  }
  auto stats = terrain::bakeHeightfield(_graph_data, ctx, _dimension, _extent_m, _height_scale_m);
  // stats return in FLUSH (topo) order, channels enumerate in name-sorted module order — key by
  // the self-describing _channel name (index-zipping the two shuffles stats across channels)
  std::map<std::string, terrain::fieldstats_ptr_t> stats_by_ch;
  for (const auto& st : stats)
    if (st)
      stats_by_ch[st->_channel] = st;
  // the manifest (TerrainManifest.write schema, version 1). Hand-formatted: the schema is small
  // and fixed; consumers PARSE it (byte layout irrelevant, key set is the contract).
  auto sem = [](const std::string& ch) -> std::string {
    if (ch == "height") return "height_normalized";
    if (ch == "normal") return "normal_world";
    return ch;
  };
  std::ostringstream js;
  js << std::setprecision(9);  // float max_digits10: the manifest is the authoritative scale contract
  js << "{\n";
  js << "  \"version\": 1,\n";
  js << "  \"scale\": {\n";
  js << "    \"extent_m\": " << double(_extent_m) << ",\n";
  js << "    \"height_m\": " << double(_height_scale_m) << ",\n";
  js << "    \"dim\": " << _dimension << ",\n";
  js << "    \"origin_m\": [0.0, 0.0, 0.0],\n";
  js << "    \"up_axis\": \"y\",\n";
  js << "    \"height_anchor\": \"stored_unit\"\n";
  js << "  },\n";
  js << "  \"format\": \"" << ext << "\",\n";
  js << "  \"channels\": {\n";
  for (size_t i = 0; i < channels.size(); i++) {
    const auto& ch = channels[i];
    float mn = 0.0f, mx = 0.0f, me = 0.0f;
    auto it_st = stats_by_ch.find(ch);
    if (it_st != stats_by_ch.end()) {
      mn = it_st->second->_min;
      mx = it_st->second->_max;
      me = it_st->second->_mean;
    }
    js << "    \"" << ch << "\": {\n";
    js << "      \"file\": \"" << ch << "." << ext << "\",\n";
    js << "      \"semantic\": \"" << sem(ch) << "\",\n";
    js << "      \"min\": " << double(mn) << ",\n";
    js << "      \"max\": " << double(mx) << ",\n";
    js << "      \"mean\": " << double(me) << "\n";
    js << "    }" << (i + 1 < channels.size() ? "," : "") << "\n";
  }
  js << "  },\n";
  js << "  \"provenance\": {\n";
  js << "    \"asset_name\": \"" << _asset_name << "\"\n";
  js << "  }\n";
  js << "}\n";
  std::string manifest_path = outdir + "/" + name + ".terrain.json";
  {
    FILE* f = fopen(manifest_path.c_str(), "w");
    OrkAssert(f != nullptr);
    auto body = js.str();
    fwrite(body.data(), 1, body.size(), f);
    fclose(f);
  }
  // E.2 — POST-BAKE scatter sinks: run the C++ placer on each reflected ScatterSinkData
  // and write <outdir>/<sink>.ogeo (the deterministic artifact path consumers resolve),
  // so a deserialized scene PLACES at load with zero Python. The weight channels are the
  // bake's captured images (channel name -> <outdir>/<channel>.<ext>).
  for (const auto& sink : _scatters) {
    if (not sink)
      continue;
    std::map<std::string, std::string> chans;
    chans["height"] = outdir + "/height." + ext;
    for (const auto& ch : sink->_type_channels)
      chans[ch] = outdir + "/" + ch + "." + ext;
    auto geo = terrain::scatterPlace(*sink, chans, _extent_m, _height_scale_m);
    if (not geo) {
      printf("HeightFieldGenData<%s>: scatter sink<%s> FAILED to place (missing channels?)\n",
             name.c_str(), sink->_name.c_str());
      OrkAssert(false);
    }
    std::string opath = outdir + "/" + sink->_name + ".ogeo";
    geo->writeChunkfile(file::Path(opath.c_str()));
    printf("terrain scatter (C++): %s -> %d points -> %s\n",
           sink->_name.c_str(), geo->numPoints(), opath.c_str());
  }
  return manifest_path;
}


///////////////////////////////////////////////////////////////////////////////

void AssetGenData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("asset_name", &AssetGenData::_asset_name);
}

///////////////////////////////////////////////////////////////////////////////

void HeightFieldGenData::describeX(object::ObjectClass* clazz) {
  // the embedded terrain graph serializes inline (owned, NOT a cross-asset ref),
  // so the .ecs is self-contained and loads with no Python / no DSL file.
  clazz->directObjectProperty("graph", &HeightFieldGenData::_graph_data);
  // D.4: the scatter sinks (placement contract + type bindings) round-trip with the asset
  clazz->directObjectVectorProperty("scatters", &HeightFieldGenData::_scatters);
  clazz->directProperty("dimension", &HeightFieldGenData::_dimension);
  clazz->directProperty("extent_m", &HeightFieldGenData::_extent_m);
  clazz->directProperty("height_scale_m", &HeightFieldGenData::_height_scale_m);
  // E.6/2.20 — the terrain↔material contract rides the TERRAIN asset
  clazz->directProperty("material_asset", &HeightFieldGenData::_material_asset);
  clazz->directMapProperty("channel_samplers", &HeightFieldGenData::_channel_samplers);
}

///////////////////////////////////////////////////////////////////////////////

void ScatterSinkData::describeX(object::ObjectClass* clazz) {
  clazz->directProperty("name", &ScatterSinkData::_name);
  clazz->directProperty("density", &ScatterSinkData::_density);
  clazz->directProperty("count", &ScatterSinkData::_count);
  clazz->directProperty("seed", &ScatterSinkData::_seed);
  clazz->directProperty("align", &ScatterSinkData::_align);
  clazz->directProperty("yaw_lo", &ScatterSinkData::_yaw_lo);
  clazz->directProperty("yaw_hi", &ScatterSinkData::_yaw_hi);
  clazz->directProperty("scale_lo", &ScatterSinkData::_scale_lo);
  clazz->directProperty("scale_hi", &ScatterSinkData::_scale_hi);
  clazz->directProperty("cutoff", &ScatterSinkData::_cutoff);
  clazz->directProperty("jitter", &ScatterSinkData::_jitter);
  clazz->directProperty("max_points", &ScatterSinkData::_max_points);
  clazz->directProperty("lift", &ScatterSinkData::_lift);
  clazz->directVectorProperty("type_names", &ScatterSinkData::_type_names);
  clazz->directVectorProperty("type_channels", &ScatterSinkData::_type_channels);
  clazz->directMapProperty("type_assets", &ScatterSinkData::_type_assets);
  clazz->directMapProperty("type_materials", &ScatterSinkData::_type_materials);
  clazz->directMapProperty("type_colliders", &ScatterSinkData::_type_colliders);
}

///////////////////////////////////////////////////////////////////////////////

void HypermeshGenData::describeX(object::ObjectClass* clazz) {
  // the embedded hypermesh graph serializes inline (owned, NOT a cross-asset ref) —
  // same model-B contract as HeightFieldGenData.
  clazz->directObjectProperty("graph", &HypermeshGenData::_graph_data);
  clazz->directProperty("dsl_file", &HypermeshGenData::_dsl_file);
  clazz->directProperty("vtx_budget", &HypermeshGenData::_vtx_budget);
}

hypermesh::livehypermesh_ptr_t HypermeshGenData::materialize(Context* ctx) const {
  if (not _graph_data) {
    printf("HypermeshGenData::materialize: asset<%s> has NO embedded graph\n", _asset_name.c_str());
    return nullptr;
  }
  return hypermesh::materializeLive(_graph_data, ctx, _vtx_budget);
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
  // D.4: sampler uniform -> texture image path (alias-form OK; loaded + bound at materialize)
  clazz->directMapProperty("sampler_textures", &PbrMaterialGenData::_sampler_textures);
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
  // D.2 model B: the embedded particle graph (null on legacy model-A gendatas)
  clazz->directObjectProperty("graph", &ParticleSystemGenData::_graph_data);
  clazz->directProperty("probe_entity_name", &ParticleSystemGenData::_probe_entity_name);
  clazz->directProperty("emitter_intensity", &ParticleSystemGenData::_emitter_intensity);
  clazz->directProperty("emitter_radius", &ParticleSystemGenData::_emitter_radius);
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
