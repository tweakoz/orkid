#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void pyinit_gfx_material(py::module& module_lev2);
void pyinit_gfx_shader(py::module& module_lev2);
void pyinit_gfx(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();
  pyinit_gfx_material(module_lev2);
  pyinit_gfx_shader(module_lev2);
  /////////////////////////////////////////////////////////////////////////////////
  auto refresh_policy_type = //
      py::enum_<ERefreshPolicy>(module_lev2, "RefreshPolicy")
          .value("RefreshFastest", EREFRESH_FASTEST)
          .value("RefreshWhenDirty", EREFRESH_WHENDIRTY)
          .value("RefreshFixedFPS", EREFRESH_FIXEDFPS)
          .export_values();
  type_codec->registerStdCodec<ERefreshPolicy>(refresh_policy_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto gfxenv_type = //
      py::class_<GfxEnv>(module_lev2, "GfxEnv")
          .def_readonly_static("ref", &GfxEnv::GetRef())
          .def("loadingContext", [](const GfxEnv& e) -> ctx_t { return ctx_t(GfxEnv::GetRef().loadingContext()); })
          .def("__repr__", [](const GfxEnv& e) -> std::string {
            fxstring<64> fxs;
            fxs.format("GfxEnv(%p)", &e);
            return fxs.c_str();
          });
  // type_codec->registerStdCodec<GfxEnv>(gfxenv_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto ctx_type = //
      py::class_<ctx_t>(module_lev2, "GfxContext")
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
          //.def(
          //    "topRCFD",
          //  [](ctx_t& c) -> rcfd_ptr_t {
          //  auto rcfd = c.get()->topRenderContextFrameData();
          // return rcfd_ptr_t(const_cast<RenderContextFrameData*>(rcfd));
          //})
          .def_property_readonly("frameIndex", [](ctx_t& c) -> int { return c.get()->GetTargetFrame(); })
          //.def_property("currentMaterial", [](ctx_t& c)&Context::currentMaterial, &Context::BindMaterial)
          .def("__repr__", [](const ctx_t& c) -> std::string {
            fxstring<64> fxs;
            fxs.format("Context(%p)", c.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<ctx_t>(ctx_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fbi_t>(module_lev2, "FrameBufferInterface")
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
          [](const fbi_t& fbi, rtg_t& rtg, int rtbindex, CaptureBuffer& capbuf, std::string format) -> bool {
            auto crc_fmt = CrcString(format.c_str());
            return fbi.get()->captureAsFormat(rtg.ref(), rtbindex, &capbuf, EBufferFormat(crc_fmt._hashed));
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
  py::class_<fxi_t>(module_lev2, "FxInterface").def("__repr__", [](const fxi_t& fxi) -> std::string {
    fxstring<256> fxs;
    fxs.format("FXI(%p)", fxi.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<gbi_t>(module_lev2, "GeometryBufferInterface")
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
      .def("drawTriangles", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, EPrimitiveType::TRIANGLES); })
      .def("drawTriangleStrip", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, EPrimitiveType::TRIANGLESTRIP); })
      .def("drawLines", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, EPrimitiveType::LINES); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vw_vtxa_t>(module_lev2, "Writer_V12N12B12T8C4")
      .def(
          "__repr__",
          [](const vw_vtxa_t& vw) -> std::string {
            fxstring<256> fxs;
            fxs.format("Writer_V12N12B12T8C4(%p)", &vw);
            return fxs.c_str();
          })
      .def("add", [](vw_vtxa_t& vw, vtxa_t& vtx) { vw.AddVertex(vtx); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<txi_t>(module_lev2, "TextureInterface").def("__repr__", [](const txi_t& txi) -> std::string {
    fxstring<256> fxs;
    fxs.format("TXI(%p)", txi.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<rsi_t>(module_lev2, "RasterStateInterface").def("__repr__", [](const rsi_t& rsi) -> std::string {
    fxstring<256> fxs;
    fxs.format("RSI(%p)", rsi.get());
    return fxs.c_str();
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<rtb_t>(module_lev2, "RtBuffer")
      .def(
          "__repr__",
          [](const rtb_t& rtb) -> std::string {
            fxstring<256> fxs;
            fxs.format("RtBuffer(%p)", rtb.get());
            return fxs.c_str();
          })
      .def_property_readonly("texture", [](rtb_t& rtb) -> tex_t { return tex_t(rtb->texture()); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<rtg_t>(module_lev2, "RtGroup")
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
  py::class_<CaptureBuffer>(module_lev2, "CaptureBuffer", pybind11::buffer_protocol())
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
  py::class_<tex_t>(module_lev2, "Texture")
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
  py::class_<RenderContextFrameData>(module_lev2, "RenderContextFrameData").def(py::init([](ctx_t& ctx) { //
    auto rcfd = std::unique_ptr<RenderContextFrameData>(new RenderContextFrameData(ctx.get()));
    return rcfd;
  }));
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<PixelFetchContext>(module_lev2, "PixelFetchContext")
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
  py::class_<VertexBufferBase>(module_lev2, "VertexBufferBase");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vtxa_t>(module_lev2, "VtxV12N12B12T8C4")
      .def(py::init<fvec3, fvec3, fvec3, fvec2, uint32_t>())
      .def_static(
          "staticBuffer",
          [](size_t size) -> vb_static_vtxa_t //
          { return vb_static_vtxa_t(size, 0, EPrimitiveType::NONE); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vb_static_vtxa_t, VertexBufferBase>(module_lev2, "VtxV12N12B12T8C4_StaticBuffer");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<FontMan>(module_lev2, "FontManager")
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
  /*py::class_<font_t>(module_lev2, "Font")
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
  py::class_<drwev_t>(module_lev2, "DrawEvent").def_property_readonly("context", [](drwev_t& event) -> ctx_t { //
    return ctx_t(event->GetTarget());
  });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<UpdateData>(module_lev2, "UpdateData");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Drawable, drawable_ptr_t>(module_lev2, "Drawable");
  /////////////////////////////////////////////////////////////////////////////////
  auto camdattype = //
      py::class_<CameraData, cameradata_ptr_t>(module_lev2, "CameraData")
          .def(py::init<>())
          .def(
              "perspective",                                                    //
              [](cameradata_ptr_t camera, float near, float ffar, float fovy) { //
                camera->Persp(near, ffar, fovy);
              })
          .def(
              "lookAt",                                                        //
              [](cameradata_ptr_t camera, fvec3& eye, fvec3& tgt, fvec3& up) { //
                camera->Lookat(eye, tgt, up);
              });
  type_codec->registerStdCodec<cameradata_ptr_t>(camdattype);
  /////////////////////////////////////////////////////////////////////////////////
  auto camdatluttype = //
      py::class_<CameraDataLut, cameradatalut_ptr_t>(module_lev2, "CameraDataLut")
          .def(py::init<>())
          .def("addCamera", [](cameradatalut_ptr_t lut, std::string key, cameradata_ptr_t camera) {
            lut->AddSorted(AddPooledString(key.c_str()), camera.get());
          });
  type_codec->registerStdCodec<cameradatalut_ptr_t>(camdatluttype);
}
} // namespace ork::lev2
