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
    _updata = std::make_shared<ui::UpdateData>();
    _updata->_abstime = 0.0f;
    _updata->_dt = 0.01f;

  }
  ///////////////////////////////////////////////////////////////
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

  }
  ///////////////////////////////////////////////////////////////
  void _update(){
    _updata->_abstime += _updata->_dt;
    _graphinst->compute(_updata);
  }
  ///////////////////////////////////////////////////////////////
  void _render(const RenderContextInstData& RCID){
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    auto context    = RCID.context();
    const RenderContextFrameData* RCFD = RCID._RCFD;
    const auto& CPD  = RCFD->topCPD();
    bool isPickState = context->FBI()->isPickState();
    if (not _initted){
      gpuInit(context);
      _initted = true;
    }
    auto ptcl_context = _graphinst->_impl.getShared<particle::Context>();
    if(ptcl_context->_rcidlambda){
      ptcl_context->_rcidlambda(RCID);
    }
  }
  ///////////////////////////////////////////////////////////////
  static void renderParticles(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    renderable->GetDrawableDataA().getShared<ParticlesDrawableInst>()->_render(RCID);
  }
  ///////////////////////////////////////////////////////////////
  const ParticlesDrawableData* _data;
  PBRMaterial* _pbrmaterial;
  dataflow::graphinst_ptr_t _graphinst;

  texture_ptr_t _colortexture;
  ui::updatedata_ptr_t _updata;
  fxpipelinecache_constptr_t _fxcache;
  bool _initted = false;

};

///////////////////////////////////////////////////////////////////////////////

void ParticlesDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t ParticlesDrawableData::createDrawable() const {

  if(nullptr==_graphdata)
    return nullptr;

  using namespace ork::dataflow;
  using namespace particle;

  ////////////////////////////////////////////////////
  // create topology
  ////////////////////////////////////////////////////

  auto dg_context = std::make_shared<dgcontext>();
  dg_context->createRegisters<float>("ptc_float", 16);
  dg_context->createRegisters<fvec3>("ptc_vec3f", 16);
  dg_context->createRegisters<ParticleBufferData>("ptc_buffer", 4);
  auto dg_sorter                       = std::make_shared<DgSorter>(_graphdata.get(), dg_context);
  dg_sorter->_logchannel->_enabled     = true;
  dg_sorter->_logchannel_reg->_enabled = true;
  auto topo         = dg_sorter->generateTopology();

  ////////////////////////////////////////////////////
  // create graphinst
  ////////////////////////////////////////////////////

  auto graphinst    = GraphData::createGraphInst(_graphdata);
  auto ptcl_context = graphinst->_impl.makeShared<particle::Context>();
  graphinst->updateTopology(topo);

  ////////////////////////////////////////////////////

  auto impl = std::make_shared<ParticlesDrawableInst>(this);
  impl->_graphinst = graphinst;


  auto rval = std::make_shared<CallbackDrawable>(nullptr);
  rval->_enqueueOnLayerLambda = [impl](drawablebufitem_constptr_t cdb){
    impl->_update();
  };
  rval->SetRenderCallback(ParticlesDrawableInst::renderParticles);
  rval->SetUserDataA(impl);
  ptcl_context->_drawable = rval;
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
