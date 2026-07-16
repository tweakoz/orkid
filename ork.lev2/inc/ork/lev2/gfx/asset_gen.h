////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <ork/object/Object.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/util/Context.h>
#include <ork/kernel/varmap.inl>

// Reflected DATA wrappers for the HYPERECS asset DSL (M2b).
//
// The Python asset gens in ork.ecs.scene.assets each get a thin reflected
// C++ data class here. The reflected fields are POD (or simple reflected
// containers) that mirror the Python constructor kwargs 1:1. The actual
// "materialize" logic remains in Python for M2b — these classes are pure
// DATA HOLDERS that survive JSON round-trip. M2b's acid test is:
//
//   gen  = HollowFunnelMesh(top_outer=8.0, ...).gendata
//   js   = gen.serializeJson()
//   gen2 = Object.deserializeJson(js)
//   assert HollowFunnelMesh._wrap(gen2).build() ≡ HollowFunnelMesh(...).build()
//
// Cross-asset references (e.g. MeshToSdf input_mesh) are stored by NAME
// (string), resolved at materialize time through the Scene-side asset
// registry (M2b.4). Direct shared_ptr references between gens are not
// stored on the data — that's what kept M2a from round-tripping.

// forward decl — HeightFieldGenData owns a terrain compute GraphData inline
// (serialized into the scene; the full type is only needed in the .cpp).
namespace ork::dataflow {
struct GraphData;
using graphdata_ptr_t = std::shared_ptr<GraphData>;
}

// forward decl — HypermeshGenData materializes into a LiveHypermesh (runtime
// artifact; the full type is only needed in the .cpp).
namespace ork::lev2::hypermesh {
struct LiveHypermesh;
using livehypermesh_ptr_t = std::shared_ptr<LiveHypermesh>;
}

namespace ork::lev2 {

struct Context;
struct PBRMaterial;
using pbrmaterial_ptr_t = std::shared_ptr<PBRMaterial>;
struct Image;
using image_ptr_t = std::shared_ptr<Image>;

///////////////////////////////////////////////////////////////////////////////

struct AssetGenData : public ork::Object {
  DeclareAbstractX(AssetGenData, ork::Object);

public:
  AssetGenData() = default;
  ~AssetGenData() override = default;

  // Scene-side asset name (the first arg to self.asset.X("name", ...)).
  // Used by downstream gens that reference this one by name, and by the
  // Scene's asset registry for lookup. Defaults to empty for standalone
  // (Tier 1/2) use where the gen has no name.
  std::string _asset_name;
};

using assetgendata_ptr_t = std::shared_ptr<AssetGenData>;

///////////////////////////////////////////////////////////////////////////////
// ImplicitSdfGenData — reflected fields for the AX-shaded volume
// voxelizer. This is the universal reflected representation for SDF
// assets: any shape that can be expressed as an OpenVDB AX volume
// shader serializes through this type.
//
// Domain-specific Python wrappers (ThickSaddleSdf, eventually
// HollowFunnelSdf, etc.) construct an ImplicitSdfGenData with the
// right shader source + params and store it as their underlying
// gendata. They do NOT have their own reflected C++ types — the
// `shader` string carries the shape definition, and `params` carry
// the per-instance scalars.
//
// _float_params / _int_params split the Python `params` dict by value
// type so directMapProperty has a concrete value type to reflect.
///////////////////////////////////////////////////////////////////////////////

struct ImplicitSdfGenData : public AssetGenData {
  DeclareConcreteX(ImplicitSdfGenData, AssetGenData);

public:
  ImplicitSdfGenData()           = default;
  ~ImplicitSdfGenData() override = default;

