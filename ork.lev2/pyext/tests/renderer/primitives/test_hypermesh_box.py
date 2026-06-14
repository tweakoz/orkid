#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
################################################################################
# hypermesh box — materialize a Box asset (QUAD topology) and render it to a SGVP. The
# render VS triangulates each quad via gl_VertexID (the prim_type=1 path), proving the
# tri/quad topology support through the same FWD_SSBO_CUSTOM multi-block SoA pull.
################################################################################

from orkengine.core import vec3, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.hypergraph.assets.hypermesh import Box
from ork.hypergraph.dflow.hypermesh import make_drawable

tokens = CrcStringProxy()


class HypermeshBoxApp(ComponentizedApplication):
  def __init__(self):
    super().__init__()
    self.SGC = self.addComponent(
      "std_scenegraph", StandardSceneGraphComponent,
      eye=vec3(3, 2.5, 4), tgt=vec3(0, 0, 0), up=vec3(0, 1, 0),
      grid_variant=None)
    self.createEzApp()

  def _onGpuInit(self, ctx):
    self._gmesh = Box(size=1.0).materialize(ctx)
    print(f"hypermesh box: {self._gmesh.vtx_count} stored verts, prim_type={self._gmesh.prim_type} (1=QUAD)", flush=True)
    cdd, self._gmtl = make_drawable(self._gmesh, ctx, albedo=vec3(0.80, 0.80, 0.86), roughness=0.4)
    self.node = self.SGC.layer_fwd.createDrawableNodeFromData("box", cdd)
    print("hypermesh box: rendering QUAD mesh (VS triangulates). orbit to inspect.", flush=True)

  def _onUpdate(self, updinfo):
    self.SGC.scenegraph.updateScene(self.SGC.cameralut)

  def _onUiEvent(self, uievent):
    self.SGC._onCameraUiEvent(uievent)
    return lev2.ui.HandlerResult()


if __name__ == "__main__":
  app = HypermeshBoxApp()
  app.ezapp.mainThreadLoop()
