###############################################################################
# scatterdemo — HeightField.scatter() on top of the ErodeFlow terrain.
###############################################################################

from ork.hypergraph.dflow import terrain as T
from ork.hypergraph.assets.terrain.erodeflow import ErodeFlow
from ork.hypergraph.colors import hsv
from orkengine.core import vec3

###############################################################################

class ScatterDemo(ErodeFlow):

    #################################################
    # DFLOW side (extend ErodeFlow's by scattering flora)
    #################################################

    def __init__(self, iters=32):
        super().__init__(iters=iters,origin=vec3(0))           # builds eroded terrain + captures; sets self.z
        z     = self.z                          # inherited final height node
        slope = T.slope(z, radius_m=8.0)        # 0 flat .. ~1 steep (scale= tunes angle->value)
        alt   = T.normalize(z)                  # elevation [0,1]
        # reject slopes: 1 on flat ground (slope < 0.10), -> 0 on steep (slope > 0.40). Tune the two
        # thresholds (and T.slope's scale=) to your terrain; smaller window = sharper tree line.
        flat  = 1.0 - T.smoothstep(slope, 0.10, 0.40)
        self.scatter("flora",
            density = 0.0030,                   # points / m^2 (ErodeFlow is ~16 km, so sparse)
            seed    = 7,
            cutoff  = 0.1,
            jitter  = 1.0,
            align   = "up",                     # trees grow vertical (use "normal" to tilt on slopes)
            scale   = (0.7, 1.4),
            types   = {                         # per-type weight fields (exclusive within)
              "pine":  T.band(alt, 0.30, 0.50, soft=0.1) * flat,   # type_id 0  high-dry
              "oak":   T.band(alt, 0.20, 0.50, soft=0.1) * flat,   # type_id 1  mid
              "shrub": T.band(alt, 0.20, 0.30, soft=0.1),          # type_id 2  low
              "shrub2": T.band(alt, 0.00, 0.20, soft=0.1),          # type_id 2  low
            })

    #################################################
    # RENDER side: the ASSET owns the per-type drawable (mesh + material)
    #################################################

    def scatter_models(self, sink_name, ctx):
        """Ordered list (indexed by type_id) of (RigidPrimitive, PBRMaterial). Meshes from the
        mesh-asset module (Cone / IcoSphere -> rigid_primitive); sized in METERS (scale 0.7..1.4
        multiplies). Sizes are bumped for the ~16 km ErodeFlow terrain."""
        from orkengine.core import vec3
        from ork.hypergraph.assets.mesh.cone import Cone
        from ork.hypergraph.assets.mesh.icosphere import IcoSphere
        from ork.hypergraph.assets.materials import Solid   # plain solid-color PBR (accepts hsv())

        pine  = Cone(radius=4.0,  height=6.0, segments=10, cap=True,flip_winding=True,).rigid_primitive(ctx)   # type 0
        oak   = Cone(radius=3.0,  height=5.0, segments=10, cap=True,flip_winding=True,).rigid_primitive(ctx)   # type 1
        # lift the centered sphere by its radius so its base sits on the terrain (not half-buried).
        # (flip_winding removed: the scatter TRS is now right-handed (det=+1, GL RH Y-up), so it no
        #  longer reflects/flips winding — the shipped CCW winding renders correct.)
        shrub = IcoSphere(radius=2.0, subdivisions=1, flip_winding=False,
                          translation=vec3(0.0, 1.0, 0.0)).rigid_primitive(ctx)    # type 2
        shrub2 = IcoSphere(radius=2.0, subdivisions=0,
                           translation=vec3(0.0, 1.0, 0.0)).rigid_primitive(ctx)   # type 3
        # instancing_matrices_only: flora needs only per-instance transforms (no per-instance color),
        # so use the matrices-only dynamic block (FWD_CT_NM_IM_NI_MO).
        return [(pine,   Solid(ctx, hsv(100, 0.50, 0.5), instancing_matrices_only=True)),
                (oak,    Solid(ctx, hsv(140, 0.50, 0.5), instancing_matrices_only=True)),
                (shrub,  Solid(ctx, hsv(120, 0.24, 0.5), instancing_matrices_only=True)),
                (shrub2, Solid(ctx, hsv(140, 0.05, 0.5), instancing_matrices_only=True))]
