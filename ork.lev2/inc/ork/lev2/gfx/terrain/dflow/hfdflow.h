////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// Heightfield compute-dataflow (HyperSyn `terrain` family, bake side).
//
// A serializable ork::dataflow GraphData whose modules dispatch COMPUTE shaders
// to produce 2D float-field channels (height, slope, masks, ...), then read the
// result back and encode PNG/EXR. This is a BAKE tool — it runs once at
// materialization; there is no runtime GraphInst. See the hypersyn skill.
//
// First slice: the `GpuComputeImage2D` plug type (R32F, SSBO-backed — Vulkan
// storage-image binding for compute isn't implemented yet, so a std430 float[]
// SSBO addressed buf[y*W+x] is the working backing), a `TerrainModuleData` base,
// an `FbmModule` generator, a `CaptureModule` sink (readback -> writeToFile), and
// a `bakeHeightfield()` driver building+sorting+computing a 2-node graph.
//
////////////////////////////////////////////////////////////////
#pragma once

#include <ork/dataflow/all.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/file/path.h>
#include <vector>
#include <memory>

namespace ork::lev2::terrain {

namespace dflow = ::ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
// GpuComputeImage2D — the value carried on a heightfield-channel plug. A 2D
// float field of WxH, backed (for now) by an SSBO. `_channels` selects R/RG/RGBA
// (1/2/4 floats per texel); only R (1ch) is exercised by the first slice.
///////////////////////////////////////////////////////////////////////////////

struct GpuComputeImage2DData {
  int _channels = 1; // 1=R32F, 2=RG32F, 4=RGBA32F (3 packs into 4)
};
using gpucomputeimage2d_data_ptr_t = std::shared_ptr<GpuComputeImage2DData>;

struct GpuComputeImage2DInst {
  GpuComputeImage2DInst(gpucomputeimage2d_data_ptr_t data)
      : _data(data) {
  }
  gpucomputeimage2d_data_ptr_t _data;
  FxShaderStorageBuffer* _ssbo = nullptr; // the backing buffer (lazily allocated)
  int _w                       = 0;
  int _h                       = 0;
  int _channels                = 1;
};
using gpucomputeimage2d_inst_ptr_t = std::shared_ptr<GpuComputeImage2DInst>;

struct HfImagePlugTraits {
  using elemental_data_type          = GpuComputeImage2DData;
  using elemental_inst_type          = GpuComputeImage2DInst;
  using data_impl_type_t             = GpuComputeImage2DData;
  using inst_impl_type_t             = GpuComputeImage2DInst;
  using xformer_t                    = dflow::nullpassthrudata;
  using range_type                   = no_range;
  using out_traits_t                 = HfImagePlugTraits;
  static constexpr size_t max_fanout = 0; // a channel may feed many consumers
  static gpucomputeimage2d_inst_ptr_t data_to_inst(gpucomputeimage2d_data_ptr_t inp);
};

using hfimg_inplugdata_t      = dflow::inplugdata<HfImagePlugTraits>;
using hfimg_outplugdata_t     = dflow::outplugdata<HfImagePlugTraits>;
using hfimg_inpluginst_t      = dflow::inpluginst<HfImagePlugTraits>;
using hfimg_outpluginst_t     = dflow::outpluginst<HfImagePlugTraits>;
using hfimg_inpluginst_ptr_t  = std::shared_ptr<hfimg_inpluginst_t>;
using hfimg_outpluginst_ptr_t = std::shared_ptr<hfimg_outpluginst_t>;

///////////////////////////////////////////////////////////////////////////////
// BakeEnv — per-bake environment stashed on the GraphInst _impl so every module's
// compute() can reach the gfx Context + grid resolution.
///////////////////////////////////////////////////////////////////////////////

struct CaptureRequest {
  gpucomputeimage2d_inst_ptr_t _img; // the SOURCE field (resolved from the connected output)
  ork::file::Path _path;
};

// min/max/mean of a captured field — returned by the driver so callers (the
// self-test) can assert against analytically-known expectations.
struct FieldStats {
  float _min  = 0.0f;
  float _max  = 0.0f;
  float _mean = 0.0f;
};
using fieldstats_ptr_t = std::shared_ptr<FieldStats>;

struct BakeEnv {
  Context* _ctx           = nullptr;
  int _w                  = 0;
  int _h                  = 0;
  // world units (make the graph resolution-independent): spatial op params are in
  // meters and converted to texels here per-bake. texelsPerMeter() == dim / extent.
  float _extent_m         = 4096.0f;  // horizontal world size (meters across the field)
  float _height_scale_m   = 9830.25f; // what normalized height 1.0 means in meters
  std::vector<CaptureRequest> _captures; // collected during compute, flushed after submit

