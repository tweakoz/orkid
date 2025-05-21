#include <ork/lev2/gfx/scenegraph/sgnode_imposter.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/image.h>
#include <ork/kernel/opq.h>
#include <ork/math/sphere.h>
#include <ork/core_types.h>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::ImposterDrawableData, "ImposterDrawableData");
namespace ork::lev2 {

struct ImposterDrawableImpl {

  ImposterDrawableImpl(const ImposterDrawableData* grid, callback_drawable_wkptr_t drw);
  ~ImposterDrawableImpl();
  void gpuInit(lev2::Context* ctx);
  void _render(const RenderContextInstData& RCID);
  static void renderImp(RenderContextInstData& RCID);

  const ImposterDrawableData* _impdata = nullptr;
  fxpipelinecache_constptr_t _fxcache;

  bool _initted = false;
  meshutil::submesh_ptr_t _submesh;
  meshutil::rigidprim_V12N12B12T8C4_ptr_t _primitive;
  callback_drawable_wkptr_t _drawable;
  fxparam_constptr_t _paramRTGTEX;
  freestyle_mtl_ptr_t _blit_material;
  const FxShaderTechnique* _blit_tek = nullptr;
  fxparam_constptr_t _blit_par_mvp = nullptr;
  fxparam_constptr_t _blit_par_tex = nullptr;
  float _radius = 0.0f;

};

///////////////////////////////////////////////////////////////////////////////

ImposterDrawableImpl::ImposterDrawableImpl(const ImposterDrawableData* grid, callback_drawable_wkptr_t drw)
    : _impdata(grid) {

  _primitive = std::make_shared<meshutil::rigidprim_V12N12B12T8C4_t>();
  _submesh   = std::make_shared<meshutil::submesh>();
  _drawable  = drw;
}

///////////////////////////////////////////////////////////////////////////////

ImposterDrawableImpl::~ImposterDrawableImpl() {
}

///////////////////////////////////////////////////////////////////////////////

void ImposterDrawableImpl::gpuInit(lev2::Context* ctx) {
  if (auto as_sphere = _impdata->_shape.tryAs<::ork::sphere_ptr_t>()) {
    auto the_sphere = as_sphere.value();
    fvec3 center    = the_sphere->mCenter;
    _radius    = the_sphere->mRadius;
    printf("imposter::gpuInit radius<%g>\n", _radius);
    size_t dim = 4;
    switch (_impdata->_detail) {
      case 0:
      default:
        dim = 4;
        break;
      case 1:
        dim = 8;
        break;
      case 2:
        dim = 16;
        break;
      case 3:
        dim = 32;
        break;
    }
    submesh_fromUvSphere(_radius, dim, dim, *_submesh);
    _primitive->fromSubMesh(*_submesh, ctx);
    auto as_fstyle = std::dynamic_pointer_cast<FreestyleMaterial>(_impdata->_pipeline->_sharedMaterial);
    OrkAssert(as_fstyle != nullptr);
    _paramRTGTEX = as_fstyle->param("rtgtex");
    OrkAssert(_paramRTGTEX != nullptr);

    _blit_material = std::make_shared<FreestyleMaterial>();
    _blit_material->gpuInit(ctx, "orkshader://solid");
    _blit_tek = _blit_material->technique("texcolor");
    _blit_par_mvp = _blit_material->param("MatMVP");
    _blit_par_tex = _blit_material->param("ColorMap");

    OrkAssert(_blit_tek != nullptr);
    OrkAssert(_blit_par_mvp != nullptr);
    OrkAssert(_blit_par_tex != nullptr);

    _blit_material->_rasterstate->setBlendingMacro(BlendingMacro::OFF);
    _blit_material->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
    _blit_material->_rasterstate->setCullTest(ECullTest::PASS_FRONT);
    _blit_material->_rasterstate->setWriteMaskRGB(true);
    _blit_material->_rasterstate->setWriteMaskA(true);
    _blit_material->_rasterstate->setWriteMaskZ(true);

  } else {
    OrkAssert(false);
  }
}

///////////////////////////////////////////////////////////////////////////////

void ImposterDrawableImpl::_render(const RenderContextInstData& RCID) {

  auto context = RCID.context();
  
  if (not _initted) {
    gpuInit(context);
    _initted = true;
  }

  auto RTG = _impdata->_rtg;
  if(RTG){

    auto RCFD    = RCID.rcfd();
    OrkAssert(RCFD->_renderingmodel._modelID == "FORWARD_PBR"_crcu);

    ////////////////////////////////////////////
    // camera related data
    ////////////////////////////////////////////

    auto CIMPL = RCFD->topCompositor();
    const auto& CPD = CIMPL->topCPD();
    auto is_stereo = CPD.isSinglePassStereo();
    OrkAssert(not is_stereo);
    auto monocams = CPD.cameraMatrices();
    fvec2 VIEWPORT_PLV = fvec2(CPD._width, CPD._height);
    auto VP = RCFD->userPropertyAs<fmtx4>("VPMATRIX"_crcu);

    ////////////////////////////////////////////
    // node related data
    ////////////////////////////////////////////

    auto worldmatrix = RCID.worldMatrix();
    fvec3 POS = worldmatrix.translation();

    ////////////////////////////////////////////
    // compute billboard vectors
    ////////////////////////////////////////////

    fvec3 UP, RIGHT;
    monocams->pixelLengthVectors(POS,          // pos (in)
                                 VIEWPORT_PLV, // viewport (in)
                                 UP,           // up (out)
                                 RIGHT);       // right (out)

    // the pixel length vectors are in world space
    //  and represent the size of 1 pixel in world space
    //  at the given position in the world                             

    ////////////////////////////////////////////
    // compute sphere size in pixels @ pos
    ////////////////////////////////////////////

    fvec3 SPH_U = UP.normalized() * _radius;
    fvec3 SPH_R = RIGHT.normalized() * _radius;
    fvec3 SPH_Z = UP.crossWith(RIGHT).normalized() * _radius;
    ////////////////////////////////////////////
    // render imposter to texture
    ////////////////////////////////////////////

    RTG->_autoclear = true;
    RTG->_clearColor = fvec4(1.0, 0.0, 1.0, 1.0);
    auto FBI = context->FBI();
    auto FXI = context->FXI();
    auto DWI = context->DWI();
    auto vprect_rtg = RTG->viewportRect();
    FBI->pushScissor(vprect_rtg);
    FBI->pushViewport(vprect_rtg);
    FBI->PushRtGroup(RTG.get());
    FBI->PopRtGroup();
    FBI->popViewport();
    FBI->popScissor();

    //_impdata->_pipeline->bindParam(_paramRTGTEX, RTG->texture(0));

    //auto monocams   = CPD._mono_cam_matrices;
    //monocams->MVPMONO(worldmatrix);

    ////////////////////////////////////////////
    // blit pre-rendered texture to screen
    ////////////////////////////////////////////

    fvec3 V0 = -SPH_R*0.5f-(SPH_U*0.5f);
    fvec3 V1 =  SPH_R*0.5f-(SPH_U*0.5f);
    fvec3 V2 =  V1 + SPH_U;
    fvec3 V3 =  V0 + SPH_U;

    // material preamble

    _blit_material->begin(_blit_tek, RCFD );
    _blit_material->bindParam(_blit_par_tex, RTG->texture(0));
    _blit_material->bindParam(_blit_par_mvp, VP);
    _blit_material->commit();
    FXI->applyRasterState(*(_blit_material->_rasterstate));

    // render

    fvec3 P2 = POS+SPH_Z*2.5f;
    DWI->quad3DEML(
      P2+V0, P2+V1, P2+V2, P2+V3,                                     // positions
      fvec2(0, 0), fvec2(1, 0), fvec2(1, 1), fvec2(0, 1), // uv
      0xffffffff);                                        // color

    // material postamble
    _blit_material->end(RCFD);

    ////////////////////////////////////////////
    _impdata->_pipeline->wrappedDrawCall(
      RCID,                                   //
      [this, context]() {                     //
       this->_primitive->renderEML(context); //
    });

  }
  else{

    ////////////////////////////////////////////
    // render imposter to screen
    ////////////////////////////////////////////

    _impdata->_pipeline->wrappedDrawCall(
      RCID,                                   //
      [this, context]() {                     //
       this->_primitive->renderEML(context); //
    });
  }
}

///////////////////////////////////////////////////////////////////////////////

void ImposterDrawableImpl::renderImp(RenderContextInstData& RCID) { // static
  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  auto drawable   = renderable->_drawable;
  drawable->_implA.getShared<ImposterDrawableImpl>()->_render(RCID);
}

///////////////////////////////////////////////////////////////////////////////

void ImposterDrawableData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t ImposterDrawableData::createDrawable() const {
  auto drw      = std::make_shared<CallbackDrawable>(nullptr);
  auto impl     = drw->_implA.makeShared<ImposterDrawableImpl>(this, drw);
  drw->_sortkey = 10;
  drw->SetRenderCallback(ImposterDrawableImpl::renderImp);
  return drw;
}

///////////////////////////////////////////////////////////////////////////////

ImposterDrawableData::ImposterDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

ImposterDrawableData::~ImposterDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
