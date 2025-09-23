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
          .def_property_readonly("FBI", [](ctx_t& c) -> fbi_t { return fbi_t(c.get()->FBI()); })
          .def_property_readonly("FXI", [](ctx_t& c) -> fxi_t { return fxi_t(c.get()->FXI()); })
          .def_property_readonly("GBI", [](ctx_t& c) -> gbi_t { return gbi_t(c.get()->GBI()); })
          .def_property_readonly("TXI", [](ctx_t& c) -> txi_t { return txi_t(c.get()->TXI()); })
          .def_property_readonly("CI", [](ctx_t& c) -> ci_t { return ci_t(c.get()->CI()); })
          .def("setPostSwapWaitTime", [](ctx_t& c, int wt) { _g_post_swap_wait_time = wt; })
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
          .def_property_readonly("topRCFD", [](ctx_t& c) -> rcfd_ptr_t { return c.get()->topRenderContextFrameData(); }) //
          //////////////////////
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
      .def(
          "capturePixel",
          [](const fbi_t& fbi, pixelfetchctx_ptr_t pfc, int x, int y) -> captureasync_ptr_t {
            // Create a capture async future for single pixel capture
            // The implementation will populate _pixelFetchContext when complete
            return fbi.get()->capturePixelAsync(pfc, x, y);
          })
      .def(
          "captureBuffer",
          [](const fbi_t& fbi, rtbuffer_ptr_t rtb, capturebuffer_ptr_t capbuf) -> captureasync_ptr_t {
            return fbi.get()->capture(rtb.get(), capbuf);
          })
      .def(
          "captureToFile",
          [](const fbi_t& fbi, rtbuffer_ptr_t rtb, const file::Path& pth) -> captureasync_ptr_t {
            return fbi.get()->capture(rtb.get(), pth);
          })
      .def(
          "captureAsFormat",
          [](const fbi_t& fbi, rtbuffer_ptr_t rtb, capturebuffer_ptr_t capbuf, std::string format) -> captureasync_ptr_t {
            auto crc_fmt = CrcString(format.c_str());
            return fbi.get()->captureAsFormat(rtb.get(), capbuf, EBufferFormat(crc_fmt._hashed));
          })
      //.def("clear", [](const fbi_t& fbi, const fcolor4& color, float depth) { return fbi.get()->Clear(color, depth); })
      .def("rtGroupPush", [](const fbi_t& fbi, rtgroup_ptr_t rtg) { return fbi.get()->PushRtGroup(rtg.get()); })
      .def("rtGroupPop", [](const fbi_t& fbi) { return fbi.get()->PopRtGroup(); })
      .def(
          "rtGroupInit",
          [](const fbi_t& fbi, rtgroup_ptr_t rtg) {
            fbi.get()->PushRtGroup(rtg.get());
            fbi.get()->PopRtGroup();
          })
      .def("rtGroupClear", [](const fbi_t& fbi, rtgroup_ptr_t rtg) { return fbi.get()->rtGroupClear(rtg.get()); })
      .def("__repr__", [](const fbi_t& fbi) -> std::string {
        fxstring<256> fxs;
        fxs.format("FBI(%p)", fbi.get());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<fxi_t>(module_lev2, "FxInterface")
      .def(
          "__repr__",
          [](const fxi_t& fxi) -> std::string {
            fxstring<256> fxs;
            fxs.format("FXI(%p)", fxi.get());
            return fxs.c_str();
          })
      .def(
          "createShaderStorageBufferWithLength",
          [](fxi_t& fxi, size_t length) -> fxshaderstoragebuffer_ptr_t {
            return fxshaderstoragebuffer_ptr_t(fxi.get()->createStorageBuffer(length));
          })
      .def(
          "copyDataIntoShaderStorageBuffer",
          [](fxi_t& fxi, py::object data, fxshaderstoragebuffer_ptr_t buffer, size_t dest_offset) { //
            if (py::isinstance<py::float_>(data)) {
              auto as_float  = data.cast<py::float_>();
              auto datablock = std::make_shared<DataBlock>();
              datablock->addItem<float>(as_float);
              fxi.get()->copyBufferIntoStorageBuffer(buffer.get(), datablock->_storage, dest_offset);
            } else {
              auto type_str = data.get_type().attr("__name__").cast<std::string>();
              printf("copyDataIntoShaderStorageBuffer unknown type<%s>\n", type_str.c_str());
              OrkAssert(false);
            }
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
      .def(
          "lockVU32",
          [](gbi_t gbi, vtxbufferbase_ptr_t& vb, int ibase, int icount) -> py::memoryview {
            OrkAssert(vb->GetVtxSize() == sizeof(SVtxVU32));
            OrkAssert(vb->GetNumVertices() >= ibase + icount);
            int ibasebytes = ibase * vb->GetVtxSize();
            int isizebytes = icount * vb->GetVtxSize();
            auto pu32      = (uint32_t*)gbi.get()->LockVB(*vb, ibase, icount);
            OrkAssert(pu32);
            // create a buffer info object
            auto b = py::memoryview::from_buffer(
                pu32,
                sizeof(uint32_t),
                py::format_descriptor<uint32_t>::value,
                /*shape*/ py::detail::any_container<ssize_t>{icount},
                py::detail::any_container<ssize_t>{0} // strides
            );
            return b;
          })
      .def("unlockVB", [](gbi_t gbi, vtxbufferbase_ptr_t& vb) { gbi->UnLockVB(*vb); })
      .def("unlock", [](gbi_t gbi, vw_vtxa_t& vw) { vw.UnLock(gbi.get()); })
      .def("drawTriangles", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES); })
      .def("drawTriangleStrip", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLESTRIP); })
      .def("drawLines", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, PrimitiveType::LINES); });
  //.def("copyTensorIntoStorageBuffer", [](gbi_t gbi, torchtensor_ptr_t tensor, fxshaderstoragebuffer_ptr_t buffer) {
  // ci.get()->copyTensorIntoStorageBuffer(buffer.get(), tensor); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ci_t>(module_lev2, "ComputeInterface")
      .def(
          "__repr__",
          [](const ci_t& gbi) -> std::string {
            fxstring<256> fxs;
            fxs.format("CI(%p)", gbi.get());
            return fxs.c_str();
          })
