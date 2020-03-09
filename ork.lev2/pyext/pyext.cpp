#include <ork/pch.h>
#include <ork/python/context.h>
#include <ork/python/wraprawpointer.inl>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/fixedstring.hpp>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/application/application.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/object/Object.h>
#include <ork/file/fileenv.h>
#include <ork/file/filedevcontext.h>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/material_freestyle.inl>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/submesh.h>
#include <ork/lev2/gfx/primitives.inl>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/opq.h>

///////////////////////////////////////////////////////////////////////////////
namespace py = pybind11;
using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void ClassInit();
void GfxInit(const std::string& gfxlayer);
} // namespace ork::lev2

class Lev2PythonApplication : public ork::Application {
  RttiDeclareConcrete(Lev2PythonApplication, ork::Application);
};

void Lev2PythonApplication::Describe() {
}

INSTANTIATE_TRANSPARENT_RTTI(Lev2PythonApplication, "Lev2PythonApplication");

void lev2appinit() {
  ork::SetCurrentThreadName("main");

  static Lev2PythonApplication the_app;
  ApplicationStack::Push(&the_app);

  int argc      = 1;
  char* argv[1] = {"python3"};

  static ork::lev2::StdFileSystemInitalizer filesysteminit(argc, argv);

  ork::lev2::ClassInit();
  ork::rtti::Class::InitializeClasses();
  ork::lev2::GfxInit("");
  ork::lev2::FontMan::GetRef();
}

void lev2apppoll() {
  while (ork::opq::mainSerialQueue().Process()) {
  }
}

////////////////////////////////////////////////////////