  std::string     _shader;
  fvec3           _bbox_min   = fvec3(-1, -1, -1);
  fvec3           _bbox_max   = fvec3( 1,  1,  1);
  float           _voxel_size = 0.1f;
  // 0.0 = "auto" sentinel — the Python wrapper computes the
  // bbox-diagonal length and stores that BEFORE serialization, so
  // deserialized objects always carry a concrete value.
  float           _background = 0.0f;
  std::string     _grid_name  = "sdf";
  // Mixed float/int params — reflected via directVarMapProperty
  // (DirectVarMap routes through the variant-as-tagged-string codec
  // path that NODEENC<var_t>/decode_value<svar128_t> already provide).
  // Allocated lazily; DirectVarMap auto-creates it when written.
  varmap::varmap_ptr_t  _params;
};

using implicit_sdf_gendata_ptr_t = std::shared_ptr<ImplicitSdfGenData>;

///////////////////////////////////////////////////////////////////////////////
// ScatterSinkData (D.4 / review 1.10) — the reflected SCATTER CONTRACT: a mask-driven
// placement sink on a heightfield asset. Mirrors the Python authoring ScatterSpec 1:1
// (terrain base.py scatter()): one SHARED point set; per-point weighted-random pick among
// the types (mutually exclusive); type_id = declaration index. The per-type WEIGHT fields
// already ride the embedded graph as captured channels (_type_channels names them) — this
// class carries the placement parameters + the type_id→asset/material BINDING so a scene
// round-trip keeps the whole scatter story (today's live-DSL-only state lost it all).
// Placement itself (scatter.place) stays Python until the E.2 C++ placer lands.
///////////////////////////////////////////////////////////////////////////////

struct ScatterSinkData : public ork::Object {
  DeclareConcreteX(ScatterSinkData, ork::Object);

public:
  ScatterSinkData()           = default;
  ~ScatterSinkData() override = default;

  std::string _name;
  // placement amount — exactly one is meaningful (the other holds its 0 sentinel),
  // matching the authoring contract (density= points/m^2 XOR count= total points).
  float _density    = 0.0f;
  int   _count      = 0;
  int   _seed       = 0;
  std::string _align = "normal"; // "normal" (up -> surface normal) or "up" (world +Y)
  float _yaw_lo     = 0.0f;      // random rotation about the up axis, radians
  float _yaw_hi     = 6.283185307179586f;
  float _scale_lo   = 1.0f;      // uniform per-point scale, BAKED into the matrix
  float _scale_hi   = 1.0f;
  float _cutoff     = 0.0f;      // reject where total weight W < cutoff
  float _jitter     = 1.0f;      // 0..1 in-cell positional jitter
  int   _max_points = 6000000;
  float _lift       = 0.0f;      // meters along the up axis, baked into the translation

  // ordered (type_id = index): the type names + the captured weight-channel each reads.
  std::vector<std::string> _type_names;
  std::vector<std::string> _type_channels;
  // the reflected type→asset/material BINDING (asset names into the AssetSystem registry;
  // the consumer resolves mesh + material per type_id at load). Optional per type.
  std::map<std::string, std::string> _type_assets;
  std::map<std::string, std::string> _type_materials;
  // per-type PHYSICS PROXY ("kind:d0:d1:d2"; kind 0=sphere 1=capsule 2=box) — the placer
  // bakes it PER POINT into the ScatterSet (proxy_kind/proxy_dims channels), so collider
  // shape rides the DATA and BulletShapeScatter hardcodes nothing.
  std::map<std::string, std::string> _type_colliders;
};

using scattersink_data_ptr_t = std::shared_ptr<ScatterSinkData>;

///////////////////////////////////////////////////////////////////////////////
// PbrMaterialGenData — reflected recipe for a PBRMaterial. Materialize
// step (Python side) builds the live material via GfxEnv loading
// context, sets baseColor/metallic/roughness, assigns either path-
// loaded images OR procedural solid-color images (the
// shaders.createPbrMaterialWithColor default), and calls gpuInit.
//
// Empty color/normal/mtlruf paths → procedural defaults (same as the
// existing createPbrMaterialWithColor helper). Non-empty paths load
// the named image via lev2.Image.createFromFile.
///////////////////////////////////////////////////////////////////////////////

struct PbrMaterialGenData : public AssetGenData {
  DeclareConcreteX(PbrMaterialGenData, AssetGenData);

public:
  PbrMaterialGenData()           = default;
  ~PbrMaterialGenData() override = default;

