#!/usr/bin/env ork.python
################################################################################
# Copyright 1996-2026, Michael T. Mayers. MIT License.
################################################################################
# Step 3 of the FWD_SSBO_CUSTOM path (see project_fwd_ssbo_custom): GPU-culled chunked
# terrain rendered through a REAL generated ptex3d PBR material (hmview: height/slope blend,
# full forward lighting) — the material is a black box (material_ptr_t). The terrain DELEGATES
# the SSBO-pull vertex side via TerrainChunkVertexSource: it supplies the std430 layout, the
# pull VS (the VS IS the gen — derives position/normal/uv from heights[] per vertex), and the
# reset/cull/finalize compute that fills the visible-chunk list + the VkDrawIndirectCommand.
# ComputeDrawable selects the material's FWD_SSBO_CUSTOM technique by name and indirect-draws it.
#
# This is the precursor to converting ork.terrain.viewer2.py to a ComputeDrawable.
################################################################################

import sys
import numpy as np
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.hypergraph.ecs.scene.assets import Ptex3d as Ptex3dAsset
from ork.hypergraph.assets.materials.hmview import HMView
from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource

tokens = CrcStringProxy()

DIM, EXTENT, HSCALE, CHUNK = 512, 1000.0, 120.0, 128

def synth_height():
    xs = (np.arange(DIM, dtype=np.float32) / DIM - 0.5)
    X, Z = np.meshgrid(xs, xs)
    h = (0.5 + 0.25 * np.sin(X * 18.0) * np.cos(Z * 15.0)
             + 0.20 * np.exp(-((X * 2.2) ** 2 + (Z * 2.2) ** 2)))
    return np.clip(h, 0.0, 1.0).astype(np.float32).reshape(-1)

################################################################################

class TerrainPbrApp(ComponentizedApplication):
    def __init__(self):
        super().__init__()
        self.SGC = self.addComponent(
            "std_scenegraph",
            StandardSceneGraphComponent,
            eye=vec3(0, 250, 600),
            tgt=vec3(0, 0, 0),
            up=vec3(0, 1, 0),
            near=2.0,
            far=4000.0,
            explicit_near_far=True,
            grid_variant=None)
        self.createEzApp()

    def _onGpuInit(self, ctx):
        self.scene     = self.SGC.scenegraph
        self.layer_fwd = self.SGC.layer_fwd

        # the terrain owns the GPU geometry contract (layout + pull VS + cull compute)
        vs = TerrainChunkVertexSource(dim=DIM, extent_m=EXTENT, height_m=HSCALE, chunk=CHUNK)

        # generated ptex3d PBR material (hmview) — black box — with the vertex side DELEGATED.
        # The codegen splices FWD_SSBO_CUSTOM (pull VS + reset/cull/finalize compute) into it.
        self._mat_asset = Ptex3dAsset(dsl_class=HMView, height_scale=HSCALE, vertex_source=vs)
        self._mat_asset._ctx = ctx
        self._pbrmat = self._mat_asset.as_gfx_material   # _lev2.PBRMaterial (gpuInit'd) — kept alive on self
        fs = self._pbrmat.freestyle                      # internal FreestyleMaterial (technique/storage/compute)

        FXI = ctx.FXI
        self.terr = FXI.createShaderStorageBufferWithLength(vs.TOTAL)
        FXI.copyDataIntoShaderStorageBuffer(synth_height(), self.terr, vs.HEIGHTS_OFF)

        # STANDARD path: hand the ComputeDrawable the MATERIAL — at render it calls findPipeline(RCID)
        # with RCID._isSSBOSourced=true, so the material's cache picks its FWD_SSBO_CUSTOM variant
        # (with full forward lighting). No forced technique, no hand-built pipeline. The vertex-source
        # SSBO is bound onto that pipeline via addGraphicsStorage.
        sif = fs.storage("sif_ptex_vtx")

        cdd = lev2.ComputeDrawableData()
        cdd.material = self._pbrmat
        cdd.addGraphicsStorage(sif, self.terr)
        cdd.setCameraParams(self.terr, vs.CAM_OFF)
        for (name, gx, gy, gz) in vs.compute_passes():      # reset -> cull -> finalize
            cdd.addComputePass(fs.computeShader(name), [(sif, self.terr)], gx, gy, gz)
        cdd.setIndirect(args=self.terr, args_offset=vs.ARGS_OFF, primtype=tokens.TRIANGLES)
        self.node = self.layer_fwd.createDrawableNodeFromData("terrain", cdd)

        print(f"terrain_pbr_indirect: {vs.nchunk} chunks, ssbo {vs.TOTAL/1e6:.1f}MB, "
              f"hmview PBR via FWD_SSBO_CUSTOM (VS-is-gen)", flush=True)

    def _onUpdate(self, updinfo):
        self.SGC.scenegraph.updateScene(self.SGC.cameralut)

    def _onUiEvent(self, uievent):
        self.SGC._onCameraUiEvent(uievent)
        return lev2.ui.HandlerResult()

################################################################################

if __name__ == "__main__":
    app = TerrainPbrApp()
    app.ezapp.mainThreadLoop()