PYBIND11_MODULE(orklev2, m) {
  using namespace ork;
  using namespace ork::lev2;
  using ctx_t            = ork::python::unmanaged_ptr<Context>;
  using fbi_t            = ork::python::unmanaged_ptr<FrameBufferInterface>;
  using gbi_t            = ork::python::unmanaged_ptr<GeometryBufferInterface>;
  using fxi_t            = ork::python::unmanaged_ptr<FxInterface>;
  using rsi_t            = ork::python::unmanaged_ptr<RasterStateInterface>;
  using txi_t            = ork::python::unmanaged_ptr<TextureInterface>;
  using tex_t            = ork::python::unmanaged_ptr<Texture>;
  using rtb_t            = ork::python::unmanaged_ptr<RtBuffer>;
  using rtg_t            = ork::python::unmanaged_ptr<RtGroup>;
  using font_t           = ork::python::unmanaged_ptr<Font>;
  using capbuf_t         = ork::python::unmanaged_ptr<CaptureBuffer>;
  using fxshader_t       = ork::python::unmanaged_ptr<FxShader>;
  using fxparam_t        = ork::python::unmanaged_ptr<FxShaderParam>;
  using fxtechnique_t    = ork::python::unmanaged_ptr<FxShaderTechnique>;
  using rcfd_ptr_t       = ork::python::unmanaged_ptr<RenderContextFrameData>;
  using fxparammap_t     = std::map<std::string, fxparam_t>;
  using fxtechniquemap_t = std::map<std::string, fxtechnique_t>;
  using vtxa_t           = SVtxV12N12B12T8C4;
  using vb_static_vtxa_t = StaticVertexBuffer<vtxa_t>;
  using vw_vtxa_t        = VtxWriter<vtxa_t>;
  using cstrref_t        = const std::string&;
  /////////////////////////////////////////////////////////////////////////////////
  m.doc() = "Orkid Lev2 Library (graphics,audio,vr,input,etc..)";
  /////////////////////////////////////////////////////////////////////////////////
  m.def("lev2appinit", &lev2appinit);
  m.def("lev2apppoll", &lev2apppoll);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<GfxEnv>(m, "GfxEnv")
      .def_readonly_static("ref", &GfxEnv::GetRef())
      .def("loadingContext", [](const GfxEnv& e) -> ctx_t { return ctx_t(GfxEnv::GetRef().loadingContext()); })
      .def("__repr__", [](const GfxEnv& e) -> std::string {
        fxstring<64> fxs;
        fxs.format("GfxEnv(%p)", &e);
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ctx_t>(m, "GfxContext")
      .def("mainSurfaceWidth", [](ctx_t& c) -> int { return c.get()->mainSurfaceWidth(); })
      .def("mainSurfaceHeight", [](ctx_t& c) -> int { return c.get()->mainSurfaceHeight(); })
      .def("makeCurrent", [](ctx_t& c) { c.get()->makeCurrentContext(); })
      .def("beginFrame", [](ctx_t& c) { return c.get()->beginFrame(); })
      .def("endFrame", [](ctx_t& c) { return c.get()->endFrame(); })
      .def("debugPushGroup", [](ctx_t& c, cstrref_t str) { return c.get()->debugPushGroup(str); })
      .def("debugPopGroup", [](ctx_t& c) { return c.get()->debugPopGroup(); })
      .def("debugMarker", [](ctx_t& c, cstrref_t str) { return c.get()->debugMarker(str); })
      .def("defaultRTG", [](ctx_t& c) -> rtg_t { return rtg_t(c.get()->_defaultRTG); })
      .def("resize", [](ctx_t& rtg, int w, int h) { rtg.get()->resizeMainSurface(w, h); })
      .def("FBI", [](ctx_t& c) -> fbi_t { return fbi_t(c.get()->FBI()); })
      .def("FXI", [](ctx_t& c) -> fxi_t { return fxi_t(c.get()->FXI()); })
      .def("GBI", [](ctx_t& c) -> gbi_t { return gbi_t(c.get()->GBI()); })
      .def("TXI", [](ctx_t& c) -> txi_t { return txi_t(c.get()->TXI()); })
      .def("RSI", [](ctx_t& c) -> rsi_t { return rsi_t(c.get()->RSI()); })
      //////////////////////
      // todo move to mtxi when we add it
      //////////////////////
      .def(
          "perspective",
          [](ctx_t& c, float fovy, float aspect, float near, float ffar) -> fmtx4 {
            fmtx4 rval = c.get()->MTXI()->Persp(fovy, aspect, near, ffar);
            return rval;
          })
      .def(
          "lookAt",
          [](ctx_t& c, fvec3& eye, fvec3& tgt, fvec3& up) -> fmtx4 {
            fmtx4 rval = c.get()->MTXI()->LookAt(eye, tgt, up);
            return rval;
          })
      //////////////////////
      .def(
          "topRCFD",
          [](ctx_t& c) -> rcfd_ptr_t {
            auto rcfd = c.get()->topRenderContextFrameData();
            return rcfd_ptr_t(const_cast<RenderContextFrameData*>(rcfd));
          })
      .def_property_readonly("frameIndex", [](ctx_t& c) -> int { return c.get()->GetTargetFrame(); })
      //.def_property("currentMaterial", [](ctx_t& c)&Context::currentMaterial, &Context::BindMaterial)
      .def("__repr__", [](const ctx_t& c) -> std::string {
        fxstring<64> fxs;
        fxs.format("Context(%p)", c.get());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<GfxMaterial>(m, "GfxMaterial")
      .def_property("name", &GfxMaterial::GetName, &GfxMaterial::SetName)
      .def("__repr__", [](const GfxMaterial& m) -> std::string {
        fxstring<64> fxs;
        fxs.format("GfxMaterial(%p:%s)", &m, m.mMaterialName.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<FreestyleMaterial, GfxMaterial>(m, "FreestyleMaterial")
      .def(py::init<>())
      .def(
          "gpuInit",
          [](FreestyleMaterial& m, ctx_t& c, file::Path& path) {
            m.gpuInit(c.get(), path);
            m._rasterstate.SetCullTest(ECULLTEST_OFF);
          })
      .def(
          "gpuInitFromShaderText",
          [](FreestyleMaterial& m, ctx_t& c, const std::string& name, const std::string& shadertext) {
            m.gpuInitFromShaderText(c.get(), name, shadertext);
            m._rasterstate.SetCullTest(ECULLTEST_OFF);
          })
      .def_property_readonly("shader", [](const FreestyleMaterial& m) -> fxshader_t { return fxshader_t(m._shader); })
      .def("bindTechnique", [](FreestyleMaterial& m, const fxtechnique_t& tek) { m.bindTechnique(tek.get()); })
      .def("bindParamFloat", [](FreestyleMaterial& m, fxparam_t& p, float value) { m.bindParamFloat(p.get(), value); })
      .def("bindParamVec2", [](FreestyleMaterial& m, fxparam_t& p, const fvec2& value) { m.bindParamVec2(p.get(), value); })
      .def("bindParamVec3", [](FreestyleMaterial& m, fxparam_t& p, const fvec3& value) { m.bindParamVec3(p.get(), value); })
      .def("bindParamVec4", [](FreestyleMaterial& m, fxparam_t& p, const fvec4& value) { m.bindParamVec4(p.get(), value); })
      .def("bindParamMatrix3", [](FreestyleMaterial& m, fxparam_t& p, const fmtx3& value) { m.bindParamMatrix(p.get(), value); })
      .def("bindParamMatrix4", [](FreestyleMaterial& m, fxparam_t& p, const fmtx4& value) { m.bindParamMatrix(p.get(), value); })
      .def(
          "bindParamTexture", [](FreestyleMaterial& m, fxparam_t& p, const tex_t& value) { m.bindParamCTex(p.get(), value.get()); })
      .def("begin", [](FreestyleMaterial& m, rcfd_ptr_t& rcfd) { m.begin(*rcfd.get()); })
      .def("end", [](FreestyleMaterial& m, rcfd_ptr_t& rcfd) { m.end(*rcfd.get()); })
      .def("__repr__", [](const FreestyleMaterial& m) -> std::string {
        fxstring<256> fxs;
        fxs.format("FreestyleMaterial(%p:%s)", &m, m.mMaterialName.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fxshader_t>(m, "FxShader")
      .def(py::init<>())
      .def_property_readonly("name", [](const fxshader_t& sh) -> std::string { return sh->mName; })
      .def_property_readonly(
          "params",
          [](const fxshader_t& sh) -> fxparammap_t {
            fxparammap_t rval;
            for (auto item : sh->_parameterByName) {
              // python has no concept of const
              //  so we must cast away constness
              rval[item.first] = fxparam_t(const_cast<FxShaderParam*>(item.second));
            }
            return rval;
          })
      .def(
          "param",
          [](const fxshader_t& sh, cstrref_t named) -> fxparam_t {
            auto it = sh->_parameterByName.find(named);
            fxparam_t rval(nullptr);
            if (it != sh->_parameterByName.end())
              rval = fxparam_t(const_cast<FxShaderParam*>(it->second));
            return rval;
          })
      .def_property_readonly(
          "techniques",
          [](const fxshader_t& sh) -> fxtechniquemap_t {
            fxtechniquemap_t rval;
            for (auto item : sh->_techniques) {
              // python has no concept of const
              //  so we must cast away constness
              rval[item.first] = fxtechnique_t(const_cast<FxShaderTechnique*>(item.second));
            }
            return rval;
          })
      .def(
          "technique",
          [](const fxshader_t& sh, cstrref_t named) -> fxtechnique_t {
            auto it = sh->_techniques.find(named);
            fxtechnique_t rval(nullptr);
            if (it != sh->_techniques.end())
              rval = fxtechnique_t(const_cast<FxShaderTechnique*>(it->second));
            return rval;
          })
      .def("__repr__", [](const fxshader_t& sh) -> std::string {
        fxstring<256> fxs;
        fxs.format("FxShader(%p:%s)", sh.get(), sh->mName.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fxparam_t>(m, "FxShaderParam")
      .def_property_readonly("name", [](const fxparam_t& p) -> std::string { return p->_name; })
      .def("__repr__", [](const fxparam_t& p) -> std::string {
        fxstring<256> fxs;
        fxs.format("FxShader(%p:%s)", p.get(), p->_name.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fxtechnique_t>(m, "FxShaderTechnique")
      .def_property_readonly("name", [](const fxtechnique_t& t) -> std::string { return t->mTechniqueName; })
      .def("__repr__", [](const fxtechnique_t& t) -> std::string {
        fxstring<256> fxs;
        fxs.format("FxShaderTechnique(%p:%s)", t.get(), t->mTechniqueName.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fbi_t>(m, "FrameBufferInterface")
      .def_property(
          "autoclear",
          [](const fbi_t& fbi) -> bool { return fbi.get()->GetAutoClear(); },
          [](fbi_t& fbi, bool value) { fbi.get()->SetAutoClear(value); })
      .def_property(
          "clearcolor",
          [](const fbi_t& fbi) -> fvec4 { return fbi.get()->GetClearColor(); },
          [](fbi_t& fbi, const fvec4& value) { fbi.get()->SetClearColor(value); })
      .def("capturePixel", [](const fbi_t& fbi, const fvec4& at, PixelFetchContext& pfc) { return fbi.get()->GetPixel(at, pfc); })
      .def(
          "captureBuffer",
          [](const fbi_t& fbi, rtg_t& rtg, int rtbindex, CaptureBuffer& capbuf) -> bool {
            return fbi.get()->capture(rtg.ref(), rtbindex, &capbuf);
          })
      .def(
          "captureAsFormat",
          [](const fbi_t& fbi, rtg_t& rtg, int rtbindex, CaptureBuffer& capbuf, int format) -> bool {
            return fbi.get()->captureAsFormat(rtg.ref(), rtbindex, &capbuf, EBufferFormat(format));
          })
      .def("clear", [](const fbi_t& fbi, const fcolor4& color, float depth) { return fbi.get()->Clear(color, depth); })
      .def("rtGroupPush", [](const fbi_t& fbi, rtg_t& rtg) { return fbi.get()->PushRtGroup(rtg.get()); })
      .def("rtGroupPop", [](const fbi_t& fbi) { return fbi.get()->PopRtGroup(); })
      .def("rtGroupClear", [](const fbi_t& fbi, rtg_t& rtg) { return fbi.get()->rtGroupClear(rtg.get()); })
      .def("__repr__", [](const fbi_t& fbi) -> std::string {
        fxstring<256> fxs;
        fxs.format("FBI(%p)", fbi.get());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fxi_t>(m, "FxInterface").def("__repr__", [](const fxi_t& fxi) -> std::string {
    fxstring<256> fxs;
    fxs.format("FXI(%p)", fxi.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<gbi_t>(m, "GeometryBufferInterface")
      .def(
          "__repr__",
          [](const gbi_t& gbi) -> std::string {
            fxstring<256> fxs;
            fxs.format("GBI(%p)", gbi.get());
            return fxs.c_str();
          })
      .def(
          "lock",
          [](gbi_t gbi, vb_static_vtxa_t& vb, int icount) -> vw_vtxa_t {
            vw_vtxa_t vw;
            vw.Lock(gbi.get(), &vb, icount);
            return vw;
          })
      .def("unlock", [](gbi_t gbi, vw_vtxa_t& vw) { vw.UnLock(gbi.get()); })
      .def("drawTriangles", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, EPRIM_TRIANGLES); })
      .def("drawTriangleStrip", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, EPRIM_TRIANGLESTRIP); })
      .def("drawLines", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, EPRIM_LINES); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vw_vtxa_t>(m, "Writer_V12N12B12T8C4")
      .def(
          "__repr__",
          [](const vw_vtxa_t& vw) -> std::string {
            fxstring<256> fxs;
            fxs.format("Writer_V12N12B12T8C4(%p)", &vw);
            return fxs.c_str();
          })
      .def("add", [](vw_vtxa_t& vw, vtxa_t& vtx) { vw.AddVertex(vtx); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<txi_t>(m, "TextureInterface").def("__repr__", [](const txi_t& txi) -> std::string {
    fxstring<256> fxs;
    fxs.format("TXI(%p)", txi.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<rsi_t>(m, "RasterStateInterface").def("__repr__", [](const rsi_t& rsi) -> std::string {
    fxstring<256> fxs;
    fxs.format("RSI(%p)", rsi.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<rtb_t>(m, "RtBuffer")
      .def(
          "__repr__",
          [](const rtb_t& rtb) -> std::string {
            fxstring<256> fxs;
            fxs.format("RtBuffer(%p)", rtb.get());
            return fxs.c_str();
          })
      .def_property_readonly("texture", [](rtb_t& rtb) -> tex_t { return tex_t(rtb->texture()); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<rtg_t>(m, "RtGroup")
      .def("resize", [](rtg_t& rtg, int w, int h) { rtg.get()->Resize(w, h); })
      .def(
          "__repr__",
          [](const rtg_t& rtg) -> std::string {
            fxstring<256> fxs;
            fxs.format("RtGroup(%p)", rtg.get());
            return fxs.c_str();
          })
      .def("buffer", [](const rtg_t& rtg, int irtb) -> rtb_t { return rtg->GetMrt(irtb); })
      .def("texture", [](const rtg_t& rtg, int irtb) -> tex_t { return rtg->GetMrt(irtb)->texture(); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<CaptureBuffer>(m, "CaptureBuffer", pybind11::buffer_protocol())
      .def(py::init<>())
      .def_buffer([](CaptureBuffer& capbuf) -> pybind11::buffer_info {
        return pybind11::buffer_info(
            capbuf._data,          // Pointer to buffer
            sizeof(unsigned char), // Size of one scalar
            pybind11::format_descriptor<unsigned char>::format(),
            1,                 // Number of dimensions
            {capbuf.length()}, // Buffer dimensions
            {1});              // Strides (in bytes) for each index
      })
      .def_property_readonly("length", [](CaptureBuffer& capbuf) -> int { return int(capbuf.length()); })
      .def_property_readonly("width", [](CaptureBuffer& capbuf) -> int { return int(capbuf.width()); })
      .def_property_readonly("height", [](CaptureBuffer& capbuf) -> int { return int(capbuf.height()); })
      .def_property_readonly("format", [](CaptureBuffer& capbuf) -> int { return int(capbuf.format()); })
      .def("__len__", [](const CaptureBuffer& capbuf) -> int { return int(capbuf.length()); })
      .def("__repr__", [](const CaptureBuffer& capbuf) -> std::string {
        fxstring<256> fxs;
        fxs.format("CaptureBuffer(%p)", &capbuf);
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<tex_t>(m, "Texture")
      .def(
          "__repr__",
          [](const tex_t& tex) -> std::string {
            fxstring<256> fxs;
            fxs.format(
                "Texture(%p:\"%s\") w<%d> h<%d> d<%d>", tex.get(), tex->_debugName.c_str(), tex->_width, tex->_height, tex->_depth);
            return fxs.c_str();
          })
      .def_static("load", [](std::string path) -> tex_t { return Texture::LoadUnManaged(path); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<rcfd_ptr_t>(m, "RenderContextFrameData").def("__repr__", [](const rcfd_ptr_t& rcfd) -> std::string {
    fxstring<256> fxs;
    fxs.format("RCFD(%p)", rcfd.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<PixelFetchContext>(m, "PixelFetchContext")
      .def(py::init<>())
      .def(py::init([](rtg_t& rtg, int mask) {
        auto pfc       = std::unique_ptr<PixelFetchContext>(new PixelFetchContext);
        pfc->mRtGroup  = rtg.get();
        pfc->miMrtMask = mask;
        return pfc;
      }))
      .def_property(
          "rtgroup",
          [](PixelFetchContext& pfc) -> rtg_t { return pfc.mRtGroup; },
          [](PixelFetchContext& pfc, rtg_t& rtg) { pfc.mRtGroup = rtg.get(); })
      .def_property(
          "rtgmask",
          [](PixelFetchContext& pfc) -> int { return pfc.miMrtMask; },
          [](PixelFetchContext& pfc, int mask) { pfc.miMrtMask = mask; })
      .def(
          "color",
          [](const PixelFetchContext& pfc, int index) -> fvec4 {
            OrkAssert(index >= 0);
            OrkAssert(index < PixelFetchContext::kmaxitems);
            return pfc.mPickColors[index];
          })
      .def("__repr__", [](const PixelFetchContext& pfc) -> std::string {
        fxstring<256> fxs;
        fxs.format("PixelFetchContext(%p)", &pfc);
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<VertexBufferBase>(m, "VertexBufferBase");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vtxa_t>(m, "VtxV12N12B12T8C4")
      .def(py::init<fvec3, fvec3, fvec3, fvec2, uint32_t>())
      .def_static(
          "staticBuffer",
          [](size_t size) -> vb_static_vtxa_t //
          { return vb_static_vtxa_t(size, 0, EPRIM_NONE); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vb_static_vtxa_t, VertexBufferBase>(m, "VtxV12N12B12T8C4_StaticBuffer");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<FontMan>(m, "FontManager")
      .def_static("gpuInit", [](ctx_t& ctx) { FontMan::gpuInit(ctx.get()); })
      .def_static(
          "beginTextBlock",
          [](ctx_t& ctx, const std::string& fontid, fvec4 color, int uiw, int uih, int maxchars) {
            ctx->MTXI()->PushMMatrix(fmtx4());
            ctx->MTXI()->PushUIMatrix(uiw, uih);
            ctx->PushModColor(color);
            FontMan::PushFont(fontid);
            FontMan::BeginTextBlock(ctx.get(), maxchars);
          })
      .def_static(
          "endTextBlock",
          [](ctx_t& ctx) {
            FontMan::EndTextBlock(ctx.get());
            FontMan::PopFont();
            ctx->PopModColor();
            ctx->MTXI()->PopMMatrix();
            ctx->MTXI()->PopUIMatrix();
          })
      .def_static("draw", [](ctx_t& ctx, int x, int y, std::string text) { FontMan::DrawText(ctx.get(), x, y, text.c_str()); });
  /////////////////////////////////////////////////////////////////////////////////
  /*py::class_<font_t>(m, "Font")
      .def(
          "__repr__",
          [](const font_t& font) -> std::string {
            fxstring<256> fxs;
            fxs.format("Font(\"%s\")", font->msFontName.c_str());
            return fxs.c_str();
          })
      .def("bind", [](const font_t& font) { return FontMan::GetRef().bindFont(font.get()); })
      .def_property_readonly("filename", [](const font_t& font) -> std::string { return font->msFileName; })
      .def_property_readonly("fontname", [](const font_t& font) -> std::string { return font->msFontName; })
      .def_property_readonly("charWidth", [](const font_t& font) -> int { return font->mFontDesc.miCharWidth; })
      .def_property_readonly("charHeight", [](const font_t& font) -> int { return font->mFontDesc.miCharHeight; })
      .def_property_readonly("cellWidth", [](const font_t& font) -> int { return font->mFontDesc.miCellWidth; })
      .def_property_readonly("cellHeight", [](const font_t& font) -> int { return font->mFontDesc.miCellHeight; })
      .def_property_readonly("advanceWidth", [](const font_t& font) -> int { return font->mFontDesc.miAdvanceWidth; })
      .def_property_readonly("advanceHeight", [](const font_t& font) -> int { return font->mFontDesc.miAdvanceHeight; });
*/
  /////////////////////////////////////////////////////////////////////////////////
  auto meshutil = m.def_submodule("meshutil", "Mesh operations");
  {
    meshutil.def("triangulate", [](const MeshUtil::submesh& inpsubmesh, MeshUtil::submesh& outsubmesh) {
      MeshUtil::submeshTriangulate(inpsubmesh, outsubmesh);
    });
    /////////////////////////////////////////////////////////////////////////////////
    py::class_<MeshUtil::PrimitiveV12N12B12T8C4>(meshutil, "PrimitiveV12N12B12T8C4")
        .def(py::init<>())
        .def(py::init([](MeshUtil::submesh& submesh, ctx_t context) {
          auto prim = std::unique_ptr<MeshUtil::PrimitiveV12N12B12T8C4>(new MeshUtil::PrimitiveV12N12B12T8C4);
          prim->fromSubMesh(submesh, context.get());
          return prim;
        }))
        .def(
            "fromSubMesh",
            [](MeshUtil::PrimitiveV12N12B12T8C4& prim, const MeshUtil::submesh& submesh, Context* context) {
              prim.fromSubMesh(submesh, context);
            })
        .def("draw", [](MeshUtil::PrimitiveV12N12B12T8C4& prim, ctx_t context) { prim.draw(context.get()); });
    /////////////////////////////////////////////////////////////////////////////////
    py::class_<MeshUtil::submesh>(meshutil, "SubMesh")
        .def(py::init<>())
        .def("numPolys", [](const MeshUtil::submesh& submesh, int numsides = 0) -> int { return submesh.GetNumPolys(numsides); })
        .def("numVertices", [](const MeshUtil::submesh& submesh) -> int { return submesh.mvpool.GetNumVertices(); })
        .def(
            "writeObj",
            [](const MeshUtil::submesh& submesh, const std::string& outpath) { return submeshWriteObj(submesh, outpath); })
        .def(
            "addQuad",
            [](MeshUtil::submesh& submesh,
               fvec3 p0,
               fvec3 p1,
               fvec3 p2,
               fvec3 p3,
               fvec2 uv0,
               fvec2 uv1,
               fvec2 uv2,
               fvec2 uv3,
               fvec4 c) { return submesh.addQuad(p0, p1, p2, p3, uv0, uv1, uv2, uv3, c); })
        .def(
            "addQuad",
            [](MeshUtil::submesh& submesh,
               fvec3 p0,
               fvec3 p1,
               fvec3 p2,
               fvec3 p3,
               fvec3 n0,
               fvec3 n1,
               fvec3 n2,
               fvec3 n3,
               fvec2 uv0,
               fvec2 uv1,
               fvec2 uv2,
               fvec2 uv3,
               fvec4 c) { return submesh.addQuad(p0, p1, p2, p3, n0, n1, n2, n3, uv0, uv1, uv2, uv3, c); });

    /////////////////////////////////////////////////////////////////////////////////
    py::class_<MeshUtil::vertexpool>(meshutil, "VertexPool").def(py::init<>());
    /////////////////////////////////////////////////////////////////////////////////
    py::class_<MeshUtil::poly>(meshutil, "Poly").def(py::init<>());
    /////////////////////////////////////////////////////////////////////////////////
    py::class_<MeshUtil::edge>(meshutil, "Edge").def(py::init<>());
    /////////////////////////////////////////////////////////////////////////////////
  }
  /////////////////////////////////////////////////////////////////////////////////
  auto primitives = m.def_submodule("primitives", "BuiltIn Primitives");
  {
    /////////////////////////////////////////////////////////////////////////////////
    py::class_<primitives::CubePrimitive>(primitives, "CubePrimitive")
        .def(py::init<>())
        .def_property(
            "size",
            [](const primitives::CubePrimitive& prim) -> float { return prim._size; },
            [](primitives::CubePrimitive& prim, const float& value) { prim._size = value; })

        .def_property(
            "topColor",
            [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorTop; },
            [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorTop = value; })

        .def_property(
            "bottomColor",
            [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorBottom; },
            [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorBottom = value; })

        .def_property(
            "frontColor",
            [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorFront; },
            [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorFront = value; })

        .def_property(
            "backColor",
            [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorBack; },
            [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorBack = value; })

        .def_property(
            "leftColor",
            [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorLeft; },
            [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorLeft = value; })

        .def_property(
            "rightColor",
            [](const primitives::CubePrimitive& prim) -> fvec4 { return prim._colorRight; },
            [](primitives::CubePrimitive& prim, const fvec4& value) { prim._colorRight = value; })

        .def("gpuInit", [](primitives::CubePrimitive& prim, ctx_t& context) { prim.gpuInit(context.get()); })
        .def("draw", [](primitives::CubePrimitive& prim, ctx_t& context) { prim.draw(context.get()); });
    /////////////////////////////////////////////////////////////////////////////////
    py::class_<primitives::FrustumPrimitive>(primitives, "FrustumPrimitive")
        .def(py::init<>())
        .def_property(
            "frustum",
            [](const primitives::FrustumPrimitive& prim) -> Frustum { return prim._frustum; },
            [](primitives::FrustumPrimitive& prim, const Frustum& value) { prim._frustum = value; })

        .def_property(
            "topColor",
            [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorTop; },
            [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorTop = value; })

        .def_property(
            "bottomColor",
            [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorBottom; },
            [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorBottom = value; })

        .def_property(
            "frontColor",
            [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorFront; },
            [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorFront = value; })

        .def_property(
            "backColor",
            [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorBack; },
            [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorBack = value; })

        .def_property(
            "leftColor",
            [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorLeft; },
            [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorLeft = value; })

        .def_property(
            "rightColor",
            [](const primitives::FrustumPrimitive& prim) -> fvec4 { return prim._colorRight; },
            [](primitives::FrustumPrimitive& prim, const fvec4& value) { prim._colorRight = value; })

        .def("gpuInit", [](primitives::FrustumPrimitive& prim, ctx_t& context) { prim.gpuInit(context.get()); })
        .def("draw", [](primitives::FrustumPrimitive& prim, ctx_t& context) { prim.draw(context.get()); });
    /////////////////////////////////////////////////////////////////////////////////
  }
  /////////////////////////////////////////////////////////////////////////////////
};
