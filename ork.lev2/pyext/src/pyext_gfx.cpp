////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/image.h>
#include <ork/math/cvector4.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
extern int _g_post_swap_wait_time;

void pyinit_gfx(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
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
          .def_static("loadingContext", [] -> ctx_t { return ctx_t(ork::lev2::contextForCurrentThread()); })
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
          .def("defaultRTG", [](ctx_t& c) -> rtgroup_ptr_t { return rtgroup_ptr_t(c.get()->_defaultRTG); })
          .def("resize", [](ctx_t& rtg, int w, int h) { rtg.get()->resizeMainSurface(w, h); })
          .def("FBI", [](ctx_t& c) -> fbi_t { return fbi_t(c.get()->FBI()); })
          .def("FXI", [](ctx_t& c) -> fxi_t { return fxi_t(c.get()->FXI()); })
          .def("GBI", [](ctx_t& c) -> gbi_t { return gbi_t(c.get()->GBI()); })
          .def("TXI", [](ctx_t& c) -> txi_t { return txi_t(c.get()->TXI()); })
          .def("RSI", [](ctx_t& c) -> rsi_t { return rsi_t(c.get()->RSI()); })
          .def("setPostSwapWaitTime", [](ctx_t& c, int wt) { 
            _g_post_swap_wait_time = wt;
          })
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
          [](const fbi_t& fbi, rtbuffer_ptr_t rtb, CaptureBuffer& capbuf) -> bool {
            return fbi.get()->capture(rtb.get(), &capbuf);
          })
      .def(
          "captureAsFormat",
          [](const fbi_t& fbi, rtbuffer_ptr_t rtb, CaptureBuffer& capbuf, std::string format) -> bool {
            auto crc_fmt = CrcString(format.c_str());
            return fbi.get()->captureAsFormat(rtb.get(), &capbuf, EBufferFormat(crc_fmt._hashed));
          })
      .def("clear", [](const fbi_t& fbi, const fcolor4& color, float depth) { return fbi.get()->Clear(color, depth); })
      .def("rtGroupPush", [](const fbi_t& fbi, rtgroup_ptr_t rtg) { return fbi.get()->PushRtGroup(rtg.get()); })
      .def("rtGroupPop", [](const fbi_t& fbi) { return fbi.get()->PopRtGroup(); })
      .def("rtGroupClear", [](const fbi_t& fbi, rtgroup_ptr_t rtg) { return fbi.get()->rtGroupClear(rtg.get()); })
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
      .def("drawTriangles", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES); })
      .def("drawTriangleStrip", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLESTRIP); })
      .def("drawLines", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, PrimitiveType::LINES); });
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
  py::class_<TextureInitData,textureinitdata_ptr_t>(module_lev2, "TextureInitData")
      .def(py::init<>())
      .def_property("width", [](textureinitdata_ptr_t tid) -> int { return tid->_w; }, [](textureinitdata_ptr_t tid, int w) { tid->_w = w; })
      .def_property("height", [](textureinitdata_ptr_t tid) -> int { return tid->_h; }, [](textureinitdata_ptr_t tid, int h) { tid->_h = h; })
      .def_property("depth", [](textureinitdata_ptr_t tid) -> int { return tid->_d; }, [](textureinitdata_ptr_t tid, int d) { tid->_d = d; })
      .def_property("src_format", [](textureinitdata_ptr_t tid) -> EBufferFormat { return tid->_src_format; }, [](textureinitdata_ptr_t tid, EBufferFormat fmt) { tid->_src_format = fmt; })
      .def_property("dst_format", [](textureinitdata_ptr_t tid) -> EBufferFormat { return tid->_dst_format; }, [](textureinitdata_ptr_t tid, EBufferFormat fmt) { tid->_dst_format = fmt; })
      .def_property("autogenmips", [](textureinitdata_ptr_t tid) -> bool { return tid->_autogenmips; }, [](textureinitdata_ptr_t tid, bool b) { tid->_autogenmips = b; });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<TextureArrayInitData,texturearrayinitdata_ptr_t>(module_lev2, "TextureArrayInitData")
      .def(py::init<>())
      .def(py::init([](py::list list) {
        texturearrayinitdata_ptr_t rval = std::make_shared<TextureArrayInitData>();
        for (int i = 0; i < list.size(); i++) {
          image_ptr_t img = list[i].cast<image_ptr_t>();
          rval->_slices.push_back(TextureArrayInitSubItem{0, img});
        }
        return rval;
      }))
      .def_property_readonly("size", [](texturearrayinitdata_ptr_t tid) -> int { return int(tid->_slices.size()); })
      .def("append", [](texturearrayinitdata_ptr_t tid, image_ptr_t img) { tid->_slices.push_back(TextureArrayInitSubItem{0, img}); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<txi_t>(module_lev2, "TextureInterface")
      .def(
          "createColorTexture",
          [](const txi_t& the_txi,                           //
             fvec4 color,                                    //
             int w,                                          //
             int h) -> texture_ptr_t {                       //
            return the_txi->createColorTexture(color, w, h); //
          })
      .def("updateTextureArraySlice", // 
           [](const txi_t& the_txi, //
              texture_ptr_t ptex, //
              int slice, //
              image_ptr_t img) { //
        the_txi->updateTextureArraySlice(ptex.get(), slice, img);
      })
      .def("__repr__", [](const txi_t& txi) -> std::string {
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
  py::class_<SRasterState>(module_lev2, "RasterState") //
      .def_property(
          "culltest",
          [](const SRasterState& state) -> crcstring_ptr_t { //
            auto crcstr = std::make_shared<CrcString>(uint64_t(state._culltest));
            return crcstr;
          },
          [](SRasterState& state, crcstring_ptr_t ctest) { //
            state._culltest = ECullTest(ctest->hashed());
          })
      .def_property(
          "depthtest",
          [](const SRasterState& state) -> crcstring_ptr_t { //
            auto crcstr = std::make_shared<CrcString>(uint64_t(state._depthtest));
            return crcstr;
          },
          [](SRasterState& state, crcstring_ptr_t ctest) { //
            state._depthtest = EDepthTest(ctest->hashed());
          })
      .def_property(
          "blending",
          [](const SRasterState& state) -> crcstring_ptr_t { //
            auto crcstr = std::make_shared<CrcString>(uint64_t(state._blending));
            return crcstr;
          },
          [](SRasterState& state, crcstring_ptr_t ctest) { //
            state._blending = Blending(ctest->hashed());
          })
      .def("__repr__", [](const SRasterState& state) -> std::string {
        fxstring<256> fxs;
        fxs.format("RasterState()");
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<RtBuffer, rtbuffer_ptr_t>(module_lev2, "RtBuffer")
      .def(
          "__repr__",
          [](rtbuffer_ptr_t rtb) -> std::string {
            fxstring<256> fxs;
            fxs.format("RtBuffer(%p)", rtb.get());
            return fxs.c_str();
          })
      .def_property_readonly("texture", [](rtbuffer_ptr_t rtb) -> texture_ptr_t { return rtb->_texture; });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<RtGroup, rtgroup_ptr_t>(module_lev2, "RtGroup")
      .def("resize", [](rtgroup_ptr_t rtg, int w, int h) { rtg.get()->Resize(w, h); })
      .def_property_readonly("width", [](rtgroup_ptr_t rtg) -> int { return int(rtg->width()); })
      .def_property_readonly("height", [](rtgroup_ptr_t rtg) -> int { return int(rtg->height()); })
      .def(
          "__repr__",
          [](rtgroup_ptr_t rtg) -> std::string {
            fxstring<256> fxs;
            fxs.format("RtGroup(%p)", rtg.get());
            return fxs.c_str();
          })
      .def_property_readonly("numBuffers", [](rtgroup_ptr_t rtg) -> int { return rtg->GetNumTargets(); })
      .def_property_readonly("depth_buffer", [](rtgroup_ptr_t rtg) -> rtbuffer_ptr_t { return rtg->_depthBuffer; })
      .def("mrt_buffer", [](rtgroup_ptr_t rtg, int irtb) -> rtbuffer_ptr_t { return rtg->GetMrt(irtb); });
  //.def("texture", [](rtgroup_ptr_t rtg, int irtb) -> texture_ptr_t { return rtg->GetMrt(irtb)->texture(); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<CaptureBuffer>(module_lev2, "CaptureBuffer", pybind11::buffer_protocol())
      .def(py::init<>())
      .def_buffer([](CaptureBuffer& capbuf) -> pybind11::buffer_info {
        pybind11::buffer_info rval;
        switch (capbuf.format()) {
          case EBufferFormat::RGBA8: {
            rval = pybind11::buffer_info(
                capbuf._data,          // Pointer to buffer
                sizeof(unsigned char), // Size of one scalar
                pybind11::format_descriptor<unsigned char>::format(),
                1,                 // Number of dimensions
                {capbuf.length()}, // Buffer dimensions
                {1});              // Strides (in bytes) for each index
            break;
          }
          case EBufferFormat::RGBA32F: {
            rval = pybind11::buffer_info(
                capbuf._data,  // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                     // Number of dimensions
                {capbuf.length() / 4}, // Buffer dimensions
                {4});                  // Strides (in bytes) for each index
            break;
          }
          case EBufferFormat::R32F: {
            rval = pybind11::buffer_info(
                capbuf._data,  // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                     // Number of dimensions
                {capbuf.length() / 4}, // Buffer dimensions
                {4});                  // Strides (in bytes) for each index
            break;
          }
          default:
            OrkAssert(false);
            break;
        }
        return rval;
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
  auto texture_asset_type = //
      py::class_<TextureAsset, ::ork::asset::Asset, textureassetptr_t>(module_lev2, "TextureAsset")
          .def_property_readonly("texture", [](textureassetptr_t ta) -> texture_ptr_t { return ta->_texture; });
  type_codec->registerStdCodec<textureassetptr_t>(texture_asset_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto texture_type = //
      py::class_<Texture, texture_ptr_t>(module_lev2, "Texture")
          .def(
              "__repr__",
              [](texture_ptr_t self) -> std::string {
                fxstring<256> fxs;
                fxs.format(
                    "Texture(%p:\"%s\") w<%d> h<%d> d<%d> fmt<%s>",
                    self.get(),
                    self->_debugName.c_str(),
                    self->_width,
                    self->_height,
                    self->_depth,
                    EBufferFormatToName(self->_texFormat).c_str());
                return fxs.c_str();
              })
          .def_property_readonly("width", [](texture_ptr_t self) -> int { return int(self->_width); })
          .def_property_readonly("height", [](texture_ptr_t self) -> int { return int(self->_height); })
          .def_static("load", [](std::string path) -> texture_ptr_t { return Texture::LoadUnManaged(path); })
          .def_static("declare", [](std::string path) -> texture_ptr_t { return nullptr; });
  // using rawtexptr_t = Texture*;
  type_codec->registerStdCodec<texture_ptr_t>(texture_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pfc_type = py::class_<PixelFetchContext, pixelfetchctx_ptr_t>(module_lev2, "PixelFetchContext")
                      .def_property(
                          "rtgroup",
                          [](pixelfetchctx_ptr_t pfc) -> rtgroup_ptr_t { return pfc->_rtgroup; },
                          [](pixelfetchctx_ptr_t pfc, rtgroup_ptr_t rtg) { pfc->_rtgroup = rtg; })
                      .def_property(
                          "rtgmask",
                          [](pixelfetchctx_ptr_t pfc) -> int { return pfc->miMrtMask; },
                          [](pixelfetchctx_ptr_t pfc, int mask) { pfc->miMrtMask = mask; })
                      .def_property_readonly("numValues", [](pixelfetchctx_ptr_t pfc) -> int { return pfc->_pickvalues.size(); })
                      .def(
                          "value",
                          [type_codec](pixelfetchctx_ptr_t pfc, int index) -> py::object {
                            OrkAssert(index >= 0);
                            OrkAssert(index < pfc->_pickvalues.size());
                            auto encoded = type_codec->encode(pfc->_pickvalues[index]);
                            return encoded;
                          })
                      .def(
                          "dump",
                          [](pixelfetchctx_ptr_t pfc) -> std::string {
                            std::string rval;
                            rval += FormatString("PixelFetchContext(%p){\n", &pfc);
                            rval += FormatString("  numvals: %zu\n", pfc->_pickvalues.size());
                            for (int i = 0; i < pfc->_pickvalues.size(); i++) {
                              auto& val = pfc->_pickvalues[i];
                              rval += FormatString("    val<%d> : %s\n", i, val.typestr().c_str());
                            }
                            rval += "}\n";
                            return rval;
                          })
                      .def("__repr__", [](pixelfetchctx_ptr_t pfc) -> std::string {
                        fxstring<256> fxs;
                        fxs.format("PixelFetchContext(%p)", &pfc);
                        return fxs.c_str();
                      });
  type_codec->registerStdCodec<pixelfetchctx_ptr_t>(pfc_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<VertexBufferBase>(module_lev2, "VertexBufferBase");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vtxa_t>(module_lev2, "VtxV12N12B12T8C4")
      .def(py::init<fvec3, fvec3, fvec3, fvec2, uint32_t>())
      .def_static(
          "staticBuffer",
          [](size_t size) -> vb_static_vtxa_t //
          { return vb_static_vtxa_t(size, 0); });
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
            FontMan::beginTextBlock(ctx.get(), maxchars);
          })
      .def_static(
          "endTextBlock",
          [](ctx_t& ctx) {
            FontMan::endTextBlock(ctx.get());
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
  auto inpgrp_typ = //
      py::class_<InputGroup, inputgroup_ptr_t>(module_lev2, "InputGroup")
          .def_property_readonly(
              "numchannels",
              [](inputgroup_ptr_t grp) -> int { //
                int rval = 0;
                grp->_channels.atomicOp([&rval](InputGroup::channelmap_t& chmap) { //
                  rval = chmap.size();
                });
                return rval;
              })
          .def("channel", [type_codec](inputgroup_ptr_t grp, std::string named) -> py::object { //
            svar64_t value;
            grp->_channels.atomicOp([&value, named, type_codec](InputGroup::channelmap_t& chmap) { //
              auto it = chmap.find(named);
              if (it != chmap.end()) {
                value = it->second._value;
              }
            });
            auto encoded = type_codec->encode(value);
            return encoded;
          });
  type_codec->registerStdCodec<inputgroup_ptr_t>(inpgrp_typ);
  /////////////////////////////////////////////////////////////////////////////////
  auto inpmgr_typ = //
      py::class_<InputManager, inputmanager_ptr_t>(module_lev2, "InputManager")
          .def_static("instance", [] -> inputmanager_ptr_t { return InputManager::instance(); })
          .def("inputGroup", [](inputmanager_ptr_t mgr, std::string named) { return mgr->inputGroup(named); });
  type_codec->registerStdCodec<inputmanager_ptr_t>(inpmgr_typ);
  /////////////////////////////////////////////////////////////////////////////////
  auto displaybuffer_typ = //
      py::class_<DisplayBuffer, displaybuffer_ptr_t>(module_lev2, "DisplayBuffer");
  type_codec->registerStdCodec<displaybuffer_ptr_t>(displaybuffer_typ);
  /////////////////////////////////////////////////////////////////////////////////
  auto window_typ = //
      py::class_<Window, DisplayBuffer, window_ptr_t>(module_lev2, "DisplayWindow");
  type_codec->registerStdCodec<window_ptr_t>(window_typ);
  /////////////////////////////////////////////////////////////////////////////////
  auto appwindow_typ = //
      py::class_<AppWindow, Window, appwindow_ptr_t>(module_lev2, "AppWindow")
          .def_property_readonly("rootWidget", [](appwindow_ptr_t appwin) -> uiwidget_ptr_t { //
            return appwin->_rootWidget;
          });
  type_codec->registerStdCodec<appwindow_ptr_t>(appwindow_typ);
  /////////////////////////////////////////////////////////////////////////////////
  auto dbufcontext_type = //
      py::class_<DrawQueueContext, dbufcontext_ptr_t>(module_lev2, "DrawQueueContext");
  type_codec->registerStdCodec<dbufcontext_ptr_t>(dbufcontext_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto gpuev_t = py::class_<GpuEvent, gpuevent_ptr_t>(module_lev2, "GpuEvent")
                     .def(
                         "__repr__",
                         [](gpuevent_ptr_t ev) -> std::string {
                           fxstring<256> fxs;
                           fxs.format("GpuEvent(%p)", ev.get());
                           return fxs.c_str();
                         })
                     .def_property_readonly("eventID", [](gpuevent_ptr_t ev) -> std::string { return ev->_eventID; });
  type_codec->registerStdCodec<gpuevent_ptr_t>(gpuev_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto gpuevsink_t = py::class_<GpuEventSink, gpueventsink_ptr_t>(module_lev2, "GpuEventSink")
                         .def(
                             "__repr__",
                             [](gpueventsink_ptr_t ev) -> std::string {
                               fxstring<256> fxs;
                               fxs.format("GpuEventSink(%p)", ev.get());
                               return fxs.c_str();
                             })
                         .def_property_readonly("eventID", [](gpueventsink_ptr_t ev) -> std::string { return ev->_eventID; });
  type_codec->registerStdCodec<gpueventsink_ptr_t>(gpuevsink_t);
  /////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
} // namespace ork::lev2
