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
    int griddim = _grounddata->_griddim;
    if (griddim < 1) {
      griddim = 1;
    }
    _built_griddim = griddim;

    const int num_cells = griddim * griddim;
    const int num_verts = 6 * num_cells;

    _vb = std::make_shared<vb_t>(num_verts, num_verts);
    _vb->SetRingLock(false);

    VtxWriter<VtxV12T8> vw;
    vw.Lock(ctx, _vb.get(), num_verts);

    const float inv_gd = 1.0f / float(griddim);

    for (int iz = 0; iz < griddim; ++iz) {
      for (int ix = 0; ix < griddim; ++ix) {
        float u0 = float(ix) * inv_gd;
        float u1 = u0 + inv_gd;
        float v0 = float(iz) * inv_gd;
        float v1 = v0 + inv_gd;

        VtxV12T8 v_tl(u0, 0.0f, v0, u0, v0);
        VtxV12T8 v_tr(u1, 0.0f, v0, u1, v0);
        VtxV12T8 v_br(u1, 0.0f, v1, u1, v1);
        VtxV12T8 v_bl(u0, 0.0f, v1, u0, v1);

        vw.AddVertex(v_tl);
        vw.AddVertex(v_br);
        vw.AddVertex(v_tr);
        vw.AddVertex(v_tl);
        vw.AddVertex(v_bl);
        vw.AddVertex(v_br);
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
    if (not _vb or _built_griddim != _grounddata->_griddim) {
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
  int _built_griddim = -1;
  int _num_verts     = 0;
  bool _initted      = false;
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
