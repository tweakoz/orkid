###############################################################################
# lsystem.presets — the four ARCHETYPE PRESET EMITTERS (GRAMMARS GR1.d).
#
# "species = data": each legacy C++ growth procedure (LArchetype sympodial/conifer/
# saguaro/ocotillo, hmdflow_module_lsystem.cpp) re-authored as a combinator-DSL
# GRAMMAR. `H.lsystem(archetype=...)` selects one of these emitters — the DSL
# surface is unchanged for callers; only the backing moves from a C++ enum switch
# to reflected LRuleSet data derived by the (untouched) GR1.b evaluator.
#
# THE PRESET CONTRACT (what stays live, what folds):
#   * float tweakables (seg_len/base_radius/branch_angle/roll/len_decay/rad_decay/
#     taper/apical + the jitter channels) are referenced as S.<name> PARAM exprs —
#     they resolve through the module's 21 reflected scalars (the LExpr PARAM env,
#     A8) and stay live-pokeable. tropism / jitter*jit_wave additionally act in the
#     interpret stage on every SEGMENT (evaluator behavior, all grammars).
#   * int structurals (depth/children/internodes) are EMITTER-BUILD-TIME parameters:
#     they shape the rule tree itself (unroll counts, FORK counts, guard bounds).
#     Changing them re-emits the grammar -> new content hash -> re-cook. This is the
#     same rebuild boundary the legacy path had (ints reshaped the recursion).
#   * seed/budget ride LRuleSet._seed/_segmentBudget (the stateless counter-hash
#     RNG contract, GR-T10 — derivation is deterministic per (grammar, env, seed)).
#   * per-archetype jitter-channel defaults live HERE (PRESET_JIT) as preset
#     parameters — the single source the DSL's jit_* None-fallback reads.
#
# PARITY vs the legacy procedures (GR-T10: node-count-CLASS + silhouette, NOT bit
# parity — RNG consumption order differs by design). Known approximations, each
# arbitrated by the GR1.d evidence pack:
#   * conifer: per-branch tropism is not expressible (interpret applies ONE module
#     tropism to every segment). The preset folds the authored `tropism=` kwarg
#     into the lateral droop pitch (legacy droop = 0.6*|tropism|+0.04 rad/internode)
#     and overrides the module scalar to a small constant leader-hold (+0.10 rad,
#     compensated in the droop constant). Live-poking module tropism no longer
#     re-droops a conifer; re-emit the preset instead.
#   * saguaro: legacy arms curl by a FIXED accelerating schedule (0.06+0.5*j/N
#     rad/segment) independent of the tropism kwarg; the preset overrides module
#     tropism to a constant (0.13 rad, the schedule's reach-vertical-equivalent)
#     so interpret's clamped-at-vertical bend reproduces the curl arc (the trunk
#     is vertical -> the hold is preserved).
#   * sympodial forks: leader (apical-dominant, k==0) is an explicit branch();
#     the remaining children-1 laterals FORK with evaluator azimuth spread
#     2pi/(children-1) after a 360/children frame roll (legacy spread 2pi/children).
#   * conifer lateral tips fork uniformly (no k==0 apical carve-out at tip level)
#     and do NOT droop-pitch: legacy tip droop is a world-frame -Y bend preserving
#     the launch azimuth; a local pitch on a fanned child folds the fan inward
#     (halved radial reach, measured). The un-drooped fan matches reach; the
#     residual hang difference is arbitrated by the GPU silhouette gate.
#   * azimuth-jitter SCALE: legacy scales each child's azimuth draw by the
#     divergence angle (div*jit_azimuth*jitter, div=roll -> +-0.42 rad for
#     mesquite); the evaluator's FORK draws unscaled (+-0.175 rad, same params),
#     so preset crowns have tighter per-child azimuth scatter. Width channels and
#     occupied (union) volume are UNAFFECTED — bit-exact / equal-within-seed-noise,
#     see gr1d_evidence/reconcile_volume.md — only layout statistics differ. Fold
#     div-scaled per-child rolls into the grammars (with jit_azimuth as a folded
#     preset parameter + a jit_azimuth=0 module override) if silhouette review
#     ever asks for legacy scatter.
###############################################################################

