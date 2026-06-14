from orkengine.lev2 import particles as _particles
from ._chain import chain_op


def poly_drag(upstream, *packs, name=None, **plug_kwargs):
    """Polynomial drag force opposing velocity:
        |F| = Constant + Linear*|v| + Quadratic*|v|^2 + Cubic*|v|^3

    Plug names: Constant, Linear, Quadratic, Cubic (all default 0 = off).
    Physical readings:
      Constant   — Coulomb friction (uniform decel)
      Linear     — Stokes drag (laminar, low-Reynolds)
      Quadratic  — Newtonian drag (turbulent, high-Re — air resistance)
      Cubic      — high-velocity nonlinear damping

    Clamps to v=0 instead of overshooting, so it's safe to use heavy
    coefficients without jitter at rest. Maps to particles.PolyDrag."""
    return chain_op(_particles.PolyDrag, "DRAG", upstream,
                    name=name, packs=packs, **plug_kwargs)
