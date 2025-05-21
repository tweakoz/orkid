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
#include <ork/math/cmatrix4.hpp>
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
  fxparam_constptr_t _paramMVP;
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
    _paramMVP = as_fstyle->param("mvp");

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
    _blit_material->_rasterstate->setCullTest(ECullTest::OFF);
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
    auto P = RCFD->userPropertyAs<fmtx4>("PMATRIX"_crcu);
    auto V = RCFD->userPropertyAs<fmtx4>("VMATRIX"_crcu);
    auto eye_pos = V.inverse().translation();

    ////////////////////////////////////////////
    // node related data
    ////////////////////////////////////////////

    auto worldmatrix = RCID.worldMatrix();
    fvec3 POS = worldmatrix.translation();
    
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

    auto target = POS;

    fvec3 sphereToCamera = eye_pos - POS;
    float distanceToSphere = sphereToCamera.length();
    fvec3 sphereToCameraDir = sphereToCamera / distanceToSphere;
    
    ////////////////////////////////////////////
    // Calculate billboard orientation that perfectly faces camera
    ////////////////////////////////////////////
    
    // Find a stable up vector (not parallel to view direction)
    fvec3 worldUp = fvec3(0, 1, 0);
    if (std::abs(sphereToCameraDir.dotWith(worldUp)) > 0.99f) {
        worldUp = fvec3(1, 0, 0);
    }
    
    // Calculate right and up vectors for billboard
    fvec3 right = sphereToCameraDir.crossWith(worldUp).normalized();
    fvec3 up = right.crossWith(sphereToCameraDir).normalized();

    float bbrad = _radius * 1.1f;
    fvec3 V0 = POS - right * bbrad - up * bbrad; // bottom left
    fvec3 V1 = POS + right * bbrad - up * bbrad; // bottom right
    fvec3 V2 = POS + right * bbrad + up * bbrad; // top right
    fvec3 V3 = POS - right * bbrad + up * bbrad; // top left
    
    // Create a view matrix looking directly at the sphere center
    fmtx4 rtgView;
    rtgView.lookAt(eye_pos, POS, up );
    
    // For a 3D sphere, we want perspective projection
    fmtx4 rtgProj;
    
    // Calculate field of view that frames the sphere nicely
    // Using the formula: fovy = 2 * atan(radius / distance)
    float fovy = 2.0f * atan(_radius / distanceToSphere);
    fovy *= 1.1f;
    
    // Add a small margin to ensure the sphere is fully visible (20%)
    
    // Create perspective projection matrix
    // Use aspect ratio 1:1 since we're rendering to a square texture
    float near = std::max(0.1f, distanceToSphere - _radius * 2.0f);
    float far = distanceToSphere + _radius * 2.0f;
    rtgProj.perspective(fovy, 1.0f, near, far);    //printf("eye_pos<%g %g %g>\n", eye_pos.x, eye_pos.y, eye_pos.z);

    auto SUBMVP = (rtgProj * rtgView)*worldmatrix; 
    _impdata->_pipeline->bindParam(_paramMVP, SUBMVP);
    ////////////////////////////////////////////
    _impdata->_pipeline->wrappedDrawCall(
      RCID,                                   //
      [this, context]() {                     //
       this->_primitive->renderEML(context); //
    });

    FBI->PopRtGroup();
    FBI->popViewport();
    FBI->popScissor();

    ////////////////////////////////////////////
    // blit pre-rendered texture to screen
    ////////////////////////////////////////////


    // material preamble

    _blit_material->begin(_blit_tek, RCFD );
    _blit_material->bindParam(_blit_par_tex, RTG->texture(0));
    _blit_material->bindParam(_blit_par_mvp, VP);
    _blit_material->commit();
    FXI->applyRasterState(*(_blit_material->_rasterstate));

    // render

    DWI->quad3DEML(
      V0, V1, V2, V3,                                     // positions
      fvec2(1, 0), fvec2(0, 0), fvec2(0, 1), fvec2(1, 1), // uv
      0xffffffff);                                        // color

    // material postamble
    _blit_material->end(RCFD);

    if( _impdata->_debug_viz ){
      _impdata->_pipeline->bindParam(_paramMVP, (P*V)*worldmatrix);
      _impdata->_pipeline->wrappedDrawCall(
        RCID,                                   //
        [this, context]() {                     //
         this->_primitive->renderEML(context); //
      });
    }

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
