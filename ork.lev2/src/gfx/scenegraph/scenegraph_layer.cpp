////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/ui/event.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/kernel/opq.h>
#include <ork/util/logger.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>

using namespace std::string_literals;
using namespace ork;

static constexpr bool DEBUG_LOG = false;

namespace ork::lev2::scenegraph {
static logchannel_ptr_t logchan_sglayer = logger()->createChannel("SG-LAYER", fvec3(0.9, 0.2, 0.9));

///////////////////////////////////////////////////////////////////////////////

Layer::Layer(Scene* scene, std::string named)
    : _scene(scene)
    , _name(named) {
}

///////////////////////////////////////////////////////////////////////////////

Layer::~Layer() {
}

///////////////////////////////////////////////////////////////////////////////

drawable_node_ptr_t Layer::createDrawableNode(std::string named, drawable_ptr_t drawable) {
  drawable_node_ptr_t rval = std::make_shared<DrawableNode>(named, drawable);
  drawable->_sgnode        = rval;
  _drawable_nodes.atomicOp([rval](Layer::drawablenodevect_t& unlocked) { unlocked.push_back(rval); });
  if (DEBUG_LOG) {
    logchan_sglayer->log(
        "createDrawableNode layer<%s> named<%s> drawable<%p> node<%p>", //
        _name.c_str(),                                                  //
        named.c_str(),                                                  //
        drawable.get(),                                                 //
        rval.get());                                                    //
  }
  drawable->_pickID.set<object_ptr_t>(rval);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::addDrawableNode(drawable_node_ptr_t node) {
  _drawable_nodes.atomicOp([node](Layer::drawablenodevect_t& unlocked) { unlocked.push_back(node); });
}

///////////////////////////////////////////////////////////////////////////////

void Layer::removeDrawableNode(drawable_node_ptr_t node) {
  _drawable_nodes.atomicOp([node](Layer::drawablenodevect_t& unlocked) {
    auto it = std::remove_if(unlocked.begin(), unlocked.end(), [node](drawable_node_ptr_t d) -> bool {
      bool matched = (node == d);
      return matched;
    });
    unlocked.erase(it);
  });
}

///////////////////////////////////////////////////////////////////////////////

instanced_drawable_node_ptr_t Layer::createInstancedDrawableNode(std::string named, instanced_drawable_ptr_t drawable) {
  instanced_drawable_node_ptr_t rval = std::make_shared<InstancedDrawableNode>(named, drawable);
  size_t count                       = 0;
  _instanced_drawable_map.atomicOp([rval, drawable, &count](Layer::instanced_drawmap_t& unlocked) { //
    auto& vect                   = unlocked[drawable];
    rval->_instanced_drawable_id = vect.size();
    vect.push_back(rval);
    count = rval->_instanced_drawable_id + 1;
  });
  drawable->_instancedata->resize(count);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::removeInstancedDrawableNode(instanced_drawable_node_ptr_t node) {

  _instanced_drawable_map.atomicOp([node](Layer::instanced_drawmap_t& unlocked) {
    auto drawable = node->_shared_drawable;

    auto it_d = unlocked.find(drawable);
    OrkAssert(it_d != unlocked.end());

    auto& vect = it_d->second;

    if (vect.size() > 1) {
      auto it  = vect.begin() + node->_instanced_drawable_id;
      auto rit = vect.rbegin();
      *it      = *rit;
      vect.erase(rit.base());
    } else {
      vect.clear();
    }
  });
}

///////////////////////////////////////////////////////////////////////////////

lightnode_ptr_t Layer::createLightNode(std::string named, light_ptr_t light) {
  lightnode_ptr_t rval = std::make_shared<LightNode>(named, light);

  _lightnodes.atomicOp([rval](Layer::lightnodevect_t& unlocked) { unlocked.push_back(rval); });

  auto lmgr = _scene->_lightManager;
  lmgr->mGlobalMovingLights.AddLight(light.get());

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::removeLightNode(lightnode_ptr_t node) {
  _lightnodes.atomicOp([node](Layer::lightnodevect_t& unlocked) { //
    auto it = std::remove_if(unlocked.begin(), unlocked.end(), [node](lightnode_ptr_t d) -> bool {
      bool matched = (node == d);
      return matched;
    });
    if (it != unlocked.end()){
      unlocked.erase(it);
    }
   });
}

///////////////////////////////////////////////////////////////////////////////

probenode_ptr_t Layer::createProbeNode(std::string named, lightprobe_ptr_t probe) {
  probenode_ptr_t rval = std::make_shared<ProbeNode>(named, probe);

  _probenodes.atomicOp([rval](Layer::probenodevect_t& unlocked) { unlocked.push_back(rval); });

  auto lmgr = _scene->_lightManager;
  lmgr->_lightprobes.push_back(probe);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void Layer::removeProbeNode(probenode_ptr_t node) {
  _probenodes.atomicOp([node](Layer::probenodevect_t& unlocked) {
    auto it = std::remove_if(unlocked.begin(), unlocked.end(), [node](probenode_ptr_t d) -> bool {
      bool matched = (node == d);
      return matched;
    });
    if (it != unlocked.end()){
      unlocked.erase(it);
    }
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph
