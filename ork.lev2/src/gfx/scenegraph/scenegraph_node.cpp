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

namespace ork::lev2::scenegraph {

///////////////////////////////////////////////////////////////////////////////

void Node::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////

Node::Node(std::string named)
    : _name(named) {
  _userdata = std::make_shared<varmap::VarMap>();
}

///////////////////////////////////////////////////////////////////////////////

DrawableNode::DrawableNode(std::string named, drawable_ptr_t drawable)
    : Node(named)
    , _drawable(drawable) {
  _modcolor = fvec4(1, 1, 1, 1);
}

///////////////////////////////////////////////////////////////////////////////

DrawableNode::~DrawableNode() {
}

///////////////////////////////////////////////////////////////////////////////

LightNode::LightNode(std::string named, light_ptr_t light)
    : Node(named)
    , _light(light) {
}

///////////////////////////////////////////////////////////////////////////////

LightNode::~LightNode() {
}

///////////////////////////////////////////////////////////////////////////////

ProbeNode::ProbeNode(std::string named, lightprobe_ptr_t probe)
    : Node(named)
    , _probe(probe) {
}

///////////////////////////////////////////////////////////////////////////////

ProbeNode::~ProbeNode() {
}
} // namespace ork::lev2::scenegraph