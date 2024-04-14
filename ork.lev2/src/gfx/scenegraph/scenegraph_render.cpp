////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/ui/event.h>
#include <ork/util/logger.h>

using namespace std::string_literals;
using namespace ork;


///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::scenegraph {
///////////////////////////////////////////////////////////////////////////////
static constexpr bool RENDER_DEBUG_LOG = false;
static logchannel_ptr_t logchan_sgrender = logger()->createChannel("SGRENDER", fvec3(0.9, 0.2, 0.9));

///////////////////////////////////////////////////////////////////////////////
// enqueue scenegraph to renderer (update thread)
///////////////////////////////////////////////////////////////////////////////

void Scene::enqueueToRenderer(cameradatalut_ptr_t cameras, on_enqueue_fn_t on_enqueue) {

  if (_synchro) {
    bool OK = _synchro->beginUpdate();
    if (not OK) {
      return;
    }
  }

  auto DB = _dbufcontext_SG->acquireForWriteLocked();
  DB->Reset();
  DB->copyCameras(*cameras.get());

  auto cam = cameras->find("spawncam");
  // auto eye = cam->GetEye();

  on_enqueue(DB);

  if (RENDER_DEBUG_LOG) {
    logchan_sgrender->log("Scene<%p> enqueueToRenderer DB<%p>", this, DB);
  }

  _nodes2draw.clear();
  //_instancednodes2draw.clear();

  std::vector<layer_ptr_t> layers;

  _layers.atomicOp([&](const layer_map_t& unlocked) {
    for (auto layer_item : unlocked) {
      layers.push_back(layer_item.second);
    }
  });

  ////////////////////////////////////////////////////////////////////////////

  for (auto l : layers) {

    auto drawable_layer = DB->MergeLayer(l->_name);

    l->_drawable_nodes.atomicOp([drawable_layer, this](const Layer::drawablenodevect_t& unlocked) {
      for (auto n : unlocked) {
        if (n->_drawable and n->_enabled) {

          DrawItem item;
          item._layer   = drawable_layer;
          item._drwnode = n;
          _nodes2draw.push_back(item);
        }
      }
    });

    ////////////////////////////////////////////////////////////////////////////

    l->_instanced_drawable_map.atomicOp([drawable_layer, this](const Layer::instanced_drawmap_t& unlocked) {
      for (const auto& map_item : unlocked) {
        instanced_drawable_ptr_t drawable = map_item.first;
        if (drawable) {
          const Layer::instanced_drawablenodevect_t& instances_vect = map_item.second;
          size_t instances_count                                    = instances_vect.size();
          drawable->resize(instances_count);
          if (RENDER_DEBUG_LOG) {
            logchan_sgrender->log("enqueue instanced-drawable<%s> instances_count<%zu>", drawable->_name.c_str(), instances_count);
          }
        }
      }
    });

  } // for (auto l : layers) {

  ////////////////////////////////////////////////////////////////////////////

  if (1)
    for (auto item : _nodes2draw) {
      auto& drawable_layer = item._layer;
      auto n               = item._drwnode;
      if (RENDER_DEBUG_LOG) {

        logchan_sgrender->log(
            "enqueue drawable<%s> on layer<%s>", //
            (void*)n->_drawable->_name.c_str(),  //
            drawable_layer->_name.c_str());
      }
      n->_drawable->_pickable = n->_pickable;
      if (n->_viewRelative) {
        n->_dqxfdata._worldTransform->_viewRelative = true;
      }
      n->_drawable->enqueueOnLayer(n->_dqxfdata, *drawable_layer);
    }

  ////////////////////////////////////////////////////////////////////////////

  if (1)
    for (auto item : _staticDrawables) {
      auto drawable_layer = DB->MergeLayer(item._layername);
      auto drawable       = item._drawable;
      DrawQueueXfData xfd;
      if (RENDER_DEBUG_LOG) {
        logchan_sgrender->log("enqueue static-drawable<%s>", (void*)drawable->_name.c_str());
      }
      drawable->enqueueOnLayer(xfd, *drawable_layer);
    }

  _dbufcontext_SG->releaseFromWriteLocked(DB);

  if (_synchro) {
    _synchro->endUpdate();
  }
}

///////////////////////////////////////////////////////////////////////////////

void Scene::enablePickHud() {
  _enable_pick_hud = true;
}

void Scene::_renderIMPL(Context* context, rcfd_ptr_t RCFD) {

  if (_dogpuinit) {
    gpuInit(context);
  }

  if (_synchro) {
    bool OK = _synchro->beginRender();
    if (not OK) {
      return;
    }
  }

  auto DB = _dbufcontext_SG->acquireForReadLocked();

  RCFD->setUserProperty("DB"_crc, lev2::rendervar_t(DB));
  RCFD->setUserProperty("time"_crc, _currentTime);

  auto pick_mvp_matrix = std::make_shared<fmtx4>();

  RCFD->setUserProperty("pickbufferMvpMatrix"_crc, pick_mvp_matrix);

  RCFD->pushCompositor(_compositorImpl);

  _renderer->setContext(context);

  context->pushRenderContextFrameData(RCFD.get());
  auto fbi  = context->FBI();  // FrameBufferInterface
  auto fxi  = context->FXI();  // FX Interface
  auto mtxi = context->MTXI(); // matrix Interface
  auto gbi  = context->GBI();  // GeometryBuffer Interface
  auto dwi  = context->DWI();  // GeometryBuffer Interface
  ///////////////////////////////////////
  // compositor setup
  ///////////////////////////////////////
  float TARGW = fbi->GetVPW();
  float TARGH = fbi->GetVPH();
  lev2::UiViewportRenderTarget rt(nullptr);
  auto tgtrect            = ViewportRect(0, 0, TARGW, TARGH);
  _topCPD->_irendertarget = &rt;
  _topCPD->SetDstRect(tgtrect);
  _compositorImpl->pushCPD(*_topCPD);
  ///////////////////////////////////////
  // Draw!
  ///////////////////////////////////////
  fbi->SetClearColor(fvec4(0, 0, 0, 1));
  fbi->setViewport(tgtrect);
  fbi->setScissor(tgtrect);
  if (1) {
    // printf( "SceneGraph::_renderIMPL\n");
    context->beginFrame();
    FrameRenderer framerenderer(*RCFD, [&]() {});
    CompositorDrawData drawdata(framerenderer);
    drawdata._properties["primarycamindex"_crcu].set<int>(0);
    drawdata._properties["cullcamindex"_crcu].set<int>(0);
    drawdata._properties["irenderer"_crcu].set<lev2::IRenderer*>(_renderer.get());
    drawdata._properties["simrunning"_crcu].set<bool>(true);
    drawdata._properties["DB"_crcu].set<const DrawableBuffer*>(DB);
    drawdata._cimpl = _compositorImpl;
    _compositorImpl->assemble(drawdata);
    _compositorImpl->composite(drawdata);
    _compositorImpl->popCPD();
    context->popRenderContextFrameData();

    if (_on_render_complete) {
      _on_render_complete(context);
    }

    ////////////////////////////////////////////////////////////////////////////
    // debug picking here, so it shows up in renderdoc (within frame boundaries)
    ////////////////////////////////////////////////////////////////////////////
    if (0) {
      // auto r   = std::make_shared<fray3>(fvec3(0, 0, 5), fvec3(0, 0, -1));
      // auto val = pickWithRay(r);
      //  printf("%zx\n", val);
    }
    ////////////////////////////////////////////////////////////////////////////
    if (_enable_pick_hud) {
      static bool gpuinit                   = true;
      static freestyle_mtl_ptr_t pickhudmat = std::make_shared<lev2::FreestyleMaterial>();
      static fxtechnique_constptr_t tek_texcolor;
      static fxtechnique_constptr_t tek_texcolorpik;
      static fxtechnique_constptr_t tek_texcolormod1;
      static fxtechnique_constptr_t tek_texcolornrm;
      static fxparam_constptr_t par_colormap;
      static fxparam_constptr_t par_pickidmap;
      static fxparam_constptr_t par_mvp;

      if (gpuinit) {
        gpuinit = false;
        pickhudmat->gpuInit(context, "orkshader://solid");
        tek_texcolor     = pickhudmat->technique("texcolor");
        tek_texcolorpik  = pickhudmat->technique("texcolorpik");
        tek_texcolormod1 = pickhudmat->technique("texcolormod1");
        tek_texcolornrm  = pickhudmat->technique("texcolornrm");
        par_colormap     = pickhudmat->param("ColorMap");
        par_pickidmap    = pickhudmat->param("PickIdMap");
        par_mvp          = pickhudmat->param("MatMVP");
      }
      auto uimatrix = mtxi->uiMatrix(TARGW, TARGH);
      context->debugPushGroup("pickhud");
      size_t DIM = 200;
      if (_sgpickbuffer->_pickIDtexture) {
        pickhudmat->begin(tek_texcolorpik, *RCFD);
        fxi->BindParamCTex(par_pickidmap, _sgpickbuffer->_pickIDtexture);
        fxi->BindParamMatrix(par_mvp, uimatrix);
        dwi->quad2DEML(
            fvec4(0, 0, DIM, DIM), // quadrect
            fvec4(1, 0, -1, 1),    // uvrect
            fvec4(1, 0, -1, 1),    // uvrect2
            0.0f);                 // depth

        pickhudmat->end(*RCFD);
      }
      if (_sgpickbuffer->_pickPOStexture) {
        pickhudmat->begin(tek_texcolormod1, *RCFD);
        fxi->BindParamCTex(par_colormap, _sgpickbuffer->_pickPOStexture);
        fxi->BindParamMatrix(par_mvp, uimatrix);
        dwi->quad2DEML(
            fvec4(0, DIM, DIM, DIM), // quadrect
            fvec4(1, 0, -1, 1),      // uvrect
            fvec4(1, 0, -1, 1),      // uvrect2
            0.0f);                   // depth

        pickhudmat->end(*RCFD);
      }
      if (_sgpickbuffer->_pickNRMtexture) {
        pickhudmat->begin(tek_texcolornrm, *RCFD);
        fxi->BindParamCTex(par_colormap, _sgpickbuffer->_pickNRMtexture);
        fxi->BindParamMatrix(par_mvp, uimatrix);
        dwi->quad2DEML(
            fvec4(0, DIM * 2, DIM, DIM), // quadrect
            fvec4(1, 0, -1, 1),          // uvrect
            fvec4(1, 0, -1, 1),          // uvrect2
            0.0f);                       // depth

        pickhudmat->end(*RCFD);
      }
      if (_sgpickbuffer->_pickUVtexture) {
        pickhudmat->begin(tek_texcolor, *RCFD);
        fxi->BindParamCTex(par_colormap, _sgpickbuffer->_pickUVtexture);
        fxi->BindParamMatrix(par_mvp, uimatrix);
        dwi->quad2DEML(
            fvec4(0, DIM * 3, DIM, DIM), // quadrect
            fvec4(1, 0, -1, 1),          // uvrect
            fvec4(1, 0, -1, 1),          // uvrect2
            0.0f);                       // depth

        pickhudmat->end(*RCFD);
      }
      context->debugPopGroup();
    }
    ////////////////////////////////////////////////////////////////////////////
    context->endFrame();
  }
  _dbufcontext_SG->releaseFromReadLocked(DB);

  RCFD->popCompositor();

  if (_synchro) {
    _synchro->endRender();
  }
}

