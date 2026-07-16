###############################################################################
# lsystem.examples — the normative GR-1 grammars (UNIFIED_SUBSTRATE §17.2). CaneCholla is the
# combinator vertical-slice target (rendered in scn_cane_cholla.py); DesertBroom is the small
# dual-form grammar authored BOTH ways (the string-vs-combinator identical-JSON gate). Kept in one
# place so the gate tests and the render scene share a single source of truth.
###############################################################################

from ork.hypergraph.dflow import lsystem as L


class CaneCholla(L.Lsystem):
  """A New Mexico cane cholla (§17.2). Tuberculate joints fork stochastically into child canes,
  studded with areole spine slots, blooming & stopping with low probability or at generation 11.
  GR-1 renders the CANE mesh (H.sweep); spine/flower instancing is GR-2 (the SLOT ops emit now).

  NOTE (seed): §17.2 authored seed='nm-cholla-07' illustratively — that draw happens to take the
  terminal `bloom` at the very first `choose` (a 2-node plant; the spec example was never executed).
  The grammar STRUCTURE here is byte-faithful to §17.2; the seed is 'staghorn' (a real cane-cholla
  species), which grows the full stochastic cactus (~253 skeleton nodes / ~294 areole+bloom slots)."""

  seed = "staghorn"

  def grammar(self):
    Shoot = self.symbol("Shoot", len=0.16, rad=0.020, gen=0)

    @L.rule(Shoot, until=Shoot.gen >= 11)
    def grow(S):
      return [
          L.segment(len=S.len, rad=S.rad, gid="cane"),          # one tuberculate joint
          L.spines(every=0.035, gid="spine"),                   # areole slots down the joint
          L.choose(
              (0.50, Shoot(len=S.len * 0.96, rad=S.rad * 0.92, gen=S.gen + 1)),   # keep growing
              (0.34, L.fork(2, 3, lean=(22, 42),                                  # 2-3 child canes
                            then=Shoot(len=S.len * 0.80, rad=S.rad * 0.80, gen=S.gen + 1))),
              (0.16, L.bloom(gid="flower")),                    # bloom & stop
          ),
      ]

    @L.rule(Shoot, when=Shoot.gen >= 11)
    def cap(S):
      return [L.segment(len=S.len, rad=S.rad, gid="cane"), L.bloom(gid="flower")]

    self.axiom(Shoot())
    self.iterate(depth=11, segment_budget=700)   # budget caps bake cost


###############################################################################
# DesertBroom — the SAME grammar authored two ways (§17.2). The gate: string-form and
# combinator-form produce IDENTICAL uuid-stripped LRuleSet JSON.
###############################################################################

class DesertBroomString(L.Lsystem):
  """string dual form: F=segment(wood)  + -=yaw  [ ]=branch  !=taper(param)  A=CALL."""
  axiom  = "A"
  rules  = {"A": "F[+(25)A][-(25)A]F!(0.8)A"}
  legend = {"F": L.segment(len=0.12, gid="wood"), "!": L.taper}
  iterate = dict(depth=6, segment_budget=200)


class DesertBroomCombinator(L.Lsystem):
  """combinator form of the SAME grammar (must lower to byte-identical uuid-stripped JSON)."""

  def grammar(self):
    A = self.symbol("A")

    @L.rule(A)
    def expand(S):
      return [
          L.segment(len=0.12, gid="wood"),
          L.branch(L.yaw(25), A()),
          L.branch(L.yaw(-25), A()),
          L.segment(len=0.12, gid="wood"),
          L.taper(0.8),
          A(),
      ]

    self.axiom(A())
    self.iterate(depth=6, segment_budget=200)


__all__ = ["CaneCholla", "DesertBroomString", "DesertBroomCombinator"]
