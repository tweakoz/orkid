////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <pybind11/numpy.h>
#include <ork/lev2/init.h>
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <ork/lev2/gfx/gpumicrotask.h>
#include <ork/lev2/gfx/renderphasestats.h>
#include <ork/lev2/gfx/image.h>
#include <ork/math/cvector4.h>
#include <ork/python/gil_safe_pyobj.h>

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
          // hasDeferredOps / waitForDeferredOps moved to Context (Phase 6.3
          // Variant B: per-context deferred queues). Call on the specific
          // context, e.g. `GfxEnv.loadingContext().hasDeferredOps()`.
          .def("__repr__", [](const GfxEnv& e) -> std::string {
            fxstring<64> fxs;
            fxs.format("GfxEnv(%p)", &e);
            return fxs.c_str();
          });
  // type_codec->registerStdCodec<GfxEnv>(gfxenv_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto ctx_type = //
      py::class_<ctx_t>(module_lev2, "GfxContext")
          // bool(ctx) → False when the underlying Context* is null. Lets
          // headless scripts `assert ctx` early instead of segfaulting on
          // the first property access (TXI/FBI/etc.) when loadingContext()
          // is called before the main thread has stood up the loader ctx.
          .def("__bool__", [](ctx_t& c) -> bool { return c.get() != nullptr; })
          .def("mainSurfaceWidth", [](ctx_t& c) -> int { return c.get()->mainSurfaceWidth(); })
          .def("mainSurfaceHeight", [](ctx_t& c) -> int { return c.get()->mainSurfaceHeight(); })
          .def("setClipboardText", [](ctx_t& c, const std::string& text) {
            auto ctxbase = c.get()->GetCtxBase();
            if (ctxbase) ctxbase->setClipboardText(text);
          })
          .def("getClipboardText", [](ctx_t& c) -> std::string {
            auto ctxbase = c.get()->GetCtxBase();
            return ctxbase ? ctxbase->getClipboardText() : "";
          })
          // Map a WINDOW-LOCAL coordinate to a GLOBAL (screen) coordinate — the minimal
          // read-only OS-window-geometry surface the editor needs to place a secondary
          // window relative to the main window + its viewport (glfwGetWindowPos under the
          // hood). (0,0) yields the main window's client-area screen origin.
          .def("mapCoordToGlobal", [](ctx_t& c, int x, int y) -> py::tuple {
            auto ctxbase = c.get()->GetCtxBase();
            if (not ctxbase)
              return py::make_tuple(x, y);
            fvec2 g = ctxbase->MapCoordToGlobal(fvec2(float(x), float(y)));
            return py::make_tuple(int(g.x), int(g.y));
          })
          .def("makeCurrent", [](ctx_t& c) { c.get()->makeCurrentContext(); })
          .def("beginFrame", [](ctx_t& c) { return c.get()->beginFrame(); })
          .def("endFrame", [](ctx_t& c) { return c.get()->endFrame(); })
          // Per-context deferred-op queue (Phase 6.3 Variant B). Only the
          // non-blocking query is exposed — a blocking wait from this
          // context's own thread would deadlock the drain.
          .def("hasDeferredOps", [](ctx_t& c) -> bool { return c.get()->hasDeferredOps(); })
          // DEVICE capability probe (SKYLIGHT §4.B.1 froxel risk retirement).
          .def(
              "supportsVolumeRenderTarget",
              [](ctx_t& c, std::string format) -> bool {
                auto crc_fmt = CrcString(format.c_str());
                return c.get()->supportsVolumeRenderTarget(EBufferFormat(crc_fmt._hashed));
              })
          .def("debugPushGroup", [](ctx_t& c, cstrref_t str) { return c.get()->debugPushGroup(str); })
          .def("debugPopGroup", [](ctx_t& c) { return c.get()->debugPopGroup(); })
          .def("debugMarker", [](ctx_t& c, cstrref_t str) { return c.get()->debugMarker(str); })
          .def("defaultRTG", [](ctx_t& c) -> rtgroup_ptr_t { return rtgroup_ptr_t(c.get()->_defaultRTG); })
          .def("resize", [](ctx_t& rtg, int w, int h) { rtg.get()->resizeMainSurface(w, h); })
          .def_property_readonly("FBI", [](ctx_t& c) -> fbi_t { return fbi_t(c.get()->FBI()); })
          .def_property_readonly("FXI", [](ctx_t& c) -> fxi_t { return fxi_t(c.get()->FXI()); })
          .def_property_readonly("GBI", [](ctx_t& c) -> gbi_t { return gbi_t(c.get()->GBI()); })
          .def_property_readonly("DWI", [](ctx_t& c) -> dwi_t { return dwi_t(c.get()->DWI()); })
          .def_property_readonly("TXI", [](ctx_t& c) -> txi_t { return txi_t(c.get()->TXI()); })
          .def_property_readonly("CI", [](ctx_t& c) -> ci_t { return ci_t(c.get()->CI()); })
          .def("setPostSwapWaitTime", [](ctx_t& c, int wt) { _g_post_swap_wait_time = wt; })
          .def("scheduleBeforeDoEndFrameOneShot", [](ctx_t& c, py::function callback) {
            pyfn_ptr_t f_ptr = std::make_shared<py::function>(callback);
            c.get()->_pyimpl_beforeEndFrame.set<pyfn_ptr_t>(f_ptr);
            c.get()->scheduleBeforeDoEndFrameOneShot([c]() {
                py::gil_scoped_acquire gil;
                auto f_ptr = c.get()->_pyimpl_beforeEndFrame.get<pyfn_ptr_t>();
                (*f_ptr)();
            });
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
          .def_property_readonly("topRCFD", [](ctx_t& c) -> rcfd_ptr_t { return c.get()->topRenderContextFrameData(); }) //
          //////////////////////
          .def_property_readonly("frameIndex", [](ctx_t& c) -> int { return c.get()->GetTargetFrame(); })
          // GPU queue submits issued during the last completed frame (all queues)
          .def_property_readonly("submitCount", [](ctx_t& c) -> int { return int(c.get()->submitCount()); })
          //////////////////////
          // GPU-CULL RESULT COUNTS (terrain + hypermesh), the cull oracle's ask.
          //
          // Both cull sites read their result buffers back ONLY while this is
          // enabled, so it is off by default and a caller must arm it: an
          // all-zero snapshot from an UNARMED run means "nobody counted", not
          // "nothing was culled". Hence cullStatsEnabled is readable — a gate
          // has to be able to prove it armed the counter before believing a
          // zero. The per-family *_valid flags say whether that family
          // contributed at all this frame (a scene with no terrain never sets
          // terrain_valid), which is what separates "no such geometry" from
          // "everything culled".
          //////////////////////
          .def_property(
              "cullStatsEnabled",
              [](ctx_t& c) -> bool { return CullStats::instance().enabled(); },
              [](ctx_t& c, bool e) { CullStats::instance().setEnabled(e); })
          .def_property_readonly(
              "cullStats",
              [](ctx_t& c) -> py::dict {
                auto cc = CullStats::instance().snapshot();
                py::dict rval;
                rval["enabled"]       = CullStats::instance().enabled();
                rval["terrain_valid"] = cc.terrain_valid;
                rval["t_total"]       = cc.t_total;
                rval["t_frustum"]     = cc.t_frustum;
                rval["t_visible"]     = cc.t_visible;
                rval["hyper_valid"]   = cc.hyper_valid;
                rval["h_variants"]    = cc.h_variants;
                rval["h_total"]       = cc.h_total;
                rval["h_frustum"]     = cc.h_frustum;
                rval["h_visible"]     = cc.h_visible;
                rval["h_occluded"]    = cc.h_occluded;
                return rval;
              })
          // device capability: VK_EXT_mesh_shader (false on every device that lacks it)
          .def_property_readonly("supports_mesh_shader", [](ctx_t& c) -> bool { return c.get()->supportsMeshShader(); })
          // + the taskShader (amplification) feature; strictly stronger than supports_mesh_shader
          .def_property_readonly("supports_task_shader", [](ctx_t& c) -> bool { return c.get()->supportsTaskShader(); })
          .def_property_readonly("max_task_payload_size", [](ctx_t& c) -> int { return int(c.get()->maxTaskPayloadSize()); })
          // draws issued with a task stage actually bound — zero after real frames means the pass
          // ran taskless, whatever the picture looks like
          .def_property_readonly("task_shader_draws", [](ctx_t& c) -> int { return c.get()->taskShaderDrawCount(); })
          // device capability: core VK1.1 multiview (false on every device that lacks it)
          .def_property_readonly("supports_multiview", [](ctx_t& c) -> bool { return c.get()->supportsMultiview(); })
          .def_property_readonly("max_multiview_views", [](ctx_t& c) -> int { return c.get()->maxMultiviewViewCount(); })
          // + GPU-driven workgroup counts (vkCmdDrawMeshTasksIndirectEXT); implies supports_mesh_shader
          .def_property_readonly(
              "supports_mesh_shader_indirect", [](ctx_t& c) -> bool { return c.get()->supportsMeshShaderIndirect(); })
          // device capability: core multiview, and the mesh stage's legality inside a multiview pass
          .def_property_readonly("supports_multiview", [](ctx_t& c) -> bool { return c.get()->supportsMultiview(); })
          .def_property_readonly(
              "supports_multiview_mesh_shader", [](ctx_t& c) -> bool { return c.get()->supportsMultiviewMeshShader(); })
          .def_property_readonly(
              "max_mesh_multiview_views", [](ctx_t& c) -> int { return int(c.get()->maxMeshMultiviewViewCount()); })
          // validation-layer state: error count so far, and whether the layer is loaded at all
          // (a zero count from an unarmed run proves nothing — check armed first)
          .def_property_readonly("validation_errors", [](ctx_t& c) -> int { return c.get()->validationErrorCount(); })
          .def_property_readonly("validation_armed", [](ctx_t& c) -> bool { return c.get()->validationArmed(); })
          // hw MSAA ceiling (framebufferColor+DepthSampleCounts); query it before asking RtGroup for a count
          .def_property_readonly("msaa_max_samples", [](ctx_t& c) -> int { return c.get()->msaaMaxSamples(); })
          // sample count the FORWARD pass's primary target is built at (app --msaa level, device-clamped)
          .def_property_readonly("msaa_forward_samples", [](ctx_t& c) -> int { return msaaForwardSampleCount(c.get()); })
          // .def_property("currentMaterial", [](ctx_t& c)&Context::currentMaterial, &Context::BindMaterial)
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
          [](const fbi_t& fbi, rtbuffer_ptr_t rtb, py::object in_path) -> captureasync_ptr_t {
            file::Path pth;
            if(py::isinstance<file::Path>(in_path)) {
              pth = in_path.cast<file::Path>();
            }
            else { // cast to str and convert
              auto path_str = in_path.cast<py::str>();
              pth = file::Path(path_str);
            }
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
      .def_property_readonly("main_RTG", [](const fbi_t& fbi) -> rtgroup_ptr_t {
        return fbi.get()->_main_rtg;
      })
      .def("ensureMainRTG", [](const fbi_t& fbi) -> rtgroup_ptr_t {
        return fbi.get()->_ensureMainRtg();
      })
      .def("__repr__", [](const fbi_t& fbi) -> std::string {
        fxstring<256> fxs;
        fxs.format("FBI(%p)", fbi.get());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  // FxShaderStorageBufferMapping must be defined before FxInterface uses it
  auto storagebufmapping_type = //
      py::class_<FxShaderStorageBufferMapping,storagebuffermappingptr_t>(module_lev2, "FxShaderStorageBufferMapping")
          .def_property_readonly("length", [](storagebuffermappingptr_t m) -> size_t { return m->_length; })
          .def_property_readonly("offset", [](storagebuffermappingptr_t m) -> size_t { return m->_offset; })
          .def_property_readonly(
              "data",
              [](storagebuffermappingptr_t m) -> py::bytes {
                return py::bytes(reinterpret_cast<const char*>(m->_mappedaddr), m->_length);
              })
          .def( // host write into a WRITE-mapped buffer (`data` returns a COPY, so it can't be assigned through)
              "writeBytes",
              [](storagebuffermappingptr_t m, py::bytes data, size_t offset) {
                std::string s = data;
                OrkAssert(offset + s.size() <= m->_length);
                std::memcpy(reinterpret_cast<char*>(m->_mappedaddr) + offset, s.data(), s.size());
              },
              py::arg("data"),
              py::arg("offset") = 0)
          .def("__repr__", [](storagebuffermappingptr_t m) -> std::string {
            fxstring<256> fxs;
            fxs.format("FxShaderStorageBufferMapping(%p, len=%zu)", m.get(), m->_length);
            return fxs.c_str();
          });
  type_codec->registerStdCodec<storagebuffermappingptr_t>(storagebufmapping_type);
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
              // Single float
              auto as_float  = data.cast<py::float_>();
              auto datablock = std::make_shared<DataBlock>();
              datablock->addItem<float>(as_float);
              fxi.get()->copyBufferIntoStorageBuffer(buffer.get(), datablock->_storage, dest_offset);
            } else if (py::isinstance<py::int_>(data)) {
              // Single int
              auto as_int  = data.cast<py::int_>();
              auto datablock = std::make_shared<DataBlock>();
              datablock->addItem<int32_t>(as_int);
              fxi.get()->copyBufferIntoStorageBuffer(buffer.get(), datablock->_storage, dest_offset);
            } else if (py::isinstance<py::array>(data)) {
              // Numpy array — direct memcpy from buffer pointer
              auto arr = py::cast<py::array_t<float, py::array::c_style | py::array::forcecast>>(data);
              auto buffer_info = arr.request();
              auto ptr = static_cast<const uint8_t*>(buffer_info.ptr);
              size_t num_bytes = buffer_info.size * sizeof(float);
              std::vector<uint8_t> vec(ptr, ptr + num_bytes);
              fxi.get()->copyBufferIntoStorageBuffer(buffer.get(), vec, dest_offset);
            } else if (py::hasattr(data, "__iter__") && !py::isinstance<py::str>(data)) {
              // Iterable (list, tuple) of numbers
              auto datablock = std::make_shared<DataBlock>();
              for (auto item : data) {
                if (py::isinstance<py::float_>(item)) {
                  datablock->addItem<float>(item.cast<float>());
                } else if (py::isinstance<py::int_>(item)) {
                  datablock->addItem<float>(float(item.cast<int>()));
                }
              }
              fxi.get()->copyBufferIntoStorageBuffer(buffer.get(), datablock->_storage, dest_offset);
            } else {
              auto type_str = data.get_type().attr("__name__").cast<std::string>();
              printf("copyDataIntoShaderStorageBuffer unknown type<%s>\n", type_str.c_str());
              OrkAssert(false);
            }
          })
      .def(
          "shaderFromShaderText",
          [](fxi_t& fxi, std::string name, std::string shadertext) -> pyfxshader_ptr_t {
            return pyfxshader_ptr_t(fxi.get()->shaderFromShaderText(name, shadertext));
          })
      .def(
          "computeShader",
          [](fxi_t& fxi, pyfxshader_ptr_t shader, std::string name) -> pyfxcomputeshader_ptr_t {
            return pyfxcomputeshader_ptr_t(fxi.get()->computeShader(shader.get(), name));
          })
      .def(
          "mapStorageBuffer",
          [](fxi_t& fxi, fxshaderstoragebuffer_ptr_t buffer, size_t base, size_t length, crcstring_ptr_t access) -> storagebuffermappingptr_t {
            return fxi.get()->mapStorageBuffer(buffer.get(), base, length, BufferMapAccess(access->hashed()));
          })
      .def(
          "unmapStorageBuffer",
          [](fxi_t& fxi, storagebuffermappingptr_t mapping) {
            fxi.get()->unmapStorageBuffer(mapping.get());
          })
      .def(
          "bindStorageBuffer",
          [](fxi_t& fxi, pyfxstorage_ptr_t block, fxshaderstoragebuffer_ptr_t buffer) {
            fxi.get()->bindStorageBuffer(block.get(), buffer.get());
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
      .def("drawLines", [](gbi_t gbi, vw_vtxa_t& vw) { gbi.get()->DrawPrimitiveEML(vw, PrimitiveType::LINES); })
      .def(
          "drawMeshTasks", // taskless mesh-shader draw; args are mesh workgroup counts
          [](gbi_t gbi, uint32_t x, uint32_t y, uint32_t z) { gbi.get()->DrawMeshTasksEML(x, y, z); },
          py::arg("x") = 1,
          py::arg("y") = 1,
          py::arg("z") = 1)
      .def(
          // mesh-shader draw whose workgroup counts come from a compute-written
          // VkDrawMeshTasksIndirectCommandEXT{x,y,z} at args_offset (all-culled -> zero workgroups).
          "drawMeshTasksIndirect",
          [](gbi_t gbi, fxshaderstoragebuffer_ptr_t args, size_t args_offset) {
            gbi.get()->DrawMeshTasksIndirectEML(args.get(), args_offset);
          },
          py::arg("args"),
          py::arg("args_offset") = 0)
      .def(
          // SSBO vertex-pull, NON-indexed indirect draw: the vertex count comes from a
          // compute-written VkDrawIndirectCommand at args_offset; vertices are pulled from the
          // storage block bound to the VS (pipeline.bindStorage). The raw-GBI counterpart of
          // drawMeshTasks — the two are the A/B pair for SSBO-sourced geometry.
          "drawIndirect",
          [](gbi_t gbi, fxshaderstoragebuffer_ptr_t args, crcstring_ptr_t primtype, size_t args_offset) {
            auto pt = primtype ? PrimitiveType(primtype->hashed()) : PrimitiveType::TRIANGLES;
            gbi.get()->DrawIndirectEML(pt, args.get(), args_offset);
          },
          py::arg("args"),
          py::arg("primtype")    = crcstring_ptr_t(nullptr),
          py::arg("args_offset") = 0);
  //.def("copyTensorIntoStorageBuffer", [](gbi_t gbi, torchtensor_ptr_t tensor, fxshaderstoragebuffer_ptr_t buffer) {
  // ci.get()->copyTensorIntoStorageBuffer(buffer.get(), tensor); });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<dwi_t>(module_lev2, "DrawingInterface")
      .def(
          "__repr__",
          [](const dwi_t& dwi) -> std::string {
            fxstring<256> fxs;
            fxs.format("DWI(%p)", dwi.get());
            return fxs.c_str();
          })
      .def("quad2D",
          [](dwi_t dwi, fvec4 quad_rect, fvec4 uv_rect, fvec4 uv_rect2, float depth) {
            dwi.get()->quad2D(quad_rect, uv_rect, uv_rect2, depth);
          },
          py::arg("quad_rect"),
          py::arg("uv_rect"),
          py::arg("uv_rect2") = fvec4(0,0,0,0),
          py::arg("depth") = 0.0f)
      .def("fullscreenQuad",
          [](dwi_t dwi, fvec4 uv_rect, fvec4 uv_rect2, float depth) {
            dwi.get()->fullscreenQuad(uv_rect, uv_rect2, depth);
          },
          py::arg("uv_rect") = fvec4(0,0,1,1),
          py::arg("uv_rect2") = fvec4(0,0,0,0),
          py::arg("depth") = 0.0f);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ci_t>(module_lev2, "ComputeInterface")
      .def(
          "__repr__",
          [](const ci_t& gbi) -> std::string {
            fxstring<256> fxs;
            fxs.format("CI(%p)", gbi.get());
            return fxs.c_str();
          })
      .def("beginDispatchPhase", [](ci_t& ci) {
        ci.get()->beginDispatchPhase();
      })
      .def("endDispatchPhase", [](ci_t& ci) {
        ci.get()->endDispatchPhase();
      })
      .def("storageBarrier", [](ci_t& ci) {
        ci.get()->storageBarrier();
      })
      .def("dispatch", [](ci_t& ci, pyfxcomputeshader_ptr_t csh, uint32_t numx, uint32_t numy, uint32_t numz) {
        ci.get()->dispatchCompute(csh.get(), numx, numy, numz);
      })
      .def("dispatchIndirect", [](ci_t& ci, pyfxcomputeshader_ptr_t csh, fxshaderstoragebuffer_ptr_t args, size_t args_offset) {
        // C.3: group counts from a GPU-written VkDispatchIndirectCommand (x,y,z) at args_offset
        ci.get()->dispatchComputeIndirect(csh.get(), args.get(), args_offset);
      }, py::arg("shader"), py::arg("args"), py::arg("args_offset") = 0)
      .def("dispatchInline", [](ci_t& ci, pyfxcomputeshader_ptr_t csh, uint32_t numx, uint32_t numy, uint32_t numz) {
        // record the dispatch + producer->consumer barrier onto the FRAME primary CB (no separate
        // submit/fence); call mid-frame, OUTSIDE any render pass, AFTER bindStorageBuffer.
        ci.get()->dispatchComputeInline(csh.get(), numx, numy, numz);
      })
      .def("bindStorageBuffer", [](ci_t& ci, pyfxcomputeshader_ptr_t csh, uint32_t binding_index, fxshaderstoragebuffer_ptr_t buffer) {
        ci.get()->bindStorageBuffer(csh.get(), binding_index, buffer.get());
      })
      .def("bindSampler", [](ci_t& ci, pyfxcomputeshader_ptr_t csh, uint32_t binding_index, texture_ptr_t tex) {
        ci.get()->bindSampler(csh.get(), binding_index, tex.get());
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
             image_ptr_t img,      //
             bool async,           //
             bool mipmapped) {     //
            the_txi->initTextureFromImage(tex.get(), img, mipmapped, async);
          },
          py::arg("tex"),
          py::arg("img"),
          py::arg("async") = true,
          py::arg("mipmapped") = false)
      .def(
          "updateTextureArraySlice",           //
          [](const txi_t& the_txi,             //
             texturearraysliceref_ptr_t slice, //
             image_ptr_t img) {                //
            the_txi->updateTextureArraySlice(slice.get(), img);
          })
      .def(
          "updateTextureArray",           //
          [](const txi_t& the_txi, texturearray_ptr_t array) {                //
            the_txi->updateTextureArray(array.get());
          })
      .def(
          "applySamplingMode",
          [](const txi_t& the_txi, texture_ptr_t tex) {
            the_txi->ApplySamplingMode(tex.get());
          })
      //////////////////////////////////////////
      // Chunked-upload API (see ork/lev2/gfx/txi.h)
      //////////////////////////////////////////
      .def(
          "reserveTexture",
          [](const txi_t& the_txi,
             texture_ptr_t tex,
             int w,
             int h,
             int num_mips,
             crcstring_ptr_t fmt) {
            auto efmt = EBufferFormat(fmt->hashed());
            the_txi->reserveTexture(tex.get(), w, h, num_mips, efmt);
          },
          py::arg("tex"),
          py::arg("w"),
          py::arg("h"),
          py::arg("num_mips"),
          py::arg("fmt"))
      .def(
          "reserveTextureArray",
          [](const txi_t& the_txi,
             texturearray_ptr_t tarr,
             int w,
             int h,
             int num_slices,
             int num_mips,
             crcstring_ptr_t fmt) {
            auto efmt = EBufferFormat(fmt->hashed());
            the_txi->reserveTextureArray(tarr.get(), w, h, num_slices, num_mips, efmt);
          },
          py::arg("tarr"),
          py::arg("w"),
          py::arg("h"),
          py::arg("num_slices"),
          py::arg("num_mips"),
          py::arg("fmt"))
      .def(
          "uploadTextureRegion",
          [](const txi_t& the_txi,
             texture_ptr_t tex,
             int mip_level,
             int array_layer,
             int offset_x,
             int offset_y,
             int offset_z,
             int extent_w,
             int extent_h,
             int extent_d,
             py::bytes data,
             py::object on_complete) {
            TextureRegionUpload up;
            up._mip_level   = mip_level;
            up._array_layer = array_layer;
            up._offset_x    = offset_x;
            up._offset_y    = offset_y;
            up._offset_z    = offset_z;
            up._extent_w    = extent_w;
            up._extent_h    = extent_h;
            up._extent_d    = extent_d;
            // py::bytes is a non-owning view of contiguous bytes; the chunked
            // upload internally copies into a staging buffer before returning,
            // so it's safe to use the pointer directly here.
            char*      bptr = nullptr;
            Py_ssize_t blen = 0;
            PyBytes_AsStringAndSize(data.ptr(), &bptr, &blen);
            up._data      = bptr;
            up._data_size = size_t(blen);
            ::ork::void_lambda_t cb = nullptr;
            if (not on_complete.is_none()) {
              auto safe = ork::python::gil_safe_pyobj(on_complete);
              cb = [safe]() {
                py::gil_scoped_acquire gil;
                auto fn = safe.valueAs<py::object>();
                if (fn) (*fn)();
              };
            }
            the_txi->uploadTextureRegion(tex.get(), up, cb);
          },
          py::arg("tex"),
          py::arg("mip_level")   = 0,
          py::arg("array_layer") = 0,
          py::arg("offset_x")    = 0,
          py::arg("offset_y")    = 0,
          py::arg("offset_z")    = 0,
          py::arg("extent_w"),
          py::arg("extent_h"),
          py::arg("extent_d")    = 1,
          py::arg("data"),
          py::arg("on_complete") = py::none())
      .def(
          "finalizeUpload",
          [](const txi_t& the_txi,
             texture_ptr_t tex,
             py::object on_complete) {
            ::ork::void_lambda_t cb = nullptr;
            if (not on_complete.is_none()) {
              auto safe = ork::python::gil_safe_pyobj(on_complete);
              cb = [safe]() {
                py::gil_scoped_acquire gil;
                auto fn = safe.valueAs<py::object>();
                if (fn) (*fn)();
              };
            }
            the_txi->finalizeUpload(tex.get(), cb);
          },
          py::arg("tex"),
          py::arg("on_complete") = py::none())
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
                   .def_property_readonly("texture_provider", [](rtbuffer_ptr_t rtb) -> texture_provider_ptr_t {
                     return std::make_shared<LambdaTextureProvider>(
                         [rtb]() -> texture_ptr_t { return rtb->_texture; }
                     );
                   })
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
                   // msaa is a literal hw sample COUNT (1/2/4/8/16), not a --msaa level. Invalid counts and
                   // counts above the device ceiling THROW rather than silently rounding down — a caller
                   // measuring per-sample cost must never be handed a quieter target than it asked for.
                   .def(py::init([](ctx_t& ctx, int w, int h, int msaa) -> rtgroup_ptr_t {
                     uint64_t usage = "user"_crcu;
                     switch (msaa) {
                       case 1:
                       case 2:
                       case 4:
                       case 8:
                       case 16:
                         break;
                       default:
                         throw std::invalid_argument(
                             FormatString("RtGroup: msaa=%d is not a hw sample count (1,2,4,8,16)", msaa));
                     }
                     int devmax = ctx.get()->msaaMaxSamples();
                     if (msaa > devmax) {
                       throw std::invalid_argument(
                           FormatString("RtGroup: msaa=%d exceeds device max %d", msaa, devmax));
                     }
                     MsaaSamples msaa_samples = msaaSamplesFromInt(msaa);
                     auto rtg                 = std::make_shared<RtGroup>(ctx.get(), w, h, msaa_samples, usage);
                     return rtg;
                   }),
                        py::arg("ctx"),
                        py::arg("w"),
                        py::arg("h"),
                        py::arg("msaa") = 1)
                   .def_property_readonly(
                       "msaa_samples", [](rtgroup_ptr_t rtg) -> int { return msaaEnumToInt(rtg->_msaa_samples); })
                   // SINGLE-PASS STEREO. Both must be set BEFORE the first createBuffer: the layer
                   // count is copied into each RtBuffer at construction, and a group whose buffers
                   // disagree about layer count is an attachment mismatch the pass only discovers at
                   // render time.
                   .def_property(
                       "numLayers",
                       [](rtgroup_ptr_t rtg) -> int { return rtg->_numLayers; },
                       [](rtgroup_ptr_t rtg, int n) { rtg->_numLayers = n; })
                   .def_property(
                       "multiview",
                       [](rtgroup_ptr_t rtg) -> bool { return rtg->_multiview; },
                       [](rtgroup_ptr_t rtg, bool b) { rtg->_multiview = b; })
                   // the mask the pass is actually rendered with: 0 unless the group is BOTH layered
                   // and multiview. This is the value both vulkan rendering structs read.
                   .def_property_readonly("viewMask", [](rtgroup_ptr_t rtg) -> int { return int(rtg->viewMask()); })
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
                   .def("texture", [](rtgroup_ptr_t rtg, int irtb) -> texture_ptr_t { return rtg->texture(irtb); })
                   .def_property(
                       "autoclear",
                       [](rtgroup_ptr_t rtg) -> bool { return rtg->_autoclear; },
                       [](rtgroup_ptr_t rtg, bool autoclear) { rtg->_autoclear = autoclear; })
                   // cubeMap must be set BEFORE createBuffer (it selects ETEXTYPE_CUBE and
                   // the 6-layer image); cubeRenderFace selects which face the next
                   // rtGroupPush targets.
                   .def_property(
                       "cubeMap",
                       [](rtgroup_ptr_t rtg) -> bool { return rtg->_cubeMap; },
                       [](rtgroup_ptr_t rtg, bool cube) { rtg->_cubeMap = cube; })
                   .def_property(
                       "cubeRenderFace",
                       [](rtgroup_ptr_t rtg) -> int { return rtg->_cubeRenderFace; },
                       [](rtgroup_ptr_t rtg, int face) { rtg->_cubeRenderFace = face; })
                   // layered / multiview: BOTH must be set BEFORE createBuffer (they select the
                   // N-layer 2D_ARRAY image and the pass's viewMask); view_mask is what the pass
                   // is actually rendered with, and is 0 unless the group is layered AND multiview.
                   .def_property(
                       "numLayers",
                       [](rtgroup_ptr_t rtg) -> int { return rtg->_numLayers; },
                       [](rtgroup_ptr_t rtg, int n) { rtg->_numLayers = n; })
                   .def_property(
                       "multiview",
                       [](rtgroup_ptr_t rtg) -> bool { return rtg->_multiview; },
                       [](rtgroup_ptr_t rtg, bool mv) { rtg->_multiview = mv; })
                   .def_property_readonly("view_mask", [](rtgroup_ptr_t rtg) -> int { return int(rtg->viewMask()); });
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
      // which array layer of a LAYERED (multiview) buffer to read back; 0 for every
      // single-layer buffer, so leaving it alone is the pre-layered behavior.
      .def_property(
          "capture_layer",
          [](CaptureBuffer& capbuf) -> int { return capbuf._captureLayer; },
          [](CaptureBuffer& capbuf, int layer) { capbuf._captureLayer = layer; })
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
  auto texture_provider_type = //
      py::class_<TextureProvider, texture_provider_ptr_t>(module_lev2, "TextureProvider")
          .def("getTexture", &TextureProvider::getTexture);
  type_codec->registerStdCodec<texture_provider_ptr_t>(texture_provider_type);
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
          .def_property_readonly("update_provider", [](texture_ptr_t self) -> texture_provider_ptr_t { return self->_update_provider; })
          .def_property(
              "streaming",
              [](texture_ptr_t self) -> bool { return self->_streaming; },
              [](texture_ptr_t self, bool v) { self->_streaming = v; })
          .def_static("load", [](std::string path) -> texture_ptr_t { return Texture::LoadUnManaged(path); })
          .def_static("declare", [](std::string path) -> texture_ptr_t { return nullptr; })
          .def_property(
              "name",
              [](texture_ptr_t tex) -> std::string { return tex->_debugName; },
              [](texture_ptr_t tex, std::string name) { tex->_debugName = name; })
          .def(
              "setAddressMode",
              [](texture_ptr_t tex, crcstring_ptr_t s, crcstring_ptr_t t, crcstring_ptr_t r) {
                auto parse = [](crcstring_ptr_t m) -> TextureAddressMode {
                  static const auto WRAP_CRC = CrcString("WRAP").hashed();
                  static const auto CLAMP_CRC = CrcString("CLAMP").hashed();
                  auto h = m->hashed();
                  if (h == WRAP_CRC) return TextureAddressMode::WRAP;
                  if (h == CLAMP_CRC) return TextureAddressMode::CLAMP;
                  OrkAssert(false);
                  return TextureAddressMode::CLAMP;
                };
                tex->TexSamplingMode()._texAddrModeS = parse(s);
                tex->TexSamplingMode()._texAddrModeT = parse(t);
                tex->TexSamplingMode()._texAddrModeR = parse(r);
              })
          .def(
              "setMipRange",
              [](texture_ptr_t tex, int min_mip, int max_mip) {
                tex->TexSamplingMode()._minMipLevel = min_mip;
                tex->TexSamplingMode()._maxMipLevel = max_mip;
              },
              py::arg("min_mip"),
              py::arg("max_mip"));

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
                          "decodePickID",
                          [type_codec](pixelfetchctx_ptr_t pfc, uint32_t pick_id) -> py::object {
                            auto decoded = pfc->decodePickID(pick_id);
                            return type_codec->encode(decoded);
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
      .def(py::init([](py::kwargs kwargs) -> texturearray_ptr_t {
        auto rval        = std::make_shared<TextureArray>();
        size_t w         = 1;
        size_t h         = 1;
        size_t maxslices = 1;
        crcstring_ptr_t fmt = std::make_shared<CrcString>("RGB8"_crcu);
        bool needs_mips = false;
        if (kwargs.contains("w")) {
          w = kwargs["w"].cast<size_t>();
        }
        if (kwargs.contains("h")) {
          h = kwargs["h"].cast<size_t>();
        }
        if (kwargs.contains("slices")) {
          maxslices = kwargs["slices"].cast<size_t>();
        }
        if (kwargs.contains("fmt")) {
          fmt = kwargs["fmt"].cast<crcstring_ptr_t>();
        }
        if (kwargs.contains("mipmapped")) {
          needs_mips = kwargs["mipmapped"].cast<bool>();
        }
        auto efmt      = EBufferFormat(fmt->hashed());
        rval->_requires_mips = needs_mips;
        rval->resize(w, h, maxslices, efmt);
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
      .def_property_readonly(
          "tex",
          [](texturearray_ptr_t texarray) -> texture_ptr_t { //
            return texarray->_tex;
          })
      .def_property(
          "streaming",
          [](texturearray_ptr_t texarray) -> bool {
            return texarray->_tex ? texarray->_tex->_streaming : false;
          },
          [](texturearray_ptr_t texarray, bool v) {
            if (texarray->_tex) texarray->_tex->_streaming = v;
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
  // COMFORT-1: the microtask scheduler's learned per-slice cost, keyed by
  // GpuMicrotask::_costKey. Process-wide (the registry is), so it reads the same
  // whichever context ran the slices. This is the surface a hitch/comfort
  // investigation measures against — worst_ms is the number that has to fit
  // inside a frame.
  /////////////////////////////////////////////////////////////////////////////////
  module_lev2.def("microtaskCostStats", []() -> py::dict {
    py::dict rval;
    for (const auto& item : MicrotaskCostRegistry::snapshot()) {
      const auto& e = item.second;
      py::dict d;
      d["slices"]           = e._sliceCount;
      d["last_ms"]          = double(e._lastSliceUs) * 0.001;
      d["worst_ms"]         = double(e._worstSliceUs) * 0.001;
      d["second_worst_ms"]  = double(e._secondWorstSliceUs) * 0.001;
      d["total_ms"]         = double(e._totalSliceUs) * 0.001;
      d["deferrals"]        = e._deferrals;
      d["escapes"]          = e._escapes;
      d["instances_seeded"] = e._instancesSeeded;
      d["scale"]            = e._estScaleQ16Ema / 65536.0;
      d["seed_scale"]       = double(e._lastSeedScaleQ16) / 65536.0;
      rval[py::str(item.first)] = d;
    }
    return rval;
  });
  module_lev2.def("resetMicrotaskCostStats", []() { //
    MicrotaskCostRegistry::resetMeasurements();
  });
  /////////////////////////////////////////////////////////////////////////////////
  // LIVE forward-MSAA level (0=off,1=2x,2=4x,3=8x). This is the app-level knob the
  // forward compositor sizes its primary target from, and it is legitimately settable
  // AFTER init — the scene `msaa=` param lands here at runtime (SceneGraphSystem) and
  // ForwardPbrNodeImpl rebuilds its RtgSet when the level it built at no longer matches.
  // Same handle, exposed: headless kwargs cannot reach _ginitdata (headless_appinit
  // pre-seeds it with a default AppInitData before lev2appinit's kwargs are read).
  /////////////////////////////////////////////////////////////////////////////////
  module_lev2.def("forwardMsaaLevel", []() -> int { //
    return _ginitdata ? _ginitdata->_msaa_samples : 0;
  });
  module_lev2.def("setForwardMsaaLevel", [](int level) { //
    if (not _ginitdata)
      throw std::runtime_error("setForwardMsaaLevel: lev2 is not initialized (no app init data)");
    if (level < 0)
      throw std::runtime_error(FormatString("setForwardMsaaLevel: level %d is negative", level));
    _ginitdata->_msaa_samples = level;
  });
  /////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
} // namespace ork::lev2
