#include <ork/lev2/gfx/scenegraph/sgnode_terclipmap.h>
#include <ork/lev2/gfx/meshutil/terclipmap.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::ClipMapDrawableData, "ClipMapDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct ClipMapRenderImpl {

  ClipMapRenderImpl(const ClipMapDrawableData* gpd) : _grounddata(gpd) {

  }
  ~ClipMapRenderImpl(){
  }
  void gpuInit(lev2::Context* ctx) {

    using namespace meshutil;

    auto params = std::make_shared<terclipmap::Parameters>();
    params->_levels = 5;
    params->_scale = 2;
    params->_gridSize = 16;

    auto generator = std::make_shared<terclipmap::Generator>(params);

    auto mesh = generator->generateClipmaps();
    auto subm = mesh->submeshFromGroupName("clipmappolys");
    
    printf( "generating primitive...\n");

    _mesh_primitive = std::make_shared<rigidprim_SVtxV12N12T16_t>();
    _mesh_primitive->fromSubMesh(*subm,ctx);

    if( _grounddata->_material ){
        _pbrmaterial = _grounddata->_material;
        _fxcache = _pbrmaterial->pipelineCache();
    }
    else{
        _pbrmaterial       = std::make_shared<PBRMaterial>();
        _pbrmaterial->gpuInit(ctx);
        _pbrmaterial->_metallicFactor  = 0.0f;
        _pbrmaterial->_roughnessFactor = 1.0f;
        _pbrmaterial->_baseColor       = fvec3(1, 1, 1);
        _pbrmaterial->_doubleSided = true;
        
        _fxcache = _pbrmaterial->pipelineCache();
    }

    _initted                   = true;
  }
  void _render(const RenderContextInstData& RCID){

    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    auto context    = RCID.context();

    if (not _initted){
      gpuInit(context);
    }

    bool isPickState = context->FBI()->isPickState();

    const RenderContextFrameData* RCFD = RCID._RCFD;

    bool is_depth_prepass = RCFD->_renderingmodel._modelID=="DEPTH_PREPASS"_crcu;

    const auto& CPD  = RCFD->topCPD();

    auto mtxi = context->MTXI();
    auto gbi  = context->GBI();
    mtxi->PushMMatrix(fmtx4::Identity());
    fvec4 modcolor = fcolor4::White();
    if (isPickState) {
      auto pickBuf = context->FBI()->currentPickBuffer();
      //uint64_t pid = pickBuf ? pickBuf->AssignPickId((ork::Object*)nullptr) : 0;
      modcolor.setRGBAU64(uint64_t(0xffffffffffffffff));
    }
    context->PushModColor(modcolor);

    auto pipeline = _fxcache->findPipeline(RCID);
    OrkAssert(pipeline);

    pipeline->wrappedDrawCall(RCID, [&]() {
      //gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, 6);
      _mesh_primitive->renderEML(context);
    });

    context->PopModColor();
    mtxi->PopMMatrix();
  }
  static void renderClipMap(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    renderable->GetDrawableDataA().getShared<ClipMapRenderImpl>()->_render(RCID);
  }
  const ClipMapDrawableData* _grounddata;
  pbrmaterial_ptr_t _pbrmaterial;

  texture_ptr_t _colortexture;
  fxpipelinecache_constptr_t _fxcache;
  meshutil::rigidprim_SVtxV12N12T16_ptr_t _mesh_primitive;
  bool _initted = false;

};

///////////////////////////////////////////////////////////////////////////////

void ClipMapDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t ClipMapDrawableData::createDrawable() const {


  auto rval = std::make_shared<CallbackDrawable>(nullptr);
  rval->SetRenderCallback(ClipMapRenderImpl::renderClipMap);
  auto impl = rval->mDataA.makeShared<ClipMapRenderImpl>(this);
  rval->_sortkey = 10;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

ClipMapDrawableData::ClipMapDrawableData() {
  //_colortexpath = "lev2://textures/gridcell_grey";
}

///////////////////////////////////////////////////////////////////////////////

ClipMapDrawableData::~ClipMapDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
