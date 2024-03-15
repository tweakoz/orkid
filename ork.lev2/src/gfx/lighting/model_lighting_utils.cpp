#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/material_freestyle.h>
#include <random>

#define USE_OIIO

#if defined(USE_OIIO)

#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING
#endif

namespace ork::lev2 {

struct triangle {
  fvec3 _v0;
  fvec3 _v1;
  fvec3 _v2;
  fvec3 _tc0;
  fvec3 _tc1;
  fvec3 _tc2;
  fvec3 _N;
  fvec3 _edge0;
  fvec3 _edge1;
  fvec3 _C;
  float _det;
  fplane3 _plane;
};

using triangle_vect_t = std::vector<triangle>;

struct LightingInfo {
  triangle_vect_t triangles;
  float* _ambient_occlusion = nullptr;
};

triangle_vect_t model_to_triangles(meshutil::mesh_ptr_t model) {
  std::vector<triangle> triangles;
  auto& submeshes = model->RefSubMeshLut();
  for (auto submesh_item : submeshes) {
    auto name    = submesh_item.first;
    auto submesh = submesh_item.second;

    auto triangulated = std::make_shared<ork::meshutil::submesh>();
    meshutil::submeshTriangulate(*submesh, *triangulated);

    int inumpolys = triangulated->numPolys();
    for (int ip = 0; ip < inumpolys; ip++) {
      auto p = triangulated->poly(ip);
      triangle tri;
      tri._v0     = dvec3_to_fvec3(p->vertex(0)->mPos);
      tri._v1     = dvec3_to_fvec3(p->vertex(1)->mPos);
      tri._v2     = dvec3_to_fvec3(p->vertex(2)->mPos);
      tri._tc0    = p->vertex(0)->mUV[0].mMapTexCoord;
      tri._tc1    = p->vertex(1)->mUV[0].mMapTexCoord;
      tri._tc2    = p->vertex(2)->mUV[0].mMapTexCoord;
      auto normal = (tri._v1 - tri._v0).crossWith(tri._v2 - tri._v0);
      tri._N      = normal;

      const auto& v0 = tri._v0;
      const auto& v1 = tri._v1;
      const auto& v2 = tri._v2;

      tri._edge0 = v1 - v0;
      tri._edge1 = v2 - v0;

      tri._C   = tri._edge0.crossWith(tri._edge1);
      tri._det = tri._C.dotWith(tri._C);

      tri._plane = fplane3(tri._N, tri._v0);

      triangles.push_back(tri);
    }
    // submesh->computeAmbientOcclusion();
  }
  OrkAssert(triangles.size() != 0);
  return triangles;
}

void computeAmbientOcclusion(int numsamples, meshutil::mesh_ptr_t model, Context* ctx) {

  auto triangles       = model_to_triangles(model);
  size_t num_triangles = triangles.size();
  size_t num_verts     = num_triangles * 3;
  printf("num_triangles<%zu> num_verts<%zu>\n", num_triangles, num_verts);
  using vtx_t = SVtxV12N12T16;
  auto vbuf   = std::make_shared<StaticVertexBuffer<vtx_t>>(num_verts, 0);
  auto vbuf2  = std::make_shared<StaticVertexBuffer<vtx_t>>(num_verts, 0);
  VtxWriter<vtx_t> vw;
  VtxWriter<vtx_t> vw2;
  vw.Lock(ctx, vbuf.get(), num_verts);
  vw2.Lock(ctx, vbuf2.get(), num_verts);

  auto vbase  = (vtx_t*)vw.mpBase;
  auto vbase2 = (vtx_t*)vw2.mpBase;
  int ivertex = 0;

  AABox aabb;

  for (int it = 0; it < triangles.size(); it++) {
    auto& tri = triangles[it];

    auto normal = (tri._v1 - tri._v0).crossWith(tri._v2 - tri._v0);

    vbase[ivertex + 0].mPosition = tri._tc0;
    vbase[ivertex + 0].mUV       = tri._v0;
    vbase[ivertex + 0].mNormal   = normal;

    vbase[ivertex + 1].mPosition = tri._tc1;
    vbase[ivertex + 1].mUV       = tri._v1;
    vbase[ivertex + 1].mNormal   = normal;

    vbase[ivertex + 2].mPosition = tri._tc2;
    vbase[ivertex + 2].mUV       = tri._v2;
    vbase[ivertex + 2].mNormal   = normal;

    vbase2[ivertex + 0].mPosition = tri._v0;
    vbase2[ivertex + 0].mUV       = tri._tc0;
    vbase2[ivertex + 0].mNormal   = normal;

    vbase2[ivertex + 1].mPosition = tri._v1;
    vbase2[ivertex + 1].mUV       = tri._tc1;
    vbase2[ivertex + 1].mNormal   = normal;

    vbase2[ivertex + 2].mPosition = tri._v2;
    vbase2[ivertex + 2].mUV       = tri._tc2;
    vbase2[ivertex + 2].mNormal   = normal;

    aabb.Grow(tri._v0);
    aabb.Grow(tri._v1);
    aabb.Grow(tri._v2);

    ivertex += 3;
  }
  vw.UnLock(ctx);
  OrkAssert(ivertex == num_verts);

  // auto minst = std::make_shared<XgmModelInst>(model.get());

  auto material = std::make_shared<FreestyleMaterial>();
  material->gpuInit(ctx, "orkshader://ren_ambocc");
  auto tek_posnrm            = material->technique("tek_posnrm");
  auto tek_depmap            = material->technique("tek_depthmap");
  auto tek_depacc            = material->technique("tek_depthaccum");
  auto fxparameterMVP        = material->param("MatMVP");
  auto fxparameterPTMTX      = material->param("ProjectionTextureMatrix");
  auto fxparameterTextureDMAP    = material->param("mesh_depthmap");
  auto fxparameterTexturePOS = material->param("mesh_posmap");
  auto fxparameterNearFar    = material->param("nearFar");
  // auto fxparameterNumPolys = material->param("numpolys");

  int DIM         = 1024;
  auto pos_buffer = std::make_shared<RtGroup>(ctx, DIM, DIM, MsaaSamples::MSAA_1X);
  auto rtb_pos       = pos_buffer->createRenderTarget(EBufferFormat::RGBA32F);
  auto rtb_normal       = pos_buffer->createRenderTarget(EBufferFormat::RGBA32F);
  ctx->beginFrame();
  ctx->FBI()->PushRtGroup(pos_buffer.get());
  ctx->FBI()->Clear(fvec4(0, 0, 0, 0), 1.0);

  RenderContextFrameData RCFD(ctx);
  material->_rasterstate.SetCullTest(ECullTest::OFF);
  material->begin(tek_posnrm, RCFD);
  material->bindParamMatrix(fxparameterMVP, fmtx4::Identity());
  // material->bindParamCTex(fxparameterTexture, mesh_tex.get());
  // material->bindParamInt(fxparameterNumPolys, num_verts/3);
  ctx->RSI()->BindRasterState(material->_rasterstate, true);
  ctx->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, num_verts);
  material->end(RCFD);

