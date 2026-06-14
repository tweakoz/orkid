#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
################################################################################
# hypermesh basic — materialize a hypermesh ASSET (RippleGrid) through the GPU mesh
# compute-dataflow (C++ modules composed by the Python DSL, baked to a SoA GpuMesh), and
# render it to a SGVP via FWD_SSBO_CUSTOM multi-block SoA pull (P/N/B/uv/color from separate
# channel SSBOs, indirect-drawn) through a stock ptex3d material. Same shape as
# gpu_mesh_primitive.py, but the geometry comes from the real ork::dataflow machine.
################################################################################

from orkengine.core import vec3, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.hypergraph.assets.hypermesh import RippleGrid
from ork.hypergraph.dflow.hypermesh import make_drawable

tokens = CrcStringProxy()


class HypermeshBasicApp(ComponentizedApplication):
  def __init__(self):
    super().__init__()
    self.SGC = self.addComponent(
      "std_scenegraph", StandardSceneGraphComponent,
      eye=vec3(6, 5, 9), tgt=vec3(0, 0, 0), up=vec3(0, 1, 0),
      grid_variant=None)
    self.createEzApp()

  def _onGpuInit(self, ctx):
    # author + bake the asset (DSL -> graphdata -> bakeMesh -> SoA GpuMesh)
    asset       = RippleGrid(grid=64, amp=1.2, freq=2.2, subdivide=1)
    self._gmesh = asset.materialize(ctx)   # keep alive: owns the pooled channel buffers
    print(f"hypermesh basic: materialized {self._gmesh.vtx_count} verts (cap {self._gmesh.capacity})", flush=True)

    cdd, self._gmtl = make_drawable(self._gmesh, ctx, albedo=vec3(0.72, 0.74, 0.78), roughness=0.55)
    self.node = self.SGC.layer_fwd.createDrawableNodeFromData("hypermesh", cdd)
    print("hypermesh basic: rendering compute-dataflow mesh via FWD_SSBO_CUSTOM SoA. orbit to inspect.", flush=True)

  def _onUpdate(self, updinfo):
    self.SGC.scenegraph.updateScene(self.SGC.cameralut)

  def _onUiEvent(self, uievent):
    self.SGC._onCameraUiEvent(uievent)
    return lev2.ui.HandlerResult()


if __name__ == "__main__":
  app = HypermeshBasicApp()
  app.ezapp.mainThreadLoop()