///////////////////////////////////////////////////////////////////////////////

void Scene::_renderWithAcquiredRenderDrawBuffer(acqdrawbuffer_constptr_t acqbuf) {
  auto DB      = acqbuf->_DB;
  auto rcfd    = acqbuf->_RCFD;
  auto context = rcfd->context();
  if (_dogpuinit) {
    gpuInit(context);
  }
  rcfd->setUserProperty("DB"_crc, lev2::rendervar_t(DB));
  rcfd->setUserProperty("time"_crc, _currentTime);
  rcfd->pushCompositor(_compositorImpl);
  _renderer->setContext(context);
  context->pushRenderContextFrameData(rcfd.get());
  auto fbi  = context->FBI();  // FrameBufferInterface
  auto fxi  = context->FXI();  // FX Interface
  auto mtxi = context->MTXI(); // matrix Interface
  auto gbi  = context->GBI();  // GeometryBuffer Interface
  ///////////////////////////////////////
  // compositor setup
  ///////////////////////////////////////
  float TARGW = fbi->GetVPW();
  float TARGH = fbi->GetVPH();
  lev2::UiViewportRenderTarget rt(nullptr);
  auto tgtrect            = ViewportRect(0, 0, TARGW, TARGH);
  _topCPD->_irendertarget = &rt;
  _topCPD->SetDstRect(tgtrect);
  _compositorImpl->pushCPD(*_topCPD);
  ///////////////////////////////////////
  // Draw!
  ///////////////////////////////////////
  if (1) {
    // printf( "SceneGraph::_renderWithAcquiredRenderDrawBuffer\n");
    fbi->SetClearColor(fvec4(0, 0, 0, 1));
    fbi->setViewport(tgtrect);
    fbi->setScissor(tgtrect);
    FrameRenderer framerenderer(*rcfd, [&]() {});
    CompositorDrawData drawdata(framerenderer);
    drawdata._properties["primarycamindex"_crcu].set<int>(0);
    drawdata._properties["cullcamindex"_crcu].set<int>(0);
    drawdata._properties["irenderer"_crcu].set<lev2::IRenderer*>(_renderer.get());
    drawdata._properties["simrunning"_crcu].set<bool>(true);
    drawdata._properties["DB"_crcu].set<const DrawableBuffer*>(DB);
    drawdata._cimpl = _compositorImpl;
    _compositorImpl->assemble(drawdata);
    _compositorImpl->composite(drawdata);
  }
  _compositorImpl->popCPD();
  context->popRenderContextFrameData();
  if (_on_render_complete) {
    _on_render_complete(context);
  }
  rcfd->popCompositor();
}

///////////////////////////////////////////////////////////////////////////////

void Scene::renderWithStandardCompositorFrame(standardcompositorframe_ptr_t sframe) {
  auto context = sframe->_drawEvent->GetTarget();
  if (_dogpuinit) {
    sframe->attachDrawBufContext(_dbufcontext_SG);
    gpuInit(context);
  }
  _renderer->setContext(context);
  sframe->compositor = _compositorImpl;
  sframe->renderer   = _renderer;
  sframe->passdata   = _topCPD;
  sframe->render();
}

///////////////////////////////////////////////////////////////////////////////

void Scene::renderOnContext(Context* context, rcfd_ptr_t RCFD) {
  _renderIMPL(context, RCFD);
}

///////////////////////////////////////////////////////////////////////////////

void Scene::renderOnContext(Context* context) {
  // from SceneGraphSystem::_onRender
  auto rcfd = std::make_shared<RenderContextFrameData>(context); // renderer per/frame data
  _renderIMPL(context, rcfd);
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::scenegraph {