  ctx->FBI()->PopRtGroup();
  ctx->endFrame();

  auto capbufPOS = std::make_shared<CaptureBuffer>();
  auto capbufNRM = std::make_shared<CaptureBuffer>();

  ctx->FBI()->captureAsFormat(rtb_pos.get(), capbufPOS.get(), EBufferFormat::RGBA32F);
  ctx->FBI()->captureAsFormat(rtb_normal.get(), capbufNRM.get(), EBufferFormat::RGBA32F);

  OrkAssert(capbufPOS->length() == DIM * DIM * 4 * 4);
  OrkAssert(capbufNRM->length() == DIM * DIM * 4 * 4);

  auto posdata = (const fvec4*)capbufPOS->_data;
  auto nrmdata = (const fvec4*)capbufNRM->_data;

  size_t num_pixels = DIM * DIM;

  std::mt19937 engine(123); // 123 is the seed
  std::uniform_int_distribution<int> dist(0, RAND_MAX);

  float aabox_size = aabb.GetSize().length();
  float radius     = aabox_size * 2.0f;

  auto dep_buffer = std::make_shared<RtGroup>(ctx, DIM, DIM, MsaaSamples::MSAA_1X);
  auto rtb_dep    = dep_buffer->createRenderTarget(EBufferFormat::R32F);