#if defined(ENABLE_PYTORCH)
      .def(
          "createShaderStorageBufferFromTensor",
          [](ci_t& ci, torchtensor_ptr_t tensor) -> fxshaderstoragebuffer_ptr_t {
            return fxshaderstoragebuffer_ptr_t(ci.get()->storageBufferFromTensor(tensor));
          })
      .def(
          "copyTensorIntoShaderStorageBuffer",
          [](ci_t& ci, torchtensor_ptr_t tensor, fxshaderstoragebuffer_ptr_t buffer, size_t dest_offset) {
            ci.get()->copyTensorIntoStorageBuffer(buffer.get(), tensor, dest_offset);
          })
#endif
      .def("dispatch", [](ci_t& ci, pyfxcomputeshader_ptr_t csh, uint32_t numx, uint32_t numy, uint32_t numz) {
        ci.get()->dispatchCompute(csh.get(), numx, numy, numz);
      });

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
  py::class_<TextureInitData, textureinitdata_ptr_t>(module_lev2, "TextureInitData")
      .def(py::init<>())
      .def_property(
          "width", [](textureinitdata_ptr_t tid) -> int { return tid->_w; }, [](textureinitdata_ptr_t tid, int w) { tid->_w = w; })
      .def_property(
          "height", [](textureinitdata_ptr_t tid) -> int { return tid->_h; }, [](textureinitdata_ptr_t tid, int h) { tid->_h = h; })
      .def_property(
          "depth", [](textureinitdata_ptr_t tid) -> int { return tid->_d; }, [](textureinitdata_ptr_t tid, int d) { tid->_d = d; })
      .def_property(
          "src_format",
          [](textureinitdata_ptr_t tid) -> EBufferFormat { return tid->_src_format; },
          [](textureinitdata_ptr_t tid, EBufferFormat fmt) { tid->_src_format = fmt; })
      .def_property(
          "dst_format",
          [](textureinitdata_ptr_t tid) -> EBufferFormat { return tid->_dst_format; },
          [](textureinitdata_ptr_t tid, EBufferFormat fmt) { tid->_dst_format = fmt; })
      .def_property(
          "autogenmips",
          [](textureinitdata_ptr_t tid) -> bool { return tid->_autogenmips; },
          [](textureinitdata_ptr_t tid, bool b) { tid->_autogenmips = b; });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<TextureArrayInitData, texturearrayinitdata_ptr_t>(module_lev2, "TextureArrayInitData")
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
      .def("append", [](texturearrayinitdata_ptr_t tid, image_ptr_t img) {
        tid->_slices.push_back(TextureArrayInitSubItem{0, img});
      });
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
      .def(
          "updateTexture",         //
          [](const txi_t& the_txi, //
             texture_ptr_t tex,    //
             image_ptr_t img) {    //
            the_txi->initTextureFromImage(tex.get(), img);
          })
      .def(
          "updateTextureArraySlice",           //
          [](const txi_t& the_txi,             //
             texturearraysliceref_ptr_t slice, //
             image_ptr_t img) {                //
            the_txi->updateTextureArraySlice(slice.get(), img);
          })
      .def(
          "updateTextureArray",           //
          [](const txi_t& the_txi) {                //
            the_txi->updateTextureArray();
          })
