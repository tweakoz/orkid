////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

///////////////////////////////////////////////////////////////////////////////

#include <unordered_set>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/material_freestyle.inl>

namespace ork::lev2::scenegraph {

struct Layer;
struct Node;
struct SceneGraph;
using scenelayer_ptr_t = std::shared_ptr<Layer>;
using scenenode_ptr_t = std::shared_ptr<Node>;
using scenegraph_ptr_t = std::shared_ptr<SceneGraph>;

///////////////////////////////////////////////////////////////////////////////

struct Node {

	Node();
	~Node();

	drawable_ptr_t _drawable;
	scenelayer_ptr_t _layer;
};


///////////////////////////////////////////////////////////////////////////////

struct Layer {
	Layer(std::string name);
	~Layer();

	void addNode(scenenode_ptr_t node);
	void removeNode(scenenode_ptr_t node);

	std::string _name;
	std::unordered_set<scenenode_ptr_t> _nodeset;
	std::vector<scenenode_ptr_t> _nodevect;
};	


///////////////////////////////////////////////////////////////////////////////

struct SceneGraph {

	SceneGraph();
	~SceneGraph();

	scenelayer_ptr_t createLayer(std::string named);
    void enqueueToRenderer();
    void renderOnContext(Context* ctx);

	std::map<std::string,scenelayer_ptr_t> _layers;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph