#include <ork/lev2/gfx/scenegraph/sgnode_projectedgrid.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::ProjectedGridDrawableData, "ProjectedGridDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct ProjectedGridRenderImpl {

  using vb_t = StaticVertexBuffer<VtxV12T8>;

  ProjectedGridRenderImpl(const ProjectedGridDrawableData* gpd)
      : _grounddata(gpd) {
  }
  ~ProjectedGridRenderImpl() {
  }

  void gpuInit(lev2::Context* ctx) {

    if (_grounddata->_pipeline_color) {
      _pipeline_color = _grounddata->_pipeline_color;
    } else if (_grounddata->_material) {
      _pbrmaterial = _grounddata->_material;
      _fxcache     = _pbrmaterial->pipelineCache();
    } else {
      _pbrmaterial = std::make_shared<PBRMaterial>();
      _pbrmaterial->gpuInit(ctx);
      _pbrmaterial->_metallicFactor  = 0.0f;
      _pbrmaterial->_roughnessFactor = 1.0f;
      _pbrmaterial->_baseColor       = fvec3(1, 1, 1);
      _pbrmaterial->_doubleSided     = true;
      _fxcache                       = _pbrmaterial->pipelineCache();
    }

    _initted = true;
  }

  void _buildStaticVB(lev2::Context* ctx) {
    // snapshot the params we built with, so we can detect stale rebuilds
    float lod0_cs = _grounddata->_lod0_cell_size;
    int   M       = _grounddata->_lod0_cells;
    int   L       = _grounddata->_lod_count;

    if (lod0_cs <= 0.0f) lod0_cs = 0.5f;
    if (M < 4) M = 4;
    M &= ~3; // force multiple of 4 so the hole is on cell boundaries
    if (L < 1) L = 1;

    _built_lod0_cs    = lod0_cs;
    _built_lod0_cells = M;
    _built_lod_count  = L;

    // cell count: LOD 0 is solid, LODs 1..L-1 are annuli (M*M - (M/2)*(M/2))
    const int full_cells   = M * M;
    const int inner_cells  = (M / 2) * (M / 2);
    const int ring_cells   = full_cells - inner_cells;
    const int total_cells  = full_cells + ring_cells * (L - 1);
    const int num_verts    = 6 * total_cells;

    _vb = std::make_shared<vb_t>(num_verts, num_verts);
    _vb->SetRingLock(false);

    VtxWriter<VtxV12T8> vw;
    vw.Lock(ctx, _vb.get(), num_verts);

    const int half     = M / 2;
    const int inner_hi = M / 4;  // hole is [-inner_hi, +inner_hi) in grid indices

    for (int k = 0; k < L; ++k) {
      const float cs        = lod0_cs * float(1 << k);
      const bool  has_hole  = (k > 0);
      const float lod_debug = float(k) / float(L > 1 ? (L - 1) : 1);

      for (int iz = -half; iz < half; ++iz) {
        for (int ix = -half; ix < half; ++ix) {
          if (has_hole) {
            if (ix >= -inner_hi && ix < inner_hi && iz >= -inner_hi && iz < inner_hi) {
              continue;
            }
          }

          const float x0 = float(ix) * cs;
          const float x1 = x0 + cs;
          const float z0 = float(iz) * cs;
          const float z1 = z0 + cs;

          VtxV12T8 v_tl(x0, 0.0f, z0, lod_debug, 0.0f);
          VtxV12T8 v_tr(x1, 0.0f, z0, lod_debug, 0.0f);
          VtxV12T8 v_br(x1, 0.0f, z1, lod_debug, 0.0f);
          VtxV12T8 v_bl(x0, 0.0f, z1, lod_debug, 0.0f);

          vw.AddVertex(v_tl);
          vw.AddVertex(v_br);
          vw.AddVertex(v_tr);
          vw.AddVertex(v_tl);
          vw.AddVertex(v_bl);
          vw.AddVertex(v_br);
        }
      }
    }

    vw.UnLock(ctx, EULFLG_ASSIGNVBLEN);
    _num_verts = num_verts;
  }

  void _render(const RenderContextInstData& RCID) {

    auto context = RCID.context();

    if (not _initted) {
      gpuInit(context);
    }
    if (not _vb
        or _built_lod0_cs    != _grounddata->_lod0_cell_size
        or _built_lod0_cells != _grounddata->_lod0_cells
        or _built_lod_count  != _grounddata->_lod_count) {
      _buildStaticVB(context);
    }

    bool isPickState      = context->FBI()->isPickState();
    auto RCFD             = RCID.rcfd();
    bool is_depth_prepass = RCFD->_renderingmodel._modelID == "DEPTH_PREPASS"_crcu;

    auto mtxi = context->MTXI();
    auto gbi  = context->GBI();
    mtxi->PushMMatrix(fmtx4::Identity());

    fvec4 modcolor = fcolor4::White();
    if (isPickState) {
      modcolor.setRGBAU64(uint64_t(0xffffffffffffffff));
    }
    context->PushModColor(modcolor);

    fxpipeline_ptr_t pipeline = nullptr;

    if (_pipeline_color and not is_depth_prepass) {
      pipeline = _pipeline_color;
    } else {
      OrkAssert(_fxcache);
      pipeline = _fxcache->findPipeline(RCID);
    }
    OrkAssert(pipeline);

    pipeline->wrappedDrawCall(RCID, [&]() { //
      gbi->DrawPrimitiveEML(*_vb, PrimitiveType::TRIANGLES, 0, _num_verts);
    });

    context->PopModColor();
    mtxi->PopMMatrix();
  }

  static void renderProjectedGrid(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    renderable->GetDrawableDataA().getShared<ProjectedGridRenderImpl>()->_render(RCID);
  }

  const ProjectedGridDrawableData* _grounddata;
  pbrmaterial_ptr_t _pbrmaterial;
  fxpipeline_ptr_t _pipeline_color;
  fxpipelinecache_constptr_t _fxcache;

  std::shared_ptr<vb_t> _vb;
  float _built_lod0_cs    = -1.0f;
  int   _built_lod0_cells = -1;
  int   _built_lod_count  = -1;
  int   _num_verts        = 0;
  bool  _initted          = false;
};

///////////////////////////////////////////////////////////////////////////////

void ProjectedGridDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t ProjectedGridDrawableData::createDrawable() const {
  auto rval = std::make_shared<CallbackDrawable>(nullptr);
  rval->SetRenderCallback(ProjectedGridRenderImpl::renderProjectedGrid);
  auto impl       = rval->_implA.makeShared<ProjectedGridRenderImpl>(this);
  rval->_sortkey  = 10;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

ProjectedGridDrawableData::ProjectedGridDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

ProjectedGridDrawableData::~ProjectedGridDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
