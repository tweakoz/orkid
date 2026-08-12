////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeDecompBlur.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeSSSS.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeHeatDistort.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeHSVG.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeACES.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeUser.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/PostFxNodeFadeToColor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_compositor(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();

  auto scf_type = py::class_<StandardCompositorFrame, standardcompositorframe_ptr_t>(module_lev2, "StandardCompositorFrame")
                      .def(py::init<>())
                      .def_property(
                          "drawEvent",
                          [](standardcompositorframe_ptr_t scf) -> uidrawevent_ptr_t {
                            auto mut = std::const_pointer_cast<::ork::ui::DrawEvent>(scf->_drawEvent);
                            return mut;
                          },
                          [](standardcompositorframe_ptr_t scf, uidrawevent_ptr_t de) { scf->_drawEvent = de; });
  type_codec->registerStdCodec<standardcompositorframe_ptr_t>(scf_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto compositorpassdata_type = //
      py::class_<CompositingPassData, compositingpassdata_ptr_t>(module_lev2, "CompositingPassData")
          .def(py::init<>())
          .def_property(
              "cameramatrices_clone",
              [](compositingpassdata_ptr_t cpd) -> cameramatrices_ptr_t {
                // TODO: cannot return a const shared_ptr yet, due to bindings weirdness
                // so for now clone it
                auto clone = std::make_shared<CameraMatrices>(*(cpd->_mono_cam_matrices));
                return clone;
              },
              [](compositingpassdata_ptr_t cpd, cameramatrices_ptr_t m) { cpd->setSharedCameraMatrices(m); })
          // SINGLE-PASS STEREO: the one flag every stereo technique/pipeline fork reads off the
          // active CPD. Set it on a CPD pushed below the compositor and a draw takes the _ST arm.
          .def(
              "setSinglePassStereo",
              [](compositingpassdata_ptr_t cpd, bool ena) { cpd->setSinglePassStereo(ena); })
          .def_property_readonly(
              "is_single_pass_stereo",
              [](compositingpassdata_ptr_t cpd) -> bool { return cpd->isSinglePassStereo(); })
          .def("__repr__", [](compositingpassdata_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingPassData(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositingpassdata_ptr_t>(compositorpassdata_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<RenderPresetContext>(module_lev2, "RenderPresetContext");
  /////////////////////////////////////////////////////////////////////////////////
  auto rendernode_type = //
      py::class_<RenderCompositingNode, ::ork::Object, compositorrendernode_ptr_t>(module_lev2, "RenderCompositingNode")
          .def_property(
              "layers",
              [](compositorrendernode_ptr_t rnode) -> std::string { return rnode->_layers; },
              [](compositorrendernode_ptr_t rnode, std::string l) { rnode->_layers = l; })
          .def_property_readonly(
              "outputGroup",
              [](compositorrendernode_ptr_t rnode) -> rtgroup_ptr_t { //
                return rnode->GetOutputGroup();
              })
          .def_property_readonly(
              "outputBuffer",
              [](compositorrendernode_ptr_t rnode) -> rtbuffer_ptr_t { //
                return rnode->GetOutput();
              })
              .def_property(
                "debugRenderingModel",
                [](compositorrendernode_ptr_t node) -> crcstring_ptr_t { //
                  uint32_t id = node->_debugRenderingModel;
                  auto crc    = std::make_shared<CrcString>(uint64_t(id)); //
                  return crc;
                },
                [](compositorrendernode_ptr_t node, crcstring_ptr_t value) {        //
                  node->_debugRenderingModel = uint32_t(value->hashed()); //
                })
            .def_property(
                "debugPassID",
                [](compositorrendernode_ptr_t node) -> crcstring_ptr_t { //
                  uint32_t id = node->_debugPassID;
                  auto crc    = std::make_shared<CrcString>(uint64_t(id)); //
                  return crc;
                },
                [](compositorrendernode_ptr_t node, crcstring_ptr_t value) { //
                  node->_debugPassID = uint32_t(value->hashed());  //
                })
            .def_property(
                "debugSubPassID",
                [](compositorrendernode_ptr_t node) -> crcstring_ptr_t { //
                  uint32_t id = node->_debugSubPassID;
                  auto crc    = std::make_shared<CrcString>(uint64_t(id)); //
                  return crc;
                },
                [](compositorrendernode_ptr_t node, crcstring_ptr_t value) {   //
                  node->_debugSubPassID = uint32_t(value->hashed()); //
                })
            .def("__repr__", [](compositorrendernode_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("RenderCompositingNode(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositorrendernode_ptr_t>(rendernode_type);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  auto postnode_type = //
      py::class_<PostCompositingNode, ::ork::Object, compositorpostnode_ptr_t>(module_lev2, "PostFxNode")
          .def(
              "__repr__",
              [](compositorpostnode_ptr_t d) -> std::string {
                fxstring<64> fxs;
                fxs.format("PostCompositingNode(%p)", d.get());
                return fxs.c_str();
              })
          .def_property_readonly("outputGroup", [](compositorpostnode_ptr_t d) -> rtgroup_ptr_t { return d->GetOutputGroup(); })
          .def_property_readonly("outputBuffer", [](compositorpostnode_ptr_t d) -> rtbuffer_ptr_t { return d->GetOutput(); })
          .def("addToSceneVars", [](compositorpostnode_ptr_t dcnode, varmap::varmap_ptr_t vm, const std::string& key) {
            vm->reifyValueForKey<postfx_node_chain_t>(key).push_back(dcnode);
          });
  type_codec->registerStdCodec<compositorpostnode_ptr_t>(postnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto lambdapostnode_type = //
      py::class_<LambdaPostCompositingNode, PostCompositingNode, lambda_postnode_ptr_t>(module_lev2, "LambdaPostFxNode")
          .def(py::init<>())
          .def("__repr__", [](lambda_postnode_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("LambdaPostCompositingNode(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<lambda_postnode_ptr_t>(lambdapostnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto dcblurpostnode_type = //
      py::class_<PostFxNodeDecompBlur, PostCompositingNode, decompblur_postnode_ptr_t>(module_lev2, "PostFxNodeDecompBlur")
          .def(py::init<>())
          .def("gpuInit", [](decompblur_postnode_ptr_t dcnode, ctx_t ctx, int w, int h) { dcnode->gpuInit(ctx.get(), w, h); })
          .def_property(
              "threshold",
              [](decompblur_postnode_ptr_t dcnode) -> float { return dcnode->_threshold; },
              [](decompblur_postnode_ptr_t dcnode, float threshold) { dcnode->_threshold = threshold; })
          .def_property(
              "blurwidth",
              [](decompblur_postnode_ptr_t dcnode) -> float { return dcnode->_blurwidth; },
              [](decompblur_postnode_ptr_t dcnode, float blurwidth) { dcnode->_blurwidth = blurwidth; })
          .def_property(
              "blurfactor",
              [](decompblur_postnode_ptr_t dcnode) -> float { return dcnode->_blurfactor; },
              [](decompblur_postnode_ptr_t dcnode, float blurfactor) { dcnode->_blurfactor = blurfactor; })
          .def_property(
              "amount",
              [](decompblur_postnode_ptr_t dcnode) -> float { return dcnode->_amount; },
              [](decompblur_postnode_ptr_t dcnode, float amount) { dcnode->_amount = amount; })
          .def("__repr__", [](decompblur_postnode_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("DecompBlurPostFxNode(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<decompblur_postnode_ptr_t>(dcblurpostnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  // PBR2 Phase 3 (P3.D) — Separable Subsurface Scattering post-fx node.
  auto sssspostnode_type =
      py::class_<PostFxNodeSSSS, PostCompositingNode, postnode_ssss_ptr_t>(module_lev2, "PostFxNodeSSSS")
          .def(py::init<>())
          .def("gpuInit", [](postnode_ssss_ptr_t n, ctx_t ctx, int w, int h) { n->gpuInit(ctx.get(), w, h); })
          .def_property(
              "blurfactor",
              [](postnode_ssss_ptr_t n) -> float { return n->_blurfactor; },
              [](postnode_ssss_ptr_t n, float v) { n->_blurfactor = v; })
          .def_property(
              "strength",
              [](postnode_ssss_ptr_t n) -> float { return n->_strength; },
              [](postnode_ssss_ptr_t n, float v) { n->_strength = v; })
          .def_property(
              "subsurface_tint",
              [](postnode_ssss_ptr_t n) -> fvec3 { return n->_subsurface_tint; },
              [](postnode_ssss_ptr_t n, fvec3 v) { n->_subsurface_tint = v; })
          .def_property(
              "debug_mode",
              [](postnode_ssss_ptr_t n) -> int { return n->_debug_mode; },
              [](postnode_ssss_ptr_t n, int v) { n->_debug_mode = v; })
          .def("__repr__", [](postnode_ssss_ptr_t n) -> std::string {
            fxstring<64> fxs;
            fxs.format("PostFxNodeSSSS(%p)", n.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<postnode_ssss_ptr_t>(sssspostnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  // E2B item D — heat-distortion post-fx (reads the forward node's generic
  // "aux_<channel>" RT; declared in scenes via addPostFxNode -> round-trips).
  auto heatdistortpostnode_type =
      py::class_<PostFxNodeHeatDistort, PostCompositingNode, postnode_heatdistort_ptr_t>(module_lev2, "PostFxNodeHeatDistort")
          .def(py::init<>())
          .def("gpuInit", [](postnode_heatdistort_ptr_t n, ctx_t ctx, int w, int h) { n->gpuInit(ctx.get(), w, h); })
          .def_property(
              "strength",
              [](postnode_heatdistort_ptr_t n) -> float { return n->_strength; },
              [](postnode_heatdistort_ptr_t n, float v) { n->_strength = v; })
          .def_property(
              "chroma",
              [](postnode_heatdistort_ptr_t n) -> float { return n->_chroma; },
              [](postnode_heatdistort_ptr_t n, float v) { n->_chroma = v; })
          .def_property(
              "channel",
              [](postnode_heatdistort_ptr_t n) -> std::string { return n->_channel; },
              [](postnode_heatdistort_ptr_t n, std::string v) { n->_channel = v; })
          .def("__repr__", [](postnode_heatdistort_ptr_t n) -> std::string {
            fxstring<64> fxs;
            fxs.format("PostFxNodeHeatDistort(%p)", n.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<postnode_heatdistort_ptr_t>(heatdistortpostnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto dchsvgpostnode_type = //
      py::class_<PostFxNodeHSVG, PostCompositingNode, postnode_hsvg_ptr_t>(module_lev2, "PostFxNodeHSVG")
          .def(py::init<>())
          .def("gpuInit", [](postnode_hsvg_ptr_t dcnode, ctx_t ctx, int w, int h) { dcnode->gpuInit(ctx.get(), w, h); })
          .def_property(
              "hue",
              [](postnode_hsvg_ptr_t dcnode) -> float { return dcnode->_hue; },
              [](postnode_hsvg_ptr_t dcnode, float hue) { dcnode->_hue = hue; })
          .def_property(
              "saturation",
              [](postnode_hsvg_ptr_t dcnode) -> float { return dcnode->_saturation; },
              [](postnode_hsvg_ptr_t dcnode, float saturation) { dcnode->_saturation = saturation; })
          .def_property(
              "value",
              [](postnode_hsvg_ptr_t dcnode) -> float { return dcnode->_value; },
              [](postnode_hsvg_ptr_t dcnode, float value) { dcnode->_value = value; })
          .def_property(
              "gamma",
              [](postnode_hsvg_ptr_t dcnode) -> float { return dcnode->_gamma; },
              [](postnode_hsvg_ptr_t dcnode, float gamma) { dcnode->_gamma = gamma; })
          .def("__repr__", [](postnode_hsvg_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("PostFxNodeHSVG(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<postnode_hsvg_ptr_t>(dchsvgpostnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto dcfade2clrpostnode_type = //
      py::class_<PostFxNodeFadeToColor, PostCompositingNode, postnode_fadetocolor_ptr_t>(module_lev2, "PostFxNodeFadeToColor")
          .def(py::init<>())
          .def("gpuInit", [](postnode_fadetocolor_ptr_t dcnode, ctx_t ctx, int w, int h) { dcnode->gpuInit(ctx.get(), w, h); })
          .def_property(
              "fadeColor",
              [](postnode_fadetocolor_ptr_t dcnode) -> fvec4 { return dcnode->_fadeColor; },
              [](postnode_fadetocolor_ptr_t dcnode, fvec4 c) { dcnode->_fadeColor = c; })
          .def_property(
              "fadeAmount",
              [](postnode_fadetocolor_ptr_t dcnode) -> float { return dcnode->_fadeAmount; },
              [](postnode_fadetocolor_ptr_t dcnode, float a) { dcnode->_fadeAmount = a; })
          .def("__repr__", [](postnode_fadetocolor_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("PostFxNodeFadeToColor(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<postnode_fadetocolor_ptr_t>(dcfade2clrpostnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto dcacespostnode_type = //
      py::class_<PostFxNodeACES, PostCompositingNode, postnode_aces_ptr_t>(module_lev2, "PostFxNodeACES")
          .def(py::init<>())
          .def("gpuInit", [](postnode_aces_ptr_t dcnode, ctx_t ctx, int w, int h) { dcnode->gpuInit(ctx.get(), w, h); })
          .def_property(
              "exposure",
              [](postnode_aces_ptr_t dcnode) -> float { return dcnode->_exposure; },
              [](postnode_aces_ptr_t dcnode, float exposure) { dcnode->_exposure = exposure; })
#define _ACES_ADAPT_PROP(pyname, member)                                                       \
  .def_property(                                                                               \
      pyname,                                                                                  \
      [](postnode_aces_ptr_t n) -> float { return n->member; },                                \
      [](postnode_aces_ptr_t n, float v) { n->member = v; })
          _ACES_ADAPT_PROP("adapt_day_luminance", _adaptDayLuminance)      //
          _ACES_ADAPT_PROP("adapt_twilight_luminance", _adaptTwilightLuminance) //
          _ACES_ADAPT_PROP("adapt_floor_luminance", _adaptFloorLuminance)  //
          _ACES_ADAPT_PROP("adapt_day", _adaptDay)                         //
          _ACES_ADAPT_PROP("adapt_twilight", _adaptTwilight)               //
          _ACES_ADAPT_PROP("adapt_floor", _adaptFloor)
#undef _ACES_ADAPT_PROP
          // the adaptation curve itself, so a gate can sweep it without a frame.
          // seed_sun_elevation_sin is only consulted when luminance < 0.
          .def(
              "sceneAdaptation",
              [](postnode_aces_ptr_t n, float luminance, float seed_sun_elevation_sin) -> float {
                return n->sceneAdaptation(luminance, seed_sun_elevation_sin);
              },
              py::arg("luminance"),
              py::arg("seed_sun_elevation_sin") = 1.0f)
          .def("__repr__", [](postnode_aces_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("PostFxNodeACES(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<postnode_aces_ptr_t>(dcacespostnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  // materialinst params proxy
  /////////////////////////////////////////////////////////////////////////////////
  struct usernode_param_proxy {
    postnode_user_ptr_t _usernode;
  };
  using usernode_param_proxy_ptr_t = std::shared_ptr<usernode_param_proxy>;
  auto usernode_params_type        =                                                                   //
      py::class_<usernode_param_proxy, usernode_param_proxy_ptr_t>(module_lev2, "UserNodeParamsProxy") //
          .def(
              "__repr__",
              [](usernode_param_proxy_ptr_t proxy) -> std::string {
                std::string output;
                output += FormatString("UserNodeParamsProxy<%p>{\n", proxy.get());
                for (auto item : proxy->_usernode->_bindings) {
                  const auto& k = item.first;
                  const auto& v = item.second;
                  auto vstr     = v.typestr();
                  output += FormatString("  binding(%s): valtype(%s),\n", k.c_str(), vstr.c_str());
                }
                output += "}\n";
                return output.c_str();
              })
          .def(
              "__setattr__",                                                                   //
              [type_codec](usernode_param_proxy_ptr_t proxy, py::object key, py::object val) { //
                auto var_key = type_codec->decode(key);
                auto var_val = type_codec->decode(val);
                if (auto as_str = var_key.tryAs<std::string>()) {
                  proxy->_usernode->_bindings[as_str.value()] = var_val;
                } else {
                  OrkAssert(false);
                }
              })
          .def(
              "__getattr__",                                                                 //
              [type_codec](usernode_param_proxy_ptr_t proxy, py::object key) -> py::object { //
                auto var_key = type_codec->decode(key);
                if (auto as_str = var_key.tryAs<std::string>()) {
                  auto it = proxy->_usernode->_bindings.find(as_str.value());
                  if (it != proxy->_usernode->_bindings.end()) {
                    auto var_val = it->second;
                    return type_codec->encode(var_val);
                  }
                } else {
                  OrkAssert(false);
                }
                return py::none();
              });

  type_codec->registerStdCodec<usernode_param_proxy_ptr_t>(usernode_params_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto dcuserpostnode_type = //
      py::class_<PostFxNodeUser, PostCompositingNode, postnode_user_ptr_t>(module_lev2, "PostFxNodeUser")
          .def(py::init<>())
          .def("gpuInit", [](postnode_user_ptr_t dcnode, ctx_t ctx, int w, int h) { dcnode->gpuInit(ctx.get(), w, h); })
          .def_property(
              "disabled", //
              [](postnode_user_ptr_t dcnode) -> bool { return dcnode->_disabled; },
              [](postnode_user_ptr_t dcnode, bool disabled) { dcnode->_disabled = disabled; })
          .def_property(
              "shader_path", //
              [](postnode_user_ptr_t dcnode) -> std::string { return dcnode->_shader_path; },
              [](postnode_user_ptr_t dcnode, std::string shaderpath) { dcnode->_shader_path = shaderpath; })
          .def_property(
              "technique", //
              [](postnode_user_ptr_t dcnode) -> std::string { return dcnode->_technique_name; },
              [](postnode_user_ptr_t dcnode, std::string technique) { dcnode->_technique_name = technique; })
          .def_property(
              "double_buffer", //
              [](postnode_user_ptr_t dcnode) -> bool { return dcnode->_double_buffer; },
              [](postnode_user_ptr_t dcnode, bool double_buffer) { dcnode->_double_buffer = double_buffer; })
          .def_property(
              "flip_vertical", //
              [](postnode_user_ptr_t dcnode) -> bool { return dcnode->_flip_vertical; },
              [](postnode_user_ptr_t dcnode, bool flip_vertical) { dcnode->_flip_vertical = flip_vertical; })
          .def_property_readonly("texture_provider", [](postnode_user_ptr_t dcnode) -> texture_provider_ptr_t {
            // Return provider that dynamically gets current read buffer texture
            return std::make_shared<LambdaTextureProvider>(
                [dcnode]() -> texture_ptr_t {
                  return dcnode->getCurrentReadTexture();
                }
            );
          })
          .def_property_readonly(
              "params",                                                                //
              [type_codec](postnode_user_ptr_t dcnode) -> usernode_param_proxy_ptr_t { //
                auto proxy       = std::make_shared<usernode_param_proxy>();
                proxy->_usernode = dcnode;
                return proxy;
              })
          .def("__repr__", [](postnode_user_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("PostFxNodeUSER(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<postnode_user_ptr_t>(dcuserpostnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  auto outputnode_type = //
      py::class_<OutputCompositingNode, ::ork::Object, compositoroutnode_ptr_t>(module_lev2, "OutputCompositingNode")
          .def(
              "__repr__",
              [](compositoroutnode_ptr_t d) -> std::string {
                fxstring<64> fxs;
                fxs.format("OutputCompositingNode(%p)", d.get());
                return fxs.c_str();
              })
          .def_property(
              "flipY",
              [](compositoroutnode_ptr_t n) -> bool { return n->_flipY; },
              [](compositoroutnode_ptr_t n, bool b) { n->_flipY = b; })
          .def(
              "onBeginAssemble",
              [type_codec](compositoroutnode_ptr_t n, py::function f) {
                pyfn_ptr_t f_ptr = std::make_shared<py::function>(f);
                n->_pyimpl_oba.set<pyfn_ptr_t>(f_ptr); // store the function
                n->_onBeginAssemble = [n, type_codec](CompositorDrawData& drawdata) {
                  py::gil_scoped_acquire gil;
                  compositordrawdata_ptr_t ddptr = compositordrawdata_ptr_t(&drawdata);
                  auto ddpy                      = type_codec->encode(ddptr);
                  auto f_ptr                     = n->_pyimpl_oba.get<pyfn_ptr_t>();
                  (*f_ptr)(ddpy);
                };
              })
          .def(
              "onEndAssemble",
              [type_codec](compositoroutnode_ptr_t n, py::function f) {
                pyfn_ptr_t f_ptr = std::make_shared<py::function>(f);
                n->_pyimpl_oea.set<pyfn_ptr_t>(f_ptr); // store the function
                n->_onEndAssemble = [n, type_codec](CompositorDrawData& drawdata) {
                  py::gil_scoped_acquire gil;
                  compositordrawdata_ptr_t ddptr = compositordrawdata_ptr_t(&drawdata);
                  auto ddpy                      = type_codec->encode(ddptr);
                  auto f_ptr                     = n->_pyimpl_oea.get<pyfn_ptr_t>();
                  (*f_ptr)(ddpy);
                };
              })
          .def("onCameraChange", [type_codec](compositoroutnode_ptr_t n, py::function f) {
            pyfn_ptr_t f_ptr = std::make_shared<py::function>(f);
            n->_pyimpl_oba.set<pyfn_ptr_t>(f_ptr); // store the function
            n->_onCameraChange = [n, type_codec](CompositorDrawData& drawdata) {
              py::gil_scoped_acquire gil;
              compositordrawdata_ptr_t ddptr = compositordrawdata_ptr_t(&drawdata);
              auto ddpy                      = type_codec->encode(ddptr);
              auto f_ptr                     = n->_pyimpl_oba.get<pyfn_ptr_t>();
              (*f_ptr)(ddpy);
            };
          });
  type_codec->registerStdCodec<compositoroutnode_ptr_t>(outputnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto scene_type = //
      py::class_<CompositingScene, compositingscene_ptr_t>(module_lev2, "CompositingScene")
          .def(
              "createSceneItem",
              [](compositingscene_ptr_t scene, std::string named) -> compositingsceneitem_ptr_t {
                auto item            = std::make_shared<CompositingSceneItem>();
                scene->_items[named] = item;
                auto cdata           = scene->_parent;
                cdata->_activeItem   = named;
                return item;
              })
          .def("__repr__", [](compositingscene_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingScene(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositingscene_ptr_t>(scene_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto sceneitem_type = //
      py::class_<CompositingSceneItem, compositingsceneitem_ptr_t>(module_lev2, "CompositingSceneItem")
          .def(py::init<>())
          .def_property(
              "technique",
              [](compositingsceneitem_ptr_t item) -> compositortechnique_ptr_t { return item->_technique; },
              [](compositingsceneitem_ptr_t item, compositortechnique_ptr_t t) { item->_technique = t; })
          .def("__repr__", [](compositingsceneitem_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingSceneItem(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositingsceneitem_ptr_t>(sceneitem_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto technique_type = //
      py::class_<CompositingTechnique, compositortechnique_ptr_t>(module_lev2, "CompositingTechnique")
          .def("__repr__", [](compositortechnique_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingTechnique(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositortechnique_ptr_t>(technique_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto nodecompositortechnique_type = //
      py::class_<NodeCompositingTechnique, CompositingTechnique, nodecompositortechnique_ptr_t>(
          module_lev2, "NodeCompositingTechnique")
          .def(py::init<>())
          .def_property(
              "renderNode",
              [](nodecompositortechnique_ptr_t ntek) -> compositorrendernode_ptr_t { return ntek->_renderNode; },
              [](nodecompositortechnique_ptr_t ntek, compositorrendernode_ptr_t t) { ntek->_renderNode = t; })
          .def_property(
              "outputNode",
              [](nodecompositortechnique_ptr_t ntek) -> compositoroutnode_ptr_t { return ntek->_outputNode; },
              [](nodecompositortechnique_ptr_t ntek, compositoroutnode_ptr_t t) { ntek->_outputNode = t; })
          .def_property_readonly(
              "postEffectNodes",
              [](nodecompositortechnique_ptr_t ntek) -> std::vector<compositorpostnode_ptr_t> { return ntek->_postEffectNodes; })
          .def("__repr__", [](nodecompositortechnique_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("NodeCompositingTechnique(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<nodecompositortechnique_ptr_t>(nodecompositortechnique_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto compositordata_type = //
      py::class_<CompositingData, compositordata_ptr_t>(module_lev2, "CompositingData")
          .def(py::init<>())
          .def(
              "createScene",
              [](compositordata_ptr_t cdata, std::string named) -> compositingscene_ptr_t {
                auto scene            = std::make_shared<CompositingScene>();
                cdata->_scenes[named] = scene;
                cdata->_activeScene   = named;
                scene->_parent        = cdata.get();
                return scene;
              })
          .def("__repr__", [](compositordata_ptr_t d) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingData(%p)", d.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositordata_ptr_t>(compositordata_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto vd_t = py::class_<ViewData, viewdata_ptr_t>(module_lev2, "ViewData")
                  .def_property_readonly(
                      "camposmono",
                      [](viewdata_ptr_t vd) -> fvec3 { //
                        return vd->_camposmono;
                      })
                  .def_property_readonly(
                      "zndc2eye",
                      [](viewdata_ptr_t vd) -> fvec2 { //
                        return vd->_zndc2eye;
                      })
                  .def_property_readonly(
                      "near",
                      [](viewdata_ptr_t vd) -> float { //
                        return vd->_near;
                      })
                  .def_property_readonly(
                      "far",
                      [](viewdata_ptr_t vd) -> float { //
                        return vd->_far;
                      })
                  .def_property_readonly(
                      "VL",
                      [](viewdata_ptr_t vd) -> fmtx4 { //
                        return vd->VL;
                      })
                  .def_property_readonly(
                      "VR",
                      [](viewdata_ptr_t vd) -> fmtx4 { //
                        return vd->VR;
                      })
                  .def_property_readonly(
                      "VM",
                      [](viewdata_ptr_t vd) -> fmtx4 { //
                        return vd->VM;
                      })
                  .def_property_readonly(
                      "PL",
                      [](viewdata_ptr_t vd) -> fmtx4 { //
                        return vd->PL;
                      })
                  .def_property_readonly(
                      "PR",
                      [](viewdata_ptr_t vd) -> fmtx4 { //
                        return vd->PR;
                      })
                  .def_property_readonly(
                      "PM",
                      [](viewdata_ptr_t vd) -> fmtx4 { //
                        return vd->PM;
                      })
                  .def_property_readonly(
                      "VPL",
                      [](viewdata_ptr_t vd) -> fmtx4 { //
                        return vd->VPL;
                      })
                  .def_property_readonly(
                      "VPR",
                      [](viewdata_ptr_t vd) -> fmtx4 { //
                        return vd->VPR;
                      })
                  .def_property_readonly(
                      "VPM",
                      [](viewdata_ptr_t vd) -> fmtx4 { //
                        return vd->VPM;
                      })
                  .def_property_readonly(
                      "IVPL",
                      [](viewdata_ptr_t vd) -> fmtx4 { //
                        return vd->IVPL;
                      })
                  .def_property_readonly(
                      "IVPR",
                      [](viewdata_ptr_t vd) -> fmtx4 { //
                        return vd->IVPR;
                      })
                  .def_property_readonly("IVPM", [](viewdata_ptr_t vd) -> fmtx4 { //
                    return vd->IVPM;
                  });
  type_codec->registerStdCodec<viewdata_ptr_t>(vd_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto cdd_t = py::class_<compositordrawdata_ptr_t>(module_lev2, "CompositorDrawData")
                   .def_property_readonly(
                       "camposmono",
                       [](compositordrawdata_ptr_t cdd) -> fvec3 { //
                         return cdd->computeViewData()._camposmono;
                       })
                   .def_property_readonly(
                       "viewdata",
                       [](compositordrawdata_ptr_t cdd) -> viewdata_ptr_t { //
                         return std::make_shared<ViewData>(cdd->computeViewData());
                       })
                   .def(
                       "rendererProperty",
                       [type_codec](compositordrawdata_ptr_t cdd, crcstring_ptr_t crcstr) -> py::object { //
                         auto it = cdd->_properties.find(crcstr->hashed());
                         // todo - use type codec to decode svar16_t
                         if (it != cdd->_properties.end()) {
                           auto svar          = it->second;
                           py::object ret_val = py::none();
                           if (auto as_int = svar.tryAs<int>()) {
                             ret_val = py::int_(as_int.value());
                           } else if (auto as_float = svar.tryAs<float>()) {
                             ret_val = py::float_(as_float.value());
                           } else if (auto as_vec3 = svar.tryAs<fvec3>()) {
                             ret_val = type_codec->encode(as_vec3.value());
                           } else if (auto as_vec4 = svar.tryAs<fvec4>()) {
                             ret_val = type_codec->encode(as_vec4.value());
                           } else if (auto as_camm = svar.tryAs<cameramatrices_ptr_t>()) {
                             ret_val = type_codec->encode(as_camm.value());
                           }
                           return ret_val;
                         }
                         return py::none();
                       })
                   .def("__repr__", [](compositordrawdata_ptr_t d) -> std::string {
                     fxstring<64> fxs;
                     fxs.format("CompositorDrawData(%p)", d.get());
                     return fxs.c_str();
                   });
  type_codec->registerStdCodec<compositordrawdata_ptr_t>(cdd_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto compositorimpl_type = //
      py::class_<CompositingImpl, compositorimpl_ptr_t>(module_lev2, "CompositingImpl")
          .def(py::init([](compositordata_ptr_t cdata) -> compositorimpl_ptr_t { //
            return std::make_shared<CompositingImpl>(cdata);
          }))
          .def("pushCPD", [](compositorimpl_ptr_t ci, compositingpassdata_ptr_t cpd) { ci->pushCPD(*cpd); })
          .def("popCPD", [](compositorimpl_ptr_t ci) { ci->popCPD(); })
          .def_property_readonly(
              "context",
              [](compositorimpl_ptr_t ci) -> compositorctx_ptr_t { //
                return ci->_compcontext;
              })
          .def("__repr__", [](compositorimpl_ptr_t i) -> std::string {
            fxstring<64> fxs;
            fxs.format("CompositingImpl(%p)", i.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositorimpl_ptr_t>(compositorimpl_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto compositorctx_type = //
      py::class_<CompositingContext, compositorctx_ptr_t>(module_lev2, "CompositingContext");
  type_codec->registerStdCodec<compositorctx_ptr_t>(compositorctx_type);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  using unlit_ptr_t   = std::shared_ptr<compositor::UnlitNode>;
  auto unlitnode_type = //
      py::class_<compositor::UnlitNode, RenderCompositingNode, unlit_ptr_t>(module_lev2, "UnlitRenderNode")
          .def(py::init([] -> unlit_ptr_t { //
            return std::make_shared<compositor::UnlitNode>();
          }))
          .def("__repr__", [](unlit_ptr_t i) -> std::string {
            fxstring<64> fxs;
            fxs.format("UnlitRenderNode(%p)", i.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<unlit_ptr_t>(unlitnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  using fwdpbrnode_ptr_t = std::shared_ptr<pbr::ForwardNode>;
  auto fwdpbrnode_type   = //
      py::class_<pbr::ForwardNode, RenderCompositingNode, fwdpbrnode_ptr_t>(module_lev2, "PbrForwardNode")
          .def(py::init([] -> fwdpbrnode_ptr_t { //
            return std::make_shared<pbr::ForwardNode>(nullptr);
          }))
          .def_property_readonly(
              "pbr_common",
              [](fwdpbrnode_ptr_t node) -> pbr::commonstuff_ptr_t { //
                return node->_pbrcommon;
              })
          .def("__repr__", [](fwdpbrnode_ptr_t node) -> std::string {
            fxstring<64> fxs;
            fxs.format("PbrForwardNode(%p)", node.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<fwdpbrnode_ptr_t>(fwdpbrnode_type);

  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  using scroutnode_ptr_t = std::shared_ptr<ScreenOutputCompositingNode>;
  auto scroutnode_type   = //
      py::class_<ScreenOutputCompositingNode, OutputCompositingNode, scroutnode_ptr_t>(module_lev2, "ScreenOutputNode")
          .def(py::init([] -> scroutnode_ptr_t { //
            return std::make_shared<ScreenOutputCompositingNode>();
          }))
          .def_property(
              "format",
              [](scroutnode_ptr_t self) -> std::string { return EBufferFormatToName(self->_format); },
              [](scroutnode_ptr_t self, std::string str_val) {
                uint64_t hashed = CrcString(str_val.c_str()).hashed();
                self->_format   = EBufferFormat(hashed);
              })
          .def_property(
              "mono",
              [](scroutnode_ptr_t self) -> bool { return self->_monoviewer; },
              [](scroutnode_ptr_t self, bool value) { self->_monoviewer = value; })
          .def("__repr__", [](scroutnode_ptr_t i) -> std::string {
            fxstring<64> fxs;
            fxs.format("ScreenOutputNode(%p)", i.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<scroutnode_ptr_t>(scroutnode_type);

  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  using vroutnode_ptr_t = std::shared_ptr<VrOutputNode>;
  auto vroutnode_type   = //
      py::class_<VrOutputNode, OutputCompositingNode, vroutnode_ptr_t>(module_lev2, "VrOutputNode")
          .def(py::init([] -> vroutnode_ptr_t { //
            return std::make_shared<VrOutputNode>();
          }))
          .def_property(
              "mono",
              [](vroutnode_ptr_t self) -> bool { return self->_monoviewer; },
              [](vroutnode_ptr_t self, bool value) { self->_monoviewer = value; })
          .def("__repr__", [](vroutnode_ptr_t n) -> std::string {
            fxstring<64> fxs;
            fxs.format("VrOutputNode(%p)", n.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<vroutnode_ptr_t>(vroutnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  // SPVR — THE VR output node. downsampledEyeRtGroup(left) hands back the per-eye
  //  final downsampled buffer (the one the XR runtime and the mirror blit consume).
  using spvroutnode_ptr_t = std::shared_ptr<SinglePassStereoVrOutputNode>;
  auto spvroutnode_type   = //
      py::class_<SinglePassStereoVrOutputNode, OutputCompositingNode, spvroutnode_ptr_t>(
          module_lev2, "SinglePassStereoVrOutputNode")
          .def(
              "downsampledEyeRtGroup",
              [](spvroutnode_ptr_t self, bool left) -> rtgroup_ptr_t { //
                return self->downsampledEyeRtGroup(left);
              },
              py::arg("left"))
          .def("__repr__", [](spvroutnode_ptr_t n) -> std::string {
            fxstring<64> fxs;
            fxs.format("SinglePassStereoVrOutputNode(%p)", n.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<spvroutnode_ptr_t>(spvroutnode_type);
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  auto rtgoutnode_type = //
      py::class_<RtGroupOutputCompositingNode, OutputCompositingNode, compositoroutnode_rtgroup_ptr_t>(
          module_lev2, "RtGroupOutputCompositingNode")
          .def(py::init([] -> compositoroutnode_rtgroup_ptr_t { //
            return std::make_shared<RtGroupOutputCompositingNode>();
          }))
          .def_property(
              "supersample", //
              [](compositoroutnode_rtgroup_ptr_t self) -> int { return self->_supersample; },
              [](compositoroutnode_rtgroup_ptr_t self, int ss) { self->_supersample = ss; })
          .def_property(
              "temporal_frames", //
              [](compositoroutnode_rtgroup_ptr_t self) -> int { return self->_temporalFrames; },
              [](compositoroutnode_rtgroup_ptr_t self, int tf) { self->_temporalFrames = tf; })
          .def("__repr__", [](compositoroutnode_rtgroup_ptr_t i) -> std::string {
            fxstring<64> fxs;
            fxs.format("RtGroupOutputCompositingNode(%p)", i.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<compositoroutnode_rtgroup_ptr_t>(rtgoutnode_type);

  /////////////////////////////////////////////////////////////////////////////////
}
} // namespace ork::lev2
