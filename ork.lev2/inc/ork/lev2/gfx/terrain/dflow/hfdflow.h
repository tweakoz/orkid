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

struct BakeEnv {
  Context* _ctx = nullptr;
  int _w        = 0;
  int _h        = 0;
  std::vector<CaptureRequest> _captures; // collected during compute, flushed after submit
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

  float _frequency = 4.0f;
  int _octaves     = 5;
  float _amplitude = 1.0f;
};
using fbmmoduledata_ptr_t = std::shared_ptr<FbmModuleData>;

///////////////////////////////////////////////////////////////////////////////
// CaptureModule — sink. Input "In" : GpuComputeImage2D. At bake-flush time the
// source SSBO is read back and encoded to `_path` (PNG/EXR by extension).
///////////////////////////////////////////////////////////////////////////////

struct CaptureModuleData : public TerrainModuleData {
  DeclareConcreteX(CaptureModuleData, TerrainModuleData);
  CaptureModuleData();
  static std::shared_ptr<CaptureModuleData> createShared();
  dflow::dgmoduleinst_ptr_t createInstance(dflow::GraphInst* ginst) const final;

  ork::file::Path _path;
};
using capturemoduledata_ptr_t = std::shared_ptr<CaptureModuleData>;

///////////////////////////////////////////////////////////////////////////////
// Driver — sort, instantiate, run the compute, flush captures. `dim` is the
// square grid resolution (W=H=dim).
///////////////////////////////////////////////////////////////////////////////

void bakeHeightfield(dflow::graphdata_ptr_t graph, Context* ctx, int dim);

// first-slice convenience: build a 2-node fbm -> capture graph and bake it to
// `outpath` (PNG/EXR by extension), the minimal end-to-end exerciser.
void bakeHeightfieldTest(Context* ctx, const ork::file::Path& outpath, int dim);

} // namespace ork::lev2::terrain