  fvec4       _base_color  = fvec4(1, 1, 1, 1);
  float       _metallic    = 0.0f;
  float       _roughness   = 1.0f;
  // Texture paths — empty string means "use the procedural
  // solid-color default" (matches createPbrMaterialWithColor).
  std::string _color_path;
  std::string _normal_path;
  std::string _mtlruf_path;
  // GEOV2 Phase 3 — optional custom fxv2 (a generated ptex3d surface). Empty =
  // stock PBR shader. When set, PBRMaterial loads this instead; the procedural
  // surface supplies albedo/metallic/roughness directly (the *Factor fields are
  // then identity). Lets a Ptex3d round-trip AS a PbrMaterialGenData.
  std::string _shaderpath;
  // GEOV2 Phase 3 — bindable uniform_block params the generated shader exposes
  // (name -> default value, float/vec*). Round-trips the bindable spec; the
  // materialize step binds these defaults, and they can be overridden live via
  // PBRMaterial.bindParam(name, value).
  varmap::varmap_ptr_t _shader_params;
  // D.4 (review 1.11) — sampler→texture bindings for the generated shader's sampler2D
  // uniforms (ptex3d ctx.tex). Key = sampler uniform name; value = the texture image
  // path (alias-form allowed — expandPathString at materialize; terrain-channel paths
  // like <assetcache>/terrain/<asset>/<channel>.exr are deterministic so they're
  // machine-independent despite being "paths"). materialize() loads each image, makes
  // the texture, and bindParam()s it; missing files skip loudly (declaration order in
  // the AssetSystem is the dependency order, so bake-producing gens must come first).
  std::map<std::string, std::string> _sampler_textures;

  // load `path` as a texture and bind it to the material's sampler2D uniform `sampler`
  // (the D.4 binding, factored for reuse: PbrMaterialGenData::materialize AND the
  // E.6/2.20 terrain↔material post-pass). Missing param/file skips LOUDLY (`who` tags
  // the log). With the 2.12 rebind contract the bind is live in every cached pipeline.
  // WS1: pre-decoded variant — the caller decoded the image off-thread
  // (LoadJoinSet fan-out); this does only the GPU upload + bind (ctx thread).
  static bool bindSamplerImage(
      pbrmaterial_ptr_t mat, Context* ctx, const std::string& sampler, image_ptr_t img, const std::string& who);
  static bool bindSamplerTexture(
      pbrmaterial_ptr_t mtl, Context* ctx, const std::string& sampler, const std::string& path_in, const std::string& who);

  // PBR2 Phase 2 — 8 glTF KHR-extension lobes. Each lobe has a `has_<lobe>`
  // flag (default off) + factor; color-bearing lobes also have a color
  // vec3. Authored via DSL kwargs (snake_case). Materialize step copies
  // these onto the live PBRMaterial.
  bool  _has_transmission           = false;
  float _transmission_factor        = 0.0f;
  // P3.D — separate transmission roughness (beyond glTF spec; Filament-style).
  // When _has_transmission_roughness=false, the lobe falls back to the BRDF
  // _roughness. When true, _transmission_roughness drives the refracted env
  // sample's mip level independently — letting authors keep a sharp front
  // BRDF but get a soft / frosted refraction (or vice versa).
  bool  _has_transmission_roughness = false;
  float _transmission_roughness     = 0.0f;
  bool  _has_ior                    = false;
  float _ior                        = 1.5f;
  bool  _has_volume                 = false;
  float _volume_thickness_factor    = 0.0f;
  bool  _has_diffuse_transmission   = false;
  float _diffuse_transmission_factor = 0.0f;
  bool  _has_specular               = false;
  float _specular_factor            = 1.0f;
  bool  _has_clearcoat              = false;
  float _clearcoat_factor           = 0.0f;
  bool  _has_sheen                  = false;
  float _sheen_factor               = 0.0f;
  bool  _has_iridescence            = false;
  float _iridescence_factor         = 0.0f;
  fvec3 _sheen_color                = fvec3(0, 0, 0);
  fvec3 _specular_color             = fvec3(1, 1, 1);
  fvec3 _attenuation_color          = fvec3(1, 1, 1);
  fvec3 _diffuse_transmission_color = fvec3(1, 1, 1);
  float _clearcoat_roughness        = 0.0f;
  float _sheen_roughness            = 0.0f;
  float _attenuation_distance       = 1.0f;
  // PBR2 Phase 3 (P3.D) — KHR_materials_subsurface (in-flight ext).
  // Screen-space separable subsurface scattering. radius is per-channel
  // (mm); typical skin = (1.4, 0.5, 0.3) so R bleeds farthest. factor is
  // the blend strength between blurred and unblurred diffuse.
  bool  _has_subsurface             = false;
  fvec3 _subsurface_color           = fvec3(1, 1, 1);
  fvec3 _subsurface_radius          = fvec3(1, 1, 1);
  float _subsurface_factor          = 0.0f;

