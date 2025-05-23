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

    auto imppass   = _impdata->_imp_pass;
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

    auto blmtl = std::make_shared<FreestyleMaterial>();
    blmtl->gpuInit(ctx, "orkshader://solid");
    auto tek       = blmtl->technique("imposter_blit");
    auto par_mvp   = blmtl->param("MatMVP");
    auto par_tex   = blmtl->param("ColorMap");
    auto par_dmp   = blmtl->param("ImpDepthMap");
    auto par_near  = blmtl->param("ImpNear");
    auto par_far   = blmtl->param("ImpFar");
    auto par_near2 = blmtl->param("ImpNear2");
    auto par_far2  = blmtl->param("ImpFar2");
    auto par_ivp   = blmtl->param("ImpIVP");

    OrkAssert(tek != nullptr);
    OrkAssert(par_mvp != nullptr);
    OrkAssert(par_tex != nullptr);

    auto blrs = blmtl->_rasterstate;
    blrs->setBlendingMacro(BlendingMacro::ALPHA);
    blrs->setDepthTest(EDepthTest::LEQUALS);
    blrs->setCullTest(ECullTest::OFF);
    blrs->setWriteMaskRGB(true);
    blrs->setWriteMaskA(false);
    blrs->setWriteMaskZ(true);

    auto blud = blpass->_userdata;
    blud->set("material", blmtl);
    blud->set("tek", tek);
    blud->set("par_mvp", par_mvp);
    blud->set("par_tex", par_tex);
    blud->set("par_dmp", par_dmp);
    blud->set("par_near", par_near);
    blud->set("par_far", par_far);
    blud->set("par_near2", par_near2);
    blud->set("par_far2", par_far2);
    blud->set("par_ivp", par_ivp);

    if (not blud->hasKey("color_rtg")) {
      blud->set("color_rtg", imppass->_rtg);
    }
    if (not blud->hasKey("depth_rtg")) {
      blud->set("depth_rtg", imppass->_rtg);
    }

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
  auto blpass  = _impdata->_blit_pass;

  auto RTG = imppass->_rtg;
  if (RTG) {

    RTG->_autoclear  = true;
    RTG->_clearColor = fvec4(0, 0, 0, 0);
    auto vprect_rtg  = RTG->viewportRect();
    FBI->pushScissor(vprect_rtg);
    FBI->pushViewport(vprect_rtg);
    FBI->PushRtGroup(RTG.get());

    if(imppass->_onPreRender) {
      imppass->_onPreRender();
    }

    auto par_mvp = imppass->_userdata->typedValueForKey<fxparam_constptr_t>("par_mvp").value();

    imppass->_pipeline->bindParam(par_mvp, SUBMVP);
    ////////////////////////////////////////////
    GBI->_debugNextPrimitive = imppass->_debug_shaderstate;
    context->debugPushGroup("imp-pass");
    imppass->_pipeline->wrappedDrawCall(
        RCID,                                   //
        [this, context]() {                     //
          this->_primitive->renderEML(context); //
        });
    context->debugPopGroup();

    if(imppass->_onPostRender) {
      imppass->_onPostRender();
    }

    FBI->PopRtGroup();
    FBI->popViewport();
    FBI->popScissor();
  }

  ////////////////////////////////////////////
  // user passes
  ////////////////////////////////////////////
  int ip = 0;
  for (auto p : _impdata->_user_passes) {
    auto RTG = p->_rtg;
    if (RTG) {
      if(p->_onPreRender) {
        p->_onPreRender();
      }
      RTG->_autoclear  = true;
      RTG->_clearMaskColor  = true;
      RTG->_clearMaskDepth  = true;
      RTG->_autoclear  = true;
      RTG->_clearColor = fvec4(0, 0, 0, 0);
      auto vprect_rtg  = RTG->viewportRect();
      FBI->pushScissor(vprect_rtg);
      FBI->pushViewport(vprect_rtg);
      FBI->PushRtGroup(RTG.get());
      GBI->_debugNextPrimitive = p->_debug_shaderstate;
      context->debugPushGroup("user-pass-%d",ip);
      p->_pipeline->wrappedDrawCall(
          RCID,               //
          [=]() { //
            DWI->quad2DEML(
                fvec4(-1,-1,2,2),
                fvec4(0,0,1,1),
                fvec4(0,0,1,1));
          });
      FBI->PopRtGroup();
      FBI->popViewport();
      FBI->popScissor();
      if(p->_onPostRender != nullptr) {
        p->_onPostRender();
      }
      ip++;
    } // if(RTG){
  } // for( auto p : _impdata->_user_passes ){

  ////////////////////////////////////////////
  // blit pass
  //  pre-rendered texture -> screen
  ////////////////////////////////////////////

  // material preamble

  auto blud       = blpass->_userdata;
  auto bmat       = blud->typedValueForKey<freestyle_mtl_ptr_t>("material").value();
  auto btek       = blud->typedValueForKey<const FxShaderTechnique*>("tek").value();
  auto bpar_mvp   = blud->typedValueForKey<fxparam_constptr_t>("par_mvp").value();
  auto bpar_tex   = blud->typedValueForKey<fxparam_constptr_t>("par_tex").value();
  auto bpar_dmp   = blud->typedValueForKey<fxparam_constptr_t>("par_dmp").value();
  auto bpar_near  = blud->typedValueForKey<fxparam_constptr_t>("par_near").value();
  auto bpar_far   = blud->typedValueForKey<fxparam_constptr_t>("par_far").value();
  auto bpar_near2 = blud->typedValueForKey<fxparam_constptr_t>("par_near2").value();
  auto bpar_far2  = blud->typedValueForKey<fxparam_constptr_t>("par_far2").value();
  auto bpar_ivp   = blud->typedValueForKey<fxparam_constptr_t>("par_ivp").value();
  auto COLOR_RTG  = blud->typedValueForKey<rtgroup_ptr_t>("color_rtg").value();
  auto DEPTH_RTG  = blud->typedValueForKey<rtgroup_ptr_t>("depth_rtg").value();

  bmat->begin(btek, RCFD);
  bmat->bindParam(bpar_tex, COLOR_RTG->texture(0));
  bmat->bindParam(bpar_dmp, DEPTH_RTG->depthTexture());
  bmat->bindParam(bpar_mvp, VP);
  bmat->bindParamFloat(bpar_near, CAMDAT.mNear);
  bmat->bindParamFloat(bpar_far, CAMDAT.mFar);
  bmat->bindParamFloat(bpar_near2, CAMDAT.mNear);
  bmat->bindParamFloat(bpar_far2, CAMDAT.mFar);
  bmat->bindParam(bpar_ivp, SUBVP.inverse());
  bmat->commit();

  if(blpass->_onPreRender) {
    blpass->_onPreRender();
  }

  // render

  GBI->_debugNextPrimitive = blpass->_debug_shaderstate;

  context->debugPushGroup("blit-pass");
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

  context->debugPopGroup();

  if(blpass->_onPostRender) {
    blpass->_onPostRender();
  }

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
  _imp_pass  = std::make_shared<ImposterPassData>();
  _blit_pass = std::make_shared<ImposterPassData>();
}

///////////////////////////////////////////////////////////////////////////////

ImposterDrawableData::~ImposterDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
