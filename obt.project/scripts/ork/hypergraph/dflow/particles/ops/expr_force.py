from orkengine.lev2 import particles as _particles
from ._chain import chain_op
from ..exprir_particles import capture_force


def expr_force(upstream, *packs, fx=None, fy=None, fz=None, strength=1.0,
               name=None, **plug_kwargs):
    """ExprIR-driven per-particle force (E2.5 S8, context "particles.force").

    Each of fx/fy/fz is a force-channel expression (a PF.* builder or a plain
    number) evaluated per particle on the CPU by the C++ ExprForce module:
        Δv = vec3(fx, fy, fz) * Strength * dt
    Symbols: PF.unit_age / age / random / pos_{x,y,z} / vel_{x,y,z} / speed.
    An unset axis contributes zero. `strength` is the A8-parametric multiplier
    (a Strength float plug). e.g. a swirl that stiffens with age:

        from ork.hypergraph.dflow.particles.exprir_particles import PF
        chain = P.ExprForce(emit,
                            fx=PF.vel_z * -2.0,
                            fz=PF.vel_x *  2.0,
                            fy=PF.smoothstep(0.0, 0.3, PF.unit_age) * -3.0)
    """
    node = chain_op(_particles.ExprForce, "EFRC", upstream,
                    name=name, packs=packs, Strength=strength, **plug_kwargs)
    m = node.module
    if fx is not None:
        m.force_x = capture_force(fx)
    if fy is not None:
        m.force_y = capture_force(fy)
    if fz is not None:
        m.force_z = capture_force(fz)
    return node
