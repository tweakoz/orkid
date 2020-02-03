#include <QWindow>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/material_freestyle.inl>

using namespace std::string_literals;
using namespace ork;
using namespace ork::lev2;
typedef SVtxV12C4T16 vtx_t; // position, vertex color, 2 UV sets
int main(int argc, char** argv) {
  auto qtapp  = OrkEzQtApp::create(argc, argv);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  FreestyleMaterial material;
  const FxShaderTechnique* fxtechnique = nullptr;
  const FxShaderParam* fxparameterMVP  = nullptr;
  Timer timer;
  //////////////////////////////////////////////////////////
  timer.Start();
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([&](Context* ctx) {
    material.gpuInit(ctx, "orkshader://solid");
    fxtechnique    = material.technique("vtxcolor");
    fxparameterMVP = material.param("MatMVP");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    material._rasterstate.SetCullTest(ECULLTEST_OFF);
  });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([&](const ui::DrawEvent& drwev) {
    auto context = drwev.GetTarget();
    RenderContextFrameData RCFD(context); // renderer per/frame data
    auto fbi  = context->FBI();           // FrameBufferInterface
    auto fxi  = context->FXI();           // FX Interface
    auto mtxi = context->MTXI();          // FX Interface
    auto gbi  = context->GBI();           // GeometryBuffer Interface
    ///////////////////////////////////////
    // compute view and projection matrices
    ///////////////////////////////////////
    float TARGW  = context->mainSurfaceWidth();
    float TARGH  = context->mainSurfaceHeight();
    float aspect = TARGW / TARGH;
    float phase  = timer.SecsSinceStart() * PI2 * 0.1f;
    fvec3 eye(sinf(phase) * 5.0f, 5.0f, -cosf(phase) * 5.0f);
    fvec3 tgt(0, 0, 0);
    fvec3 up(0, 1, 0);
    float N         = -1.0f;
    float P         = +1.0f;
    auto projection = mtxi->Persp(45, aspect, 0.1, 100.0);
    auto view       = mtxi->LookAt(eye, tgt, up);
    ///////////////////////////////////////
    // vertex colors
    ///////////////////////////////////////
    uint32_t red = fvec4::Red().GetABGRU32();
    uint32_t grn = fvec4::Green().GetABGRU32();
    uint32_t blu = fvec4::Blue().GetABGRU32();
    uint32_t yel = fvec4::Yellow().GetABGRU32();
    ///////////////////////////////////////
    // get shared dynamic vertex buffer
    ///////////////////////////////////////
    DynamicVertexBuffer<vtx_t>& vtxbuf = GfxEnv::GetRef().GetSharedDynamicVB();
    VtxWriter<vtx_t> vwriter;
    ///////////////////////////////////////
    // Draw!
    ///////////////////////////////////////
    fbi->SetClearColor(fvec4(0, 0, 0, 1));
    context->beginFrame();

    // create 4 quads in dynamic VB
    vwriter.Lock(context, &vtxbuf, 24); // reserve 24 verts (4 quads, or 8 triangles)

    auto writevtx = [&](float x, float y, float z, uint32_t c) { vwriter.AddVertex(vtx_t(x, y, z, 0, 0, 0, 0, c)); };

    writevtx(N, N, N, red);
    writevtx(P, N, N, red);
    writevtx(N, P, N, red);
    writevtx(N, P, N, red);
    writevtx(P, N, N, red);
    writevtx(P, P, N, red);

    writevtx(N, N, N, grn);
    writevtx(N, N, P, grn);
    writevtx(N, P, N, grn);
    writevtx(N, P, N, grn);
    writevtx(N, N, P, grn);
    writevtx(N, P, P, grn);

    writevtx(P, P, P, blu);
    writevtx(N, P, P, blu);
    writevtx(P, N, P, blu);
    writevtx(P, N, P, blu);
    writevtx(N, P, P, blu);
    writevtx(N, N, P, blu);

    writevtx(P, P, P, yel);
    writevtx(P, P, N, yel);
    writevtx(P, N, P, yel);
    writevtx(P, N, P, yel);
    writevtx(P, P, N, yel);
    writevtx(P, N, N, yel);

    vwriter.UnLock(context);

    // draw the verts as triangles
    material.bindTechnique(fxtechnique);
    material.begin(RCFD);
    material.bindParamMatrix(fxparameterMVP, view * projection);

    gbi->DrawPrimitiveEML(vwriter, EPRIM_TRIANGLES);
    material.end(RCFD);
    context->endFrame();
  });
  //////////////////////////////////////////////////////////
  qtapp->setRefreshPolicy({EREFRESH_FASTEST, -1});
  return qtapp->exec();
}