from ork.hypergraph.dflow import lsystem as L

_R2D = 57.29577951308232

# archetype ids — keep in sync with the Python Archetype IntEnum in the hypermesh DSL
# (plain ints here so presets never import the hypermesh package: no import cycle).
SYMPODIAL, CONIFER, SAGUARO, OCOTILLO = 0, 1, 2, 3

# per-archetype known-good chaos-channel defaults (effective chaos = jitter * jit_X).
# THE preset parameter source for the jit channels (was hypermesh._ARCH_JIT); the DSL's
# jit_* None-fallback reads this table. A channel a given form doesn't use is a no-op.
PRESET_JIT = {
  SYMPODIAL: dict(jit_azimuth=0.5, jit_pitch=0.35, jit_length=0.5, jit_spacing=0.3, jit_drop=0.25, jit_wave=0.25),
  CONIFER:   dict(jit_azimuth=0.8, jit_pitch=0.35, jit_length=0.5, jit_spacing=0.3, jit_drop=0.25, jit_wave=0.25),
  SAGUARO:   dict(jit_azimuth=0.6, jit_pitch=0.35, jit_length=0.5, jit_spacing=0.3, jit_drop=0.25, jit_wave=0.05),
  OCOTILLO:  dict(jit_azimuth=1.2, jit_pitch=0.5,  jit_length=0.5, jit_spacing=0.3, jit_drop=0.25, jit_wave=0.3),
}


###############################################################################
# sympodial — repeated forking (trees, shrubs, cholla). Each generation: a shoot of
# `internodes` segments (radius interpolating rad -> rad*rad_decay), a phyllotactic
# frame roll (roll*gen), then an apical-dominant leader branch + children-1 laterals,
# recursing with len_decay/rad_decay until generation `depth`.
###############################################################################
def sympodial(depth=7, children=2, internodes=1, seed=1, budget=4000, tropism=0.0, **_unused):
  D  = max(1, int(depth))
  C  = max(1, int(children))
  IN = max(1, int(internodes))

  class _Sympodial(L.Lsystem):
    def grammar(self):
      Shoot = self.symbol("Shoot", len=0.0, rad=0.0, gen=0.0)

      @L.rule(Shoot, until=Shoot.gen >= D)
      def grow(S):
        ops = []
        for i in range(IN):                       # the shoot: internode segments, radius tapering
          t = float(i + 1) / IN
          ops.append(L.segment(len=S.len * (1.0 / IN),
                               rad=S.rad * (1.0 - (1.0 - S.rad_decay) * t),
                               gen=S.gen))
        ops.append(L.roll(S.roll * S.gen))        # phyllotaxis: divergence roll accumulates per generation
        child = lambda: Shoot(len=S.len * S.len_decay, rad=S.rad * S.rad_decay, gen=S.gen + 1.0)
        # leader (legacy k==0): continues straighter under apical dominance
        ops.append(L.branch(
            L.pitch(S.branch_angle * (1.0 - S.apical) * (1.0 + S.jit_pitch * S.jitter * S.rnd(-1, 1))),
            child()))
        if C > 1:                                 # laterals: evaluator FORK spreads 2pi/(C-1) + jit_azimuth
          ops.append(L.roll(360.0 / C))           # offset off the leader's azimuth (legacy spread 2pi/C)
          ops.append(L.fork(C - 1, lean=S.branch_angle, then=child()))
        return ops

      self.axiom(Shoot(len=Shoot.seg_len, rad=Shoot.base_radius, gen=0.0))
      self.iterate(depth=D + 2, segment_budget=int(budget))

  _Sympodial.seed = int(seed)
  return _Sympodial(), {}