  // D.1: the C++ MATERIALIZER — a 1:1 port of the Python build() (hypergraph ecs/scene/assets.py
  // PbrMaterial): images from paths (procedural solid-color defaults otherwise), factors + the
  // PBR2 lobes mirrored, the optional generated-fxv2 _shaderpath set PRE-gpuInit (resolves via the
  // <hyperassets> alias, B.5c), the bindable _shader_params pre-bound POST-gpuInit. The Python
  // wrapper delegates here (ORK_HM_PYMAT=1 = the retained Python reference during transition).
  pbrmaterial_ptr_t materialize(Context* ctx) const;
};

using pbr_material_gendata_ptr_t = std::shared_ptr<PbrMaterialGenData>;

///////////////////////////////////////////////////////////////////////////////
// FreestyleMaterialGenData — reflected recipe for a FreestyleMaterial
// (custom shader). Carries the shader source/path + raster state +
// technique selection. Pipeline parameter bindings are intentionally
// omitted from this first slice — the binding story lands properly
// once we have a dataflow-driven shader graph; until then the Python
// wrapper sets bindings imperatively at materialize time.
///////////////////////////////////////////////////////////////////////////////

struct FreestyleMaterialGenData : public AssetGenData {
  DeclareConcreteX(FreestyleMaterialGenData, AssetGenData);

public:
  FreestyleMaterialGenData()           = default;
  ~FreestyleMaterialGenData() override = default;

  // Exactly one of _shader_text / _shader_file is meant to be set.
  // If _shader_text is non-empty, the wrapper calls
  // gpuInitFromShaderText(name=_shader_name, _shader_text); otherwise
  // it falls back to gpuInit(_shader_file).
  std::string _shader_text;
  std::string _shader_file = "orkshader://manip";
  std::string _shader_name = "x";
  // Technique + rendermodel for the FxPipelinePermutation lookup.
  std::string _technique   = "std_mono_fwd";
  std::string _rendermodel = "ForwardPBR";
  // Raster state — stored as token strings; the wrapper resolves them
  // through CrcStringProxy at materialize time.
  std::string _blending    = "OFF";
  std::string _culltest    = "PASS_FRONT";
  std::string _depthtest   = "LEQUALS";
};

using freestyle_material_gendata_ptr_t = std::shared_ptr<FreestyleMaterialGenData>;

///////////////////////////////////////////////////////////////////////////////
// VdbGridToDrawableGenData — reflected fields for a one-shot SDF → mesh
// → drawable conversion at marching-cubes iso. The input grid is
// referenced BY NAME (not by direct shared_ptr) so the gen graph
// survives JSON round-trip; the AssetSystem (M2b.4) resolves the
// name to a built artifact at materialize time.
//
// Material is NOT reflected here. Materials carry textures and
// shader params that the reflection layer doesn't yet round-trip;
// the Python MeshToDrawable wrapper holds the material externally
// and supplies it at build time. Material round-trip is a separate
// future concern.
///////////////////////////////////////////////////////////////////////////////

struct VdbGridToDrawableGenData : public AssetGenData {
  DeclareConcreteX(VdbGridToDrawableGenData, AssetGenData);

public:
  VdbGridToDrawableGenData()           = default;
  ~VdbGridToDrawableGenData() override = default;

