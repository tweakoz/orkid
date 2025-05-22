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

ImposterPassData::ImposterPassData() {
  _userdata = std::make_shared<varmap::varmap_t>();
}

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

  /*
  fxparam_constptr_t _paramRTGTEX;
  fxparam_constptr_t _paramMVP;
  freestyle_mtl_ptr_t _blit_material;
  const FxShaderTechnique* _blit_tek = nullptr;
  fxparam_constptr_t _blit_par_mvp = nullptr;
  fxparam_constptr_t _blit_par_tex = nullptr;
  fxparam_constptr_t _blit_par_dmp = nullptr;
  fxparam_constptr_t _blit_par_near = nullptr;
  fxparam_constptr_t _blit_par_far = nullptr;
  fxparam_constptr_t _blit_par_near2 = nullptr;
  fxparam_constptr_t _blit_par_far2 = nullptr;
  fxparam_constptr_t _blit_par_ivp = nullptr;*/

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
    _radius         = the_sphere->mRadius;
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

    ///////////////////////////////////////
    // primary pass
    ///////////////////////////////////////

    auto imppass = _impdata->_imp_pass;
    auto as_fstyle = std::dynamic_pointer_cast<FreestyleMaterial>(imppass->_pipeline->_sharedMaterial);
    OrkAssert(as_fstyle != nullptr);

    auto paramRTGTEX = as_fstyle->param("rtgtex");
    OrkAssert(paramRTGTEX != nullptr);
    auto paramMVP = as_fstyle->param("mvp");
    imppass->_userdata->set("par_rtgtex", paramRTGTEX);
    imppass->_userdata->set("par_mvp", paramMVP);

    ///////////////////////////////////////
    // blit pass
    ///////////////////////////////////////

    auto blpass = _impdata->_blit_pass;

    auto blit_material = std::make_shared<FreestyleMaterial>();
    blit_material->gpuInit(ctx, "orkshader://solid");
    auto blit_tek       = blit_material->technique("imposter_blit");
    auto blit_par_mvp   = blit_material->param("MatMVP");
    auto blit_par_tex   = blit_material->param("ColorMap");
    auto blit_par_dmp   = blit_material->param("ImpDepthMap");
    auto blit_par_near  = blit_material->param("ImpNear");
    auto blit_par_far   = blit_material->param("ImpFar");
    auto blit_par_near2 = blit_material->param("ImpNear2");
    auto blit_par_far2  = blit_material->param("ImpFar2");
    auto blit_par_ivp   = blit_material->param("ImpIVP");

    OrkAssert(blit_tek != nullptr);
    OrkAssert(blit_par_mvp != nullptr);
    OrkAssert(blit_par_tex != nullptr);

    blit_material->_rasterstate->setBlendingMacro(BlendingMacro::ALPHA);
    blit_material->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
    blit_material->_rasterstate->setCullTest(ECullTest::OFF);
    blit_material->_rasterstate->setWriteMaskRGB(true);
    blit_material->_rasterstate->setWriteMaskA(true);
    blit_material->_rasterstate->setWriteMaskZ(false);

    blpass->_userdata->set("blit_material", blit_material);
    blpass->_userdata->set("blit_tek", blit_tek);
    blpass->_userdata->set("blit_par_mvp", blit_par_mvp);
    blpass->_userdata->set("blit_par_tex", blit_par_tex);
    blpass->_userdata->set("blit_par_dmp", blit_par_dmp);
    blpass->_userdata->set("blit_par_near", blit_par_near);
    blpass->_userdata->set("blit_par_far", blit_par_far);
    blpass->_userdata->set("blit_par_near2", blit_par_near2);
    blpass->_userdata->set("blit_par_far2", blit_par_far2);
    blpass->_userdata->set("blit_par_ivp", blit_par_ivp);

    ///////////////////////////////////////
    // user passes
    ///////////////////////////////////////

    for (auto p : _impdata->_user_passes) {
      auto as_fstyle = std::dynamic_pointer_cast<FreestyleMaterial>(p->_pipeline->_sharedMaterial);
      OrkAssert(as_fstyle != nullptr);

      auto paramRTGTEX = as_fstyle->param("rtgtex");
      OrkAssert(paramRTGTEX != nullptr);
      auto paramMVP = as_fstyle->param("mvp");
      p->_userdata->set("par_rtgtex", paramRTGTEX);
      p->_userdata->set("par_mvp", paramMVP);
    }

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

  auto FBI = context->FBI();
  auto FXI = context->FXI();
  auto GBI = context->GBI();
  auto DWI = context->DWI();

  auto RCFD       = RCID.rcfd();
  auto CIMPL      = RCFD->topCompositor();
  const auto& CPD = CIMPL->topCPD();
  auto is_stereo  = CPD.isSinglePassStereo();
  OrkAssert(not is_stereo);
  auto monocams           = CPD.cameraMatrices();
  fvec2 VIEWPORT_PLV      = fvec2(CPD._width, CPD._height);
  auto VP                 = RCFD->userPropertyAs<fmtx4>("VPMATRIX"_crcu);
  auto P                  = RCFD->userPropertyAs<fmtx4>("PMATRIX"_crcu);
  auto V                  = RCFD->userPropertyAs<fmtx4>("VMATRIX"_crcu);
  auto eye_pos            = V.inverse().translation();
  const auto& CAMDAT      = monocams->_camdat;
  auto worldmatrix        = RCID.worldMatrix();
  fvec3 POS               = worldmatrix.translation();
  fvec3 sphereToCamera    = eye_pos - POS;
  float distanceToSphere  = sphereToCamera.length();
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
  fvec3 right = sphereToCameraDir.crossWith(worldUp).normalized() * -1.0f;
  fvec3 up    = right.crossWith(sphereToCameraDir).normalized() * -1.0f;

  float bbrad = _radius * 1.1f;
  fvec3 V0    = POS - right * bbrad - up * bbrad; // bottom left
  fvec3 V1    = POS + right * bbrad - up * bbrad; // bottom right
  fvec3 V2    = POS + right * bbrad + up * bbrad; // top right
  fvec3 V3    = POS - right * bbrad + up * bbrad; // top left

  // Create a view matrix looking directly at the sphere center
  fmtx4 rtgView;
  rtgView.lookAt(eye_pos, POS, up);

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
  float far  = distanceToSphere + _radius * 2.0f;
  rtgProj.perspective(fovy, 1.0f, CAMDAT.mNear, CAMDAT.mFar); // printf("eye_pos<%g %g %g>\n", eye_pos.x, eye_pos.y, eye_pos.z);
  auto SUBVP  = (rtgProj * rtgView);
  auto SUBMVP = SUBVP * worldmatrix;

  auto imppass = _impdata->_imp_pass;
  auto blpass = _impdata->_blit_pass;

  auto RTG = imppass->_rtg;
  if (RTG) {

    RTG->_autoclear  = true;
      RTG->_clearColor = fvec4(0, 0, 0, 0);
      auto vprect_rtg  = RTG->viewportRect();
      FBI->pushScissor(vprect_rtg);
      FBI->pushViewport(vprect_rtg);
      FBI->PushRtGroup(RTG.get());

      auto par_mvp  = imppass->_userdata->typedValueForKey<fxparam_constptr_t>("par_mvp").value();

      imppass->_pipeline->bindParam(par_mvp, SUBMVP);
      ////////////////////////////////////////////
      imppass->_pipeline->wrappedDrawCall(
          RCID,                                   //
          [this, context]() {                     //
            this->_primitive->renderEML(context); //
          });

      FBI->PopRtGroup();
      FBI->popViewport();
      FBI->popScissor();
  }
  /*
  for (auto p : _impdata->_user_passes) {

    auto RTG = p->_rtg;
    if (RTG) {

      OrkAssert(RCFD->_renderingmodel._modelID == "FORWARD_PBR"_crcu);

      ////////////////////////////////////////////
      // camera related data
      ////////////////////////////////////////////

      ////////////////////////////////////////////
      // node related data
      ////////////////////////////////////////////

      ////////////////////////////////////////////
      // render imposter to texture
      ////////////////////////////////////////////

      RTG->_autoclear  = true;
      RTG->_clearColor = fvec4(0, 0, 0, 0);
      auto vprect_rtg  = RTG->viewportRect();
      FBI->pushScissor(vprect_rtg);
      FBI->pushViewport(vprect_rtg);
      FBI->PushRtGroup(RTG.get());

      auto par_mvp  = p->_userdata->typedValueForKey<fxparam_constptr_t>("par_mvp").value();

      p->_pipeline->bindParam(par_mvp, SUBMVP);
      ////////////////////////////////////////////
      p->_pipeline->wrappedDrawCall(
          RCID,                                   //
          [this, context]() {                     //
            this->_primitive->renderEML(context); //
          });

      FBI->PopRtGroup();
      FBI->popViewport();
      FBI->popScissor();
    } // if(RTG){

  } // for( auto p : _impdata->_user_passes ){
  */
  ////////////////////////////////////////////
  // blit pass
  //  pre-rendered texture -> screen
  ////////////////////////////////////////////

  // material preamble


  RTG             = imppass->_rtg;
  auto bmat       = blpass->_userdata->typedValueForKey<freestyle_mtl_ptr_t>("blit_material").value();
  auto btek       = blpass->_userdata->typedValueForKey<const FxShaderTechnique*>("blit_tek").value();
  auto bpar_mvp   = blpass->_userdata->typedValueForKey<fxparam_constptr_t>("blit_par_mvp").value();
  auto bpar_tex   = blpass->_userdata->typedValueForKey<fxparam_constptr_t>("blit_par_tex").value();
  auto bpar_dmp   = blpass->_userdata->typedValueForKey<fxparam_constptr_t>("blit_par_dmp").value();
  auto bpar_near  = blpass->_userdata->typedValueForKey<fxparam_constptr_t>("blit_par_near").value();
  auto bpar_far   = blpass->_userdata->typedValueForKey<fxparam_constptr_t>("blit_par_far").value();
  auto bpar_near2 = blpass->_userdata->typedValueForKey<fxparam_constptr_t>("blit_par_near2").value();
  auto bpar_far2  = blpass->_userdata->typedValueForKey<fxparam_constptr_t>("blit_par_far2").value();
  auto bpar_ivp   = blpass->_userdata->typedValueForKey<fxparam_constptr_t>("blit_par_ivp").value();
  bmat->begin(btek, RCFD);
  bmat->bindParam(bpar_tex, RTG->texture(0));
  bmat->bindParam(bpar_dmp, RTG->depthTexture());
  bmat->bindParam(bpar_mvp, VP);
  bmat->bindParamFloat(bpar_near, CAMDAT.mNear);
  bmat->bindParamFloat(bpar_far, CAMDAT.mFar);
  bmat->bindParamFloat(bpar_near2, CAMDAT.mNear);
  bmat->bindParamFloat(bpar_far2, CAMDAT.mFar);
  bmat->bindParam(bpar_ivp, SUBVP.inverse());
  bmat->commit();

  // render
  // GBI->_debugNextPrimitive = true;
  DWI->quad3DEML(
      V0,
      V1,
      V2,
      V3, // positions
      fvec2(0, 0),
      fvec2(1, 0),
      fvec2(1, 1),
      fvec2(0, 1), // uv
      0xffffffff); // color

  // material postamble
  bmat->end(RCFD);

  if (blpass->_debug_viz) {
    blpass->_pipeline->bindParam(bpar_mvp, (P * V) * worldmatrix);
    blpass->_pipeline->wrappedDrawCall(
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
  _imp_pass = std::make_shared<ImposterPassData>();
  _blit_pass = std::make_shared<ImposterPassData>();
}

///////////////////////////////////////////////////////////////////////////////

ImposterDrawableData::~ImposterDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
