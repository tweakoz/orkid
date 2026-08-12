////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <algorithm>
#include <sstream>
#include <ork/kernel/opq.h>
#include <ork/lev2/ui/event.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectObjectVector.inl>
#include <ork/math/cvector4.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/lev2/vr/vr.h>   // ECS-VR: register a NoVrDevice for a VR rendermodel preset
#include <ork/lev2/gfx/material_freestyle.h>   // ECS-VR: build the host-supplied distortion present
#include <cctype>

#include <ork/lev2/lev2_asset_cache.inl>
#include <ork/ecs/ecs.h>
#include <ork/ecs/system.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>
#include <ork/ecs/datatable.h>

#include "../core/message_private.h"
#include <ork/util/logger.h>
#include <ork/kernel/profiler.h>
#include <ork/lev2/gfx/texman.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 { extern appinitdata_ptr_t _ginitdata; } // global appinit (MSAA level sink)
///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_sgsys = logger()->configureChannel("ecs.sgcomp", fvec3(0.9, 0.7, 0));
///////////////////////////////////////////////////////////////////////////////
// Resolve layer names from either _multilayers (Python kwarg) or comma-delimited _layername (JSON).
static std::vector<std::string> resolveLayerNames(const SceneGraphNodeItemData* NID) {
  if (NID->_multilayers.size()) {
    return NID->_multilayers;
  }
  std::vector<std::string> result;
  std::istringstream ss(NID->_layername);
  std::string token;
  while (std::getline(ss, token, ',')) {
    auto start = token.find_first_not_of(" \t");
    auto end   = token.find_last_not_of(" \t");
    if (start != std::string::npos)
      result.push_back(token.substr(start, end - start + 1));
  }
  if (result.empty()) result.push_back("");
  return result;
}
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;
using namespace ork::lev2;
using modeldrawable_ptr_t = std::shared_ptr<lev2::ModelDrawableData>;
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystemData::describeX(SystemDataClass* clazz) {
  tokenize(SceneGraphSystem::UpdateCamera);
  ImplementToken(ResizeFromMainSurface);
  ImplementToken(UpdateFramebufferSize);
  ImplementToken(CreateNode);
  ImplementToken(DestroyNode);
  ImplementToken(ChangeModColor);
  ImplementToken(HighlightBySpawnData);
  ImplementToken(SyncTransformBySpawnData);
  ImplementToken(AttachEditorBillboard);
  ImplementToken(eye);
  ImplementToken(tgt);
  ImplementToken(up);
  ImplementToken(near);
  ImplementToken(far);
  ImplementToken(fovy);
  ImplementToken(width);
  ImplementToken(height);

  clazz->directVectorProperty("Layers", &SceneGraphSystemData::_declaredLayers)
      ->annotate("editor.widget", "EcsLayerFactory");
  clazz->directMapProperty("userparams", &SceneGraphSystemData::_userParams);
  clazz->directObjectVectorProperty("drawabledatas", &SceneGraphSystemData::_staticDrawableDatas);

  clazz->intProperty("CookieAtlasWidth", int_range{64, 4096}, &SceneGraphSystemData::_cookieAtlasWidth);
  clazz->intProperty("CookieAtlasHeight", int_range{64, 4096}, &SceneGraphSystemData::_cookieAtlasHeight);
  clazz->intProperty("ShadowAtlasWidth", int_range{64, 4096}, &SceneGraphSystemData::_shadowAtlasWidth);
  clazz->intProperty("ShadowAtlasHeight", int_range{64, 4096}, &SceneGraphSystemData::_shadowAtlasHeight);

  clazz->directProperty("skybox_path", &SceneGraphSystemData::_skybox_path);
  // PBR2 P3.D — reflected post-fx node registry + execution order.
  // _postfx_nodes survives JSON round-trip via directObjectMapProperty
  // (same pattern as Archetype::mComponentDatas — polymorphic shared_ptr
  // to ork::Object subclasses, each carrying its own reflected fields).
  clazz->directObjectMapProperty("postfx_nodes", &SceneGraphSystemData::_postfx_nodes);
  // system-level node declarations (shared/instanced group nodes) must round-trip
  // like the component-level ones always have.
  clazz->directObjectMapProperty("nodedatas", &SceneGraphSystemData::_nodedatas);
  clazz->directProperty("postfx_order",          &SceneGraphSystemData::_postfx_order);
}

///////////////////////////////////////////////////////////////////////////////

SceneGraphSystemData::SceneGraphSystemData() {
  _internalParams = std::make_shared<varmap::VarMap>();
}

void SceneGraphSystemData::setInternalSceneParam(const varmap::key_t& key, const varmap::VarMap::value_type& val) {
  _internalParams->setValueForKey(key, val);
}

void SceneGraphSystemData::setUserSceneParam(const std::string& key, const varmap::VarMap::value_type& val) {
  // orklut asserts on duplicate-key AddSorted in single-key mode;
  // erase any prior entry first so this behaves like dict assignment.
  auto it = _userParams.find(key);
  if (it != _userParams.end()) {
    _userParams.erase(it);
  }
  _userParams.AddSorted(key, val);
}

bool SceneGraphSystemData::hasUserSceneParam(const std::string& key) const {
  return _userParams.find(key) != _userParams.end();
}

void SceneGraphSystemData::declareLayer(const std::string& layername) {
  _declaredLayers.push_back(layername);
}

///////////////////////////////////////////////////////////////////////////////
// PBR2 P3.D — register a PostFxNode under a stable string key. Overwrites
// on collision. Reflected via _postfx_nodes (directObjectMapProperty);
// execution order is determined by _postfx_order (separate string).
// Resolution into the runtime chain happens at _onLink time.

void SceneGraphSystemData::addPostFxNode(const std::string& name, lev2::compositorpostnode_ptr_t node) {
  _postfx_nodes[name] = node;
}