  // Name of the AssetGenData (e.g. ImplicitSdfGenData) whose output
  // FloatGrid this drawable visualizes. Empty = grid is being supplied
  // out-of-band by the Python wrapper (eager/standalone path; no
  // round-trip resolution needed).
  std::string _grid_asset_name;
  // Name of the material asset (PbrMaterialGenData or
  // FreestyleMaterialGenData) to bind. Empty = material supplied
  // out-of-band by the Python wrapper.
  std::string _material_asset_name;
  float       _iso           = 0.0f;
  float       _adaptivity    = 0.0f;
  bool        _flip_windings = false;  // SDF default: openvdb winds negative-inside grids OUTWARD (CCW
                                       // front-face). Set true only for HIGH-inside density/fog grids (see vdb_drawable.h).
};

using vdb_grid_to_drawable_gendata_ptr_t = std::shared_ptr<VdbGridToDrawableGenData>;

///////////////////////////////////////////////////////////////////////////////
// ParticleSystemGenData — reflected pointer to a Python particle DSL file
// plus the kwargs that parameterize it. The DSL file is on the search
// path managed by ork.dflow.particles.resolve (typically ork.data/particles);
// the named class (auto-detect or _dsl_class) is loaded, instantiated with
// the recorded kwargs, and its .generatedflow() produces the dataflow
// GraphData. The Python wrapper wraps that in a ParticlesDrawableData and
// returns it as the materialized artifact.
//
// Cross-asset references that need to survive the round-trip (e.g.
// collision_sdf → the name of an ImplicitSdfGenData) live in
// _asset_kwargs; scalar params live in _params. Materialize resolves
// _asset_kwargs[key] against the in-progress artifact dict and merges
// the resolved values into the DSL ctor call.
///////////////////////////////////////////////////////////////////////////////

struct ParticleSystemGenData : public AssetGenData {
  DeclareConcreteX(ParticleSystemGenData, AssetGenData);

public:
  ParticleSystemGenData()           = default;
  ~ParticleSystemGenData() override = default;

  // DSL filename (e.g. "col_vdb") — resolved via the same path logic
  // ork.particle.viewer.py uses (resolve_dsl_file).
  std::string _dsl_file;
  // Optional explicit class name for multi-class files (matches the
  // --class arg on ork.particle.viewer.py); empty = auto-detect.
  std::string _dsl_class;
  // Scalar / vector / etc. kwargs forwarded to the DSL class ctor.
  // Tagged-variant encoded (same DirectVarMap codec as
  // ImplicitSdfGenData::_params).
  varmap::varmap_ptr_t _params;
  // Map of ctor-kwarg name → AssetSystemData asset name. Each entry
  // tells the materializer "resolve artifacts[<value>] and pass it as
  // <key>= when instantiating the DSL class." Encoded as a string-map
  // so it round-trips cleanly without needing a custom codec.
  std::map<std::string, std::string> _asset_kwargs;
  // PBR2 Phase 0 — per-particle-system reflection probe binding. The
  // probe's baked XIR (at <assetcache>/xirtemp/<entity_name>.xir) is
  // resolved by wire_scene_data and set as _environmentMapPath on the
  // particle drawable so loadEnvMapOverride picks it up at stage time.
  // Empty = no per-drawable override; falls back to scene-global skybox.
  std::string _probe_entity_name;
  // E2B item E — the per-system point light (ParticlesDrawableInst's
  // emission-tracking light): falloff radius (m) + intensity scale on the
  // published emission color. intensity 0 = light off.
  float _emitter_intensity = 1.0f;
  float _emitter_radius    = 1.0f;
  // D.2 — MODEL B: the particle GraphData EMBEDS inline (the DSL runs ONCE at authoring; reload
  // deserializes the graph with NO Python and NO DSL file). _dsl_file/_dsl_class/_params remain
  // as PROVENANCE (and as the legacy re-run fallback while graphs with non-serializable inputs
  // — live SDF grids etc. — still need it). Null = a legacy model-A gendata.
  ork::dataflow::graphdata_ptr_t _graph_data;
};

using particle_system_gendata_ptr_t = std::shared_ptr<ParticleSystemGenData>;

///////////////////////////////////////////////////////////////////////////////
// HeightFieldGenData — terrain heightfield asset. Unlike ParticleSystemGenData
// (which stores a DSL filename + re-runs Python at load), this EMBEDS the
// serialized terrain compute GraphData inline (model B): the DSL runs ONCE at
// authoring to produce the graph; the graph round-trips in the scene JSON, so
// reload deserializes + bakes with NO Python and NO DSL file. _dimension is the
// bake grid (W=H). Output channels are self-described by the graph's
// CaptureModules (each carries a `_channel`); their on-disk paths are derived at
// materialize time (machine-specific, never serialized). The bake is
// cook-cache-backed, so a re-materialize of an unchanged graph is all hits.
///////////////////////////////////////////////////////////////////////////////

struct HeightFieldGenData : public AssetGenData {
  DeclareConcreteX(HeightFieldGenData, AssetGenData);

public:
  HeightFieldGenData()           = default;
  ~HeightFieldGenData() override = default;

