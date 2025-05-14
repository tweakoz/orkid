////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "statedebug.h"
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/drawable.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void _FtxGlDebugger::_validateRenderer() {
  using namespace ftxui;
  node_vect_t NODES;

  // size_t num_textures = _glctx->mTxI._texture_set.size();
  auto RCFD = _glctx->topRenderContextFrameData();

  if (RCFD) {

    auto rendermodel       = renderCrcStringToString(RCFD->_renderingmodel._modelID);
    size_t cimplstack_size = RCFD->__cimplstack.size();
    std::string rcfdname   = RCFD->_name;
    const DrawQueue* db    = RCFD->GetDB();
    std::string layers;
    for (auto it : db->mLayers) {
      layers += it;
      layers += ",";
    }

    _colortext(
        NODES, //
        YEL,
        BLK, //
        "Renderer/Compositor/State\n");

    _colortext(
        NODES, //
        YEL,
        BLK, //
        "  RENDERINGMODEL: %s\n",
        rendermodel.c_str());
    _colortext(
        NODES, //
        YEL,
        BLK, //
        "  RCFDNAME: %s\n",
        rcfdname.c_str());
    _colortext(
        NODES, //
        YEL,
        BLK, //
        "  CIMPLSTACKLEN: %zu\n",
        cimplstack_size);
    _colortext(
        NODES, //
        YEL,
        BLK, //
        "    LAYERS: [%s]\n",
        layers.c_str());
    for (auto l : db->mLayerLut) {
      _colortext(
          NODES, //
          YEL,
          BLK, //
          "    LAYER: %s\n",
          l.first.c_str());
          DrawQueueLayer* layer = l.second;
          size_t num_items = 0;
          layer->_items.atomicOp([&](const DrawQueueLayer::itemvect_t& unlocked) {
            num_items = unlocked.size();
          });
          _colortext(
              NODES, //
              YEL,
              BLK, //
              "      ITEMCOUNT: %zu\n",
              num_items);
    }

    _node_renderer = vbox({
        text("Renderer"),
        separator(),
        vbox(std::move(NODES)),
    });
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
