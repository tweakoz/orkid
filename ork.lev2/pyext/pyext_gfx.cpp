////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
void pyinit_gfx_compositor(py::module& module_lev2);
void pyinit_gfx_material(py::module& module_lev2);
void pyinit_gfx_shader(py::module& module_lev2);
void pyinit_gfx_renderer(py::module& module_lev2);
void pyinit_gfx(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();
  pyinit_gfx_material(module_lev2);
  pyinit_gfx_shader(module_lev2);
  pyinit_gfx_renderer(module_lev2);
  pyinit_gfx_compositor(module_lev2);
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
          .def_static("loadingContext", []() -> ctx_t { return ctx_t(ork::lev2::contextForCurrentThread()); })
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
          [](const fbi_t& fbi, rtb_t& rtb, CaptureBuffer& capbuf) -> bool {
            return fbi.get()->capture(rtb.get(), &capbuf);
          })
      .def(
          "captureAsFormat",
          [](const fbi_t& fbi, rtb_t& rtb, CaptureBuffer& capbuf, std::string format) -> bool {
            auto crc_fmt = CrcString(format.c_str());
            return fbi.get()->captureAsFormat(rtb.get(), &capbuf, EBufferFormat(crc_fmt._hashed));
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
  py::class_<RtBuffer,rtb_t>(module_lev2, "RtBuffer")
      .def(
          "__repr__",
          [](const rtb_t& rtb) -> std::string {
            fxstring<256> fxs;
            fxs.format("RtBuffer(%p)", rtb.get());
            return fxs.c_str();
          })
      .def_property_readonly("texture", [](rtb_t& rtb) -> tex_t { return tex_t(rtb->texture()); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<RtGroup,rtg_t>(module_lev2, "RtGroup")
      .def("resize", [](rtg_t& rtg, int w, int h) { rtg.get()->Resize(w, h); })
      .def(
          "__repr__",
          [](const rtg_t& rtg) -> std::string {
            fxstring<256> fxs;
            fxs.format("RtGroup(%p)", rtg.get());
            return fxs.c_str();
          })
      .def("buffer", [](const rtg_t& rtg, int irtb) -> rtb_t { return rtg->GetMrt(irtb); });
      //.def("texture", [](const rtg_t& rtg, int irtb) -> tex_t { return rtg->GetMrt(irtb)->texture(); });
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
  auto texture_type = //
      py::class_<lev2::Texture,tex_t>(module_lev2, "Texture")
          .def(
              "__repr__",
              [](const tex_t& tex) -> std::string {
                fxstring<256> fxs;
                fxs.format(
                    "Texture(%p:\"%s\") w<%d> h<%d> d<%d>",
                    tex.get(),
                    tex->_debugName.c_str(),
                    tex->_width,
                    tex->_height,
                    tex->_depth);
                return fxs.c_str();
              })
          .def_static("load", [](std::string path) -> tex_t { return Texture::LoadUnManaged(path); });
  //using rawtexptr_t = Texture*;
  type_codec->registerStdCodec<tex_t>(texture_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct InstanceMatricesProxy {
    instanceddrawinstancedata_ptr_t _instancedata;
  };
  using matrixinstdata_ptr_t = std::shared_ptr<InstanceMatricesProxy>;
  auto matrixinstdata_type   = //
      py::class_<InstanceMatricesProxy, matrixinstdata_ptr_t>(
          module_lev2, //
          "InstancedMatrices",
          pybind11::buffer_protocol())
          .def_buffer([](InstanceMatricesProxy& proxy) -> pybind11::buffer_info {
            auto idata = proxy._instancedata;
            auto data  = idata->_worldmatrices.data(); // Pointer to buffer
            int count  = idata->_worldmatrices.size();
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                3,                                                       // Number of dimensions
                {count, 4, 4},                                           // Buffer dimensions
                {sizeof(float) * 16, sizeof(float) * 4, sizeof(float)}); // Strides (in bytes) for each index
          });
  type_codec->registerStdCodec<matrixinstdata_ptr_t>(matrixinstdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct InstanceColorsProxy {
    instanceddrawinstancedata_ptr_t _instancedata;
  };
  using colorsinstdata_ptr_t = std::shared_ptr<InstanceColorsProxy>;
  auto colorsinstdata_type   = //
      py::class_<InstanceColorsProxy, colorsinstdata_ptr_t>(
          module_lev2, //
          "InstanceColors",
          pybind11::buffer_protocol())
          .def_buffer([](InstanceColorsProxy& proxy) -> pybind11::buffer_info {
            auto idata = proxy._instancedata;
            auto data  = idata->_modcolors.data(); // Pointer to buffer
            int count  = idata->_modcolors.size();
            return pybind11::buffer_info(
                data,          // Pointer to buffer
                sizeof(float), // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                2,                                   // Number of dimensions
                {count, 4},                          // Buffer dimensions
                {sizeof(float) * 4, sizeof(float)}); // Strides (in bytes) for each index
          });
  type_codec->registerStdCodec<colorsinstdata_ptr_t>(colorsinstdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto instancedata_type = //
      py::class_<InstancedDrawableInstanceData, instanceddrawinstancedata_ptr_t>(
          module_lev2, //
          "InstancedDrawableInstanceData")
          .def_property_readonly(
              "matrices",
              [](instanceddrawinstancedata_ptr_t idata) -> matrixinstdata_ptr_t {
                auto proxy           = std::make_shared<InstanceMatricesProxy>();
                proxy->_instancedata = idata;
                return proxy;
              })
          .def_property_readonly("colors", [](instanceddrawinstancedata_ptr_t idata) -> colorsinstdata_ptr_t {
            auto proxy           = std::make_shared<InstanceColorsProxy>();
            proxy->_instancedata = idata;
            return proxy;
          });
  type_codec->registerStdCodec<instanceddrawinstancedata_ptr_t>(instancedata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto terdrawdata_type = //
      py::class_<TerrainDrawableData, terraindrawabledata_ptr_t>(module_lev2, "TerrainDrawableData")
       .def(py::init<>())
       .def_property("rock1",
                     [](terraindrawabledata_ptr_t drw) -> fvec3 {
                       return drw->_rock1;
                     },
                        [](terraindrawabledata_ptr_t drw, fvec3 val) {
                          drw->_rock1 = val;
                        }
      )
          .def(
              "writeHmapPath",
              [](terraindrawabledata_ptr_t drw, std::string path) {
                drw->_writeHmapPath(path);
              })
      ;
  type_codec->registerStdCodec<terraindrawabledata_ptr_t>(terdrawdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  /*auto terdrawinst_type = //
      py::class_<TerrainDrawableInst, terraindrawableinst_ptr_t>(module_lev2, "TerrainDrawableInst")
          .def(py::init<>([](terraindrawabledata_ptr_t data)->terraindrawableinst_ptr_t{
            return std::make_shared<TerrainDrawableInst>(data);
          }))
          // TODO - find shorter registration method for simple properties
          .def_property("worldHeight",
                        [](terraindrawableinst_ptr_t drwi) -> float {
                          return drwi->_worldHeight;
                        },
                        [](terraindrawableinst_ptr_t drwi, float val) {
                          drwi->_worldHeight = val;
                        }
          )
          .def_property("worldSizeXZ",
                        [](terraindrawableinst_ptr_t drwi) -> float {
                          return drwi->_worldSizeXZ;
                        },
                        [](terraindrawableinst_ptr_t drwi, float val) {
                          drwi->_worldSizeXZ = val;
                        }
          )
          .def(
              "createCallbackDrawable",
              [](terraindrawableinst_ptr_t drwi) {
                return drwi->createCallbackDrawable();
              })
  ;
  type_codec->registerStdCodec<terraindrawableinst_ptr_t>(terdrawinst_type);*/
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<PixelFetchContext>(module_lev2, "PixelFetchContext")
      .def(py::init<>())
      .def(py::init([](rtg_t& rtg, int mask) {
        auto pfc       = std::unique_ptr<PixelFetchContext>(new PixelFetchContext);
        pfc->_rtgroup  = rtg;
        pfc->miMrtMask = mask;
        return pfc;
      }))
      .def_property(
          "rtgroup",
          [](PixelFetchContext& pfc) -> rtg_t { return pfc._rtgroup; },
          [](PixelFetchContext& pfc, rtg_t& rtg) { pfc._rtgroup = rtg; })
      .def_property(
          "rtgmask",
          [](PixelFetchContext& pfc) -> int { return pfc.miMrtMask; },
          [](PixelFetchContext& pfc, int mask) { pfc.miMrtMask = mask; })
      .def(
          "color",
          [](const PixelFetchContext& pfc, int index) -> fvec4 {
            OrkAssert(index >= 0);
            OrkAssert(index < PixelFetchContext::kmaxitems);
            return pfc._pickvalues[index].get<fvec4>();
          })
      .def(
          "pointer",
          [](const PixelFetchContext& pfc, int index) -> fvec4 {
            OrkAssert(index >= 0);
            OrkAssert(index < PixelFetchContext::kmaxitems);
            return pfc._pickvalues[index].get<uint64_t>();
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
          { return vb_static_vtxa_t(size, 0, PrimitiveType::NONE); });
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
  py::class_<Drawable, drawable_ptr_t>(module_lev2, "Drawable");
  auto cbdrawable_type = //
      py::class_<CallbackDrawable, Drawable, callback_drawable_ptr_t>(module_lev2, "CallbackDrawable")
  ;
  type_codec->registerStdCodec<callback_drawable_ptr_t>(cbdrawable_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<LightData, lightdata_ptr_t>(module_lev2, "LightData")
      .def_property(
          "color",                                       //
          [](lightdata_ptr_t lightdata) -> fvec3_ptr_t { //
            auto color = std::make_shared<fvec3>(lightdata->mColor);
            return color;
          },
          [](lightdata_ptr_t lightdata, fvec3_ptr_t color) { //
            lightdata->mColor = *color.get();
          });
  py::class_<PointLightData, LightData, pointlightdata_ptr_t>(module_lev2, "PointLightData")
      .def(py::init<>())
      .def(
          "createNode",                      //
          [](pointlightdata_ptr_t lightdata, //
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::lightnode_ptr_t { //
            auto xfgen = []() -> fmtx4 { return fmtx4(); };
            auto light = std::make_shared<PointLight>(xfgen, lightdata.get());
            return layer->createLightNode(named, light);
          });
  py::class_<SpotLightData, LightData, spotlightdata_ptr_t>(module_lev2, "SpotLightData");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Light, light_ptr_t>(module_lev2, "Light")
      .def_property(
          "matrix",                              //
          [](light_ptr_t light) -> fmtx4_ptr_t { //
            auto copy = std::make_shared<fmtx4>(light->worldMatrix());
            return copy;
          },
          [](light_ptr_t light, fmtx4_ptr_t mtx) { //
            light->worldMatrix() = *mtx.get();
          });
  py::class_<PointLight, Light, pointlight_ptr_t>(module_lev2, "PointLight");
  py::class_<SpotLight, Light, spotlight_ptr_t>(module_lev2, "SpotLight");
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<XgmModel, model_ptr_t>(module_lev2, "Model") //
      .def(py::init([](const std::string& model_path) -> model_ptr_t {
        auto loadreq = std::make_shared<asset::LoadRequest>(model_path.c_str());
        auto modl_asset = asset::AssetManager<XgmModelAsset>::load(loadreq);
        return modl_asset->_model.atomicCopy();
      }))
      .def(
          "createNode",         //
          [](model_ptr_t model, //
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::node_ptr_t { //
            auto drw        = std::make_shared<ModelDrawable>(nullptr);
            drw->_modelinst = std::make_shared<XgmModelInst>(model.get());

            auto node = layer->createDrawableNode(named, drw);
            node->_userdata->makeValueForKey<model_ptr_t>("pyext.retain.model", model);
            return node;
          })
      .def(
          "createInstancedNode", //
          [](model_ptr_t model,  //
             int numinstances,
             std::string named,
             scenegraph::layer_ptr_t layer) -> scenegraph::drawable_node_ptr_t { //
            auto drw = std::make_shared<InstancedModelDrawable>();
            drw->bindModel(model);
            auto node = layer->createDrawableNode(named, drw);
            drw->resize(numinstances);
            auto instdata = drw->_instancedata;
            for (int i = 0; i < numinstances; i++) {
              instdata->_worldmatrices[i].compose(fvec3(0, 0, 0), fquat(), 0.0f);
            }
            return node;
          });
  /////////////////////////////////////////////////////////////////////////////////
  auto camdattype = //
      py::class_<CameraData, cameradata_ptr_t>(module_lev2, "CameraData")
          .def(py::init([]()->cameradata_ptr_t{ return std::make_shared<CameraData>(); }))
          .def(
              "perspective",                                                    //
              [](cameradata_ptr_t camera, float near, float ffar, float fovy) { //
                camera->Persp(near, ffar, fovy);
              })
          .def(
              "lookAt",                                                        //
              [](cameradata_ptr_t camera, fvec3& eye, fvec3& tgt, fvec3& up) { //
                camera->Lookat(eye, tgt, up);
              })
          .def(
              "projectDepthRay",                                              //
              [](cameradata_ptr_t camera, fvec2_ptr_t pos2d) -> fray3_ptr_t { //
                auto cammat = camera->computeMatrices(1280.0 / 720.0);
                auto rval   = std::make_shared<fray3>();
                cammat.projectDepthRay(*pos2d.get(), *rval.get());
                return rval;
              });
  type_codec->registerStdCodec<cameradata_ptr_t>(camdattype);
  /////////////////////////////////////////////////////////////////////////////////
  auto camdatluttype = //
      py::class_<CameraDataLut, cameradatalut_ptr_t>(module_lev2, "CameraDataLut")
          .def(py::init<>())
          .def("addCamera", [](cameradatalut_ptr_t lut, std::string key, cameradata_constptr_t camera) {
            (*lut)[key] = camera;
          })
          .def("create", [](cameradatalut_ptr_t lut, std::string key) -> cameradata_ptr_t {
            auto camera = lut->create(key);
            return camera;
          });
  type_codec->registerStdCodec<cameradatalut_ptr_t>(camdatluttype);
  /////////////////////////////////////////////////////////////////////////////////
  auto cammatstype = //
      py::class_<CameraMatrices, cameramatrices_ptr_t>(module_lev2, "CameraMatrices")
          .def(py::init([]()->cameramatrices_ptr_t{ //
             return std::make_shared<CameraMatrices>();
           }))
          .def(
              "setCustomProjection",                                             //
              [](cameramatrices_ptr_t cammats, fmtx4 matrix) { //
                cammats->setCustomProjection(matrix);
              })
          .def(
              "setCustomView",                                                        //
              [](cameramatrices_ptr_t cammats, fmtx4 matrix) { //
                cammats->setCustomView(matrix);
              });
  type_codec->registerStdCodec<cameramatrices_ptr_t>(cammatstype);
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
          .def_static("instance", []() -> inputmanager_ptr_t { return InputManager::instance(); })
          .def("inputGroup", [](inputmanager_ptr_t mgr, std::string named) { return mgr->inputGroup(named); });
  type_codec->registerStdCodec<inputmanager_ptr_t>(inpmgr_typ);
} // namespace ork::lev2
} // namespace ork::lev2
