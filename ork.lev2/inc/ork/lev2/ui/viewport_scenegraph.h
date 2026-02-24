////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/context.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/scenegraph/sgnode_uisurface.h>
#include <ork/lev2/editor/manip_controller.h>

namespace ork { namespace ui {

struct SceneGraphViewport : public Viewport {
  RttiDeclareAbstract(SceneGraphViewport, Viewport);
public:
  SceneGraphViewport(const std::string& name, int x=0, int y=0, int w=0, int h=0);
  ~SceneGraphViewport();
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) final;
  void _doGpuInit(lev2::Context* pTARG) final;
  void forkDB();
  void bindSceneGraph(lev2::scenegraph::scene_ptr_t sg);
  void bindManipController(lev2::editor::manipcontroller_ptr_t mc);

  /// Call gpuUpdate on all active SceneGraphViewport scenegraphs.
  /// Must be called from outside any render pass (e.g., onGpuUpdate phase).
  static void gpuUpdateAll(lev2::Context* ctx);

  lev2::scenegraph::scene_ptr_t _scenegraph;
  lev2::editor::manipcontroller_ptr_t _manipController;
  lev2::compositoroutnode_rtgroup_ptr_t _outputnode;
  int _supersample = 1;
  std::string _cameraname = "spawncam";
  lev2::acqdrawbuffer_ptr_t _override_acqdbuf;

  // Manipulation event handler (checked before camera)
  evhandler_t _manip_evhandler = nullptr;

  // Camera event handler (set by user, e.g., for EzUiCam)
  evhandler_t _camera_evhandler = nullptr;

  // Pre-render callback (called at start of DoRePaintSurface, inside frame context)
  using prerender_callback_t = std::function<void(lev2::Context*)>;
  prerender_callback_t _preRenderCallback = nullptr;

  // Track when mouse is over an embedded UI surface
  bool _overUiSurface = false;

  // Private ui::Context for embedded UI surfaces (LayoutSurfaces in 3D)
  context_ptr_t _embeddedUiContext;

protected:
  HandlerResult DoOnUiEvent(event_constptr_t ev) override;

private:
  // Route event to embedded UI surfaces, returns true if consumed
  HandlerResult _routeToEmbeddedUiSurfaces(event_constptr_t ev);
};

}} // namespace ork::ui