#if defined(ENABLE_PYTORCH)
      .def(
          "initTextureFromTensor",     //
          [](const txi_t& the_txi,     //
             texture_ptr_t ptex,       //
             torchtensor_ptr_t tensor, //
             crcstring_ptr_t fmt) {    //
            auto as_efmt = EBufferFormat(fmt->hashed());
            the_txi->initTextureFromTensor(ptex.get(), tensor, as_efmt);
          })
#endif
      .def("__repr__", [](const txi_t& txi) -> std::string {
        fxstring<256> fxs;
        fxs.format("TXI(%p)", txi.get());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  auto rstate_type = py::class_<RasterState, rasterstate_ptr_t>(module_lev2, "RasterState") //
                         .def_property(
                             "culltest",
                             [](rasterstate_ptr_t state) -> crcstring_ptr_t { //
                               auto crcstr = std::make_shared<CrcString>(uint64_t(state->_culltest));
                               return crcstr;
                             },
                             [](rasterstate_ptr_t state, crcstring_ptr_t ctest) { //
                               state->_culltest = ECullTest(ctest->hashed());
                             })
                         .def_property(
                             "depthtest",
                             [](rasterstate_ptr_t state) -> crcstring_ptr_t { //
                               auto crcstr = std::make_shared<CrcString>(uint64_t(state->_depthtest));
                               return crcstr;
                             },
                             [](rasterstate_ptr_t state, crcstring_ptr_t ctest) { //
                               state->_depthtest = EDepthTest(ctest->hashed());
                             })
                         .def(
                             "setBlendingMacro",
                             [](rasterstate_ptr_t state, crcstring_ptr_t value) { //
                               state->setBlendingMacro(BlendingMacro(value->hashed()));
                             })
                         .def_property(
                             "writeMaskRGB",
                             [](rasterstate_ptr_t state) -> bool { return state->_writemaskRGB; },
                             [](rasterstate_ptr_t state, bool value) { state->_writemaskRGB = value; })
                         .def_property(
                             "writeMaskA",
                             [](rasterstate_ptr_t state) -> bool { return state->_writemaskA; },
                             [](rasterstate_ptr_t state, bool value) { state->_writemaskA = value; })
                         .def_property(
                             "writeMaskZ",
                             [](rasterstate_ptr_t state) -> bool { return state->_writemaskZ; },
                             [](rasterstate_ptr_t state, bool value) { state->_writemaskZ = value; })
                         .def("__repr__", [](rasterstate_ptr_t state) -> std::string {
                           fxstring<256> fxs;
                           fxs.format("RasterState()");
                           return fxs.c_str();
                         });
  type_codec->registerStdCodec<rasterstate_ptr_t>(rstate_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto capture_async_t =
      py::class_<CaptureAsync, captureasync_ptr_t>(module_lev2, "CaptureAsync")
          .def(
              "__repr__",
              [](captureasync_ptr_t cap) -> std::string {
                fxstring<256> fxs;
                fxs.format("CaptureAsync(%p)", cap.get());
                return fxs.c_str();
              })
          .def_property_readonly("is_ready", [](captureasync_ptr_t cap) -> bool { return cap->isReady(); })
          .def_property_readonly("progress", [](captureasync_ptr_t cap) -> float { return cap->progress(); })
          .def_property_readonly(
              "pixelFetchContext", [](captureasync_ptr_t cap) -> pixelfetchctx_ptr_t { return cap->_pixelFetchContext; })
          .def("wait", [](captureasync_ptr_t cap, capturebuffer_ptr_t buffer) -> bool { return cap->wait(buffer.get()); });

  auto rtb_t = py::class_<RtBuffer, rtbuffer_ptr_t>(module_lev2, "RtBuffer")
                   .def(
                       "__repr__",
                       [](rtbuffer_ptr_t rtb) -> std::string {
                         fxstring<256> fxs;
                         fxs.format("RtBuffer(%p)", rtb.get());
                         return fxs.c_str();
                       })
                   .def_property_readonly("texture", [](rtbuffer_ptr_t rtb) -> texture_ptr_t { return rtb->_texture; })
                   .def_property(
                       "clearColor",
                       [](rtbuffer_ptr_t rtb) -> fvec4 { return rtb->_clearColor; },
                       [](rtbuffer_ptr_t rtb, const fvec4& color) { rtb->_clearColor = color; })
                   .def_property(
                       "clearDepth",
                       [](rtbuffer_ptr_t rtb) -> float { return rtb->_clearDepth; },
                       [](rtbuffer_ptr_t rtb, float depth) { rtb->_clearDepth = depth; })
                   .def_property(
                       "autoclear",
                       [](rtbuffer_ptr_t rtb) -> bool { return rtb->_autoclear; },
                       [](rtbuffer_ptr_t rtb, bool autoclear) { rtb->_autoclear = autoclear; });
  type_codec->registerStdCodec<rtbuffer_ptr_t>(rtb_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto rtg_t = py::class_<RtGroup, rtgroup_ptr_t>(module_lev2, "RtGroup")
                   .def(py::init([](ctx_t& ctx, int w, int h) -> rtgroup_ptr_t {
                     uint64_t usage           = "user"_crcu;
                     MsaaSamples msaa_samples = MsaaSamples::MSAA_1X;
                     auto rtg                 = std::make_shared<RtGroup>(ctx.get(), w, h, msaa_samples, usage);
                     return rtg;
                   }))
                   .def("resize", [](rtgroup_ptr_t rtg, int w, int h) { rtg.get()->Resize(w, h); })
                   .def_property_readonly("width", [](rtgroup_ptr_t rtg) -> int { return int(rtg->width()); })
                   .def_property_readonly("height", [](rtgroup_ptr_t rtg) -> int { return int(rtg->height()); })
                   .def_property(
                       "name",
                       [](rtgroup_ptr_t rtg) -> std::string { return rtg->_name; },
                       [](rtgroup_ptr_t rtg, std::string name) { rtg->_name = name; })
                   .def(
                       "createBuffer",
                       [](rtgroup_ptr_t rtg, crcstring_ptr_t format, crcstring_ptr_t usage) -> rtbuffer_ptr_t {
                         auto efmt       = EBufferFormat(format->hashed());
                         uint64_t eusage = usage ? uint64_t(usage->hashed()) : 0;
                         auto rtb        = rtg->createRenderTarget(efmt, eusage);
                         return rtb;
                       })
                   .def(
                       "createDepthBuffer",
                       [](rtgroup_ptr_t rtg, crcstring_ptr_t format, bool with_texture) -> rtbuffer_ptr_t {
                         auto efmt = EBufferFormat(format->hashed());
                         auto rtb  = rtg->createDepthBuffer(efmt, with_texture);
                         return rtb;
                       })
                   .def(
                       "__repr__",
                       [](rtgroup_ptr_t rtg) -> std::string {
                         fxstring<256> fxs;
                         fxs.format("RtGroup(%p)", rtg.get());
                         return fxs.c_str();
                       })
                   .def_property_readonly("numBuffers", [](rtgroup_ptr_t rtg) -> int { return rtg->numImageBuffers(); })
                   .def_property_readonly("depth_buffer", [](rtgroup_ptr_t rtg) -> rtbuffer_ptr_t { return rtg->_depthBuffer; })
                   .def("buffer", [](rtgroup_ptr_t rtg, int irtb) -> rtbuffer_ptr_t { return rtg->buffer(irtb); })
                   .def("texture", [](rtgroup_ptr_t rtg, int irtb) -> texture_ptr_t { return rtg->texture(irtb); });
  //.def("texture", [](rtgroup_ptr_t rtg, int irtb) -> texture_ptr_t { return rtg->buffer(irtb)->texture(); });
  type_codec->registerStdCodec<rtgroup_ptr_t>(rtg_t);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<CaptureBuffer, capturebuffer_ptr_t>(module_lev2, "CaptureBuffer", pybind11::buffer_protocol())
      .def(py::init<>())
      .def_buffer([](CaptureBuffer& capbuf) -> pybind11::buffer_info {
        pybind11::buffer_info rval;
        switch (capbuf.format()) {
          case EBufferFormat::RGBA8: {
            rval = pybind11::buffer_info(
                (void*)capbuf._image->_data->data(), // Pointer to buffer
                sizeof(unsigned char),               // Size of one scalar
                pybind11::format_descriptor<unsigned char>::format(),
                1,                 // Number of dimensions
                {capbuf.length()}, // Buffer dimensions
                {1});              // Strides (in bytes) for each index
            break;
          }
          case EBufferFormat::RGBA32F: {
            rval = pybind11::buffer_info(
                (void*)capbuf._image->_data->data(), // Pointer to buffer
                sizeof(float),                       // Size of one scalar
                pybind11::format_descriptor<float>::format(),
                1,                     // Number of dimensions
                {capbuf.length() / 4}, // Buffer dimensions
                {4});                  // Strides (in bytes) for each index
            break;
          }
          case EBufferFormat::R32F: {
            rval = pybind11::buffer_info(
                (void*)capbuf._image->_data->data(), // Pointer to buffer
                sizeof(float),                       // Size of one scalar
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
      .def_property_readonly("image", [](CaptureBuffer& capbuf) -> image_ptr_t { return capbuf._image; })
      .def_property_readonly("raw_image", [](CaptureBuffer& capbuf) -> image_ptr_t { return capbuf._raw_image; })
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
          .def(py::init([](std::string debugname) -> texture_ptr_t {
            auto tex        = std::make_shared<Texture>();
            tex->_debugName = debugname;
            return tex;
          }))
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
          .def_static("declare", [](std::string path) -> texture_ptr_t { return nullptr; })
          .def_property(
              "name",
              [](texture_ptr_t tex) -> std::string { return tex->_debugName; },
              [](texture_ptr_t tex, std::string name) { tex->_debugName = name; });

  // using rawtexptr_t = Texture*;
  type_codec->registerStdCodec<texture_ptr_t>(texture_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto pfc_type = py::class_<PixelFetchContext, pixelfetchctx_ptr_t>(module_lev2, "PixelFetchContext")
                      .def(
                          py::init([](rtgroup_ptr_t rtg, size_t size) -> pixelfetchctx_ptr_t {
                            auto pfc      = std::make_shared<PixelFetchContext>();
                            pfc->_rtgroup = rtg;
                            pfc->_resize(size);
                            return pfc;
                          }),
                          py::arg("rtgroup"),
                          py::arg("size") = 1)
                      .def(
                          "setUsage",
                          [](pixelfetchctx_ptr_t pfc, int index, crcstring_ptr_t usage) {
                            OrkAssert(index >= 0 && index < pfc->_usage.size());
                            auto eusage        = static_cast<PixelFetchContext::EPixelUsage>(usage->hashed());
                            pfc->_usage[index] = eusage;
                          },
                          py::arg("index"),
                          py::arg("usage"))
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
  py::class_<fxshaderstoragebuffer_ptr_t>(module_lev2, "FxShaderStorageBuffer")
      .def_property_readonly("length", [](fxshaderstoragebuffer_ptr_t ssb) -> size_t { return ssb->_length; })
      .def("__repr__", [](fxshaderstoragebuffer_ptr_t ssb) -> std::string {
        fxstring<256> fxs;
        fxs.format("FxShaderStorageBuffer(%p)", ssb.get());
        return fxs.c_str();
      });
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
  auto texarray_t = py::class_<TextureArray, texturearray_ptr_t>(module_lev2, "TextureArray");
  texarray_t
      .def(py::init([](size_t w, size_t h, size_t maxslices) -> texturearray_ptr_t {
        auto rval        = std::make_shared<TextureArray>();
        rval->_width     = w;
        rval->_height    = h;
        rval->_maxslices = maxslices;
        return rval;
      }))
      .def(
          "resize",
          [](texturearray_ptr_t texarray, size_t w, size_t h, size_t d, crcstring_ptr_t fmt, bool needs_mips) {
            auto efmt                = EBufferFormat(fmt->hashed());
            texarray->_requires_mips = needs_mips;
            texarray->resize(w, h, d, efmt);
          })
      .def("load", [](texturearray_ptr_t texarray, std::string path) -> texturearraysliceref_ptr_t { return texarray->load(path); })
      .def("conform", [](texturearray_ptr_t texarray, crcstring_ptr_t crc) { //
        auto fmt = EBufferFormat(crc->hashed());
        return texarray->_conform(fmt); 
      })
      .def("slice", [](texturearray_ptr_t texarray, size_t index) -> texturearraysliceref_ptr_t { return texarray->slice(index); })
      /*.def("createRtGroup", [](texturearray_ptr_t texarray, ctx_t context) -> rtgroup_ptr_t {
        auto rtg = std::make_shared<RtGroup>(context.get(),texarray->_width, texarray->_height);
        texarray->_rtg = rtg;
        rtg->_textureArray = texarray.get();
        rtg->_name = "TextureArray:"+texarray->_debugName;
        for(size_t islice=0; islice<texarray->_maxslices; islice++) {
          auto slice = texarray->slice(islice);
          uint64_t usage = "texarray"_crcu;
          auto rtb = rtg->createRenderTarget(texarray->_format,usage);
          rtb->_ta_slice = slice;
        }
        return rtg;
      })*/
      .def_property(
          "needsRadianceCache",
          [](texturearray_ptr_t texarray) -> bool { //
            return texarray->_needsRadianceCache;
          },
          [](texturearray_ptr_t texarray, bool b) { //
            texarray->_needsRadianceCache = b;
          })
      .def_property(
          "bufferFormat",
          [](texturearray_ptr_t texarray) -> crcstring_ptr_t { //
            auto crcstr = std::make_shared<CrcString>(uint64_t(texarray->_format));
            return crcstr;
          },
          [](texturearray_ptr_t texarray, crcstring_ptr_t v) { //
            auto fmt          = EBufferFormat(v->hashed());
            texarray->_format = fmt;
          })
      .def(
          "subimage",
          [](texturearray_ptr_t texarray, int slice) -> image_ptr_t { //
            OrkAssert(slice >= 0);
            OrkAssert(slice < texarray->_images.size());
            auto rval = texarray->_images[slice];
            return rval;
          })
      .def("__repr__", [](texturearray_ptr_t texarray) -> std::string {
        fxstring<256> fxs;
        fxs.format("TextureArrayLoader(%p)", texarray.get());
        return fxs.c_str();
      });
  type_codec->registerStdCodec<texturearray_ptr_t>(texarray_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto texarrayslice_t = py::class_<TextureArraySliceRef, texturearraysliceref_ptr_t>(module_lev2, "TextureArraySlice");
  texarrayslice_t
      .def_property_readonly(
          "index",
          [](texturearraysliceref_ptr_t texarrayslice) -> int { //
            return texarrayslice->_slice;
          })
      .def("__repr__", [](texturearraysliceref_ptr_t texarrayslice) -> std::string {
        fxstring<256> fxs;
        fxs.format("TextureArraySliceRef(%p)", texarrayslice.get());
        return fxs.c_str();
      });
  type_codec->registerStdCodec<texturearraysliceref_ptr_t>(texarrayslice_t);
  /////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
} // namespace ork::lev2
