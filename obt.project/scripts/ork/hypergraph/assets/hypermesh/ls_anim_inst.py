###############################################################################
# ls_anim_inst — the `ls_anim` tree (baked trunk + leaf cards, gid 0 bark / gid 1
# leaf, both VS-animated) drawn as an INSTANCED FOREST: one shared mesh + materials,
# placed grid×grid times by per-instance matrices in ONE indirect draw per gid
# bucket. Each instance is the WHOLE tree (trunk+leaves) -> one per-view frustum cull
# per tree, culled as a unit. Layout/jitter is the shared _forest.forest_matrices().
#
#   ork.hypermesh.viewer.py ls_anim_inst --msaa 2
###############################################################################
from ork.hypergraph.assets.hypermesh.ls_anim import LsAnim
from ork.hypergraph.assets.hypermesh._forest import forest_matrices


class LsAnimInst(LsAnim):
  def __init__(self, seed=1):
    super().__init__()                       # tree mesh + gid partition + materials() (bark+wind / foliage)
    # FOREST LAYOUT — tune in place (these are the forest's identity, not call-site knobs).
    self.instances = forest_matrices(grid=128,
                                     spacing=1.2,
                                     scale=0.75,
                                     pos_jitter=0.4,
                                     rot_jitter=1.0,
                                     tilt_jitter=0.15,
                                     phase_jitter=1.0,
                                     amp_jitter=0.3,
                                     freq_jitter=0.3,
                                     seed=seed)
    self.cull         = True                  # E.4: per-view GPU frustum cull (auto object-space bound;
                                              # the +5% auto-pad ~covers the small VS wind sway). Cull
                                              # tightness is now the frame-global CullFrustumScale
                                              # scenegraph param (default 1.0 = exact), not a per-asset knob.


__all__ = ["LsAnimInst"]