###############################################################################
# conifer — monopodial: a held-vertical central leader with a whorl of drooping
# laterals per level, branch length shrinking toward the top (conical crown).
# Laterals droop by an explicit per-internode pitch (see PARITY note above) and
# fork once more at their tip (the dense-foliage second generation).
###############################################################################
def conifer(depth=7, children=2, internodes=1, seed=1, budget=4000, tropism=0.0, **_unused):
  W   = max(3, int(depth))                        # whorl count (legacy max(3, depth))
  K   = max(3, int(children))                     # laterals per whorl (legacy max(3, children))
  C2  = max(1, int(children))                     # tip sub-shoots per lateral (legacy sympodial kids)
  IN  = max(1, int(internodes))
  HOLD = 0.10                                     # module-tropism override: leader hold (rad/segment)
  # legacy lateral droop 0.6*|tropism|+0.04 rad/internode; +HOLD compensates the interpret-stage
  # up-bend the hold applies to every segment (same rotation plane -> exact compensation).
  droop_deg = (0.6 * abs(float(tropism)) + 0.04 + HOLD) * _R2D

  class _Conifer(L.Lsystem):
    def grammar(self):
      Whorl = self.symbol("Whorl", w=0.0, wlen=0.0)
      Lat   = self.symbol("Lat",   blen=0.0, brad=0.0)
      Tip   = self.symbol("Tip",   tlen=0.0, trad=0.0)

      @L.rule(Whorl, until=Whorl.w >= W)
      def whorl(S):
        r0 = S.base_radius * (1.0 - S.w * (1.0 / W))          # leader radius: linear base -> 0
        r1 = S.base_radius * (1.0 - (S.w + 1.0) * (1.0 / W))
        ops = []
        for i in range(IN):                                    # leader internode(s), jittered whorl spacing (wlen)
          t = float(i + 1) / IN
          ops.append(L.segment(len=S.wlen * (1.0 / IN), rad=r0 * (1.0 - t) + r1 * t, gen=0.0))
        ops.append(L.roll(S.roll))                             # accumulates roll*w over successive whorls
        # branch length: conical shrink toward the top + per-lateral jit_length (evaluated per FORK child)
        blen = S.seg_len * 2.4 * (1.0 - 0.72 * S.w * (1.0 / W)) * (1.0 + S.jit_length * S.jitter * S.rnd(-1, 1))
        ops.append(L.fork(K, lean=S.branch_angle,
                          then=L.when(S.rnd(0, 1) >= S.jitter * S.jit_drop,    # jit_drop: whorl gaps
                                      Lat(blen=blen, brad=r1 * 0.55))))
        ops.append(Whorl(w=S.w + 1.0,
                         wlen=S.seg_len * (1.0 + S.jit_spacing * S.jitter * S.rnd(-1, 1))))
        return ops

      @L.rule(Lat)
      def lateral(S):
        ops = []
        for i in range(IN):                                    # droop applied per internode (legacy tropism-droop)
          t = float(i + 1) / IN
          ops.append(L.pitch(droop_deg))
          # gen=1.0 stamps the LATERAL (bough) — distinguishes it from the gen-0 leader so a
          # leaf scatter with min_gen 1 clothes the boughs but leaves the trunk clean (needle
          # placement fix, aug09; before this, leader and laterals were both gen 0 and needles
          # either covered the trunk too or existed only on tips).
          ops.append(L.segment(len=S.blen * (1.0 / IN),
                               rad=S.brad * (1.0 - (1.0 - S.rad_decay) * t), gen=1.0))
        # NO roll before the tip fork: legacy's tip-level roll term is div*gen with gen==0,
        # so tips fan from the lateral's outward vertical plane (radial reach depends on it).
        ops.append(L.fork(C2, lean=S.branch_angle,
                          then=Tip(tlen=S.blen * S.len_decay, trad=S.brad * S.rad_decay)))
        return ops

      @L.rule(Tip)
      def tip(S):
        # NO droop pitch at tip level: legacy droop is a WORLD-frame bend toward -Y that
        # preserves the tip's launch azimuth (radial reach); a local-frame pitch on a FANNED
        # child rotates off-plane and folds tips back toward the lateral axis (halved reach,
        # measured in the parity pack). The un-drooped fan is the closer reach approximation;
        # the staged GPU silhouette gate arbitrates the residual droop difference.
        ops = []
        for i in range(IN):
          t = float(i + 1) / IN
          ops.append(L.segment(len=S.tlen * (1.0 / IN),
                               rad=S.trad * (1.0 - (1.0 - S.rad_decay) * t), gen=2.0))
        return ops

      self.axiom(Whorl(w=0.0, wlen=Whorl.seg_len))
      self.iterate(depth=W + 4, segment_budget=int(budget))

  _Conifer.seed = int(seed)
  return _Conifer(), {"tropism": HOLD}