  ork::dataflow::graphdata_ptr_t _graph_data; // the embedded terrain graph (serialized inline)
  // D.4 (review 1.10) — the reflected scatter sinks (placement params + type bindings).
  // The weight fields they reference are captured channels INSIDE _graph_data, so the
  // whole scatter story round-trips with the scene; placement runs post-bake from these.
  std::vector<scattersink_data_ptr_t> _scatters;
  int _dimension = 512;                       // bake grid resolution (W=H), texels
  // WORLD units make the graph resolution-INDEPENDENT: spatial op params (slope/
  // curvature radius, ...) are in meters and converted to texels per-bake. _dimension
  // is purely a sampling rate; the terrain's horizontal meaning is extent_m. Heights
  // are TRUE METERS end-to-end (baked EXR + drawable SSBO) — no normalized-height scale.
  float _extent_m       = 4096.0f;    // horizontal world size (meters across the field)

  // E.6/2.20 — the terrain↔material CONTRACT, owned by the TERRAIN asset (single
  // source of truth): which material shades this terrain (+ which baked channels
  // feed which of its sampler uniforms). materializeAll's post-pass derives each
  // channel's deterministic <assetcache> path and binds it onto the resolved
  // material — no hand-synced path strings on the material side. The physical
  // params leg of the contract is the pair above (dimension/extent_m), already
  // reflected + carried by the .terrain.json manifest (heights are TRUE METERS).
  std::string _material_asset;                          // shading material asset name ("" = none)
  std::map<std::string, std::string> _channel_samplers; // baked channel -> sampler uniform name

  // the deterministic baked-channel artifact path (<assetcache>/terrain/<name>/<channel>.<ext>)
  std::string channelPath(const std::string& channel, const std::string& ext = "exr") const;

  // D.1: the C++ MATERIALIZER — paths + bake + manifest around the (already-C++) terrain bake,
  // a 1:1 port of the Python build(): derives per-channel output paths under
  // <assetcache>/terrain/<asset_name> onto the graph's CaptureModules (machine-specific, never
  // serialized), runs bakeHeightfield (cook-cache-backed), and writes the self-describing
  // .terrain.json scale-contract manifest. Returns the manifest path (the manifest carries the
  // channel files + stats — it IS the result contract consumers read). `ext` = "exr" | "png".
  // Scatter sinks are AUTHORING-only (live DSL state) and stay Python-side by design.
  std::string materialize(Context* ctx, const std::string& ext = "exr") const;
};
using heightfield_gendata_ptr_t = std::shared_ptr<HeightFieldGenData>;

///////////////////////////////////////////////////////////////////////////////
// HypermeshGenData — GPU mesh-graph asset (HyperSyn `hypermesh` family), model B
// like HeightFieldGenData: the hypermesh compute GraphData EMBEDS inline (the DSL
// runs ONCE at authoring; reload deserializes the graph with NO Python). The
// materialized artifact is a LiveHypermesh — the persistent GraphInst + GPU mesh
// the render path (HypermeshDrawableData / a future shared-mesh instancer)
// consumes. _vtx_budget caps the GraphInst SSBO pool's vertex capacity.
///////////////////////////////////////////////////////////////////////////////

struct HypermeshGenData : public AssetGenData {
  DeclareConcreteX(HypermeshGenData, AssetGenData);

public:
  HypermeshGenData()           = default;
  ~HypermeshGenData() override = default;

  ork::dataflow::graphdata_ptr_t _graph_data; // the embedded hypermesh graph (serialized inline)
  std::string _dsl_file;                      // provenance only (never re-run at load)
  int _vtx_budget = 1 << 20;                  // GraphInst pool vertex capacity