  auto acc_buffer = std::make_shared<RtGroup>(ctx, DIM, DIM, MsaaSamples::MSAA_1X);
  auto rtb_acc    = acc_buffer->createRenderTarget(EBufferFormat::R32F);

    ctx->FBI()->PushRtGroup(acc_buffer.get());
   ctx->FBI()->Clear(fvec4(0, 0, 0, 0), 1.0);
    ctx->FBI()->PopRtGroup();

  for (int i = 0; i < numsamples; i++) {

    ///////////////////////////
    // depth pass
    ///////////////////////////

    int ir1  = dist(engine);
    int ir2  = dist(engine);
    int ir3  = dist(engine);
    float r1 = float(ir1) / float(RAND_MAX);
    float r2 = float(ir2) / float(RAND_MAX);
    float r3 = float(ir3) / float(RAND_MAX);
    r1       = (r1 * 2.0f) - 1.0f;
    r2       = (r2 * 2.0f) - 1.0f;
    r3       = (r3 * 2.0f) - 1.0f;
    auto dir = fvec3(r1, r2, r3).normalized();
    auto xx  = dir.crossWith(fvec3(0.0f, 1.0f, 0.0f)).normalized();
    auto yy  = dir.crossWith(xx).normalized();
    auto zz  = dir.crossWith(yy).normalized();
    CameraData camdat;
    camdat.Lookat(aabb.center() + dir * 2.0f, aabb.center(), zz);
    camdat.Persp(0.1f, 100.0f, 90.0f);
    auto matrices = camdat.computeMatrices(1.0f);
    auto VP       = matrices.GetVPMatrix();
    RenderContextFrameData RCFD(ctx);
    material->_rasterstate.SetCullTest(ECullTest::OFF);

    ctx->beginFrame();
    ctx->FBI()->PushRtGroup(dep_buffer.get());

    material->begin(tek_depmap, RCFD);
    material->bindParamMatrix(fxparameterMVP, VP);
    ctx->RSI()->BindRasterState(material->_rasterstate, true);
    ctx->GBI()->DrawPrimitiveEML(vw2, PrimitiveType::TRIANGLES, num_verts);
    material->end(RCFD);
    ctx->FBI()->PopRtGroup();
    //ctx->endFrame();

    ///////////////////////////
    // accumulation pass
    ///////////////////////////

    //ctx->beginFrame();
    ctx->FBI()->PushRtGroup(acc_buffer.get());
    // ctx->FBI()->Clear(fvec4(0, 0, 0, 0), 1.0);

    material->begin(tek_depacc, RCFD);
    material->bindParamVec2(fxparameterNearFar, fvec2(.1, 100.0f));
    material->bindParamMatrix(fxparameterPTMTX, VP);
    material->bindParamCTex(fxparameterTextureDMAP, rtb_dep->texture());
    material->bindParamCTex(fxparameterTexturePOS, rtb_pos->texture());
    material->_rasterstate.SetBlending(Blending::ADDITIVE);
    ctx->RSI()->BindRasterState(material->_rasterstate, true);
    ctx->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES, num_verts);

    ctx->FBI()->PopRtGroup();
    ctx->endFrame();
  }

  ctx->FBI()->capture(rtb_acc.get(), "pos_buffer.png");
  // ctx->FBI()->capture(rtb_normal.get(), "nrm_buffer.png");

  // OrkAssert(false);
}
void computeLightMaps(meshutil::mesh_ptr_t model, Context* ctx) {
  // auto triangles = model_to_triangles(model);
}

} // namespace ork::lev2