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
    params->_levels = 4;
    params->_scale = 2;
    params->_gridSize = 16;

    auto generator = std::make_shared<terclipmap::Generator>(params);

    auto subm = generator->generateClipmaps();
    //auto subm = mesh->submeshFromGroupName("clipmappolys");
    
    printf( "generating primitive...\n");

    _mesh_primitive = std::make_shared<rigidprim_SVtxV12N12T16_t>();
    _mesh_primitive->fromSubMesh(*subm,ctx);

    if( _grounddata->_material ){
        _pbrmaterial = _grounddata->_material;
        _fxcache = _pbrmaterial->pipelineCache();
        _as_freestyle = _pbrmaterial->_as_freestyle;
        //_paramMYM = _as_freestyle->param("my_m");
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

    auto& CPD  = RCFD->topCPD();

    auto mtxi = context->MTXI();
    auto gbi  = context->GBI();
    auto fxi = context->FXI();
    auto pipeline = _fxcache->findPipeline(RCID);
    OrkAssert(pipeline);

    auto mcams             = CPD._cameraMatrices;
    const fmtx4& PMTX_mono = mcams->_pmatrix;
    fmtx4 vmtx_mono = mcams->_vmatrix;
    fmtx4 v_offset;
    fvec3 eye_pos = vmtx_mono.inverse().translation();
    fvec3 eye_dir = vmtx_mono.inverse().zNormal();
    fplane3 gnd_plane( 0,1,0,0 );
    float distance = 0.0;
    fvec3 result;
    bool isect = gnd_plane.Intersect(fray3(eye_pos,eye_dir),distance, result);

    fvec3 center = fvec3(eye_pos.x, 0.0f, eye_pos.z);
    //center += result*-1.0f;
    center += fvec3(eye_dir.x, 0.0f, eye_dir.z) * powf(eye_pos.y,1.0) * -1.0f;
    v_offset.setTranslation(center);


    auto mut_renderable = const_cast<CallbackRenderable*>(renderable);
    auto orig_matrix = mut_renderable->_worldMatrix;
    mut_renderable->_worldMatrix = (v_offset*orig_matrix);
    
    pipeline->wrappedDrawCall(RCID, [&]() {
      //_as_freestyle->bindParamMatrix(_paramMYM, orig_matrix);
      _mesh_primitive->renderEML(context);
    });
    mut_renderable->_worldMatrix = orig_matrix;

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
  fxparam_constptr_t _paramMYM           = nullptr;
  freestyle_mtl_ptr_t _as_freestyle = nullptr;

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