  // D.3: the C++ MATERIALIZER — hypermesh::materializeLive on the embedded graph
  // (sort + instantiate + pool; first recompute compiles/allocs). Pure C++ end to end.
  hypermesh::livehypermesh_ptr_t materialize(Context* ctx) const;
};

using hypermesh_gendata_ptr_t = std::shared_ptr<HypermeshGenData>;

///////////////////////////////////////////////////////////////////////////////
// HdriToXirGenData — reflected recipe for a static HDR-to-XIR conversion.
// The source HDR/PNG/EXR is fed through EnvMapProcessor's prefilter
// cascade and the resulting prefiltered cube datablock is written to a
// per-asset .xir under <assetcache>/xirtemp/. NOT a scene-rendered
// probe — there's no scene awareness here, just an offline image bake.
// Compare with ProbeComponent which renders the *scene* into a cube.
// Consumers: SceneGraph SkyboxTexPathStr (via wire_scene_data) and any
// drawable-level envmap override that points at the resulting .xir.
///////////////////////////////////////////////////////////////////////////////

struct HdriToXirGenData : public AssetGenData {
  DeclareConcreteX(HdriToXirGenData, AssetGenData);

public:
  HdriToXirGenData()           = default;
  ~HdriToXirGenData() override = default;

  std::string _source_path;   // input HDR/EXR/PNG path (absolute or aliased)
  int         _levels   = 6;
  int         _samples  = 128;
  float       _scale    = 1.0f;
  float       _clamp    = 16.0f;
};

using hdri_to_xir_gendata_ptr_t = std::shared_ptr<HdriToXirGenData>;

///////////////////////////////////////////////////////////////////////////////
// VdbFileSdfGenData — load a pre-baked .vdb file at runtime via
// openvdb::io::File and yield its FloatGrid. The recipe is just the
// file path + the grid name to extract; everything else is in the .vdb
// itself (voxel size, transform, narrow-band width). Counterpart to
// ImplicitSdfGenData for the "I already have an SDF on disk" case.
///////////////////////////////////////////////////////////////////////////////

struct VdbFileSdfGenData : public AssetGenData {
  DeclareConcreteX(VdbFileSdfGenData, AssetGenData);

public:
  VdbFileSdfGenData()           = default;
  ~VdbFileSdfGenData() override = default;

  std::string _vdb_path;
  std::string _grid_name = "sdf";
};

using vdb_file_sdf_gendata_ptr_t = std::shared_ptr<VdbFileSdfGenData>;

///////////////////////////////////////////////////////////////////////////////
// MeshSdfGenData — voxelize a triangle mesh loaded from disk into a
// narrow-band SDF. Counterpart to ImplicitSdfGenData for the "I have a
// closed mesh and want it as an SDF" case. Materialize step loads the
// mesh via Assimp, then runs openvdb::tools::meshToLevelSet.
///////////////////////////////////////////////////////////////////////////////

struct MeshSdfGenData : public AssetGenData {
  DeclareConcreteX(MeshSdfGenData, AssetGenData);

public:
  MeshSdfGenData()           = default;
  ~MeshSdfGenData() override = default;

  std::string _mesh_path;
  float       _voxel_size = 0.1f;
  float       _half_width = 3.0f;
  std::string _grid_name  = "sdf";
};

using mesh_sdf_gendata_ptr_t = std::shared_ptr<MeshSdfGenData>;

///////////////////////////////////////////////////////////////////////////////
// MeshGenData — generic BAKED-GEOMETRY triangle mesh asset.
//
// Unlike ImplicitSdfGenData (which carries a recipe — an AX shader + params —
// and regenerates its grid at materialize), this carries fully evaluated
// triangle geometry. The geometry itself is NOT inlined into the reflected
// JSON (that would bloat the scene with thousands of floats per mesh);
// instead it is baked to a sidecar geometry chunkfile under the asset cache
// (analogous to HdriToXir's .xir), and only the PATH to that chunkfile is
// reflected here. The chunkfile is written from Python at author time and
// read back in C++ at materialize/load time (see meshutil mesh-geometry
// chunk codec).
//
// Material is referenced BY NAME (same convention as VdbGridToDrawableGenData):
// the live PBRMaterial isn't reflected, so the Python wrapper captures the
// material asset's name and the materializer resolves it from the in-progress
// artifact dict at load time.
///////////////////////////////////////////////////////////////////////////////

struct MeshGenData : public AssetGenData {
  DeclareConcreteX(MeshGenData, AssetGenData);

public:
  MeshGenData()           = default;
  ~MeshGenData() override = default;

  // Path to the baked geometry chunkfile (positions/normals/binormals/indices).
  std::string _geometry_path;
  std::string _material_asset_name;
};

using mesh_gendata_ptr_t = std::shared_ptr<MeshGenData>;

} // namespace ork::lev2
