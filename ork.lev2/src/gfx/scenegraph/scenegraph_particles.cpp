#include <ork/lev2/gfx/scenegraph/sgnode_particles.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::ParticlesDrawableData, "ParticlesDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct ParticlesDrawableInst {

  ParticlesDrawableInst(const ParticlesDrawableData* pdd) : _data(pdd) {

  }
  ~ParticlesDrawableInst(){
  }
  void gpuInit(lev2::Context* ctx) {
    /*
    auto load_req = std::make_shared<asset::LoadRequest>(_griddata->_colortexpath);
    auto texasset = asset::AssetManager<lev2::TextureAsset>::load(load_req);
    OrkAssert(texasset);

    _colortexture = texasset->GetTexture();
    OrkAssert(_colortexture);

    _pbrmaterial       = new PBRMaterial();
    _pbrmaterial->_shaderpath = "orkshader://grid";
    _pbrmaterial->_texColor = _colortexture;
    _pbrmaterial->gpuInit(ctx);
    _pbrmaterial->_metallicFactor  = 0.0f;
    _pbrmaterial->_roughnessFactor = 1.0f;
    _pbrmaterial->_baseColor       = fvec3(1, 1, 1);

    _fxcache = _pbrmaterial->pipelineCache();
    */
    _initted                   = true;
  }
  void _render(const RenderContextInstData& RCID){
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    auto context    = RCID.context();
    const RenderContextFrameData* RCFD = RCID._RCFD;
    const auto& CPD  = RCFD->topCPD();
    bool isPickState = context->FBI()->isPickState();

    if (not _initted){
      gpuInit(context);
    }
    OrkAssert(false);
  }
  static void renderParticles(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    renderable->GetDrawableDataA().getShared<ParticlesDrawableInst>()->_render(RCID);
  }
  const ParticlesDrawableData* _data;
  PBRMaterial* _pbrmaterial;

  texture_ptr_t _colortexture;
  fxpipelinecache_constptr_t _fxcache;
  bool _initted = false;

};

///////////////////////////////////////////////////////////////////////////////

void ParticlesDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t ParticlesDrawableData::createDrawable() const {

  auto impl = std::make_shared<ParticlesDrawableInst>(this);
  auto rval = std::make_shared<CallbackDrawable>(nullptr);
  rval->SetRenderCallback(ParticlesDrawableInst::renderParticles);
  rval->SetUserDataA(impl);
  rval->_sortkey = 10;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

ParticlesDrawableData::ParticlesDrawableData() {
  //_colortexpath = "lev2://textures/gridcell_grey";
}

///////////////////////////////////////////////////////////////////////////////

ParticlesDrawableData::~ParticlesDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