void SceneGraphSystemData::appendPostFxOrder(const std::string& name) {
  // Idempotent: split the current order on commas, skip if name already present.
  size_t pos = 0;
  while (pos < _postfx_order.size()) {
    auto comma = _postfx_order.find(',', pos);
    auto end   = (comma == std::string::npos) ? _postfx_order.size() : comma;
    auto tok   = _postfx_order.substr(pos, end - pos);
    while (!tok.empty() && (tok.front() == ' ' || tok.front() == '\t')) tok.erase(tok.begin());
    while (!tok.empty() && (tok.back()  == ' ' || tok.back()  == '\t')) tok.pop_back();
    if (tok == name) return;   // already present
    pos = (comma == std::string::npos) ? _postfx_order.size() : (comma + 1);
  }
  if (!_postfx_order.empty()) _postfx_order += ",";
  _postfx_order += name;
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystemData::declareNodeOnLayer(nodedef_ptr_t ndef) {
  auto nid                  = std::make_shared<SceneGraphNodeItemData>();
  nid->_nodename            = ndef->_nodename;
  nid->_drawabledata        = ndef->_drawabledata;
  nid->_drawable_asset_name = ndef->_drawable_asset_name;
  nid->_envmap_path         = ndef->_envmap_path;
  nid->_layername           = ndef->_layername;
  nid->_multilayers         = ndef->_multilayers;
  nid->_xfoverride          = ndef->_transform;
  nid->_modcolor            = ndef->_modcolor;

  _nodedatas[ndef->_nodename] = nid;
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystemData::remapLayerName(const std::string& old_name, const std::string& new_name) {
  if (old_name == new_name) {
    return;
  }
  for (auto& s : _declaredLayers) {
    if (s == old_name) {
      s = new_name;
    }
  }
  for (auto& kv : _nodedatas) {
    if (!kv.second) continue;
    if (kv.second->_layername == old_name) {
      kv.second->_layername = new_name;
    }
    for (auto& s : kv.second->_multilayers) {
      if (s == old_name) {
        s = new_name;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystemData::addStaticDrawableData(std::string layername, lev2::drawabledata_ptr_t drwdata) {
  auto ddkvpair           = std::make_shared<lev2::scenegraph::DrawableDataKvPair>();
  ddkvpair->_layername    = layername;
  ddkvpair->_drawabledata = drwdata;
  _staticDrawableDatas.push_back(ddkvpair);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystemData::addStaticDrawable(std::string layername, lev2::drawable_ptr_t drw) {
  lev2::scenegraph::DrawableKvPair kvpair;
  kvpair._layername = layername;
  kvpair._drawable  = drw;
  _staticDrawables.push_back(kvpair);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystemData::bindToRtGroup(lev2::rtgroup_ptr_t rtgroup) {
  setInternalSceneParam("outputRTG", rtgroup);
}
void SceneGraphSystemData::bindToCamera(lev2::cameradata_ptr_t camera) {
  _camera = camera;
}

///////////////////////////////////////////////////////////////////////////////

System* SceneGraphSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new SceneGraphSystem(*this, pinst);
}

void SceneGraphSystemData::enqueueOnSystemCreation(oncreatesys_lambda_t l) {
  _onCreateSystemOperations.push_back(l);
}

void SceneGraphSystemData::declarePrefetchDrawableData(lev2::drawabledata_ptr_t data) {
  _drawdatas_prefetchlist.insert(data);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::describeX(object::ObjectClass* clazz) {
}

SceneGraphSystem::SceneGraphSystem(const SceneGraphSystemData& data, ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst)
    , _SGSD(data) {

  _mergedParams = std::make_shared<varmap::VarMap>();

  if (_SGSD._camera) {
    _camera = _SGSD._camera;
  } else {
    _camera = std::make_shared<CameraData>();
  }
  _camlut                = std::make_shared<CameraDataLut>();
  (*_camlut)["spawncam"] = _camera;
  _drwcache              = std::make_shared<DrawableCache>();
  _modelAssetCache       = std::make_shared<xgmmodel_assetcache_t>();

  for (auto item : data._onCreateSystemOperations) {
    item(this);
  }
}
///////////////////////////////////////////////////////////////////////////////

SceneGraphSystem::~SceneGraphSystem() {

  // defer destruction of the scene to the rendering thread
  //  by stashing it in the simulation for later destruction

  _simulation->_stashRenderThreadDestructable(_scene);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::_addStaticDrawable(std::string layername, lev2::drawable_ptr_t drw) {
  lev2::scenegraph::DrawableKvPair kvpair;
  kvpair._layername = layername;
  kvpair._drawable  = drw;
  _staticDrawables.push_back(kvpair);
}

void SceneGraphSystem::_removeStaticDrawable(lev2::drawable_ptr_t drw) {
  _staticDrawables.erase(
    std::remove_if(_staticDrawables.begin(), _staticDrawables.end(),
      [&](const lev2::scenegraph::DrawableKvPair& kvp) { return kvp._drawable == drw; }),
    _staticDrawables.end());
  // Also remove from the live scene if it exists
  if (_scene) {
    auto& sd = _scene->_staticDrawables;
    sd.erase(
      std::remove_if(sd.begin(), sd.end(),
        [&](const lev2::scenegraph::DrawableKvPair& kvp) { return kvp._drawable == drw; }),
      sd.end());
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::reloadDrawableData(lev2::drawabledata_ptr_t data) {
  _renderops.push([this, data]() {
    std::unordered_set<lev2::Drawable*> already_reloaded;
    _components.atomicOp([&](component_set_t& comps) {
      for (auto* comp : comps) {
        for (auto& [name, nitem] : comp->_nodeitems) {
          if (nitem->_data && nitem->_data->_drawabledata == data) {
            if (already_reloaded.insert(nitem->_drawable.get()).second) {
              data->reloadDrawable(nitem->_drawable);
            }
          }
        }
      }
    });
    // Also check system-level node items
    for (auto& [name, nitem] : _nodeitems) {
      if (nitem->_data && nitem->_data->_drawabledata == data) {
        if (already_reloaded.insert(nitem->_drawable.get()).second) {
          data->reloadDrawable(nitem->_drawable);
        }
      }
    }
  });
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::processRenderOps() {
  _rt_process();
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::initializeForEditMode(lev2::Context* ctx) {
  _onGpuInit(_simulation, ctx);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::_instantiateDeclaredNodes() {
  for (auto NID_item : _SGSD._nodedatas) {
    auto NID = NID_item.second;
    if (NID->_drawabledata) {
      auto drwdata               = NID->_drawabledata;
      auto nitem                 = std::make_shared<SceneGraphNodeItem>();
      nitem->_nodename           = NID->_nodename;
      nitem->_data               = NID;
      _nodeitems[NID->_nodename] = nitem;

      auto on_gpu_init = [=]() {
        if (drwdata->isSharedDrawable()) {
          nitem->_drawable = _drwcache->fetch(drwdata);
        } else {
          nitem->_drawable = drwdata->createDrawable();
          nitem->_drawable->_modcolor = drwdata->_modcolor;
        }
        // PBR2 Phase 0 — per-node HDRI override. NID->_envmap_path has
        // already been resolved (asset:// → .xir path) by Python-side
        // wire_scene_data before the SystemData was staged. Empty path
        // is a no-op inside loadEnvMapOverride.
        ork::lev2::loadEnvMapOverride(nitem->_drawable.get(), NID->_envmap_path);

        if (auto as_instanced = dynamic_pointer_cast<InstancedDrawable>(nitem->_drawable)) {
          // IDLE-SLOT CONTRACT: unallocated instances stay exactly as
          // InstancedDrawableInstanceData::resize() initialized them —
          // ZERO basis (degenerate -> invisible). Do NOT seed positions
          // here; an idle slot that draws anything is a bug.
          auto NODE_ON_LAYER = [=](lev2::scenegraph::layer_ptr_t layer){
            auto node      = layer->createDrawableNode(NID->_nodename, as_instanced);
            nitem->_sgnode = node;
          };
          auto layers_i = resolveLayerNames(NID.get());
          for (auto& lname : layers_i) {
            auto layer = lname.empty() ? _default_layer : _scene->createLayer(lname);
            NODE_ON_LAYER(layer);
          }
        } else {
          auto NODE_ON_LAYER = [=](lev2::scenegraph::layer_ptr_t layer){
            auto node       = layer->createDrawableNode(NID->_nodename, nitem->_drawable);
            node->_modcolor = NID->_modcolor;
            nitem->_sgnode  = node;
          };
          auto layers_n = resolveLayerNames(NID.get());
          for (auto& lname : layers_n) {
            auto layer = lname.empty() ? _default_layer : _scene->createLayer(lname);
            NODE_ON_LAYER(layer);
          }
        }
      };

      this->enqueueOnGpuInit(on_gpu_init);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::enqueueOnGpuInit(void_lambda_t L) {
  _onGpuInitOpQueue.atomicOp([L](std::vector<void_lambda_t>& unlocked) { unlocked.push_back(L); });
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::_onGpuInit(Simulation* sim, lev2::Context* ctx) { // final

  _scene->_dbufcontext_SG = sim->dbufcontext();

  _scene->gpuInit(ctx);

  /////////////////////////////////////////
  // ECS-VR (phase 1): a VR rendermodel (FWDPBRVR / FWDPBRVRDM) draws stereo via a tracking device,
  // registered globally, that supplies the per-eye poses. Unlike the python presentation (which
  // creates app.vrdev), the ECS playback path has nothing wiring one up — so create + register a
  // NoVrDevice here and seed an identity head pose so the compositor renders immediately. Live
  // head pose arrives later via the SetHmdPose message (orkidvr::device()->setTrackedPose).
  /////////////////////////////////////////
  if (auto try_preset = _mergedParams->typedValueForKey<std::string>("preset")) {
    std::string preset_uc = try_preset.value();
    for (auto& c : preset_uc)
      c = char(std::toupper((unsigned char)c));
    // MSAA level (scene param `msaa`): 0=off,1=2x,2=4x,3=8x,4=16x. Feed the global appinit the
    // forward node reads at its (lazy, first-render) RTG init — which happens after this scene
    // setup, so the value is in place. Device-clamped + RGBA16F applied there. Applies to BOTH
    // FWDPBR and FWDPBRVRDM (VR inherits the same forward RTG). Only set when explicitly provided.
    if (auto v = _mergedParams->tryKeyAsNumber("msaa")) {
      if (::ork::lev2::_ginitdata)
        ::ork::lev2::_ginitdata->_msaa_samples = int(v.value());
      logchan_sgsys->log("ECS scene MSAA level<%d>", int(v.value()));
    }
    if (preset_uc.rfind("FWDPBRVR", 0) == 0) { // FWDPBRVR or FWDPBRVRDM
      // Device selection. If an XR-runtime device is ALREADY active (selected pre-Vulkan
      // from ORKID_VR_DRIVER, session up — it OWNS HMD presentation), KEEP it: clobbering
      // it with a NoVrDevice would strand the live headset. Only when there is no live XR
      // device do we register a NoVrDevice (the desktop stereo-preview path).
      auto active_dev = ::ork::lev2::orkidvr::device();
      bool xr_active  = active_dev and active_dev->_active and active_dev->ownsHmdPresentation();
      ::ork::lev2::orkidvr::device_ptr_t vrdev;
      if (xr_active) {
        vrdev = active_dev;
        logchan_sgsys->log("ECS-VR: using ACTIVE XR-runtime device (ownsHmdPresentation) for preset<%s>",
                           try_preset.value().c_str());
      } else {
        auto novr     = ::ork::lev2::orkidvr::novr::novr_device();
        novr->_width  = 1280;
        novr->_height = 1280;
        ::ork::lev2::orkidvr::setDevice(novr);
        novr->setTrackedPose(fvec3(0, 0, 0), fquat(), fvec3(0, 0, 0), fvec3(0, 0, 0));
        vrdev = novr;
      }
      // Camera contract: the ECS scene publishes exactly ONE camera in its LUT
      // (_camlut["spawncam"] == _camera) — the very camera the host's per-frame
      // UpdateCamera notify drives. The VR output node looks up VRDEV->_camera_name in
      // the DB camera LUT for the world (root) view, onto which the XR head pose composes
      // at render time. "spawncam" is the only name that resolves; "vrcam" never did.
      vrdev->_camera_name = "spawncam";
      // Host-supplied DEVICE calibration (scene params; engine defaults otherwise).
      // IPD<0 swaps L/R (the cross-eye fix).
      constexpr float D2R = 0.01745329252f, R2D = 57.29577951f;
      // Sane baselines mirroring the stereo_grid reference (the bare Device struct default
      // _fov=90 is *radians*, an invalid frustum). A scene forced into VR by the host (no
      // VR-authored params) still projects correctly; any Vr* scene param below overrides.
      vrdev->_fov  = 90.0f * D2R;
      vrdev->_IPD  = 0.065f;
      vrdev->_near = 0.1f;
      vrdev->_far  = 1e5f;
      if (auto v = _mergedParams->tryKeyAsNumber("VrIPD"))       vrdev->_IPD            = float(v.value());
      // VrFov is in DEGREES (vertical); _fov is stored in RADIANS (the FOVD pyext setter does
      // _fov = deg*DTOR, and novr.cpp perspective() consumes radians despite the misleading comment).
      if (auto v = _mergedParams->tryKeyAsNumber("VrFov"))       vrdev->_fov            = float(v.value()) * D2R;
      if (auto v = _mergedParams->tryKeyAsNumber("VrNear"))      vrdev->_near           = float(v.value());
      if (auto v = _mergedParams->tryKeyAsNumber("VrFar"))       vrdev->_far            = float(v.value());
      if (auto v = _mergedParams->tryKeyAsNumber("VrPredAhead")) vrdev->_predictionBias = float(v.value());
      // VrDepthPublish (bool/number, default ON): per-scene toggle for depth-layer publishing to
      //  a depth-reprojection runtime. A depth-owning device (OpenXR) consults _publishDepth
      //  before chaining depth; 0 keeps the runtime in color-only/BASIC reprojection for scenes
      //  where the positional (tessellation) path misbehaves. Sanctioned per-scene mechanism; the
      //  global emergency override env ORKID_XR_NO_DEPTH=1 wins over this param.
      if (auto v = _mergedParams->tryKeyAsNumber("VrDepthPublish")) vrdev->_publishDepth = (v.value() != 0.0);
      logchan_sgsys->log("ECS-VR: device IPD=%g fovDeg=%g near=%g far=%g pred=%g publishDepth=%d",
                         vrdev->_IPD, vrdev->_fov * R2D, vrdev->_near, vrdev->_far,
                         vrdev->_predictionBias, int(vrdev->_publishDepth));
      // Host-supplied per-eye distortion present (vr.h: "the distortion shader and the calibration
      // values are supplied by the host"). The engine only BUILDS device->_presentation from
      // declarative scene params — NO shader/optics baked here. Absent VrDistortShader => null
      // _presentation => the VR output node's flat blit (raw stereo). Live pose -> SetHmdPose.
      if (auto try_shader = _mergedParams->typedValueForKey<std::string>("VrDistortShader")) {
        auto distort_mtl = std::make_shared<::ork::lev2::FreestyleMaterial>();
        distort_mtl->gpuInit(ctx, try_shader.value().c_str());   // HOST shader path (absolute), not an orkid asset
        auto pres       = std::make_shared<::ork::lev2::orkidvr::StandardVrPresentation>();
        pres->_material = distort_mtl;
        // Sane baselines: the StandardVrPresentation STRUCT defaults (lensCenter (0.5,0.5),
        // distortion 0) are NOT valid for this shader's "frg_uv0 + LensCenter" + radial-grad usage
        // — lensCenter must be a ~0 OFFSET (else the distortion centers at the corner) and grad>0.
        // Start centered + passthrough; the host params override.
        pres->_distortionR = pres->_distortionG = pres->_distortionB = fvec4(1.0f, 0.0f, 0.0f, 0.0f);
        pres->_lensCenter[0] = pres->_lensCenter[1] = fvec2(0.0f, 0.0f);
        bool got_d = false, got_lc = false;
        if (auto d = _mergedParams->typedValueForKey<fvec4>("VrDistortion")) {
          pres->_distortionR = d.value();    // achromatic (R=G=B); per-channel is VrDistortionR/G/B [future]
          pres->_distortionG = d.value();
          pres->_distortionB = d.value();
          got_d = true;
        }
        // per-eye lens center packed into ONE vec4 (left.xy, right.zw): vec2 user-params don't
        // round-trip through the .ecs, vec4 does. (0,0,0,0) = centered; headset IPD = (-hw/2,0,+hw/2,0).
        if (auto lc = _mergedParams->typedValueForKey<fvec4>("VrLensCenter")) {
          auto v = lc.value();
          pres->_lensCenter[0] = fvec2(v.x, v.y);
          pres->_lensCenter[1] = fvec2(v.z, v.w);
          got_lc = true;
        }
        // per-eye PRESENT rotation (panel orientation), carried in MatMVP: VrEyeRot = vec4(rotL_deg,
        // rotR_deg, 0, 0). The headset panel is mounted rotated -> typically (-90, 90).
        bool got_er = false;
        if (auto er = _mergedParams->typedValueForKey<fvec4>("VrEyeRot")) {
          auto v               = er.value();
          constexpr float D2R  = 0.01745329252f;
          pres->_eyeTransform[0] = fquat(fvec3(0.0f, 0.0f, 1.0f), v.x * D2R).toMatrix();
          pres->_eyeTransform[1] = fquat(fvec3(0.0f, 0.0f, 1.0f), v.y * D2R).toMatrix();
          got_er = true;
        }
        // per-eye RENDER-time view CANT (toe-in) -> _eyeViewTransform (base.cpp:266), rotation about
        // the vertical (Y) axis in eye-view space (BEFORE the present-time panel VrEyeRot). VrCant =
        // vec4(cantL_deg, cantR_deg, 0, 0); for toe-in L/R take opposite signs. Identity if absent.
        bool got_ct = false;
        if (auto ct = _mergedParams->typedValueForKey<fvec4>("VrCant")) {
          auto v               = ct.value();
          constexpr float D2R  = 0.01745329252f;
          pres->_eyeViewTransform[0] = fquat(fvec3(0.0f, 1.0f, 0.0f), v.x * D2R).toMatrix();
          pres->_eyeViewTransform[1] = fquat(fvec3(0.0f, 1.0f, 0.0f), v.y * D2R).toMatrix();
          got_ct = true;
        }
        vrdev->_presentation = pres;
        logchan_sgsys->log("ECS-VR: host present<%s> (VrDistortion=%d VrLensCenter=%d VrEyeRot=%d VrCant=%d) preset<%s>",
                           try_shader.value().c_str(), int(got_d), int(got_lc), int(got_er), int(got_ct), try_preset.value().c_str());
      } else {
        logchan_sgsys->log("ECS-VR: no VrDistortShader -> flat blit (raw stereo) for preset<%s>",
                           try_preset.value().c_str());
      }
    }
  }

  /////////////////////////////////////////
  // preload statically declared drawables (and -> assets)
  /////////////////////////////////////////

  auto scenedata = sim->GetData();
  auto compdatas = scenedata->findAllTypedComponents<SceneGraphComponentData>();
  for (auto COMPDATA : compdatas) {
    for (auto NID_item : COMPDATA->_nodedatas) {
      auto NID = NID_item.second;
      if (NID->_drawabledata) {
        _drwcache->fetch(NID->_drawabledata);
        // preload model assets into per-simulation cache
        if (auto as_model = std::dynamic_pointer_cast<ModelDrawableData>(NID->_drawabledata)) {
          _modelAssetCache->fetch(as_model->_assetpath);
        }
      }
    }
  }
  for (auto NID_item : _SGSD._nodedatas) {
    auto NID = NID_item.second;
    if (NID->_drawabledata) {
      _drwcache->fetch(NID->_drawabledata);
      if (auto as_model = std::dynamic_pointer_cast<ModelDrawableData>(NID->_drawabledata)) {
        _modelAssetCache->fetch(as_model->_assetpath);
      }
    }
  }

  for (auto DRWDATA : _SGSD._drawdatas_prefetchlist) {
    _drwcache->fetch(DRWDATA);
    if (auto as_model = std::dynamic_pointer_cast<ModelDrawableData>(DRWDATA)) {
      _modelAssetCache->fetch(as_model->_assetpath);
    }
  }

  /////////////////////////////////////////
  // Build cookie atlas from SpotLightData cookie paths
  // (scan scene DATA, not live scene graph — lights aren't staged yet)
  //
  // When the scenegraph is shared (injected), cookie arrays are owned by
  // the app via the LightManager — don't create or replace them.
  // When the scenegraph is private, create arrays and swap pointers.
  /////////////////////////////////////////

  {
    auto lmgr = _scene->_lightManager;
    if (lmgr && !_isSharedScene) {
      int atlasW  = _SGSD._cookieAtlasWidth;
      int atlasH  = _SGSD._cookieAtlasHeight;
      int shadowW = _SGSD._shadowAtlasWidth;
      int shadowH = _SGSD._shadowAtlasHeight;

      // Collect unique cookie paths and count spotlights from scene data
      std::vector<std::string> cookiePaths;
      std::set<std::string> seenPaths;
      int numSpotlights = 0;

      auto collect_spotlights = [&](const std::map<std::string, sgnodeitemdata_ptr_t>& nodedatas) {
        for (auto& NID_item : nodedatas) {
          auto NID = NID_item.second;
          if (auto as_spot = std::dynamic_pointer_cast<lev2::SpotLightData>(NID->_drawabledata)) {
            numSpotlights++;
            std::string key = as_spot->_cookiePath.c_str();
            if (!key.empty() && seenPaths.find(key) == seenPaths.end()) {
              seenPaths.insert(key);
              cookiePaths.push_back(key);
            }
          }
        }
      };

      // scan all archetypes' SceneGraphComponentData
      for (auto COMPDATA : compdatas) {
        collect_spotlights(COMPDATA->_nodedatas);
      }
      // scan system-level nodedatas
      collect_spotlights(_SGSD._nodedatas);

      {
        // Always create cookie arrays — spotlights may be added dynamically
        int numColorSlices = std::max((int)cookiePaths.size(), 1); // at least 1 (default white)
        int numDepthSlices = std::max(numSpotlights, 4);           // reserve slots for dynamic adds

        // Create color cookie TextureArray
        _cookieColorArray = std::make_shared<lev2::TextureArray>();
        _cookieColorArray->_tex->_debugName = "ecs_cookie_color";
        _cookieColorArray->_debugName       = "ecs_cookie_color";
        _cookieColorArray->_needsRadianceCache = true;
        _cookieColorArray->_requires_mips   = true;
        _cookieColorArray->resize(atlasW, atlasH, numColorSlices, lev2::EBufferFormat::RGB8);

        // Load cookie images (populates _images for GPU upload)
        for (size_t i = 0; i < cookiePaths.size(); i++) {
          auto expanded = file::expandPaths(cookiePaths[i]);
          auto sliceRef = _cookieColorArray->load(expanded);
          _cookiePathToSliceRef[cookiePaths[i]] = sliceRef;
        }

        // Create depth cookie TextureArray (blank render target for shadow maps)
        _cookieDepthArray = std::make_shared<lev2::TextureArray>();
        _cookieDepthArray->_tex->_debugName = "ecs_cookie_depth";
        _cookieDepthArray->_debugName       = "ecs_cookie_depth";
        _cookieDepthArray->resize(shadowW, shadowH, numDepthSlices, lev2::EBufferFormat::Z32F);
        _cookieDepthArray->_tex->mTexSampleMode._texAddrModeS = lev2::TextureAddressMode::CLAMP;
        _cookieDepthArray->_tex->mTexSampleMode._texAddrModeT = lev2::TextureAddressMode::CLAMP;
        _cookieDepthArray->_tex->mTexSampleMode._texAddrModeR = lev2::TextureAddressMode::CLAMP;

        // Set on light manager
        lmgr->_cookies_spot_color = _cookieColorArray;
        lmgr->_cookies_spot_depth = _cookieDepthArray;
        _nextDepthSlice = 0;
      }
    } else if (lmgr && _isSharedScene) {
      // Shared scenegraph: alias lmgr's existing arrays for local use
      _cookieColorArray = lmgr->_cookies_spot_color;
      _cookieDepthArray = lmgr->_cookies_spot_depth;
    }
  }

  /////////////////////////////////////////

  // NOTE: the _onGpuInitOpQueue drain moved to _onGpuStage. The queue is
  // populated by _instantiateDeclaredNodes() called from _onStage, which
  // runs AFTER _onGpuInit in the phase-locked FSM — so draining here
  // would be a no-op. The _onGpuStage rendezvous runs immediately after
  // _onStage on the same render-thread context, so declared drawable
  // nodes land on the scenegraph before the first frame draws.

  // GPU uploads deferred to loading phase (safe for first-run shader compilation).
  // Build the phase FULLY (all ops enqueued), then submit atomically. newLoadingPhase()
  // would publish an empty phase the drainer could pop+complete before enqueueOperation
  // runs — orphaning this cookie/shadow-array upload and mis-releasing its texture_upload
  // marker early. submitLoadingPhase closes that race class at this site.
  auto ph     = std::make_shared<lev2::LoadingPhase>();
  bool shared = _isSharedScene;
  ph->enqueueOperation([=](Context* ctx) {
    if (!shared) {
      // Private scenegraph: ECS owns the arrays, upload now
      if (_cookieColorArray) {
        ctx->TXI()->updateTextureArray(_cookieColorArray.get());
      }
      if (_cookieDepthArray) {
        ctx->TXI()->initTextureArray2D(_cookieDepthArray.get());
      }
    } else {
      // Shared scenegraph: upload color array if images have been loaded
      // (e.g. via allocateColorSlice); skip if no images yet.
      if (_cookieColorArray && !_cookieColorArray->_images.empty()) {
        ctx->TXI()->updateTextureArray(_cookieColorArray.get());
      }
      // Depth array is OWNED by the host LightManager in shared mode —
      // its GPU resource is created/maintained by the host's
      // lmgr.gpuInit (and the lmgr.gpuInit call below as a fallback).
      // Do NOT call initTextureArray2D here: it unconditionally
      // allocates a fresh GPU texture and overwrites the array's
      // _tex->_impl, orphaning the host-initialized texture. Per-spot
      // RtGroups created before this point keep a reference to the
      // orphaned image, while the shader binding follows the new one
      // — depth writes and reads land on different textures, which
      // manifests as missing/non-deterministic spotlight shadows.
    }
    if (_scene->_lightManager) {
      _scene->_lightManager->gpuInit(ctx);
    }
  });
  ctx->submitLoadingPhase(ph);

  /////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onGpuStage(Simulation* psi, lev2::Context* ctx) {
  // Drain declared-node instantiation lambdas queued by _onStage's call to
  // _instantiateDeclaredNodes. Under the phase-locked FSM, _onStage runs
  // immediately before this rendezvous, so the queue is populated now.
  _onGpuInitOpQueue.atomicOp([](std::vector<void_lambda_t>& unlocked) {
    for (auto& item : unlocked) {
      item();
    }
    unlocked.clear();
  });
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onStageComponent(SceneGraphComponent* component) {
  //////////////////////////////
  // initialize transform
  //////////////////////////////
  auto ent = component->GetEntity();
  //////////////////////////////
  if(0)printf("[SGS] stage component<%p>\n", (void*) component);
  this->_components.atomicOp([this,component](SceneGraphSystem::component_set_t& unlocked) { //
    unlocked.insert(component); 
    _numComponents = unlocked.size();                                                        //
  });
  //////////////////////////////
  auto setdrw_op = [=]() {
    auto& COMPDATA = component->_SGCD;
    for (auto NID_item : COMPDATA._nodedatas) {
      auto NID     = NID_item.second;
      auto drwdata = NID->_drawabledata;

      auto it_drw    = component->_nodeitems.find(NID->_nodename);
      bool was_found = it_drw != component->_nodeitems.end();

      if (not was_found) {
        /////////////////////////////////////////////////
        // light ?
        /////////////////////////////////////////////////
        auto as_light = dynamic_pointer_cast<LightData>(drwdata);
        if(0)printf("[SGS::_onStageComponent] NID name=%s drwdata=%p as_light=%p\n",
               NID->_nodename.c_str(), (void*)drwdata.get(), (void*)as_light.get());
        fflush(stdout);
        if (as_light) {
          auto layer = NID->_layername.empty()
              ? _default_layer
              : _scene->findLayer(NID->_layername);
          auto l = dynamic_pointer_cast<Light>(as_light->createDrawable());
          l->_castsShadows = as_light->IsShadowCaster();

          auto nitem                            = std::make_shared<SceneGraphNodeItem>();
          nitem->_drawable                      = l;
          nitem->_sgnode                        = layer->createLightNode(NID->_nodename, l);
          nitem->_nodename                      = NID->_nodename;
          nitem->_data                          = NID;
          component->_nodeitems[NID->_nodename] = nitem;
          if(0)printf("[SGS::_onStageComponent] LIGHT INJECTED name=%s scene=%p layer=%p lnode=%p\n",
                 NID->_nodename.c_str(),
                 (void*)_scene.get(),
                 (void*)layer.get(),
                 (void*)nitem->_sgnode.get());
          fflush(stdout);
          //OrkAssert(false);

          // Assign cookie atlas slices to spotlights
          if (auto as_spot = std::dynamic_pointer_cast<lev2::SpotLight>(l)) {
            auto lmgr = _scene->_lightManager;
            if (_isSharedScene && lmgr) {
              // Shared scenegraph: use LightManager's centralized allocator
              if (as_spot->_spdata) {
                std::string ckey = as_spot->_spdata->_cookiePath.c_str();
                if (!ckey.empty()) {
                  as_spot->_cookieColor = lmgr->allocateColorSlice(ckey);
                } else if (_cookieColorArray) {
                  as_spot->_cookieColor = _cookieColorArray->slice(0);
                }
              }
              as_spot->_cookieDepth = lmgr->allocateDepthSlice();
            } else {
              // Private scenegraph: use local cookie arrays
              if (as_spot->_spdata) {
                std::string ckey = as_spot->_spdata->_cookiePath.c_str();
                auto it = _cookiePathToSliceRef.find(ckey);
                if (it != _cookiePathToSliceRef.end()) {
                  as_spot->_cookieColor = it->second;
                } else if (_cookieColorArray) {
                  as_spot->_cookieColor = _cookieColorArray->slice(0);
                }
              }
              if (_cookieDepthArray && _nextDepthSlice < (int)_cookieDepthArray->_maxslices) {
                as_spot->_cookieDepth = _cookieDepthArray->slice(_nextDepthSlice++);
              }
            }
          }

          auto ent = component->GetEntity();
          nitem->_sgnode->_userdata->makeValueForKey<uint64_t>("entref") = ent->_entref;

          // register for the per-frame varmap override sweep (_updateLightBridge)
          LightBridgeItem bridge;
          bridge._component = component;
          bridge._entity    = ent;
          bridge._light     = l;
          bridge._lightdata = as_light;
          bridge._sgnode    = nitem->_sgnode;
          _lightbridges.atomicOp([&bridge](lightbridge_vect_t& unlocked) { //
            unlocked.push_back(bridge);
          });

          // For spotlights, derive view/projection from entity transform (+Z forward)
          auto as_spotl = std::dynamic_pointer_cast<lev2::SpotLight>(l);

          if (NID->_xfoverride) {
            auto static_matrix = NID->_xfoverride->composed();
            l->_xformgenerator = [=]() -> fmtx4 {
              fmtx4 rval;
              auto node = nitem->_sgnode;
              if (node) {
                auto xform = ent->transform();
                rval       = xform->composed() * static_matrix;
              }
              // Skip the view/proj rebuild if the caller has taken over
              // via setViewProj / setOrthoViewProj / setPerspectiveViewProj.
              if (as_spotl && !as_spotl->_matrices_explicit) {
                fvec3 pos = rval.translation();
                fvec3 fwd = rval.zNormal();
                fvec3 up  = rval.yNormal();
                fvec3 tgt = pos + fwd * as_spotl->getRange();
                float near = as_spotl->getRange() / 1000.0f;
                float far  = as_spotl->getRange();
                as_spotl->mProjectionMatrix.perspective(as_spotl->getFovy() * DTOR, 1.0f, near, far);
                as_spotl->mViewMatrix.lookAt(pos.x, pos.y, pos.z, tgt.x, tgt.y, tgt.z, up.x, up.y, up.z);
                as_spotl->mWorldSpaceLightFrustum.set(as_spotl->mViewMatrix, as_spotl->mProjectionMatrix);
              }
              return rval;
            };
          } else {
            l->_xformgenerator = [=]() -> fmtx4 {
              fmtx4 rval;
              auto node = nitem->_sgnode;
              if (node) {
                auto xform = ent->transform();
                rval       = xform->composed();
              }
              // Skip the view/proj rebuild if the caller has taken over
              // via setViewProj / setOrthoViewProj / setPerspectiveViewProj.
              if (as_spotl && !as_spotl->_matrices_explicit) {
                fvec3 pos = rval.translation();
                fvec3 fwd = rval.zNormal();
                fvec3 up  = rval.yNormal();
                fvec3 tgt = pos + fwd * as_spotl->getRange();
                float near = as_spotl->getRange() / 1000.0f;
                float far  = as_spotl->getRange();
                as_spotl->mProjectionMatrix.perspective(as_spotl->getFovy() * DTOR, 1.0f, near, far);
                as_spotl->mViewMatrix.lookAt(pos.x, pos.y, pos.z, tgt.x, tgt.y, tgt.z, up.x, up.y, up.z);
                as_spotl->mWorldSpaceLightFrustum.set(as_spotl->mViewMatrix, as_spotl->mProjectionMatrix);
              }
              return rval;
            };
          }

          /////////////////////////////////////////////////
          // drawable ?
          /////////////////////////////////////////////////
        } else {
            
            auto DO_ITEM = [=](scenegraph::layer_ptr_t layer) -> sgnodeitem_ptr_t {
                auto nitem                            = std::make_shared<SceneGraphNodeItem>();
                if (drwdata->isSharedDrawable()) {
                  nitem->_drawable = _drwcache->fetch(drwdata);
                } else if (auto as_model = std::dynamic_pointer_cast<ModelDrawableData>(drwdata)) {
                  auto cached_asset = _modelAssetCache->fetch(as_model->_assetpath);
                  nitem->_drawable = as_model->createDrawableWithAsset(cached_asset);
                  nitem->_drawable->_modcolor = drwdata->_modcolor;
                } else {
                  nitem->_drawable = drwdata->createDrawable();
                  nitem->_drawable->_modcolor = drwdata->_modcolor;
                }
                // PBR2 Phase 0 — per-node HDRI override (see _onStageComponent above).
                ork::lev2::loadEnvMapOverride(nitem->_drawable.get(), NID->_envmap_path);
                nitem->_nodename                      = NID->_nodename;
                nitem->_data                          = NID;
                component->_nodeitems[NID->_nodename] = nitem;

                if (auto as_instanced = dynamic_pointer_cast<InstancedDrawable>(nitem->_drawable)) {
                    nitem->_sgnode = layer->createDrawableNode(NID->_nodename, as_instanced);
                    OrkAssert(false);
                    // we should not hit this, because the instanced drawable
                    //  should be @ system scope, not component scope

                } else {
                    auto node       = layer->createDrawableNode(NID->_nodename, nitem->_drawable);
                    node->_modcolor = NID->_modcolor;
                    nitem->_sgnode  = node;
                    auto _ent = component->GetEntity();
                    if (_ent) {
                      node->_userdata->makeValueForKey<uint64_t>("entref") = _ent->_entref;
                    }
                }
                return nitem;
            };
            auto layers_c = resolveLayerNames(NID.get());
            auto first_layer = layers_c[0].empty() ? _default_layer : _scene->createLayer(layers_c[0]);
            auto item = DO_ITEM(first_layer);
            for (size_t i = 1; i < layers_c.size(); i++) {
                auto layer = _scene->createLayer(layers_c[i]);
                auto drw_node = std::dynamic_pointer_cast<lev2::scenegraph::DrawableNode>(item->_sgnode);
                layer->addDrawableNode(drw_node);
            }
            // Also add to depth_prepass layer for shadow rendering.
            // Opt-out via NodeDef::_skipAutoDepthPrepass for drawables
            // whose material samples the depth RTG (e.g. water) — those
            // must not render during depth_prepass because the depth
            // attachment is still in DEPTH_ATTACHMENT_OPTIMAL at that
            // point and binding it as a sampled texture asserts in Vulkan.
            if (not NID->_skipAutoDepthPrepass) {
              if (auto drw_node = std::dynamic_pointer_cast<lev2::scenegraph::DrawableNode>(item->_sgnode)) {
                  auto dpp_layer = _scene->findLayer("depth_prepass");
                  if (dpp_layer) {
                      dpp_layer->addDrawableNode(drw_node);
                  }
              }
            }
        }
      }
    }
    // now check for INSTANCE's (ptr form from pyext hosts, name form from
    // deserialized scenes — the name is the serializable contract)
    std::string instance_group = COMPDATA._INSTANCEDATA //
                                     ? COMPDATA._INSTANCEDATA->_groupname
                                     : COMPDATA._instanceNodeName;
    if (instance_group != "") {
      auto instance        = std::make_shared<lev2::scenegraph::NodeInstance>();
      instance->_groupname = instance_group;
      // TODO : defer until nodes created ?
      auto it = _nodeitems.find(instance->_groupname);
      OrkAssert(it != _nodeitems.end());
      sgnodeitem_ptr_t groupitem = it->second;
      auto group_drawable        = std::dynamic_pointer_cast<lev2::InstancedDrawable>(groupitem->_drawable);
      instance->_idrawable       = group_drawable;
      auto idata                 = group_drawable->_instancedata;
      instance->_idata           = idata;
      int ID                     = idata->allocInstance();
      if (ID < 0) {
        // pool exhausted (fail-soft): the entity lives on without a visual;
        // _INSTANCE stays null so unstage/physics wiring skip cleanly.
        printf("SceneGraphSystem: instance pool exhausted for group<%s> — entity gets NO visual\n", instance_group.c_str());
        return;
      }
      instance->_instance_index  = ID;
      component->_INSTANCE       = instance;
      {
        auto ent = component->GetEntity();
        auto sad = ent->_spawnanondata;
        if(sad and sad->_table){
          auto modcolor = (*sad->_table)["modcolor"_tok];
          if(auto as_v4 = modcolor.tryAs<fvec4>()){
            idata->_modcolors[ID] = as_v4.value();
          }
        }
      }
      if(component->_onInstanceCreated){
        component->_onInstanceCreated();
      }
    }
  };
  //////////////////////////////
  auto setxform_op = component->_genTransformOperation();
  //////////////////////////////
  _renderops.push(setdrw_op);
  _renderops.push(setxform_op);
  //////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onUnstageComponent(SceneGraphComponent* component) {
  ///////////////////////////////
  // untrack component
  ///////////////////////////////
  _components.atomicOp([this,component](SceneGraphSystem::component_set_t& unlocked) {
    auto it = unlocked.find(component);
    OrkAssert(it != unlocked.end());
    unlocked.erase(it);
    _numComponents = unlocked.size();                                                        //
  });
  ///////////////////////////////
  // drop this component's varmap->light bridge entries (they hold a raw
  // Entity* that dies with the entity)
  ///////////////////////////////
  _lightbridges.atomicOp([component](lightbridge_vect_t& unlocked) {
    auto it = std::remove_if(unlocked.begin(), unlocked.end(), [component](const LightBridgeItem& item) -> bool {
      return item._component == component;
    });
    unlocked.erase(it, unlocked.end());
  });
  ///////////////////////////////
  // remove from scenegraph
  ///////////////////////////////
  auto remove_operation = [=]() {
    //////////////////////////////////
    // first remove nodes
    //////////////////////////////////
    for (auto NITEM : component->_nodeitems) {
      auto sgnode = NITEM.second->_sgnode;

      if (sgnode) {
        auto as_lightnode    = std::dynamic_pointer_cast<scenegraph::LightNode>(sgnode);
        auto as_drawablenode = std::dynamic_pointer_cast<scenegraph::DrawableNode>(sgnode);

        if (as_lightnode) {
          _default_layer->removeLightNode(as_lightnode);
        } else if (as_drawablenode) {
            for( auto l : as_drawablenode->_layers ){
                l->removeDrawableNode(as_drawablenode);
            }
            as_drawablenode->_layers.clear();
        } else {
          OrkAssert(false);
        }
      }
    }
    //////////////////////////////////
    // now remove instances...
    //////////////////////////////////
    if (component->_INSTANCE) {
      int index = component->_INSTANCE->_instance_index;
      OrkAssert(index >= 0);
      component->_INSTANCE->_idata->freeInstance(index);
      //OrkAssert(false); // remove instance
    }
  };
  _renderops.push(remove_operation);
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onActivateComponent(SceneGraphComponent* component) {
  //printf("sgsys activate component<%p>\n", (void*) component);
  auto setxform_op = component->_genTransformOperation();
  _renderops.push(setxform_op);
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onDeactivateComponent(SceneGraphComponent* component) {
  auto setpos_op = [=]() {
    auto init_xf          = component->GetEntity()->data()->_dagnode->_xfnode;
    component->_currentXF = init_xf;
    //////////////////////////////////////////
    auto mtx    = init_xf->_transform->composed();
    auto mtxstr = mtx.dump4x3cn();
    for (auto NITEM : component->_nodeitems) {
      auto node = NITEM.second->_sgnode;
      if (node) {
        // printf("_onDeactivateComponent set node to init_xf<%p>\n", (void*) init_xf.get());
        // printf(" value<%s>\n", mtxstr.c_str());
      }
    }
  };
  _renderops.push(setpos_op);
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onGpuExit(Simulation* psi, lev2::Context* ctx) { // final
  if (_scene) {
    _scene->gpuExit(ctx);
  }
}

bool SceneGraphSystem::_onLink(Simulation* psi) // final
{
  /////////////////////////////////////////
  // copy in user params
  /////////////////////////////////////////

  _mergedParams->mergeVars(*_SGSD._internalParams);

  for (auto item : _SGSD._userParams) {
    auto k = item.first;
    auto v = item.second;
    _mergedParams->setValueForKey(k, v);
  }

  /////////////////////////////////////////
  // PBR2 P3.D — assemble the runtime PostFxChain from the reflected
  // _postfx_nodes map + _postfx_order string. The compositor consumes
  // the chain from _mergedParams["PostFxChain"] (scenegraph.cpp:424).
  // Names listed in _postfx_order but absent from _postfx_nodes are
  // skipped silently; names in the map but absent from order are
  // unused (lets you stage-disable a node without removing it).
  /////////////////////////////////////////
  if(0)printf("[SGS::_onLink P3.D] postfx_order=<%s> postfx_nodes.size=%zu\n",
         _SGSD._postfx_order.c_str(), _SGSD._postfx_nodes.size());
  fflush(stdout);
  if (!_SGSD._postfx_order.empty() && !_SGSD._postfx_nodes.empty()) {
    lev2::postfx_node_chain_t chain;
    std::string buf = _SGSD._postfx_order;
    size_t pos = 0;
    while (pos < buf.size()) {
      auto comma = buf.find(',', pos);
      auto end = (comma == std::string::npos) ? buf.size() : comma;
      auto name = buf.substr(pos, end - pos);
      // trim whitespace
      while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) name.erase(name.begin());
      while (!name.empty() && (name.back()  == ' ' || name.back()  == '\t')) name.pop_back();
      if (!name.empty()) {
        auto it = _SGSD._postfx_nodes.find(name);
        if (it != _SGSD._postfx_nodes.end()) {
          chain.push_back(it->second);
        } else {
          printf("[SGS::_onLink] postfx_order references unknown node <%s> — skipped\n",
                 name.c_str());
        }
      }
      pos = (comma == std::string::npos) ? buf.size() : (comma + 1);
    }
    // STAGE ORDER OVERRIDES DECLARATION ORDER (PostCompositingNode::chainStage).
    // _postfx_order records the sequence names were DECLARED in, and a scene
    // declares its sky — hence the tone stage — before the material that later
    // auto-attaches an HDR effect. Left alone that put the tone stage first,
    // where its single-buffer output is not something an MRT effect can read.
    std::stable_sort(chain.begin(), chain.end(), //
                     [](const lev2::compositorpostnode_ptr_t& a, const lev2::compositorpostnode_ptr_t& b) {
                       return a->chainStage() < b->chainStage();
                     });
    if(0)printf("[SGS::_onLink P3.D] assembled PostFxChain size=%zu\n", chain.size());
    fflush(stdout);
    if (!chain.empty()) {
      varmap::VarMap::value_type val;
      val.set<lev2::postfx_node_chain_t>(chain);
      _mergedParams->setValueForKey("PostFxChain", val);
    }
  }

  /////////////////////////////////////////
  // check simulation varmap for an injected scenegraph
  /////////////////////////////////////////

  auto sim_varmap = psi->varmap();
  if (sim_varmap->hasKey("scenegraph")) {
    auto injected = sim_varmap->typedValueForKey<scenegraph::scene_ptr_t>("scenegraph");
    if (injected) {
      _scene = injected.value();
      _isSharedScene = true;
    }
  }

  if (!_scene) {
    _scene = std::make_shared<scenegraph::Scene>(_mergedParams);
    if(0)printf("[SGS::_onStage] this=%p INJECTION FAILED — created new scene=%p\n",
           (void*)this, (void*)_scene.get());
  } else {
    if(0)printf("[SGS::_onStage] this=%p using scene=%p (isShared=%d)\n",
           (void*)this, (void*)_scene.get(), (int)_isSharedScene);
  }
  fflush(stdout);

  _scene->applyRuntimeParams(_mergedParams);
  return true;
}
void SceneGraphSystem::_onUnLink(Simulation* psi) // final
{
}
///////////////////////////////////////////////////////////////////////////////
bool SceneGraphSystem::_onStage(Simulation* psi) {
  _default_layer = _scene->createLayer("sg_default");
  _scene->createLayer("depth_prepass");
  for (auto item : _SGSD._declaredLayers) {
    _scene->createLayer(item);
  }

  _scene->_staticDrawables = _SGSD._staticDrawables;

  for (auto item : _staticDrawables) {
    _scene->_staticDrawables.push_back(item);
  }
  _instantiateDeclaredNodes();
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onUnstage(Simulation* psi) {
  _components.atomicOp([this](SceneGraphSystem::component_set_t& unlocked) { unlocked.clear(); });
  _lightbridges.atomicOp([](lightbridge_vect_t& unlocked) { unlocked.clear(); });
}
///////////////////////////////////////////////////////////////////////////////
bool SceneGraphSystem::_onActivate(Simulation* psi) // final
{

  return true;
}
void SceneGraphSystem::_onDeactivate(Simulation* inst) // final
{
}
////////////////////////////////////////////////////////////////////////////////
// Per-frame entity-varmap -> light overrides.
//
// Runs on the UPDATE thread, the same thread sim scripts write ent.vars from,
// so the VarMap itself is never read across threads. What lands on the light is
// POD the render thread already samples unsynchronized every frame — the same
// contract as the entity orientation -> Light::_xformgenerator path.
//
// An ABSENT key leaves the declaration value alone; a scene that publishes
// nothing pays four map lookups per light per tick and mutates nothing.
////////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_updateLightBridge() {
  static const varmap::key_t k_intensity("light_intensity");
  static const varmap::key_t k_color("light_color");
  static const varmap::key_t k_castsshadows("light_casts_shadows");
  static const varmap::key_t k_enable("light_enable");

  _lightbridges.atomicOp([](lightbridge_vect_t& unlocked) {
    for (auto& item : unlocked) {
      const auto& vars = item._entity->_varmap;
      if (nullptr == vars)
        continue;

      ////////////////////////////////////////

      if (auto as_f = vars->typedValueForKey<float>(k_intensity))
        item._lightdata->_intensity = as_f.value();
      if (auto as_v3 = vars->typedValueForKey<fvec3>(k_color))
        item._lightdata->mColor = as_v3.value();

      ////////////////////////////////////////

      // the renderer consults the LIGHT's cached flag, not the data's — that
      // one is snapshotted once at staging (see _onStageComponent).
      if (auto as_f = vars->typedValueForKey<float>(k_castsshadows))
        item._light->_castsShadows = (as_f.value() > 0.5f);
      if (auto as_f = vars->typedValueForKey<float>(k_enable))
        item._sgnode->_enabled = (as_f.value() > 0.5f);
    }
  });
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onUpdate(Simulation* psi) // final
{
  OrkProfilerSampleScope(CHANNEL_UPDATE, "SceneGraphSystem::_onUpdate");
  _updateLightBridge();
  if (_scene && _autoupdate) {
    // drive the renderer clock from the authoritative ECS sim time (stops on pause). The scene
    // render publishes this as RCFD["time"], which fx_pipeline's RCFD_TIME named-param provider binds
    // into any shader uniform tagged with it (VS wind, animated materials, ...) — no per-material code.
    _scene->_currentTime = psi->gameTime();
    _scene->enqueueToRenderer(_camlut);
  }
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_rt_process() {
  ///////////////////////////////////////
  // execute render ops
  ///////////////////////////////////////
  void_lambda_t render_op;
  while (_renderops.try_pop(render_op)) {
    render_op();
  }
  ///////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onGpuUpdate(Simulation* psi, lev2::Context* ctx) {
  _rt_process();
  // per-drawable view-INDEPENDENT GPU hook (Drawable::onGpuUpdate — candidate uploads for the
  // instance cull, GPU-driven geometry recompute, ...). The SGVP widget path fans this out via
  // SceneGraphViewport::gpuUpdateAll (ezapp_topwidget), but an ECS-OWNED scene never passes
  // through an SGVP — without this call those drawables never tick.
  // ONLY when IN a frame: on the windowed ezapp loop, Controller::gpuUpdate fires from the
  // app _onGpuUpdate callback BEFORE SlotRepaint — OUTSIDE beginFrame, where there is no
  // primary command buffer (LightManager::gpuInit's texture-array upload null-derefs) and
  // an early call would also claim the per-frame dedup slot, starving the correct in-frame
  // call. There the render entries cover the fan-out (Scene::_renderIMPL /
  // renderWithStandardCompositorFrame call gpuUpdate defensively; first caller per frame
  // wins). Headless hosts that drive Controller::gpuUpdate inside their frame keep this
  // early fan-out.
  if (_scene && ctx->_currentPhase != 0) {
    _scene->gpuUpdate(ctx);
  }
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onRenderWithStandardCompositorFrame(Simulation* psi, lev2::standardcompositorframe_ptr_t sframe) {
  if (_scene && _autodraw) {
    _scene->renderWithStandardCompositorFrame(sframe);
  }
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onRender(Simulation* psi, ui::drawevent_constptr_t drwev) // final
{
  if (_scene && _autodraw) {
    _scene->renderOnContext(drwev->GetTarget());
  }
}

///////////////////////////////////////////////////////////////////////////////

// Apply a head pose (pos/orient + optional linear/angular velocity + optional
// linear/angular ACCELERATION) to the VR device. Shared by _onNotify (sim-tick
// poll) and _onGpuNotify (render-tick poll) so the device application is identical
// regardless of which thread polled it. The accel keys (la*/aa*) are optional —
// absent => 0 => the device falls back to 1st-order extrapolation.
void SceneGraphSystem::_applyHmdPose(evdata_t data) {
  const auto& table = *data.getShared<DataTable>();
  if (auto dev = ::ork::lev2::orkidvr::device()) {
    fvec3 pos(table["px"_tok].get<float>(), table["py"_tok].get<float>(), table["pz"_tok].get<float>());
    fquat ori(table["qx"_tok].get<float>(), table["qy"_tok].get<float>(),
              table["qz"_tok].get<float>(), table["qw"_tok].get<float>());
    auto optf = [&table](const char* k) -> float {
      DataKey dk; dk._encoded.set<CrcString>(CrcString(k));
      auto v = table.find(dk);
      return v.valid() ? v._encoded.get<float>() : 0.0f;
    };
    fvec3 linvel(optf("lvx"), optf("lvy"), optf("lvz"));
    fvec3 angvel(optf("avx"), optf("avy"), optf("avz"));
    fvec3 linacc(optf("lax"), optf("lay"), optf("laz"));
    fvec3 angacc(optf("aax"), optf("aay"), optf("aaz"));
    dev->setTrackedPose(pos, ori, linvel, angvel, linacc, angacc);
    // optional live override of the prediction lead (VrPredAhead): the host may
    // send "pred" (seconds) to retune the bias at runtime (e.g. a [ ] key). Only
    // when PRESENT (0.0 is a valid bias, so test validity, don't use optf).
    DataKey pdk; pdk._encoded.set<CrcString>(CrcString("pred"));
    auto pv = table.find(pdk);
    if (pv.valid())
      dev->_predictionBias = pv._encoded.get<float>();
  }
}

///////////////////////////////////////////////////////////////////////////////

// Render/gpu-thread notify handler — a script's onSystemGpuUpdate (render tick)
// fires SetHmdPose here (via system->_gpuNotify) so the pose lands on the render
// thread, co-located with the device camera build (no cross-thread pose race).
void SceneGraphSystem::_onGpuNotify(token_t evID, evdata_t data) {
  switch (evID.hashed()) {
    case "SetHmdPose"_crcu:
      _applyHmdPose(data);
      break;
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::_onNotify(token_t evID, evdata_t data) {

  switch (evID.hashed()) {
    case "SetHmdPose"_crcu: {
      _applyHmdPose(data);
      break;
    }
    case ResizeFromMainSurface._hashed: {
      auto resize_op = [=]() {
        if (_scene) {
          _scene->_doResizeFromMainSurface = data.get<bool>();
        }
      };
      _renderops.push(resize_op);
      break;
    }
    case "UpdatePbrCommon"_crcu: {
      const auto& table = *data.getShared<DataTable>();
      const auto& ssaonumsamples   = table["SSAONumSamples"_tok];
        if( auto as_int = ssaonumsamples.tryAs<int>() ){
          int numsamps = as_int.value();
          _scene->_pbr_common->_ssaoNumSamples = numsamps;
        }
      // Live direct-diffuse lobe change (the player's POST editor row). Carried
      // as the model's crc, not its ordinal: the ordinal is the shader's private
      // currency, and a saved editor state that outlives a renumbering must not
      // silently mean a different lobe.
      const auto& diffusebrdf = table["DiffuseBrdfModel"_tok];
        if( auto as_crc = diffusebrdf.tryAs<uint64_t>() ){
          lev2::pbr::DiffuseBrdfModel model;
          if (lev2::pbr::diffuseBrdfModelFromCrc(as_crc.value(), model))
            _scene->_pbr_common->_diffuseBrdfModel = model;
          else
            logchan_sgsys->log(
                "UpdatePbrCommon: unknown DiffuseBrdfModel crc<0x%zx> - valid: %s",
                as_crc.value(),
                lev2::pbr::diffuseBrdfModelValidSet().c_str());
        }
      break;
    }
    // Interactive envmap swap (#43-proven path): re-filter the IBL from the named
    // .xir and live-swap it into the bound _radiance_maps. Fired by a host's envmap
    // cycle (the host owns the cycle list); the async kick is thread-safe
    // and the pointer swap is the same benign race the python viewer accepts.
    case "SetEnvmap"_crcu: {
      const auto& table = *data.getShared<DataTable>();
      auto path         = table["path"_tok].get<std::string>();
      if (_scene and _scene->_pbr_common) {
        auto maps = _scene->_pbr_common->requestRadianceMapsAsync(AssetPath(path.c_str()));
        if (maps)
          _scene->_pbr_common->_radiance_maps = maps;
        else // self-defend: a bad path would null the maps and crash envSpecularTexture()
          printf("SceneGraphSystem: SetEnvmap FAILED to load <%s> — keeping current maps\n", path.c_str());
      }
      break;
    }
    // Terrain material-override cycle (SetEnvmap sibling): a host cycles
    // declared(0)/normals(1)/slope(2)/white(3) and fires this. The mode lands on
    // pbr_common (runtime-only, like enable_SSSS); the terrain drawable's render lambda reads
    // it via the RCFD "PBR_COMMON" userProperty and forces the matching debug technique
    // (ptex3d FWD_SSBO_CUSTOM_NORMALS/SLOPE/WHITE, PATH 1). Mode 0 = declared = today's path.
    case "SetTerrainMaterialMode"_crcu: {
      // DATA-DRIVEN: just carry the mode INDEX to pbr_common; the terrain drawable interprets +
      // clamps it against its own resolved-material list (no mode-name table / fixed count here).
      const auto& table = *data.getShared<DataTable>();
      int mode          = table["mode"_tok].get<int>();
      if (_scene and _scene->_pbr_common)
        _scene->_pbr_common->_terrainMaterialMode = mode;
      printf("SceneGraphSystem: SetTerrainMaterialMode -> mode %d\n", mode);
      break;
    }
    case UpdateCamera._hashed: {
      const auto& table = *data.getShared<DataTable>();
      const auto& eye   = table["eye"_tok].get<fvec3>();
      const auto& tgt   = table["tgt"_tok].get<fvec3>();
      const auto& up    = table["up"_tok].get<fvec3>();
      float near        = table["near"_tok].get<float>();
      float far         = table["far"_tok].get<float>();
      float fovy        = table["fovy"_tok].get<float>();
      _camlut->lock();
      _camera->Lookat(eye, tgt, up);
      _camera->Persp(near, far, fovy);
      _camlut->unlock();
      break;
    }
    case UpdateFramebufferSize._hashed: {
      const auto& table = *data.getShared<DataTable>();
      int w             = table["width"_tok].get<int>();
      int h             = table["height"_tok].get<int>();
      auto resize_op    = [=]() {
        if (_scene) {
          auto cimpl = _scene->_compositorImpl;
          // printf("W<%d> H<%d> _scene<%p> cimpl<%p>\n", w, h, _scene.get(), cimpl.get() );
          auto& compositor_ctx = cimpl->compositingContext();
          compositor_ctx.Resize(w, h);
        }
      };
      _renderops.push(resize_op);

      break;
    }
    case DestroyNode._hashed: {

      auto handle           = data.get<response_ref_t>();
      auto response         = _simulation->_findSystemResponseFromRef(handle);
      auto remove_operation = [this, response]() {
        auto node = response->_responseData.get<lev2::scenegraph::drawable_node_ptr_t>();
        //_simulation->debugBanner(128,255,0,"DestroyNode <%p>\n", node.get());
        this->_default_layer->removeDrawableNode(node);
      };
      _renderops.push(remove_operation);
      break;
    }
    case HighlightBySpawnData._hashed: {
      const auto& table = *data.getShared<DataTable>();
      auto name_str = table["name"_tok].get<std::string>();
      auto modcolor = table["color"_tok].get<fvec4>();
      auto psname = AddPooledString(name_str.c_str());
      svar64_t colorvar;
      colorvar.set<fvec4>(modcolor);
      _components.atomicOp([&](component_set_t& comps) {
        for (auto* comp : comps) {
          auto ent = comp->GetEntity();
          if (ent->data()->GetName() == psname) {
            comp->_notify(_simulation, ChangeModColor, colorvar);
          }
        }
      });
      break;
    }
    case SyncTransformBySpawnData._hashed: {
      const auto& table = *data.getShared<DataTable>();
      auto name_str = table["name"_tok].get<std::string>();
      auto psname = AddPooledString(name_str.c_str());
      int matched = 0;
      _components.atomicOp([&](component_set_t& comps) {
        if(0)fprintf(stderr, "[SyncXF] spawner='%s' num_components=%zu\n", name_str.c_str(), comps.size());
        for (auto* comp : comps) {
          auto ent = comp->GetEntity();
          if (ent->data()->GetName() == psname) {
            auto spawner_xf = ent->data()->_dagnode->_xfnode->_transform;
            auto ent_xf = ent->transform();
            if(0)fprintf(stderr, "[SyncXF]   MATCH: spawner_pos=(%.2f,%.2f,%.2f) ent_pos=(%.2f,%.2f,%.2f)\n",
                    spawner_xf->_translation.x, spawner_xf->_translation.y, spawner_xf->_translation.z,
                    ent_xf->_translation.x, ent_xf->_translation.y, ent_xf->_translation.z);
            ent_xf->_translation = spawner_xf->_translation;
            ent_xf->_rotation = spawner_xf->_rotation;
            ent_xf->_uniformScale = spawner_xf->_uniformScale;
            auto setxform_op = comp->_genTransformOperation();
            _renderops.push(setxform_op);
            matched++;
          }
        }
      });
      if (matched == 0) {
        // Fallback: find entity directly (e.g. probe entities with no SceneGraphComponent)
        auto* ent = _simulation->findEntity(psname);
        if (ent) {
          auto spawner_xf = ent->data()->_dagnode->_xfnode->_transform;
          auto ent_xf = ent->transform();
          ent_xf->_translation = spawner_xf->_translation;
          ent_xf->_rotation = spawner_xf->_rotation;
          ent_xf->_uniformScale = spawner_xf->_uniformScale;
        }
      }
      break;
    }
    case "AttachEditorBillboard"_crcu: {
      const auto& table = *data.getShared<DataTable>();
      auto spawner_name = table["spawner"_tok].get<std::string>();
      auto bb_node = table["node"_tok].getShared<lev2::scenegraph::DrawableNode>();
      auto psname = AddPooledString(spawner_name.c_str());
      auto* ent = _simulation->findEntity(psname);
      if (ent) {
        bb_node->_userdata->makeValueForKey<uint64_t>("entref") = ent->_entref;
        bb_node->_dqxfdata._worldTransform = ent->_dagnode->_xfnode->_transform;
      }
      break;
    }
    default:
      System::_onNotify(evID, data);
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::_onPropertyChanged(token_t name, evdata_t value) {
  switch (name.hashed()) {
    case "autodraw"_crcu:
      _autodraw = value.get<bool>();
      break;
    case "autoupdate"_crcu:
      _autoupdate = value.get<bool>();
      break;
    default:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystem::_onRequest(impl::sys_response_ptr_t response, token_t reqID, evdata_t data) {
  printf("SGSYS REQ<%08llx>\n", (ull)reqID._hashed);
  switch (reqID.hashed()) {
    case CreateNode.hashed(): {

      ///////////////////////////////
      // fetch drawabledata / drawable
      ///////////////////////////////

      const auto& table = *data.getShared<DataTable>();
      auto mdata        = table["modeldata"_tok].get<modeldrawable_ptr_t>();

      ///////////////////////////////
      // create scenegraph node
      ///////////////////////////////

      auto add_operation = [response, mdata, this]() {
        drawable_ptr_t drawable;
        if (mdata->isSharedDrawable()) {
          drawable = _drwcache->fetch(mdata);
        } else if (auto as_model = std::dynamic_pointer_cast<ModelDrawableData>(mdata)) {
          auto cached_asset = _modelAssetCache->fetch(as_model->_assetpath);
          drawable = as_model->createDrawableWithAsset(cached_asset);
          drawable->_modcolor = mdata->_modcolor;
        } else {
          drawable = mdata->createDrawable();
          drawable->_modcolor = mdata->_modcolor;
        }

        std::string nodename = "???";
        auto sgnode          = _default_layer->createDrawableNode(nodename, drawable);

        ///////////////////////////////
        // track sgnode in response
        ///////////////////////////////

        //_simulation->debugBanner(128,255,0,"CreateNode <%p>\n", sgnode.get() );

        response->_responseData.set<lev2::scenegraph::drawable_node_ptr_t>(sgnode);
      };
      _renderops.push(add_operation);
      ///////////////////////////////

      break;
    }
    case "onCreateScenegraph"_tok._hashed: {
      printf("onCreateScenegraph <%p>\n", (void*)_scene.get());
      // response->_responseData.set<scenegraph::scene_ptr_t>(_scene);
      break;
    }
    default:
      OrkAssert(false);
      break;
  }
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::ecs::SceneGraphSystemData, "SceneGraphSystemData");
ImplementReflectionX(ork::ecs::SceneGraphSystem, "SceneGraphSystem");