  float texelsPerMeter() const { return (_extent_m > 0.0f) ? (float(_w) / _extent_m) : 1.0f; }
  // meters -> texel radius, clamped to [1, dim/4] (sub-texel features can't be
  // resolved; an over-large kernel would border-out the whole field).
  int radiusTexels(float radius_m) const {
    int r = int(radius_m * texelsPerMeter() + 0.5f);
    if (r < 1) r = 1;
    int rmax = _w / 4;
    if (rmax < 1) rmax = 1;
    if (r > rmax) r = rmax;
    return r;
  }
};
using bakeenv_ptr_t = std::shared_ptr<BakeEnv>;

///////////////////////////////////////////////////////////////////////////////
// Module base
///////////////////////////////////////////////////////////////////////////////

struct TerrainModuleData : public dflow::DgModuleData {
  DeclareAbstractX(TerrainModuleData, dflow::DgModuleData);
  TerrainModuleData();
};
using terrainmoduledata_ptr_t = std::shared_ptr<TerrainModuleData>;

///////////////////////////////////////////////////////////////////////////////
// FbmModule — generator. Output "Out" : GpuComputeImage2D (R32F). Dispatches a
// compute shader writing fbm into the SSBO (dim + params baked into the text).
///////////////////////////////////////////////////////////////////////////////

struct FbmModuleData : public TerrainModuleData {
  DeclareConcreteX(FbmModuleData, TerrainModuleData);
  FbmModuleData();
  static std::shared_ptr<FbmModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  // float input plugs: "frequency", "amplitude". `octaves` is a baked loop bound.
  int _octaves = 5;
};
using fbmmoduledata_ptr_t = std::shared_ptr<FbmModuleData>;

///////////////////////////////////////////////////////////////////////////////
// RemapModule — 1-in elementwise: Out = clamp(In*scale + bias, lo, hi). The
// canary for input-reading modules (proves the multi-SSBO bind + storageBarrier).
///////////////////////////////////////////////////////////////////////////////

struct RemapModuleData : public TerrainModuleData {
  DeclareConcreteX(RemapModuleData, TerrainModuleData);
  RemapModuleData();
  static std::shared_ptr<RemapModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
  // image input "In"; float input plugs: "scale","bias","lo","hi". output "Out".
};
using remapmoduledata_ptr_t = std::shared_ptr<RemapModuleData>;

///////////////////////////////////////////////////////////////////////////////
// ConstModule — generator. Out = constant "level" (float plug). 1 SSBO.
///////////////////////////////////////////////////////////////////////////////

struct ConstModuleData : public TerrainModuleData {
  DeclareConcreteX(ConstModuleData, TerrainModuleData);
  ConstModuleData();
  static std::shared_ptr<ConstModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using constmoduledata_ptr_t = std::shared_ptr<ConstModuleData>;

///////////////////////////////////////////////////////////////////////////////
// GradientModule — generator. Out = dot(uv, dir)*scale + bias. 1 SSBO.
// vec2 plug: dir (the ramp direction). float plugs: scale, bias.
///////////////////////////////////////////////////////////////////////////////

struct GradientModuleData : public TerrainModuleData {
  DeclareConcreteX(GradientModuleData, TerrainModuleData);
  GradientModuleData();
  static std::shared_ptr<GradientModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using gradientmoduledata_ptr_t = std::shared_ptr<GradientModuleData>;

///////////////////////////////////////////////////////////////////////////////
// CombineModule — 2-in: Out = op(A, B). `op` (ctor, baked): 0=add 1=sub 2=mul
// 3=min 4=max 5=mix. float plug "t" = mix factor. 3 SSBOs (out + A + B).
///////////////////////////////////////////////////////////////////////////////

enum class CombineOp { ADD = 0, SUB, MUL, MIN, MAX, MIX };

struct CombineModuleData : public TerrainModuleData {
  DeclareConcreteX(CombineModuleData, TerrainModuleData);
  CombineModuleData();
  static std::shared_ptr<CombineModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  int _op = int(CombineOp::ADD); // baked (selects the GLSL expression)
};
using combinemoduledata_ptr_t = std::shared_ptr<CombineModuleData>;

///////////////////////////////////////////////////////////////////////////////
// TerraceModule — 1-in: quantize to `steps` plateaus with a `sharpness` riser.
// float plugs: steps, sharpness. 2 SSBOs.
///////////////////////////////////////////////////////////////////////////////

struct TerraceModuleData : public TerrainModuleData {
  DeclareConcreteX(TerraceModuleData, TerrainModuleData);
  TerraceModuleData();
  static std::shared_ptr<TerraceModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using terracemoduledata_ptr_t = std::shared_ptr<TerraceModuleData>;

///////////////////////////////////////////////////////////////////////////////
// SlopeModule — 1-in mask generator ("Mask by Feature: slope"). Out is the
// gradient magnitude of In, soft-rolled-off (Reinhard m/(1+m)) into [0,1). The
// gradient is PRE-BLURRED: a difference of box averages offset by +/-`_radius`
// (each box radius ~radius/2), so it tracks LANDFORM slope at the chosen scale
// instead of per-pixel noise. float plug: scale (sensitivity). An edge ring of
// width (radius + box) is 0. A mask is just a [0,1] field on the SAME plug type
// — apply it compositionally via MaskBlend/mix.
///////////////////////////////////////////////////////////////////////////////

struct SlopeModuleData : public TerrainModuleData {
  DeclareConcreteX(SlopeModuleData, TerrainModuleData);
  SlopeModuleData();
  static std::shared_ptr<SlopeModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  float _radius_m = 8.0f; // baked: pre-blur / gradient-baseline scale (METERS)
};
using slopemoduledata_ptr_t = std::shared_ptr<SlopeModuleData>;

///////////////////////////////////////////////////////////////////////////////
// CurvatureModule — 1-in mask generator ("Mask by Feature: curvature"). Curvature
// is a band-pass of In: difference of two box averages (inner radius ~radius/2,
// outer `_radius`) — a difference-of-box / Laplacian-of-Gaussian that is inherently
// PRE-BLURRED, so it tracks LANDFORM ridges/valleys at the chosen scale instead of
// per-pixel noise. `_mode` selects the flavor: CONVEX = ridges/peaks, CONCAVE =
// valleys/pits, MAGNITUDE = both. A SOFT (Reinhard m/(1+m)) rolloff maps it to [0,1)
// so curvature magnitude survives (no hard clamp -> no binary speckle). float plug:
// scale (sensitivity). Edge ring of width `_radius` is 0 (undefined at the border).
///////////////////////////////////////////////////////////////////////////////

enum class CurvatureMode { CONVEX = 0, CONCAVE, MAGNITUDE };

struct CurvatureModuleData : public TerrainModuleData {
  DeclareConcreteX(CurvatureModuleData, TerrainModuleData);
  CurvatureModuleData();
  static std::shared_ptr<CurvatureModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  int _mode       = int(CurvatureMode::MAGNITUDE); // baked (selects the GLSL output)
  float _radius_m = 96.0f;                          // baked: pre-blur / curvature scale (METERS)
};
using curvaturemoduledata_ptr_t = std::shared_ptr<CurvatureModuleData>;

///////////////////////////////////////////////////////////////////////////////
// MaskBlendModule — 3-in per-texel blend: Out = mix(A, B, M). The masking
// PRIMITIVE (Houdini's lerp(input, op(input), mask)); M is a [0,1] field. Unlike
// CombineModule's MIX (a uniform-scalar t), this blends by a per-texel field. 4 SSBOs.
///////////////////////////////////////////////////////////////////////////////

struct MaskBlendModuleData : public TerrainModuleData {
  DeclareConcreteX(MaskBlendModuleData, TerrainModuleData);
  MaskBlendModuleData();
  static std::shared_ptr<MaskBlendModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;
};
using maskblendmoduledata_ptr_t = std::shared_ptr<MaskBlendModuleData>;

///////////////////////////////////////////////////////////////////////////////
// CaptureModule — sink. Input "In" : GpuComputeImage2D. At bake-flush time the
// source SSBO is read back and encoded to `_path` (PNG/EXR by extension).
///////////////////////////////////////////////////////////////////////////////

struct CaptureModuleData : public TerrainModuleData {
  DeclareConcreteX(CaptureModuleData, TerrainModuleData);
  CaptureModuleData();
  static std::shared_ptr<CaptureModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  // stable output-channel identity (e.g. "height", "slope") — REFLECTED, so the
  // serialized graph is self-describing (the bake / dflow editor know the sinks).
  std::string _channel;
  // bake-time absolute output path — machine-specific, derived from _channel at
  // materialize time, deliberately NOT reflected (would not be portable).
  ork::file::Path _path;
};
using capturemoduledata_ptr_t = std::shared_ptr<CaptureModuleData>;

///////////////////////////////////////////////////////////////////////////////
// Driver — sort, instantiate, run the compute, flush captures. `dim` is the
// square grid resolution (W=H=dim).
///////////////////////////////////////////////////////////////////////////////

std::vector<fieldstats_ptr_t> bakeHeightfield(
    dflow::graphdata_ptr_t graph,
    Context* ctx,
    int dim,
    float extent_m       = 4096.0f,    // horizontal world size (meters) -> resolution independence
    float height_scale_m = 9830.25f);  // normalized 1.0 in meters (for real slope angles)

// first-slice convenience: build a 2-node fbm -> capture graph and bake it to
// `outpath` (PNG/EXR by extension), the minimal end-to-end exerciser.
void bakeHeightfieldTest(Context* ctx, const ork::file::Path& outpath, int dim);

// bread-and-butter op self-test: bakes Const/Gradient/Combine(6 ops)/Terrace
// graphs over Const inputs and asserts each field's min/max/mean against the
// analytically-known result. Returns the number of FAILED cases (0 == all pass).
// Writes each field to /tmp/terrain_selftest_<case>.exr for visual inspection.
int terrainOpsSelfTest(Context* ctx, int dim);

// serialize -> deserialize -> bake round-trip gate: proves a terrain GraphData
// survives JSON round-trip with no loss (baked scalars _octaves/_op, float plug
// values, connections) and bakes identical stats. Returns FAILED-check count.
int terrainRoundTripTest(Context* ctx, int dim);

// per-node cook-cache gate: bake a cacheable graph twice; the warm bake must
// load every compute node's field from the DataBlockCache (content-addressed)
// and produce a byte-identical result. Returns FAILED-check count.
int terrainCacheTest(Context* ctx, int dim);

} // namespace ork::lev2::terrain
