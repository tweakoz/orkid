////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <sstream>
#include <ork/kernel/opq.h>
#include <ork/lev2/ui/event.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectObjectVector.inl>
#include <ork/math/cvector4.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>

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
}

///////////////////////////////////////////////////////////////////////////////

SceneGraphSystemData::SceneGraphSystemData() {
  _internalParams = std::make_shared<varmap::VarMap>();
}

void SceneGraphSystemData::setInternalSceneParam(const varmap::key_t& key, const varmap::VarMap::value_type& val) {
  _internalParams->setValueForKey(key, val);
}

void SceneGraphSystemData::declareLayer(const std::string& layername) {
  _declaredLayers.push_back(layername);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphSystemData::declareNodeOnLayer(nodedef_ptr_t ndef) {
  auto nid           = std::make_shared<SceneGraphNodeItemData>();
  nid->_nodename     = ndef->_nodename;
  nid->_drawabledata = ndef->_drawabledata;
  nid->_layername    = ndef->_layername;
  nid->_multilayers    = ndef->_multilayers;
  nid->_xfoverride   = ndef->_transform;
  nid->_modcolor     = ndef->_modcolor;

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
  
        if (auto as_instanced = dynamic_pointer_cast<InstancedDrawable>(nitem->_drawable)) {
          auto NODE_ON_LAYER = [=](lev2::scenegraph::layer_ptr_t layer){
            auto node      = layer->createDrawableNode(NID->_nodename, as_instanced);
            nitem->_sgnode = node;
            size_t count   = as_instanced->_count;
            auto idata     = as_instanced->_instancedata;
            for (size_t i = 0; i < count; i++) {

              int ix   = rand() & 0xffff;
              int iz   = rand() & 0xffff;
              float fx = (float(ix) / 32768.0f - 1.0f) * 100.0f;
              float fz = (float(iz) / 32768.0f - 1.0f) * 100.0f;
              fvec3 pos(fx, 0, fz);

              idata->_worldmatrices[i].setColumn(3, pos);
              idata->_modcolors[i] = fvec4(1, 1, 1, 1);
              idata->_pickids[i]   = 0;
              //printf( "init instanced<%d>\n", i );
            }
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

  _onGpuInitOpQueue.atomicOp([](std::vector<void_lambda_t>& unlocked) {
    for (auto item : unlocked) {
      item();
    }
    unlocked.clear();
  });

  // GPU uploads deferred to loading phase (safe for first-run shader compilation)
  auto ph = ctx->newLoadingPhase();
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
      // (e.g. via allocateColorSlice); skip if no images yet (A-D scenes)
      if (_cookieColorArray && !_cookieColorArray->_images.empty()) {
        ctx->TXI()->updateTextureArray(_cookieColorArray.get());
      }
      // Init depth array (render target) once
      if (_cookieDepthArray && !_cookieDepthArray->_gpuInitialized) {
        ctx->TXI()->initTextureArray2D(_cookieDepthArray.get());
        _cookieDepthArray->_gpuInitialized = true;
      }
    }
    if (_scene->_lightManager) {
      _scene->_lightManager->gpuInit(ctx);
    }
  });

  /////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void SceneGraphSystem::_onStageComponent(SceneGraphComponent* component) {
  //////////////////////////////
  // initialize transform
  //////////////////////////////
  auto ent = component->GetEntity();
  //////////////////////////////
  //printf("sgsys stage component<%p>\n", (void*) component);
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
    // now check for INSTANCE's
    if (COMPDATA._INSTANCEDATA) {
      auto instance        = std::make_shared<lev2::scenegraph::NodeInstance>();
      instance->_groupname = COMPDATA._INSTANCEDATA->_groupname;
      // TODO : defer until nodes created ?
      auto it = _nodeitems.find(instance->_groupname);
      OrkAssert(it != _nodeitems.end());
      sgnodeitem_ptr_t groupitem = it->second;
      auto group_drawable        = std::dynamic_pointer_cast<lev2::InstancedDrawable>(groupitem->_drawable);
      instance->_idrawable       = group_drawable;
      auto idata                 = group_drawable->_instancedata;
      instance->_idata           = idata;
      int ID                     = idata->allocInstance();
      instance->_instance_index  = ID;
      component->_INSTANCE       = instance;
      //printf( "sgc<%p> instanced sg pseudonode id<%d>\n", this, ID );
      if(instance){
        auto ent = component->GetEntity();
        auto sad = ent->_spawnanondata;
        if(sad and sad->_table){
          auto modcolor = (*sad->_table)["modcolor"_tok];
          //.get<fvec4>();
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
  return true;
}
void SceneGraphSystem::_onUnLink(Simulation* psi) // final
{
}
///////////////////////////////////////////////////////////////////////////////
bool SceneGraphSystem::_onStage(Simulation* psi) {
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
  }

  _scene->applyRuntimeParams(_mergedParams);

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
}
///////////////////////////////////////////////////////////////////////////////
bool SceneGraphSystem::_onActivate(Simulation* psi) // final
{

  return true;
}
void SceneGraphSystem::_onDeactivate(Simulation* inst) // final
{
}
void SceneGraphSystem::_onUpdate(Simulation* psi) // final
{
  OrkProfilerSampleScope(CHANNEL_UPDATE, "SceneGraphSystem::_onUpdate");
  if (_scene && _autoupdate) {
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

void SceneGraphSystem::_onNotify(token_t evID, evdata_t data) {

  switch (evID.hashed()) {
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
  printf("SGSYS REQ<%08llx>\n", reqID._hashed);
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