###############################################################################
# saguaro — a thick vertical trunk (children=0 -> barrel) with arms budding from
# upper-middle trunk sites, growing out then curling up. The curl rides the module
# tropism override (clamped-at-vertical, exactly the legacy arm semantics; the
# 0.31 rad/segment constant is the legacy accelerating schedule's mean).
###############################################################################
def saguaro(depth=7, children=2, internodes=1, seed=1, budget=4000, tropism=0.0, **_unused):
  T  = max(8, int(depth))                         # trunk segments (legacy max(8, depth))
  A  = max(0, int(children))                      # arm count
  NA = max(6, T // 2)                             # segments per arm
  # module-tropism override = the arm curl. Legacy runs an accelerating schedule
  # (0.06+0.5*j/N rad/segment); a CONSTANT bend that reaches vertical over the same
  # number of segments needs the schedule's EARLY mean, not its overall mean — 0.31
  # verticalized in ~3 segments (radial extent collapsed); 0.13 matches the legacy
  # arm's reach-vertical arc (calibrated against the replica in the parity pack).
  CURL = 0.13
  sites = {}                                      # trunk segment index -> [arm ordinal a, ...]
  if A > 0 and T > 2:
    for a in range(A):
      frac = 0.45 if A == 1 else 0.38 + 0.30 * a / float(A - 1)
      idx  = min(T - 1, max(1, int(T * frac)))
      sites.setdefault(idx, []).append(a)

  class _Saguaro(L.Lsystem):
    def grammar(self):
      Tr  = self.symbol("Tr", i=0.0)
      Arm = self.symbol("Arm", j=0.0, arad=0.0)

      @L.rule(Tr, until=Tr.i >= T)
      def trunk(S):
        ops = [L.segment(len=S.seg_len,
                         rad=S.base_radius * (1.0 - S.taper * (S.i + 1.0) * (1.0 / T)),
                         gen=0.0)]
        for idx in sorted(sites):
          for a in sites[idx]:
            # arm heading: azimuth roll*a about the trunk (+ FORK jit_azimuth), pitched
            # branch_angle off vertical (+ jit_pitch); thickness = site radius * rad_decay.
            ops.append(L.when(S.i == float(idx),
                L.branch(L.roll(S.roll * float(a)),
                         L.pitch(S.branch_angle * (1.0 + S.jit_pitch * S.jitter * S.rnd(-1, 1))),
                         Arm(j=0.0,
                             arad=S.base_radius * (1.0 - S.taper * ((idx + 1.0) / T)) * S.rad_decay))))
        ops.append(Tr(i=S.i + 1.0))
        return ops

      @L.rule(Arm, until=Arm.j >= NA)
      def arm(S):
        return [L.segment(len=S.seg_len * 0.85,
                          rad=S.arad * (1.0 - S.taper * S.j * (1.0 / NA)),
                          gen=1.0),
                Arm(j=S.j + 1.0, arad=S.arad)]

      self.axiom(Tr(i=0.0))
      self.iterate(depth=T + NA + 4, segment_budget=int(budget))

  _Saguaro.seed = int(seed)
  return _Saguaro(), {"tropism": CURL}


###############################################################################
# ocotillo — many long unbranched whip stems from a common base, splaying outward
# (FORK azimuth spread + lean=branch_angle with jit_pitch = exactly the legacy
# per-stem launch), then curving under the live module tropism + jit_wave.
###############################################################################
def ocotillo(depth=7, children=2, internodes=1, seed=1, budget=4000, tropism=0.0, **_unused):
  Sn = max(8, int(children))                      # stems (legacy max(8, children))
  SN = max(8, int(depth))                         # segments per stem (legacy max(8, depth))

  class _Ocotillo(L.Lsystem):
    def grammar(self):
      Stem = self.symbol("Stem", j=0.0)

      @L.rule(Stem, until=Stem.j >= SN)
      def stem(S):
        return [L.segment(len=S.seg_len,
                          rad=S.base_radius * (1.0 - S.taper * S.j * (1.0 / SN)),
                          gen=0.0),
                Stem(j=S.j + 1.0)]

      self.axiom(L.roll(Stem.roll),
                 L.fork(Sn, lean=Stem.branch_angle, then=Stem(j=0.0)))
      self.iterate(depth=SN + 3, segment_budget=int(budget))

  _Ocotillo.seed = int(seed)
  return _Ocotillo(), {}


_EMITTERS = {SYMPODIAL: sympodial, CONIFER: conifer, SAGUARO: saguaro, OCOTILLO: ocotillo}


def build_preset(archetype, depth=7, children=2, internodes=1, seed=1, budget=4000,
                 tropism=0.0, **_unused):
  """archetype id (0..3) -> (grammar, module_scalar_overrides).

  `grammar` is an Lsystem instance (resolve_grammar/derive() lowers it to the reflected
  LRuleSet). `module_scalar_overrides` maps reflected-scalar names to values the caller
  must poke onto the LSystemModule AFTER the normal kwarg assignment (the per-preset
  remaps documented in the header: conifer/saguaro tropism). Everything else rides the
  PARAM env untouched."""
  fn = _EMITTERS.get(int(archetype))
  if fn is None:
    raise ValueError("unknown L-system archetype preset %r (known: 0 sympodial, 1 conifer, "
                     "2 saguaro, 3 ocotillo)" % (archetype,))
  return fn(depth=depth, children=children, internodes=internodes, seed=seed,
            budget=budget, tropism=tropism)


def preset_lsystem(hm, archetype=SYMPODIAL, **kw):
  """Build an lsystem node on Hypermesh `hm` through a PRESET grammar — the one-call
  form the GR1.d flip routes `H.lsystem(archetype=...)` through (usable standalone
  during the parity window: pre-deletion, passing grammar= already wins over the
  legacy path). Forwards every kwarg to hm.lsystem() unchanged, then applies the
  preset's module-scalar overrides to the skeleton module."""
  grammar, overrides = build_preset(int(archetype),
                                    depth=kw.get("depth", 7),
                                    children=kw.get("children", 2),
                                    internodes=kw.get("internodes", 1),
                                    seed=kw.get("seed", 1),
                                    budget=kw.get("budget", 4000),
                                    tropism=kw.get("tropism", 0.0))
  node = hm.lsystem(archetype=int(archetype), grammar=grammar, **kw)
  for k, v in overrides.items():
    setattr(hm._skeleton, k, float(v))
  return node


__all__ = ["SYMPODIAL", "CONIFER", "SAGUARO", "OCOTILLO", "PRESET_JIT",
           "sympodial", "conifer", "saguaro", "ocotillo",
           "build_preset", "preset_lsystem"]
