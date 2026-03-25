#include <ork/lev2/gfx/scenegraph/sgnode_billboard.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/rasterstate.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/reflect/properties/registerX.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
ImplementReflectionX(ork::lev2::BillboardDrawableData, "BillboardDrawableData");
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct BillboardRenderImpl {

  BillboardRenderImpl(const BillboardDrawableData* bbd) : _bbdata(bbd) {
    // Load image eagerly (CPU-side, no GPU context needed)
    if (_bbdata->_image) {
      _sourceImage = _bbdata->_image;
    } else if (!_bbdata->_imagePath.empty()) {
      auto resolved = file::Path::expandPathString(_bbdata->_imagePath.c_str());
      _sourceImage = Image::createFromFile(resolved);
      OrkAssert(_sourceImage);
    }
  }
  ~BillboardRenderImpl(){
  }
  void gpuInit(lev2::Context* ctx) {
    OrkAssert(_sourceImage);
    _colortexture = std::make_shared<Texture>();
    // async=true so texture upload can happen during render pass
    ctx->TXI()->initTextureFromImage(_colortexture.get(), _sourceImage, false, true);

    static const char* shaderText = R"(
      import "orkshader://pbrtools.i2";
      fxconfig fxcfg_default { glsl_version = "150"; }
      ///////////////////////////////////////////////////////////////
      uniform_set ublock_bb_vtx {
        mat4 MatMVP;
      }
      sampler_set bb_samplers (descriptor_set 0) {
        sampler2D ColorMap;
      }
      ///////////////////////////////////////////////////////////////
      vertex_interface vif_bb : ublock_bb_vtx {
        inputs {
          vec4 position : POSITION;
          vec4 vtxcolor : COLOR0;
          vec2 uv0 : TEXCOORD0;
        }
        outputs {
          vec2 frg_uv0;
          vec4 frg_clr;
        }
      }
      fragment_interface fif_bb {
        inputs {
          vec2 frg_uv0;
          vec4 frg_clr;
        }
        outputs {
          layout(location = 0) vec4 out_clr;
        }
      }
      ///////////////////////////////////////////////////////////////
      vertex_shader vs_bb : vif_bb {
        gl_Position = MatMVP * position;
        frg_uv0     = uv0;
        frg_clr     = vtxcolor;
      }
      fragment_shader ps_bb : fif_bb : bb_samplers {
        vec4 texc = texture(ColorMap, frg_uv0);
        out_clr   = texc;
      }
      ///////////////////////////////////////////////////////////////
      // pick: reuse PBR pick interfaces from pbrtools.i2
      ///////////////////////////////////////////////////////////////
      vertex_shader vs_bb_pick : iface_vtx_pick_rigid_simple : ublk_std_matrices {
        gl_Position    = mvp * position;
        frg_wpos       = (m * position).xyz;
        frg_wnrm       = normalize(mrot * normal);
        frg_uv         = vec2(0,0);
        frg_pickSUBID  = uvec3(0,0,0);
      }
      ///////////////////////////////////////////////////////////////
      state_block sb_bb : default {
        DepthTest = LEQUALS;
        DepthMask = ON;
      }
      technique bb_texcolor {
        fxconfig = fxcfg_default;
        pass p0 {
          vertex_shader   = vs_bb;
          fragment_shader = ps_bb;
          state_block     = sb_bb;
        }
      }
      technique bb_pick {
        fxconfig = fxcfg_default;
        pass p0 {
          vertex_shader   = vs_bb_pick;
          fragment_shader = ps_pick;
          state_block     = sb_bb;
        }
      }
    )";
    _material.gpuInitFromShaderText(ctx, "billboard_icon", shaderText);

    // raster state: alpha blend, no depth write, no cull
    _material._rasterstate->setBlendingMacro(BlendingMacro::ALPHA);
    _material._rasterstate->setDepthTest(EDepthTest::LEQUALS);
    _material._rasterstate->setWriteMaskZ(false);
    _material._rasterstate->setCullTest(ECullTest::OFF);
    _material._rasterstate->_priority = 1 << 20;

    _parMVP      = _material.param("MatMVP");
    _parColorMap = _material.param("ColorMap");

    auto fxcache = _material.pipelineCache();

    // forward pipeline
    FxPipelinePermutation fwd_permu;
    fwd_permu._rendering_model = "FORWARD_PBR"_crcu;
    fwd_permu._forced_technique = _material.technique("bb_texcolor");
    _fwdPipeline = fxcache->findPipeline(fwd_permu);
    _fwdPipeline->_rasterstate = _material._rasterstate;

    // pick pipeline
    auto tekPick = _material.technique("bb_pick");
    auto parPickID = _material.param("obj_pickID");
    if (tekPick) {
      FxPipelinePermutation pick_permu;
      pick_permu._rendering_model = "PICKING"_crcu;
      pick_permu._forced_technique = tekPick;
      pick_permu._is_picking = true;
      _pickPipeline = fxcache->findPipeline(pick_permu);
      _pickPipeline->_rasterstate = _material._rasterstate;
      if (parPickID) {
        _pickPipeline->bindParam(parPickID, std::make_shared<CrcString>("RCID_PickID"));
      }
      _parPickMVP = _material.param("mvp");
      _parPickM   = _material.param("m");
      _parPickMrot = _material.param("mrot");
    }

    _initted = true;
  }
  void _render(const RenderContextInstData& RCID){
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    auto context    = RCID.context();
    auto RCFD = RCID.rcfd();

    // only render in forward and pick passes — must check before gpuInit
    auto modelID = RCFD->_renderingmodel._modelID;
    if (modelID != "FORWARD_PBR"_crcu && modelID != "PICKING"_crcu) {
      return;
    }

    if (not _initted){
      gpuInit(context);
    }

    auto& CPD = RCFD->topCPD();
    auto cmtx = CPD.cameraMatrices();
    if (!cmtx) return;

    bool isPick = CPD.isPicking();
    auto gbi = context->GBI();
    auto mtxi = context->MTXI();

    // world position from the node's transform
    fmtx4 world_mtx = renderable->GetMatrix();
    fvec3 world_pos  = world_mtx.translation();

    // camera axes for billboarding
    const auto& V = cmtx->_vmatrix;
    fmtx4 IV      = V.inverse();
    fvec3 cam_right = IV.xNormal();
    fvec3 cam_up    = IV.yNormal();

    // constant screen size: scale by distance from camera
    fvec3 cam_pos = IV.translation();
    float dist    = (world_pos - cam_pos).magnitude();
    float scale   = dist * _bbdata->_screenSize * 0.001f;

    // billboard quad corners in world space
    fvec3 dx = cam_right * scale;
    fvec3 dy = cam_up * scale;

    fvec3 p0 = world_pos - dx - dy; // bottom-left
    fvec3 p1 = world_pos + dx - dy; // bottom-right
    fvec3 p2 = world_pos + dx + dy; // top-right
    fvec3 p3 = world_pos - dx + dy; // top-left

    // VP matrix (vertices are in world space, so MVP = VP)
    fmtx4 VP = cmtx->_vpmatrix;

    auto normal   = fvec3(0, 0, 1);
    auto binormal = fvec3(1, 0, 0);
    uint32_t color = 0xffffffff;
    float alpha    = _bbdata->_alpha;
    color          = (color & 0x00ffffff) | (uint32_t(alpha * 255.0f) << 24);

    auto VB = GfxEnv::GetSharedDynamicVB2();
    VtxWriter<SVtxV12N12B12T8C4> vw;
    vw.Lock(context, VB.get(), 6);
    vw.AddVertex(SVtxV12N12B12T8C4(p0, normal, binormal, fvec2(0, 1), color));
    vw.AddVertex(SVtxV12N12B12T8C4(p1, normal, binormal, fvec2(1, 1), color));
    vw.AddVertex(SVtxV12N12B12T8C4(p2, normal, binormal, fvec2(1, 0), color));
    vw.AddVertex(SVtxV12N12B12T8C4(p0, normal, binormal, fvec2(0, 1), color));
    vw.AddVertex(SVtxV12N12B12T8C4(p2, normal, binormal, fvec2(1, 0), color));
    vw.AddVertex(SVtxV12N12B12T8C4(p3, normal, binormal, fvec2(0, 0), color));
    vw.UnLock(context);

    // use identity model matrix (verts already in world space)
    mtxi->PushMMatrix(fmtx4::Identity());
    context->PushModColor(fcolor4::White());

    auto pipeline = isPick ? _pickPipeline : _fwdPipeline;
    if (!pipeline) return;

    context->FXI()->pushRasterState(_material._rasterstate);
    pipeline->wrappedDrawCall(RCID, [&]() {
      if (isPick) {
        // get pick camera VP from RCFD props, use as-is (verts already in world space)
        const auto& RCFDPROPS = RCFD->userProperties();
        auto it = RCFDPROPS.find("pickbufferMvpMatrix"_crc);
        if (it != RCFDPROPS.end()) {
          auto pickVP = *(it->second.get<fmtx4_ptr_t>().get());
          auto fxi = context->FXI();
          if (_parPickMVP) fxi->bindParamMatrix(_parPickMVP, pickVP);
          if (_parPickM)   fxi->bindParamMatrix(_parPickM, fmtx4::Identity());
          if (_parPickMrot) fxi->bindParamMatrix(_parPickMrot, fmtx3::Identity);
          fxi->CommitParams();
        }
      } else {
        pipeline->_set_typed_param(RCID, _parMVP, VP);
        pipeline->_set_typed_param(RCID, _parColorMap, _colortexture.get());
      }
      gbi->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
    });
    context->FXI()->popRasterState();

    context->PopModColor();
    mtxi->PopMMatrix();
  }
  static void renderBB(RenderContextInstData& RCID) {
    auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
    renderable->GetDrawableDataA().getShared<BillboardRenderImpl>()->_render(RCID);
  }
  const BillboardDrawableData* _bbdata;
  image_ptr_t _sourceImage;
  FreestyleMaterial _material;
  fxpipeline_ptr_t _fwdPipeline;
  fxpipeline_ptr_t _pickPipeline;
  fxparam_constptr_t _parMVP      = nullptr;
  fxparam_constptr_t _parColorMap = nullptr;
  fxparam_constptr_t _parPickMVP  = nullptr;
  fxparam_constptr_t _parPickM    = nullptr;
  fxparam_constptr_t _parPickMrot = nullptr;
  texture_ptr_t _colortexture;
  bool _initted = false;

};

///////////////////////////////////////////////////////////////////////////////

void BillboardDrawableData::describeX(class_t* c) {
  c->directProperty("Image", &BillboardDrawableData::_imagePath)
      ->annotate("editor.filetype", "png")
      ->annotate("editor.filebase", "<ork_data>,<assetcache>");
  c->directProperty("Alpha", &BillboardDrawableData::_alpha);
  c->directProperty("ScreenSize", &BillboardDrawableData::_screenSize);
}

///////////////////////////////////////////////////////////////////////////////

drawable_ptr_t BillboardDrawableData::createDrawable() const {

  auto impl = std::make_shared<BillboardRenderImpl>(this);

  auto rval = std::make_shared<CallbackDrawable>(nullptr);
  rval->SetRenderCallback(BillboardRenderImpl::renderBB);
  rval->SetUserDataA(impl);
  rval->_sortkey = 10000;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

BillboardDrawableData::BillboardDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////

BillboardDrawableData::~BillboardDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
