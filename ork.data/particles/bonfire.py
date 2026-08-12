###############################################################################
# bonfire.py — A CEREMONIAL BONFIRE for the stone fire pit (bld_firepit).
#
# The read: Beltane / Burning Man scale, not a campfire. A ~5 m turbulent base
# sitting inside the pit's 6 m fire curb, a near-white core low down where the
# fuel is hottest, a column tapering to ~10 m that breaks into rising ember
# streaks, and a smoke plume drifting off the top and leaning with the wind.
#
# FOUR LAYERED SYSTEMS, NOT ONE. Layering is the look strategy AND the lighting
# strategy, because the engine publishes exactly ONE DynamicPointLight per
# particle DRAWABLE — at the sprite renderer's weighted hot-particle centroid
# (sgnode_particles.cpp:80-95). There is no per-particle light: the
# LightRenderer module is an empty stub. So every distinct pool of flickering
# light in this fire is a distinct drawable, and `layers()` below is what a host
# walks to build them.
#
#   core    small, hot, near-white -> deep orange, short life, tight ring, low
#   plume   the column: three concentric rings, flame colour all the way up
#   embers  StreakRenderer, velocity-aligned, sparse, long life, high rise
#   smoke   slow, large, FIRE-LIT head + absorptive tail, off the COLUMN TOP
#
###############################################################################
# THE COLUMN — the technique, and why the obvious levers do not build one.
#
# Recipe applied: the pyro-sim BUOYANT PLUME, expressed as a pair of VELOCITY
# SERVOS plus RADIAL CONFINEMENT — the same decomposition FumeFX / Houdini pyro
# use (buoyancy + drag toward a target rise speed + vorticity confinement to
# keep the column from mushrooming). Everything below is authored as ExprForce
# ExprIR, evaluated per particle on the CPU:
#
#   f_y = k_rise * (v_rise(y) - vel_y)         <- the chimney servo
#   f_x = k_wind * (wind_x   - vel_x) - c(y)*(pos_x - axis_x)
#   f_z = k_wind * (wind_z   - vel_z) - c(y)*(pos_z - axis_z)
#
# WHY A SERVO AND NOT Drag + DirectionalForce (the round-1 mistake):
#
#   * Drag (modules_force_drag.cpp) multiplies velocity by a constant PER TICK
#     and ignores dt entirely. Its time constant is therefore dt/(1-drag) —
#     frame-rate dependent. Offscreen runs at several hundred fps, where
#     drag=0.99 is a 0.33 s time constant and a 3 m/s^2 buoyancy saturates at
#     ~1 m/s. That is a glow pool in the hearth, not a 10 m column, and it would
#     look completely different again at 60 fps. Drag is now UNUSED here.
#   * A bare DirectionalForce with no drag gives h = 0.5*a*t^2. The median
#     height of a pool under constant acceleration is h(L)/4 — three quarters of
#     the particles sit in the bottom quarter of the column. Also a glow pool.
#
# The servo integrates as an exponential approach: v(t) = v_rise*(1 - e^(-k t)),
# h(t) = v_rise*(t - tau*(1 - e^(-t/tau))) with tau = 1/k. After ~2 tau the
# particle is at CONSTANT rise speed, so the pool is distributed UNIFORMLY in
# height — a column, not a puddle — and the shape is identical at 60 fps and at
# 600 fps because the whole thing is acceleration * dt.
#
#   PLUME  v_rise 3.40 m/s, k 4.0 (tau 0.25 s), life 3.20 s
#          h(L) = 3.40 * (3.20 - 0.25) = 10.03 m   <- the 10 m target
#          h(L/2) = 3.40 * (1.60 - 0.25) = 4.59 m  <- half the height at half
#                                                     the life: uniform fill
#          (ROUND 5 INVERTS THIS. The three rings are now RELEASED AT THREE
#           HEIGHTS around the pyre, so the authored number is the TIP
#           altitude and each ring's life is derived — see plume_seating.)
#   CORE   v_rise 2.20, k 5.0 (tau 0.20), life 0.95 s -> 1.65 m, released at
#          0.85 m so it CLEARS the pit curb from a 1.7 m eye (see the layer)
#   EMBERS v_rise 4.30 falling off above 6 m, k 3.0, life 3.60 s -> ~13 m
#   SMOKE  released AT 8.8 m, v_rise 0.95 -> 0.29, wind tau 0.55 s: it bends
#          over into a downwind trail INSIDE the frame instead of climbing out
#          of it (round 2's 2.20 m/s rise put every visible sprite above 11 m,
#          which is where both camera framings end)
#
# RADIAL CONFINEMENT is what tapers the column. c(y) ramps in with height, and
# the horizontal servo term doubles as its damper: the pair is an overdamped
# spring (k_wind^2 > 4c), so a particle drifts inward smoothly instead of
# oscillating across the axis. Quasi-static inward speed is c*r/k_wind, e.g.
# c=1.10, r=2.35, k_wind=2.6 -> 0.99 m/s of pull-in from 4.2 m up: a 4.7 m
# base narrowing to ~2 m by 10 m. k_wind^2 = 6.8 > 4c = 4.4, so overdamped.
#
###############################################################################
# EMITTER TRAP — DispersionAngle is NOT an angle, and Direction does not aim.
#
# particle_emitters.cpp:180 does `yo.lerp(dir, disp, ctx.mDispersion)`, and
# fvec3::lerp CLAMPS its parameter to [0,1]. So the engine-wide idiom of
# `DispersionAngle = 45` is really `= 1.0`: full blend to the dispersion vector.
# The nozzle emitter's plug even declares _range = {0,1}; the ring emitter's
# does not, which is how "degrees" got into the corpus.
#
# Worse for a fire: the ring emitter hardcodes EmitterDirection::CONSTANT
# (modules_emitter_ring.cpp:160), so the emit direction it hands EmitCB is the
# RING TANGENT — horizontal — and the dispersion basis is (plane_normal,
# bitangent), i.e. +-Y and horizontal in equal measure. A RingEmitter with
# Direction=(0,1,0) therefore CANNOT emit upward as a body no matter what
# DispersionAngle says: at 0 it fires horizontally, at 1 it fires half up and
# half down.
#
# So EmissionVelocity here is deliberately SMALL (0.4-0.9 m/s) and read purely
# as ignition boil at the fuel bed. ALL of the rise comes from the servo, which
# is the only lever that actually aims.
#
# AND: a RingEmitter WITH EmitterSpinRate LEFT AT ITS 0 DEFAULT IS NOT A RING.
# computePosDir places each particle at ring phase `mfPhase`, and mfPhase only
# ever advances by EmitterSpinRate (modules_emitter_ring.cpp:170). At the
# default 0 the phase is pinned, so every particle in the pool spawns at ONE
# POINT — tangent * radius, i.e. +X for the conventional Tangent=(1,0,0). The
# symptom is a fire that is both too narrow and visibly shoved to one side of
# its own pit, which is exactly what the mid-round-2 renders showed.
#
# EmitterSpinRate is in REVOLUTIONS/SEC and every layer here sets it. Mind the
# floor: _emit() computes fadaptive = clamp(|dphase|/dt, 0.1, 1.0) and
# multiplies the emission rate by it, so a spin rate below 1/(2*pi) = 0.159
# rev/s silently THROTTLES EMISSION to as little as 10%. The rates below are
# 1.7-3.1 rev/s, mutually incommensurate so the instances do not spiral in
# lockstep.
###############################################################################
#
# PROCEDURAL FRAGMENTS, NO FLIPBOOK. Every layer's cookie is authored in the
# ptex3d expression DSL (P.fbm over the quad uv, scrolled by unit_age, offset by
# the per-particle hash): continuous, no cell popping, no sheet to author, and
# per-particle variation is free.
#
# SOFT-PARTICLE DEPTH FADE is ON for every layer that touches stone — that is
# what stops the 5 m base showing a hard intersection edge against the hearth
# floor and the curb. It needs the depth prepass; without it the material says
# so by name and the fade is a no-op (never a crash).
#
# MESH-FRAME CONTRACT: all offsets below are in the pit's frame, where y = 0 is
# the HEARTH FLOOR plane and the floor slab's surface is at y = floor_top (0.15).
# The fire curb is a 6 m ring, top at y = 0.67 — the ~5 m base clears it.
#
# A8: every coefficient above is a constructor kwarg, and the servo coefficients
# land in the module's REFLECTED force_{x,y,z} ExprIR trees — authored data that
# round-trips through print_force/parse_force and is editable in the propsheet,
# not a folded constant. The master multiplier is a real float plug (Strength).
#
#   ork.particle.viewer.py bonfire            # the plume layer alone
#
###############################################################################
# ROUND-3 HEADLINE — WHITE IS A DENSITY, NOT AN INTENSITY (under ACES).
#
# The compositor tonemaps with ACES at exposure ~1.1, which needs a LINEAR
# radiance of roughly 8-16 to reach display white. No single additive sprite
# gets near that; the white in this fire is always a STACK. Two consequences
# that cost most of round 3 to learn, so they are written down here:
#
#  * Round 2's white plateau was not the core layer. It was the PLUME's 15-30
#    deep additive stack whitening because its weakest channel (blue, 0.154 at
#    the hottest stop) saturated at a stack depth of ~7. Raising saturation is
#    therefore the containment lever: it costs no energy and multiplies the
#    stack depth the colour survives. See the plume block.
#  * Conversely, once the plume was contained, raising the CORE's per-sprite
#    intensity 3x barely moved the clipped area — it just made a few sprites
#    speckle. A compact white heart is made by putting MORE sprites in a
#    SMALLER volume (rate 180 -> 520 in a 0.46 m ring) at moderate intensity.
#    Density clips smoothly; intensity clips in dots.
#
# HEAT SHIMMER (the "heat" aux channel) — WHICH LAYERS, AND WHY THE GATE.
#
# A host that declares aux_channels=["heat"] + a PostFxNodeHeatDistort gets
# nothing at all unless a material's fragment class QUERIES ctx.is_heat: the
# query is the opt-in, and a class that never asks generates no heat technique
# pair and is skipped by the aux pass (fragment.py:72-77, 256-258). So the
# opt-out is silent, which is why each layer's choice is written down:
#
#   plume  YES  heat 0.10, gate 0.60 -> shimmer over 5.7-10 m, the upper column
#   core   NO   DON'T DISTORT A CLIPPED REGION. The core is the one blown-out
#               white area in frame and it sits right on the curb's hard edge;
#               the postfx's chroma term samples r and b at different offsets,
#               so across that boundary the clipped white splits into a BLUE
#               SMEAR that reads as a rendering fault, not as heat. Its gated
#               band was only 1.9-2.65 m, which the plume covers anyway.
#   embers NO   sparse fast specks; the postfx differences the heat field over
#               12 texels, so specks boil instead of shimmering (see EmberFrag)
#   smoke  NO   wrong place (8.8-30 m, cooled and drifted), nothing behind it
#               to refract, and it would smear its own authored silhouette
#
# So the plume — and only the plume — writes heat. Both other flame-coloured
# layers keep the knob, at 0.
#
# THE AGE GATE IS THE WHOLE TECHNIQUE, not a taste knob (fireball.py:52-64 is
# the shipped precedent, at 65% of life). Heat haze is the rising hot AIR above
# the burning surface; a shimmer painted ON the flame body reads as a wobbling
# texture, and worse, the aux RTG carries no depth attachment so the pass
# depth-clips MANUALLY against the scene — an ungated blob sitting on the pit
# curb gives a hard clip edge that crawls. Gated on unit_age, the shimmer only
# exists once the servo has carried the particle clear of the stone.
#
# The gain and both gate thresholds are constructor kwargs plumbed to layers()
# like every other fragment-side number here — the fragment DSL has no
# ctx.param yet (fragment.py:216-219 raises on one), so the material's own
# reflected knobs are the only live uniforms a fragment can read, and none of
# them means "shimmer". The live runtime knobs for this effect are the postfx
# node's reflected `strength` and `chroma`.
#
# And the composition trap that hid the result: from a 1.7 m eye at 15 m the
# PIT'S OWN CURB cuts the sight line to the fire axis at ~1.3 m. A core seated
# on the hearth slab is invisible in the one view that matters, however hot it
# is. The core is now released at 0.85 m and spans 1.0-2.65 m.
#
###############################################################################
# ROUND-4 HEADLINE 1 — PUFFING. A BIG FIRE BREATHES; A SMALL ONE FLICKERS.
#
# Round 3 was a smooth, continuous column: it never necked, never puffed, never
# detached a tongue, and that is why it read as a campfire enlarged rather than
# as something enormous. The missing cue is the one that carries SCALE:
#
#   A pool fire is buoyancy-unstable. A toroidal vortex forms at the base and
#   sheds at f ~= 1.5/sqrt(D) Hz (D = pool diameter, the standard correlation).
#   Our D is ~5 m -> f = 0.67 Hz: a puff every 1.5 s. A 0.3 m campfire sheds at
#   2.7 Hz and reads as FLICKER; a 5 m fire reads as BREATHING. The frequency
#   IS the size cue, and no amount of detail substitutes for it.
#
#   Above the persistent luminous zone at the fuel bed sits the INTERMITTENT
#   zone, where the flame tip appears and disappears, and above that the plume
#   zone with no combustion at all (the three-zone turbulent diffusion flame).
#   Flame height must therefore FLUCTUATE substantially — the intermittency is
#   the subject, not noise on a constant.
#
# FOUR MECHANISMS, all of them functions of SIMULATION TIME (Expr.time, the
# Globals RelTime output — seconds since the graph's first compute), never of
# tick count, so the breathing is frame-rate independent exactly as the servo
# silhouette is:
#
#  1. EMISSION-RATE BURSTS (the density wave). EmissionRate is a real plug and
#     Expr chain-lowers sin/scale/bias/power onto it, so the rate becomes
#     rate * (1 - A + A*p/mean(p)), p = ((1+sin(wt+phi))/2)^shape. A cohort born
#     in a burst RISES TOGETHER: the density wave is advected by the servo and
#     reads as a bulge climbing the column, with a sparse neck behind it. This
#     is the pulsed-emitter trick every shipped puffing-fire effect uses, and it
#     is the only one that produces travelling structure rather than a global
#     throb.
#  2. COHORT LIFESPAN (the intermittency). LifeSpan is read once per emit tick
#     and STAMPED PER PARTICLE (particle_emitters.cpp:172), so modulating it in
#     phase with the rate makes each puff burn to a different height:
#     h(L) = v*(L - tau), so L 3.20 +-40% is a tip wandering 6.1-13.9 m. That is
#     the flame-height fluctuation the three-zone model is about.
#  3. A SHED VORTEX RING (the neck). One more ExprForce whose Strength plug is a
#     bare sin(wt+phi) — a signed, zero-mean band-limited kick low in the column:
#     +y and OUTWARD on the crest (the ring inflates and accelerates), -y and
#     INWARD on the trough (the column necks). ExprIR has no time symbol and the
#     servo's own Strength is shared with wind and confinement, so this is a
#     separate module by necessity as well as by clarity.
#  4. PER-PARTICLE RISE SPREAD + CONFINEMENT ESCAPE (the detached tongues).
#     v_rise gains a +-spread draw off PF.random, and a small tail of the pool
#     (random >= escape_q) loses most of its radial confinement, so it is NOT
#     pulled back into the chimney at the neck and separates as a tongue.
#
# POOL MATH — WHY THE RATE IS NORMALIZED. Live population is the emission rate
# integrated over the lifespan window, so modulating rate and life IN PHASE
# raises the MEAN population by A*B*var(p)/mean(p)^2. At the shipped A .90 /
# B .40 that is +16%, which overflows the host's pinned 900-slot plume pool and
# CLIPS THE PEAKS — a full pool silently drops the emit, i.e. it eats exactly
# the bursts the effect is made of. Offline population sim, inner ring, 28 s:
#   uncorrected  mean 862  peak 900 (AT THE CAP)  1822 dropped emits
#   corrected    mean 743  peak 784               0 dropped
# 743 is round 3's live count exactly, so the fire is not dimmed by this pass —
# the same photons are REDISTRIBUTED IN TIME.
#
# PHASE ACROSS THE THREE RINGS. One fire sheds ONE vortex, so the rings share a
# frequency; but they are not in lockstep, and the offset is not a hash — the
# ring expands OUTWARD from the axis, so each instance's phase lags by
# `base_r / puff_wave_speed` seconds. At the shipped 20 m/s that is 7/18/28 deg
# across the 0.60/1.50/2.35 m rings: enough that the crest visibly rolls
# outward, little enough that the fire still breathes as one body. The embers
# and the smoke carry an explicit LAG in seconds instead (the smoke's release is
# 8.8 m up the column — the puff that made it left the fuel bed ~2.6 s earlier).
#
###############################################################################
# ROUND-4 HEADLINE 2 — SUBSTANCE. AN ERODED COOKIE, NOT A SOFT ONE.
#
# Round 3's cookie was `edge^2 * (bias + gain*fbm)` — strictly POSITIVE
# everywhere inside the quad. A sum of positive blobs is a solid mass, which is
# exactly how it read: too opaque, too smooth, one continuous body of
# yellow-orange with no way to see into or through it.
#
# The recipe is the standard one for burning gas (Houdini/UE alike): DISSOLVE BY
# THRESHOLD, with the threshold rising over life.
#
#   shape = saturate((detail - thr) / (1 - thr)),  thr = mix(erode0, erode1, age)
#
# Where detail < thr the sprite is EXACTLY ZERO — a real hole, not a dim patch —
# and the surviving lobes keep FULL brightness (the remap divides by the
# headroom instead of multiplying it away). thr rising from 0.24 to 0.60 over
# life is the three-zone flame in cookie form: nearly solid at the fuel bed,
# shredding into filaments as it climbs, gone at the tip.
#
# EROSION EATS THE SPARSE RINGS FIRST — the trap that cost a round. 0.30 -> 0.66
# was the first try, and it narrowed the fire to a spire: the outer concentric
# rings carry fewer and smaller sprites, so a threshold that is uniform in the
# COOKIE is not uniform in the COLUMN — it dissolves the shoulders before the
# core. Back the YOUNG threshold off and raise the continuous floor (bias
# 0.18 -> 0.26) before reaching for more erosion.
#
# Two more terms, both cheap:
#   * RIDGED FOLD. `1 - |2n-1|` turns fbm's smooth hills into thin ridges at its
#     mean crossings — the classic ridged-multifractal move, and the cheapest
#     source of FILAMENT structure there is. Blended by `ridge`, not replacing
#     the fbm, so the lobes keep a body.
#   * fbm_aa INSTEAD OF fbm. Round 3 stopped at freq 6.4 x 5 octaves because the
#     finest octave was going sub-pixel and would alias in motion. fbm_aa
#     band-limits each octave against its own screen footprint (dsl.py:367-387),
#     fading a sub-pixel octave to its MEAN instead of letting it crawl — so the
#     detail ceiling moves up instead of being paid for in shimmer.
#
# A DARKER INTERIOR needs occlusion, and additive-with-alpha-0 has none by
# construction. The plume's last third now carries a small SOOT ALPHA (peak
# 0.030 at unit_age 0.90): sorted back-to-front, the near filaments attenuate
# the far ones, which is what gives the column an inside. Kept small on purpose
# — round 1's note that 0.03 per sprite accumulates to a solid mass is still
# true; what makes it safe here is that erosion has cut the number of sprites
# that are dense at any pixel, and that only the OLDEST (highest, sparsest)
# third carries any alpha at all.
#
# COLOUR RANGE. The ramp gets a deep-red/maroon shoulder above the yellow
# (hsv 7 -> 4 -> out) and a per-particle RAMP OFFSET (`ramp_var`): each sprite
# reads the gradient at unit_age + var*(random-0.5), so neighbours differ in
# colour temperature instead of marching through the ramp in unison. Under an
# additive stack that mottling is most of what "more range than yellow->orange"
# actually looks like.
###############################################################################

from math import comb as _comb, pi as _pi

from orkengine.core import vec3, vec4, dataflow, CrcStringProxy
from orkengine.lev2 import particles

from ork.hypergraph.colors import hsv

from ork.hypergraph.dflow.particles import ParticleSystem, FreestyleFragment, materialize_fragment
from ork.hypergraph.dflow import particles as P
from ork.hypergraph.dflow import Expr as E
from ork.hypergraph.dflow.particles.exprir_particles import PF
from ork.hypergraph.ptex3d import P as F   # GLSL expression ops (fragment DSL)

tokens = CrcStringProxy()

# hearth-floor plane of bld_firepit: the fire's y origin (mesh y=0 is the hearth
# base; floor_top is the slab surface the fuel would actually sit on).
HEARTH_Y = 0.15


###############################################################################
# FRAGMENTS — procedural cookies over the ptex3d IR.
###############################################################################

def _flame_mask(ctx, *, freq, scroll, gain, bias, octaves, seed,
                aa=1.0, ridge=0.0, erode=None, erode_soft=0.16, detail=1.0):
  """Ragged radial cookie in [0,1] -> (body, mask).

  `body` is the round-3 SMOOTH cookie: strictly positive inside the quad, the
  low-frequency field the heat channel wants (a shimmer must never carry the
  erosion's high frequencies — the distortion postfx differences the heat RT
  over a 12-texel step and turns fine structure into boiling).

  `mask` is the RENDERED cookie: the same field DISSOLVED by a threshold that
  rises over life (`erode` = (young, old)). Where the detail field falls below
  the threshold the result is exactly 0 — a see-through gap — and where it
  survives it keeps full brightness, because the remap divides by the remaining
  headroom instead of multiplying the lobe away. `ridge` folds in
  `1 - |2n-1|` (ridged multifractal) for filament structure; `detail` is the
  weight of the eroded term against `bias`'s continuous floor.

  `octaves` is an fBm LOOP BOUND — structural, so it bakes; everything else
  rides a plug/uniform. fbm_aa (not fbm) band-limits each octave against its own
  screen footprint, which is what lets the frequency go up without the finest
  octave crawling in motion."""
  uv = ctx.uv
  d  = F.length(uv - 0.5) * 2.0                       # 0 at centre, 1 at the inscribed edge
  # per-particle field offset: random decorrelates sprites, age scrolls upward
  n = F.fbm_aa(F.vec3(uv.x * freq,
                      uv.y * freq - ctx.unit_age * scroll,
                      ctx.random * seed), octaves=octaves, aa=aa)
  edge = F.saturate(1.0 - d)
  body = F.saturate(edge * edge * (bias + gain * n))
  if erode is None:
    return body, body
  # ridged fold: thin bright filaments where the fbm crosses its own mean.
  det = F.mix(n, 1.0 - F.abs(n * 2.0 - 1.0), ridge)
  # the rim is folded INTO the threshold input, not multiplied after it, so the
  # silhouette dissolves raggedly instead of keeping a smooth gaussian collar.
  det = det * F.saturate(1.28 - d)
  thr = erode[0] + (erode[1] - erode[0]) * ctx.unit_age
  shape = F.saturate((det - thr) / F.max(1.0 - thr, 0.06))
  mask = F.saturate(edge * edge * (bias * gain * n + detail * shape))
  return body, mask


class FlameFrag(FreestyleFragment):
  """Core / plume / smoke cookie. PREMA: rgb ADDS, a OCCLUDES — the gradient
  ramp carries both, the mask shapes them together. `soft` opts into the
  soft-particle depth fade (the whole premultiplied output scales, or the
  additive term would survive the fade and the contact edge with it).

  `sharpen=(lo,hi)` remaps the mask through a smoothstep, which converts the
  cookie's soft radial falloff into a DEFINED, ragged boundary. Flame layers
  leave it off (a flame sheet has no edge); the smoke turns it on, because a
  plume that reads as a shape against the sky needs a silhouette and not a
  gaussian haze — the standard billow-cookie contrast trick.

  HEAT (aux channel). `heat > 0` opts this material into the "aux_heat" pass:
  the class traces two MORE times with ctx.is_heat True, and those bodies
  write shimmer intensity into .r of the aux RT (additively; the PREMA colour
  contract does not apply there). `heat = 0` never QUERIES ctx.is_heat, so no
  heat techniques are generated at all and the material is skipped by the aux
  pass entirely — that is the opt-out, and it keeps the generated shader text
  byte-identical for the layers that stay out (fragment.py:72-77, 256-258).

  The shimmer is the SAME cookie mask, gated on unit_age — the fireball
  recipe (fireball.py:52-64). Heat haze is the rising hot AIR above the
  burning surface, not the luminous surface itself, and the gate is what
  keeps it there: without it the distortion crawls on the flame body and on
  the stone right behind it, where the aux pass's manual depth clip against
  the curb makes a hard edge that wobbles. Gated, the shimmer only appears
  once a particle has climbed clear."""

  def __init__(self, ctx, *, freq=3.0, scroll=1.6, gain=0.95, bias=0.45,
               octaves=3, seed=41.0, soft=True, streak_taper=0.55,
               sharpen=None, aa=1.0, ridge=0.0, erode=None, detail=1.0,
               ramp_var=0.0, puff_hot=0.0,
               heat=0.0, heat_gate=(0.62, 0.70), heat_out=(1.00, 0.82)):
    body, mask = _flame_mask(ctx, freq=freq, scroll=scroll, gain=gain,
                             bias=bias, octaves=octaves, seed=seed,
                             aa=aa, ridge=ridge, erode=erode, detail=detail)
    if sharpen is not None:
      mask = F.smoothstep(sharpen[0], sharpen[1], mask)
    # THE COHORT HEAT STAMP. aux.x carries the value of the puff wave AT THIS
    # PARTICLE'S BIRTH (the emitter re-pulls the Aux plug per particle through
    # mPerParticleAux, so a time-only expression stamps the whole tick's
    # cohort), i.e. 1 for a particle shed on the crest and 0 for one that
    # trickled out during the lull. A puff is a fuel-rich parcel: it burns
    # HOTTER, not merely denser, and the eye reads brightness long before it
    # reads density. Without this the shed shows up in an area/height plot and
    # barely at all on screen — measured: rate+lifespan modulation alone moved
    # the flame-height standard deviation from 0.13 to 0.22 m while the frame
    # still looked like a steady column.
    hot = 1.0
    if puff_hot > 0.0:
      hot = 1.0 - puff_hot + 2.0 * puff_hot * ctx.aux.x
    # `heat > 0.0 and ...` SHORT-CIRCUITS: ctx.is_heat is a property whose
    # getter is the opt-in, so a zero-heat layer must never reach it.
    #
    # THE HEAT FIELD READS `body`, NOT `mask` — the SMOOTH cookie, never the
    # eroded one. Heat haze is a low-frequency refraction field; the postfx
    # differences the aux RT over a 12-texel step, so feeding it the dissolve's
    # holes and filaments would make the shimmer boil instead of drift (the same
    # reason the ember layer stays out of the channel entirely).
    if heat > 0.0 and ctx.is_heat:
      h = body * heat \
          * F.smoothstep(heat_gate[0], heat_gate[1], ctx.unit_age) \
          * F.smoothstep(heat_out[0], heat_out[1], ctx.unit_age)
      self.output(F.vec4(h, 0.0, 0.0, h))
      return
    # PER-PARTICLE RAMP OFFSET: neighbours read the gradient at slightly
    # different ages, so the stack mottles in colour temperature instead of
    # marching through the ramp in unison. This is most of what widens the
    # perceived colour range under an additive stack.
    # The stamp also SLOWS the gradient for a crest particle (it reads the ramp
    # at 0.85*age, i.e. it stays in the hot stops ~18% longer) — a puff is
    # hotter for longer, not just brighter for an instant.
    t_ramp = ctx.unit_age
    if ramp_var > 0.0:
      t_ramp = t_ramp + ramp_var * (ctx.random - 0.5)
    if puff_hot > 0.0:
      t_ramp = t_ramp * (1.0 - 0.35 * puff_hot * (ctx.aux.x - 0.5))
    ramp = ctx.ramp(F.saturate(t_ramp)) if (ramp_var > 0.0 or puff_hot > 0.0) \
           else ctx.ramp()
    if ctx.is_streak:                                  # TRACE-TIME divergence
      mask = mask * F.smoothstep(1.0, streak_taper, ctx.uv.y)
    rgb = ramp.xyz * ctx.color_factor * ctx.modcolor.xyz * mask * hot
    a   = F.saturate(ramp.w * mask * ctx.alpha_factor * ctx.modcolor.w)
    if soft:
      k = ctx.soft_fade()
      self.output(F.vec4(rgb.x * k, rgb.y * k, rgb.z * k, a * k))
    else:
      self.output(F.vec4(rgb.x, rgb.y, rgb.z, a))


class EmberFrag(FreestyleFragment):
  """Ember streak: a hot point that fades along the trail. Deliberately NOT
  fbm-bitten — an ember is a spark, not a flame sheet; the shape is a tight
  radial core plus a tail ramp, with the per-particle hash driving only its
  brightness so a few embers read much hotter than the rest.

  NO HEAT: this class deliberately never queries ctx.is_heat, so no heat
  technique pair is generated and the aux pass skips the material outright.
  An ember is a 4 cm x 30 cm streak moving several m/s — a per-frame speckle
  field. The distortion postfx differences the heat RT over a 12-texel step
  (heatdistort.fxv2:59) and refracts by the gradient, so sparse fast specks
  produce a boiling salt-and-pepper warp rather than a shimmer. Heat haze is
  a LOW-frequency field; only the broad, slow layers may write to it."""

  def __init__(self, ctx, *, tail=0.15, core=0.55, spark_var=0.65, soft=False):
    uv   = ctx.uv
    d    = F.abs(uv.x - 0.5) * 2.0                     # across the streak
    body = F.saturate(1.0 - d)
    body = body * body * body                          # tight core
    tailk = F.smoothstep(1.0, tail, uv.y)              # head bright, tail gone
    hot   = 1.0 - spark_var + spark_var * ctx.random   # per-ember brightness draw
    mask  = F.saturate(body * tailk * (core + (1.0 - core) * hot))
    ramp  = ctx.ramp()
    rgb   = ramp.xyz * ctx.color_factor * ctx.modcolor.xyz * mask * hot
    a     = F.saturate(ramp.w * mask * ctx.alpha_factor * ctx.modcolor.w)
    if soft:
      k = ctx.soft_fade()
      self.output(F.vec4(rgb.x * k, rgb.y * k, rgb.z * k, a * k))
    else:
      self.output(F.vec4(rgb.x, rgb.y, rgb.z, a))


###############################################################################
# shared material construction
###############################################################################

def _material(frag_cls, stops, *, intensity, alpha=1.0, soft_fade=0.0,
              tint=None, smoothing=0.18, body_bias=0.5, blending=None,
              depthtest=None, **frag_params):
  mtl = particles.FreestyleParticleMaterial.createShared()
  mtl.shader_path = materialize_fragment(frag_cls, **frag_params)
  mtl.blending  = blending if blending is not None else tokens.PREMA
  mtl.depthtest = depthtest if depthtest is not None else tokens.LEQUALS
  # gridDim 1 = the cookie is procedural; there is no sheet and no cell select.
  # ColorMap is still DECLARED by the shared fragment interface even though no
  # layer samples it — bind a real 1-texel-ish asset so the sampler is never null.
  mtl.texture_asset = "src://effect_textures/white_64"
  mtl.gridDim = 1
  mtl.colorIntensity = intensity
  mtl.alphaIntensity = alpha
  mtl.soft_fade_distance = float(soft_fade)
  mtl.emission_smoothing = float(smoothing)
  # emission_lum_power p: the light centroid weight is lum^p * (1-occlusion)
  # (modules_renderer_sprite.cpp:146-150). p > 1 concentrates the weight on the
  # BRIGHTEST (= youngest = LOWEST) particles, which is how a 10 m column still
  # parks its light down at the fuel bed where a fire's light really comes from.
  mtl.emission_lum_power = float(body_bias)
  mtl.emission_tint = tint if tint is not None else hsv(26, 0.62, 1.0)
  mtl.gradient.setColorStops(stops)
  return mtl


def _curve(points):
  """multicurve over unit_age from [(t, v), ...] (>= 2 points)."""
  mc = dataflow.floatxf.multicurve().multicurve
  for _ in range(len(points) - 2):
    mc.splitSegment(0)
  for i, (t, v) in enumerate(points):
    mc.setPoint(i, float(t), float(v))
  return mc


###############################################################################
# THE SERVO — the one force block every layer shares.
###############################################################################

def _servo(upstream, *, name,
           rise, rise_k,
           rise_falloff=(1e9, 1e9), rise_falloff_amt=0.0,
           wind=vec3(0, 0, 0), wind_k=2.0, wind_shear=None,
           confine=0.0, confine_band=(1.0, 8.0),
           rise_spread=0.0, escape_q=1.0, escape_amt=0.0,
           axis=(0.0, 0.0),
           strength=1.0):
  """Chimney servo + sheared wind servo + radial confinement, as one ExprForce.

    rise            target rise speed (m/s) near the fuel bed
    rise_k          1/tau of the rise servo (s^-1); tau = ramp-up time
    rise_falloff    (y_lo, y_hi) band over which the target rise decays
    rise_falloff_amt fraction of `rise` lost by y_hi (0 = constant rise)
    wind            target horizontal VELOCITY (m/s), not an acceleration —
                    the lean saturates instead of accelerating quadratically
    wind_k          1/tau of the horizontal servo; also the confinement damper
    wind_shear      (y_lo, y_hi) BOUNDARY-LAYER profile: the wind target ramps
                    from 0 at y_lo to full at y_hi. Without it the whole column
                    translates downwind including its base, which reads as the
                    fire sitting off-centre in its own pit; with it the base
                    stays put and only the upper column leans, which is what a
                    ground boundary layer actually does.
    confine         peak inward spring coefficient (s^-2)
    confine_band    (y_lo, y_hi) over which confinement ramps in — the taper
    rise_spread     +-fraction drawn PER PARTICLE (PF.random) on the rise
                    target. The tip of a real flame is the few fast parcels,
                    not the mean one: at 0.30 the fastest decile reaches ~1.3x
                    the median height, which is what makes the tip come and go
                    instead of standing at one altitude.
    escape_q        the random quantile ABOVE which a particle stops being
                    confined (1.0 = nobody escapes). PF.step(q, random) is 1
                    for random >= q, so escape_q 0.86 frees the top 14%.
    escape_amt      fraction of the confinement those escapees lose. They are
                    not pulled back into the chimney at the neck, so they
                    separate — the DETACHED TONGUES the smooth column never had.
    axis            (x, z) of the column axis in the drawable's frame
  """
  y = PF.pos_y
  v_target = rise * (1.0 - rise_falloff_amt *
                     PF.smoothstep(rise_falloff[0], rise_falloff[1], y))
  if rise_spread > 0.0:
    v_target = v_target * (1.0 + rise_spread * (PF.random * 2.0 - 1.0))
  fy = (v_target - PF.vel_y) * rise_k

  if wind_shear is not None:
    shear = PF.smoothstep(wind_shear[0], wind_shear[1], y)
    wx, wz = shear * wind.x, shear * wind.z
  else:
    wx, wz = wind.x, wind.z

  fx = (wx - PF.vel_x) * wind_k
  fz = (wz - PF.vel_z) * wind_k
  if confine > 0.0:
    c = PF.smoothstep(confine_band[0], confine_band[1], y) * confine
    if escape_amt > 0.0 and escape_q < 1.0:
      c = c * (1.0 - escape_amt * PF.step(escape_q, PF.random))
    fx = fx - (PF.pos_x - axis[0]) * c
    fz = fz - (PF.pos_z - axis[1]) * c

  return P.ExprForce(upstream, name=name, fx=fx, fy=fy, fz=fz, strength=strength)


###############################################################################
# THE PUFF — the buoyant-plume shedding instability, as SIM-TIME expressions.
#
# Everything here is a function of Expr.time (Globals RelTime, seconds since the
# graph's first compute), never of tick count, so a puff lands at the same
# SIMULATION second at 60 fps and at 600 fps — the same contract the servo's
# acceleration*dt integration keeps for the silhouette.
#
# The wave is ((1 + sin(wt + phi)) / 2)^shape: a unit-amplitude burst whose duty
# cycle narrows as `shape` rises (shape 3 -> a sharp crest and a long lull,
# which is what a shed vortex looks like in a density trace; shape 1 is a bare
# sinusoid and reads as a wobble). Its period mean is the central binomial
# C(2k,k)/4^k, and dividing by that mean is what keeps the MODULATED plug's mean
# equal to the unmodulated value — a puff must not be a stealth rate rise.
###############################################################################

def _puff_mean(shape):
  """Period mean of ((1+sin)/2)^shape — the central binomial C(2k,k)/4^k."""
  return _comb(2 * shape, shape) / float(4 ** shape)


def _puff_wave(freq, phase, *, shape=3):
  """A [0,1] burst wave over SIM TIME. Chain-lowerable end to end: time ->
  scale(w) -> bias(phi) -> sine -> scale(.5) -> bias(.5) -> power(shape), i.e.
  it lands as floatxf stages on the target plug (reflected, propsheet-visible,
  A8-live) rather than as a bytecode module."""
  s = E.sin(E.time * (2.0 * _pi * freq) + phase) * 0.5 + 0.5
  return s if shape == 1 else E.pow(s, shape)


def _puff_mod(base, freq, phase, amp, *, shape=3, norm=1.0):
  """`base` modulated by the puff wave, mean-preserving:
        v(t) = base * norm * (1 - amp + amp * wave / mean(wave))
  amp 0 -> the constant `base`; amp 1 -> a plug that goes to zero between
  puffs. `norm` is the extra correction a caller applies when TWO plugs are
  modulated in phase (see _puff_norm)."""
  if amp <= 0.0:
    return base * norm
  m = _puff_mean(shape)
  w = _puff_wave(freq, phase, shape=shape)
  return w * (base * norm * amp / m) + base * norm * (1.0 - amp)


def _sweep_radius(base_r, amt, freq, phase=0.0):
  """A RING EMITTER IS A CIRCLE OF SPRITES, AND A CIRCLE HAS A HOLE IN IT.

  Round-4's smoke released 34 sprites/s onto a FIXED 1.80 m circle at a FIXED
  8.8 m, with nothing per-particle to break it up — so the release read as a
  grey donut hanging in the sky, visible in every frame of the gallery strip.
  A 2.90 m sprite is 1.74 m wide at birth (the size curve opens at 0.60), which
  is narrower than the 3.6 m ring it is laid on: the middle never gets covered.

  THE FIX IS THE ENGINE'S OWN BATCH LERP, not a new module. computePosDir
  interpolates each emitted particle's radius between mfLastRadius and
  mfThisRadius across the batch (modules_emitter_ring.cpp:196), so a radius
  that MOVES between ticks spreads that tick's cohort over the whole annulus
  it swept — and a sweep from 0.15*r to r fills the disc instead of drawing
  its rim. Standard smoke practice is to emit from a VOLUME; this is the same
  thing spelled in the vocabulary the ring emitter has.

  THE SWEEP IS SYMMETRIC ABOUT base_r, r(t) = base_r * (1 + amt*sin), NOT a
  sweep DOWN from it — and that is the second lesson, measured: an inward-only
  sweep (0.15*r -> r) halves the mean release radius, which piles the same 34
  sprites/s into a quarter of the area and the plume goes from a grey band to a
  bright mass of legible discs. Symmetric keeps the mean radius, and therefore
  the optical density, exactly where round 4 tuned it, while still crossing the
  axis twice a period.

  SIM-TIME, chain-lowered (scale/bias/sine/scale/bias onto the EmissionRadius
  plug — reflected, propsheet-visible, A8-live), and the frequency is
  deliberately incommensurate with the 0.67 Hz puff and the 2.70 rev/s spin so
  the sweep never lines up into a standing spiral. At 34 sprites/s (~1 per emit
  tick) the spread is carried tick-to-tick rather than inside a batch: each
  successive sprite lands 0.28 m further in radius and 32 deg further in phase,
  i.e. on a coarse spiral through the disc."""
  if amt <= 0.0:
    return base_r
  return (E.sin(E.time * (2.0 * _pi * freq) + phase) * (base_r * amt)
          + base_r)


def _puff_norm(rate_amp, life_amp, shape=3):
  """Mean-population correction for modulating EmissionRate and LifeSpan IN
  PHASE. Live count is the rate integrated over the lifespan window, so the
  mean population picks up E[(1+A u)(1+B u)] = 1 + A*B*var(p)/mean(p)^2 with
  u = p/mean - 1. Left uncorrected that overflows a fixed pool and the full
  pool silently DROPS the emit — i.e. it eats the crests, which are the entire
  effect. Measured against the host's 900-slot plume pool: uncorrected mean
  live 901 (peak 955, 422 drops per 30 s); corrected 743 (peak 816, zero)."""
  if rate_amp <= 0.0 or life_amp <= 0.0:
    return 1.0
  m  = _puff_mean(shape)
  m2 = _comb(4 * shape, 2 * shape) / float(4 ** (2 * shape))   # E[wave^2]
  return 1.0 / (1.0 + rate_amp * life_amp * (m2 / (m * m) - 1.0))


def _puff_force(upstream, *, name, freq, phase,
                kick, radial, band, axis=(0.0, 0.0)):
  """THE SHED VORTEX RING. A signed, zero-mean, band-limited kick low in the
  column whose Strength plug is a bare sin(wt + phi):

      crest   +y and OUTWARD  -> the ring inflates and accelerates: a BULGE
      trough  -y and INWARD   -> the column decelerates and pinches: a NECK

  Zero-mean by construction, so it redistributes the column instead of raising
  it. The velocity impulse is carried by the particle, so the bulge is ADVECTED
  — a puff climbing the column, not a standing wave painted on it.

  Why a separate module and not a term in _servo: ExprIR has no time symbol
  (exprir_particles._FORCE_SYMBOLS is per-particle only), and the servo's own
  Strength already scales its wind and confinement terms, which must not
  oscillate. Strength here is the ONE live plug the oscillation rides.

    kick    peak vertical acceleration (m/s^2) at the band centre
    radial  peak radial spring coefficient (s^-2) — outward on the crest
    band    (y0, y1, y2, y3): ramps in over y0..y1, out over y2..y3
  """
  y = PF.pos_y
  prof = PF.smoothstep(band[0], band[1], y) * \
         (1.0 - PF.smoothstep(band[2], band[3], y))
  fy = prof * kick
  fx = (PF.pos_x - axis[0]) * (prof * radial)
  fz = (PF.pos_z - axis[1]) * (prof * radial)
  return P.ExprForce(upstream, name=name, fx=fx, fy=fy, fz=fz,
                     strength=E.sin(E.time * (2.0 * _pi * freq) + phase))


###############################################################################
# LAYER 1 — CORE. The white-hot heart of the fire, low in the curb.
#
# Deliberately SMALL and SHORT-LIVED so it reads as a separate object from the
# plume rather than as the plume's bright middle: ~1.4 m tall against the
# plume's 10 m, a 0.55 m ring against the plume's 0.6-2.35 m spread, and a
# gradient that never leaves white-gold. It is the only layer allowed to clip.
#
# ROUND-3 CONTAINMENT. Round 2 ran the core at intensity 1.25 on a 0.85 m ring
# with 0.62 m sprites, and the measured clipped (lum>0.95) region was 2.0 m
# WIDE and 3 m TALL — a white plateau, not a hot object. A blown-out plateau
# does not read as hotter than its surround, it reads as a hole in the image,
# because the eye judges "hot" from the CONTRAST STEP at the core boundary and
# a plateau has no boundary inside it. So the fix is subtractive:
#
#   intensity 1.25 -> 0.55   ring 0.85 -> 0.55   size 0.62 -> 0.46
#   rate 280 -> 180 total    life 1.00 -> 0.80 (-> 1.4 m, was 1.76)
#   instance offsets +-0.45 -> +-0.26 (the offsets set the white blob's width)
#   confine 0.55 -> 0.85 over a tighter band, so it stays a ball
#
# The gradient also leaves white FASTER (white-gold done by unit_age 0.16,
# gold by 0.42) so only the youngest ~15% of the pool is ever near-white. Small
# and contained beats large and bright: the same photons in a quarter of the
# area is a hotter-looking fire.
###############################################################################

class BonfireCore(ParticleSystem):

  def __init__(self, *,
               base_r=0.46,          # emission ring radius (m)
               base_y=0.85,          # height above the hearth floor slab. NOT 0:
                                     # from a 1.7 m eye at 15 m the pit's own
                                     # curb top cuts the sight line at ~1.3 m, so
                                     # a core seated on the slab is 90% OCCLUDED
                                     # in the one view that matters. It has to
                                     # CLEAR the curb top (1.0-2.65 m) to be the
                                     # object the brief asks for — still the
                                     # bottom quarter of a 10 m column.
               size=0.40,            # peak sprite size (m)
               rate=520.0,           # sprites/sec
               lifespan=0.95,        # s — short: the core is where fuel burns out
               velocity=0.55,        # m/s ignition boil (NOT the rise — see header)
               dispersion=1.0,       # lerp blend, clamped [0,1] (NOT degrees)
               spin=3.10,            # rev/s — WITHOUT THIS THE RING IS A POINT
               rise=2.20,            # m/s target rise -> 1.21 m over the life
               rise_k=5.00,          # 1/tau; tau = 0.20 s
               confine=0.85, confine_band=(1.00, 2.50),
               wind_shear=(0.2, 2.5),
               turbulence=0.70,
               curl_strength=1.40, curl_freq=0.70, curl_speed=0.26,
               wind=vec3(0.55, 0.0, 0.20),
               wind_k=2.40,
               intensity=1.30,       # HDR: the core is what clips to white
               soft_fade=0.55,
               pool_size=320,
               seed=41.0,
               noise_freq=6.0, noise_scroll=2.6, noise_octaves=4,
               noise_gain=1.35, noise_bias=0.24,
               noise_aa=1.0, noise_ridge=0.35, noise_detail=1.05,
               # THE CORE IS THE PERSISTENT ZONE. Erosion is deliberately MILD
               # here (0.20 -> 0.44 against the plume's 0.30 -> 0.66): the base
               # of a diffusion flame always burns, and a hearth that dissolves
               # in step with the column top reads as a fire going out. The
               # white heart stays a solid object; only its collar breaks up.
               erode=(0.20, 0.44), ramp_var=0.14,
               # PUFF — small on purpose, and it modulates RATE ONLY (no
               # lifespan swing). The persistent zone is the part that does NOT
               # come and go; what it does is brighten as each vortex sheds.
               puff_freq=0.67, puff_phase=0.0, puff_rate_amt=0.28,
               puff_shape=3, puff_hot=0.22,
               # HEAT (aux channel) — OFF. See the header block: the core is the
               # only CLIPPED region in the frame, and the distortion postfx's
               # chromatic term splits a clipped white region across the hard
               # curb boundary into a blue smear that reads as a rendering
               # fault. The gate would have confined the core's shimmer to its
               # top ~1 m (1.9-2.65 m) anyway, so the whole contribution was a
               # thin band the plume already overlaps. The knob stays live for
               # a host that wants it back: heat=0.14, heat_gate=(0.58, 0.68).
               heat=0.0, heat_gate=(0.58, 0.68), heat_out=(1.00, 0.84),
               light_tint=None, light_smoothing=0.05, light_body_bias=1.60,
               axis=(0.0, 0.0),
               offset=vec3(0, 0, 0)):
    super().__init__()

    self.pool = P.PoolData(size=pool_size, name="POOL")
    self.emit = P.RingEmitter(self.pool, name="EMIT",
                              LifeSpan=lifespan,
                              EmissionRate=_puff_mod(rate, puff_freq, puff_phase,
                                                     puff_rate_amt,
                                                     shape=puff_shape),
                              EmissionVelocity=velocity,
                              EmissionRadius=base_r,
                              DispersionAngle=dispersion,
                              EmitterSpinRate=spin,
                              Direction=vec3(0, 1, 0),
                              Tangent=vec3(1, 0, 0),
                              Offset=offset + vec3(0, HEARTH_Y + base_y, 0))
    if puff_hot > 0.0:
      self.emit.Aux.x = _puff_wave(puff_freq, puff_phase, shape=puff_shape)
    self.servo = _servo(self.emit, name="SERV",
                        rise=rise, rise_k=rise_k,
                        wind=wind, wind_k=wind_k, wind_shear=wind_shear,
                        confine=confine, confine_band=confine_band,
                        axis=axis)
    self.turb = P.Turbulence(self.servo, name="TURB",
                             Amount=vec3(turbulence, turbulence * 0.45, turbulence))
    self.curl = P.CurlNoise(self.turb, name="CURL",
                            Strength=curl_strength, Frequency=curl_freq, Speed=curl_speed)

    # PURE ADDITIVE (alpha ~ 0 throughout): the core never occludes — it is the
    # light source, and any occlusion here reads as a grey hole in the flame.
    self.material = _material(
        FlameFrag,
        {0.00: hsv(46, 0.03, 1.00, 0.00),   # ignition — near white
         0.10: hsv(42, 0.10, 1.00, 0.00),   # still white: the heart
         0.28: hsv(35, 0.44, 0.60, 0.00),   # white-gold, and DONE with white
         0.55: hsv(28, 0.78, 0.24, 0.00),   # gold, already handing off
         0.80: hsv(20, 0.94, 0.10, 0.00),   # orange, into the plume
         1.00: hsv(14, 1.00, 0.00, 0.00)},  # out
        intensity=intensity, soft_fade=soft_fade,
        tint=light_tint if light_tint is not None else hsv(34, 0.45, 1.0),
        smoothing=light_smoothing, body_bias=light_body_bias,
        freq=noise_freq, scroll=noise_scroll, octaves=noise_octaves,
        seed=seed, gain=noise_gain, bias=noise_bias, soft=(soft_fade > 0.0),
        aa=noise_aa, ridge=noise_ridge, erode=erode, detail=noise_detail,
        ramp_var=ramp_var, puff_hot=puff_hot,
        heat=heat, heat_gate=heat_gate, heat_out=heat_out)

    self._size = _curve([(0.00, size * 0.35), (0.15, size),
                         (0.60, size * 0.80), (1.00, size * 0.22)])
    self.sprites = P.SpriteRenderer(self.curl, name="SPRI",
                                    material=self.material,
                                    Size=E.curve(E.ptc.unit_age, self._size))
    self.sprites.module.depth_sort = True
    self.render(self.sprites)


###############################################################################
# LAYER 2 — PLUME. The COLUMN: 10 m of flame, uniformly filled.
#
# Instanced on three concentric rings (see layers()) so the base is a filled
# 5 m disc rather than a hollow tube — the concentric-ring fill, because a ring
# emitter only ever spawns on its radius.
#
###############################################################################
# ROUND-3: SATURATION IS HEADROOM (why the base went white and the core could
# not be seen inside it).
#
# PREMA with alpha ~ 0 is pure addition, so the base pixel is the SUM of every
# sprite the ray crosses — tens of them where three rings overlap. A colour
# whitens under addition as soon as its WEAKEST channel saturates, and the
# stack count needed to get there is 1/channel. Round 2's hottest plume stop
# was hsv(29,0.84,0.96) = rgb(0.96,0.60,0.154): red clips after ~1 sprite, but
# BLUE clips after only ~6.5, so a 7-deep stack is white — and the whole base
# is 15-30 deep. That is the entire cause of the white plateau; the core layer
# was never the main culprit.
#
# Raising saturation to 0.93 at the same value gives rgb(0.96,0.55,0.067):
# blue now needs ~15 sprites, red still ~1. Same luminance, same energy in the
# frame, but the stack clips RED-FIRST and stays orange far deeper. Saturation
# is the free variable that buys clipping headroom without dimming the fire —
# so the whole ramp moves up in saturation and down slightly in value, and
# most of the pale plateau simply becomes saturated orange.
#
# ROUND-3: BUBBLINESS. Individual sprites were legible at 4-7 m because the
# cookie's detail scale was COARSER than the sprite: freq 3.6 over a 0.86-1.3 m
# quad is a ~0.3 m feature, so each sprite showed as one soft lobe. freq 6.4 at
# 5 octaves puts the finest octave at ~4 cm, below the eye-level pixel footprint
# at 15 m, and the bias/gain move (0.28/1.25 -> 0.18/1.50) deepens the cookie's
# valleys so neighbouring sprites interleave instead of pooling. The rate rise
# (460 -> 610 across the three rings, pools 620 -> 900 — the round-2 pool was
# ALREADY at its cap, so a rate rise alone would have been a no-op) is the
# smaller half of that fix, per the "detail before count" rule.
###############################################################################

class BonfirePlume(ParticleSystem):

  def __init__(self, *,
               base_r=1.50,
               base_y=0.05,
               size=1.05,
               rate=75.0,
               lifespan=3.20,        # 3.40 * (3.20 - 0.25) = 10.03 m
               velocity=0.80,        # ignition boil only
               dispersion=1.0,
               spin=2.30,            # rev/s — WITHOUT THIS THE RING IS A POINT
               rise=3.40,
               rise_k=4.00,          # tau = 0.25 s
               # ROUND 4: the falloff eased (0.35 over 6.0-10.5 -> 0.22 over
               # 7.0-12.5) and the confinement widened (1.10 over 0.8-4.2 ->
               # 0.92 over 1.2-5.0). Both existed to hold a CONSTANT 10 m
               # silhouette; with the lifespan now swinging +-45% the tall
               # cohorts have to be ALLOWED to overshoot, or the falloff eats
               # exactly the intermittency the swing is there to produce.
               rise_falloff=(7.0, 12.5), rise_falloff_amt=0.22,
               confine=0.85, confine_band=(1.4, 5.4),
               wind_shear=(0.6, 6.0),
               turbulence=0.55,
               curl_strength=1.30, curl_freq=0.45, curl_speed=0.16,
               wind=vec3(0.90, 0.0, 0.32),
               wind_k=2.60,
               intensity=0.24,
               soft_fade=0.85,
               pool_size=900,
               seed=57.0,
               # DETAIL. fbm_aa band-limits the finest octave against its own
               # screen footprint, so round 3's ceiling (6.4 x 5, where the next
               # step would have gone sub-pixel and crawled) lifts to 7.6 x 6
               # without buying shimmer. `ridge` folds in the filaments.
               noise_freq=7.6, noise_scroll=2.6, noise_octaves=6,
               noise_gain=1.50, noise_bias=0.26,
               noise_aa=1.0, noise_ridge=0.55, noise_detail=1.35,
               # SUBSTANCE. The dissolve threshold over life — the three-zone
               # flame in cookie form: nearly solid at the fuel bed (0.30),
               # shredded to filaments by the tip (0.66). This is the single
               # change that puts see-through gaps in the column.
               erode=(0.24, 0.60), ramp_var=0.20,
               # PUFF. 1.5/sqrt(D) with D ~ 5 m -> 0.67 Hz, one shed every
               # 1.5 s. Rate and lifespan swing TOGETHER (a fuel-rich burst
               # burns longer AND taller); `puff_norm` keeps the mean live
               # population where round 3 had it so the pinned pool never
               # clips a crest. The vortex kick is the neck.
               puff_freq=0.67, puff_phase=None, puff_wave_speed=20.0,
               puff_rate_amt=0.90, puff_life_amt=0.40, puff_shape=3,
               # THE KICK IS A PERTURBATION, NOT A PISTON — and the servo sets
               # the scale. A sustained kick K against a rise servo of gain
               # rise_k settles at a velocity offset of K/rise_k, so K = 7.5
               # against rise_k 4.0 is +-1.9 m/s on a 3.4 m/s column: more than
               # half the rise, reversed twice a second. Measured, that stops
               # being a shed vortex and becomes a bellows — the column
               # collapsed into a low wide mass (p90 height 3.18 -> 2.90 m).
               # 4.0 / 0.85 is +-1.0 m/s of rise and +-0.65 m/s of radial
               # (radial*r/wind_k at r = 2 m), which necks the column without
               # blowing it apart.
               puff_kick=4.00, puff_radial=0.85,
               puff_band=(0.50, 1.60, 3.60, 6.50),
               puff_hot=0.32,        # cohort heat stamp -> Aux.x -> fragment
               # TONGUES — and the reason `rise_spread` is SMALL. A flame tip
               # is an EXTREME-VALUE statistic: spread the rise draw wide and
               # every cohort contains fast parcels, so the highest lit row is
               # an average over a big ensemble and stops moving at all
               # (measured: 0.30 spread gave a tip standard deviation of 0.18 m
               # on a 10.4 m column — a fire that cannot come and go). 0.15
               # keeps the ragged edge without dissolving the cohort, and the
               # LIFESPAN swing carries the intermittency instead.
               rise_spread=0.15, escape_q=0.86, escape_amt=0.80,
               # HEAT (aux channel) — the main shimmer body. The servo puts
               # unit_age 0.60 at h = 3.40*(1.92-0.25) = 5.7 m, so the gate
               # lands the shimmer over the UPPER column and the plume tip
               # (5.7-10 m) — above the flame front, which is where a fire's
               # haze actually is, and clear of the curb the aux pass would
               # otherwise clip against.
               heat=0.10, heat_gate=(0.60, 0.70), heat_out=(1.00, 0.86),
               light_tint=None, light_smoothing=0.16, light_body_bias=2.20,
               axis=(0.0, 0.0),
               offset=vec3(0, 0, 0)):
    super().__init__()

    # PHASE BY RADIUS, not by hash: the shed ring expands outward from the
    # axis, so each concentric instance lags by base_r / puff_wave_speed
    # seconds (7/18/28 deg across the 0.60/1.50/2.35 m rings at the shipped
    # 20 m/s). The crest visibly rolls outward and the three rings still
    # breathe as one body — a random per-ring phase would smear the puff away,
    # which is the failure mode this replaces.
    if puff_phase is None:
      puff_phase = -2.0 * _pi * puff_freq * (base_r / max(puff_wave_speed, 1e-3))
    pnorm = _puff_norm(puff_rate_amt, puff_life_amt, puff_shape)

    self.pool = P.PoolData(size=pool_size, name="POOL")
    self.emit = P.RingEmitter(self.pool, name="EMIT",
                              LifeSpan=_puff_mod(lifespan, puff_freq, puff_phase,
                                                 puff_life_amt, shape=puff_shape),
                              EmissionRate=_puff_mod(rate, puff_freq, puff_phase,
                                                     puff_rate_amt,
                                                     shape=puff_shape,
                                                     norm=pnorm),
                              EmissionVelocity=velocity,
                              EmissionRadius=base_r,
                              DispersionAngle=dispersion,
                              EmitterSpinRate=spin,
                              Direction=vec3(0, 1, 0),
                              Tangent=vec3(1, 0, 0),
                              Offset=offset + vec3(0, HEARTH_Y + base_y, 0))
    # THE COHORT STAMP. Aux is the one emitter plug the C++ re-pulls PER
    # PARTICLE (mPerParticleAux, modules_emitter_ring.cpp:110-114), so binding
    # a TIME-only expression to it writes the puff phase at birth into every
    # particle of that tick and it rides with them for life. The fragment reads
    # it as ctx.aux.x. Nothing else in this asset touches Aux.
    if puff_hot > 0.0:
      self.emit.Aux.x = _puff_wave(puff_freq, puff_phase, shape=puff_shape)
    self.servo = _servo(self.emit, name="SERV",
                        rise=rise, rise_k=rise_k,
                        rise_falloff=rise_falloff, rise_falloff_amt=rise_falloff_amt,
                        wind=wind, wind_k=wind_k, wind_shear=wind_shear,
                        confine=confine, confine_band=confine_band,
                        rise_spread=rise_spread,
                        escape_q=escape_q, escape_amt=escape_amt,
                        axis=axis)
    self.puff = _puff_force(self.servo, name="PUFF",
                            freq=puff_freq, phase=puff_phase,
                            kick=puff_kick, radial=puff_radial,
                            band=puff_band, axis=axis)
    self.turb = P.Turbulence(self.puff, name="TURB",
                             Amount=vec3(turbulence, turbulence * 0.5, turbulence))
    self.curl = P.CurlNoise(self.turb, name="CURL",
                            Strength=curl_strength, Frequency=curl_freq, Speed=curl_speed)

    # Flame colour ALL THE WAY UP — the smoke is its own layer now, released at
    # the column top, so the plume no longer has to spend its last 60% of life
    # turning grey (that is what made round 1 read as a glow pool with a lid).
    # Alpha stays ~0 until the tip: with tens of sprite crossings through the
    # column, even 0.03 of occlusion per sprite accumulates to a solid mass.
    # Saturation carries the clipping headroom (see the block above): every
    # stop below y=6 m of column sits at s >= 0.86, so a 20-deep additive stack
    # still resolves as orange with the core's white ball legible inside it.
    #
    # ROUND-4 additions to the ramp, both about RANGE:
    #  * a DEEP-RED SHOULDER (0.90 hsv 7 -> 0.96 hsv 4) instead of falling
    #    straight out of orange, so the top of the column carries a colour the
    #    yellow body does not — the single biggest widening of the palette.
    #  * SOOT ALPHA in the last third (peak 0.030 at 0.90). This is the ONLY
    #    occlusion in the flame and it is what gives the column an interior:
    #    depth-sorted back to front, the near filaments attenuate the far ones,
    #    so the middle of the mass finally reads darker than its rim. It is
    #    kept small AND confined to the oldest (highest, sparsest) third
    #    because round 1's warning holds — 0.03 everywhere would accumulate
    #    into a solid grey body.
    self.material = _material(
        FlameFrag,
        {0.00: hsv(34, 0.88, 0.48, 0.000),  # emerging from the fuel bed
         0.10: hsv(31, 0.92, 0.70, 0.000),  # brightening
         0.25: hsv(27, 0.95, 0.80, 0.000),  # PEAK ~2 m up, not at the bed:
         0.48: hsv(21, 0.97, 0.74, 0.000),  # a big fire's flame front stands
         0.72: hsv(14, 1.00, 0.52, 0.012),  # off its fuel. Red tongues above,
         0.90: hsv(7,  1.00, 0.22, 0.030),  # sooting as they cool: the dark
         0.96: hsv(4,  1.00, 0.08, 0.022),  # maroon shoulder is the widest
         1.00: hsv(0,  0.00, 0.00, 0.000)}, # colour step in the frame.
        intensity=intensity, soft_fade=soft_fade,
        tint=light_tint if light_tint is not None else hsv(24, 0.70, 1.0),
        smoothing=light_smoothing, body_bias=light_body_bias,
        freq=noise_freq, scroll=noise_scroll, octaves=noise_octaves,
        seed=seed, gain=noise_gain, bias=noise_bias, soft=(soft_fade > 0.0),
        aa=noise_aa, ridge=noise_ridge, erode=erode, detail=noise_detail,
        ramp_var=ramp_var, puff_hot=puff_hot,
        heat=heat, heat_gate=heat_gate, heat_out=heat_out)

    # Size PEAKS mid-column then falls: monotone growth would mushroom the top.
    # Together with the radial confinement this is the taper.
    self._size = _curve([(0.00, size * 0.45), (0.24, size),
                         (0.52, size * 1.05), (0.80, size * 0.78),
                         (1.00, size * 0.30)])
    self.sprites = P.SpriteRenderer(self.curl, name="SPRI",
                                    material=self.material,
                                    Size=E.curve(E.ptc.unit_age, self._size))
    self.sprites.module.depth_sort = True
    self.render(self.sprites)


###############################################################################
# LAYER 3 — EMBERS. Velocity-aligned streaks breaking off the column.
#
# Same servo, higher target rise, and a falloff band ABOVE the flame top so the
# embers overshoot the column (to ~13 m), slow, and get carried off by the wind
# instead of stopping dead.
###############################################################################

class BonfireEmbers(ParticleSystem):

  def __init__(self, *,
               base_r=1.35,
               base_y=0.25,
               rate=42.0,            # SPARSE — embers are punctuation
               lifespan=3.60,
               velocity=1.10,
               dispersion=1.0,
               spin=1.70,
               rise=4.30,
               rise_k=3.00,
               rise_falloff=(6.0, 15.0), rise_falloff_amt=0.85,
               confine=0.0,          # embers are FREE — no chimney
               turbulence=3.20,      # embers wander far more than the flame
               curl_strength=2.60, curl_freq=0.55, curl_speed=0.30,
               wind=vec3(1.60, 0.0, 0.55),   # the lightest thing here
               wind_k=0.85,          # ...and the slowest to be caught by it
               wind_shear=(0.5, 7.0),
               length=0.30, width=0.045,
               intensity=4.60,
               pool_size=320,
               # PUFF — the strongest rate modulation in the fire (0.95: the
               # rate very nearly reaches zero between sheds). Sparks are
               # THROWN by a puff, they do not trickle: a fire that spits in
               # bursts synchronised with its own breathing is one of the
               # cheapest reads there is that the column has an event rate.
               # Streaks have no pool-safety interaction with the flame (their
               # own pool carries 151 live at the mean, cap 320).
               puff_freq=0.67, puff_phase=0.0, puff_rate_amt=0.95,
               puff_shape=3,
               rise_spread=0.25,
               light_tint=None,
               axis=(0.0, 0.0),
               offset=vec3(0, 0, 0)):
    super().__init__()

    self.pool = P.PoolData(size=pool_size, name="POOL")
    self.emit = P.RingEmitter(self.pool, name="EMIT",
                              LifeSpan=lifespan,
                              EmissionRate=_puff_mod(rate, puff_freq, puff_phase,
                                                     puff_rate_amt,
                                                     shape=puff_shape),
                              EmissionVelocity=velocity,
                              EmissionRadius=base_r,
                              DispersionAngle=dispersion,
                              EmitterSpinRate=spin,
                              Direction=vec3(0, 1, 0),
                              Tangent=vec3(1, 0, 0),
                              Offset=offset + vec3(0, HEARTH_Y + base_y, 0))
    self.servo = _servo(self.emit, name="SERV",
                        rise=rise, rise_k=rise_k,
                        rise_falloff=rise_falloff, rise_falloff_amt=rise_falloff_amt,
                        wind=wind, wind_k=wind_k, wind_shear=wind_shear,
                        confine=confine, rise_spread=rise_spread, axis=axis)
    self.turb = P.Turbulence(self.servo, name="TURB",
                             Amount=vec3(turbulence, turbulence * 0.8, turbulence))
    self.curl = P.CurlNoise(self.turb, name="CURL",
                            Strength=curl_strength, Frequency=curl_freq, Speed=curl_speed)

    # ADDITIVE, no soft fade: embers are points of light far off the stone, and
    # the streak geometry is thin enough that a depth fade only costs fill.
    self.material = _material(
        EmberFrag,
        {0.00: hsv(40, 0.20, 1.00, 0.00),   # spark
         0.20: hsv(30, 0.75, 0.90, 0.00),
         0.60: hsv(16, 1.00, 0.35, 0.00),   # cooling ember
         0.88: hsv(6,  1.00, 0.06, 0.00),
         1.00: hsv(0,  1.00, 0.00, 0.00)},  # dark
        intensity=intensity, soft_fade=0.0,
        blending=tokens.ADDITIVE,
        tint=light_tint if light_tint is not None else hsv(30, 0.60, 1.0),
        smoothing=0.10, body_bias=1.0,
        soft=False, tail=0.12, core=0.45, spark_var=0.70)

    self.streaks = P.StreakRenderer(self.curl, name="STRK",
                                    material=self.material,
                                    Length=length, Width=width)
    self.render(self.streaks)


###############################################################################
# LAYER 4 — SMOKE. Released at the COLUMN TOP, leaning downwind.
#
# Round-1 note: the smoke was a dim grey blob because per-tick Drag killed its
# rise within 0.7 s, so 180 live sprites piled up in a 1 m ball right at the
# emission ring. Under the servo it keeps climbing for its whole 9 s life and
# the same 180 sprites spread over ~12 m of rise and ~20 m of downwind drift —
# the same pool, an order of magnitude less optical depth per pixel.
#
###############################################################################
# ROUND-3: THE UNDER-LIT PLUME. Round 2 over-corrected — 20/s of near-black
# sprites released at 9.6 m barely registered in either frame. Three changes,
# and NOT "turn the opacity up":
#
#  1. RELEASE LOWER AND WIDER — 9.6 -> 8.5 m, ring 1.60 -> 2.35 m, sprite
#     2.70 -> 3.50 m. The plume now starts inside the flame tip's own width, so
#     it grows out of the column instead of appearing above a gap.
#  2. LIT FROM BELOW. A bonfire's smoke is not black: its underside is a
#     secondary light receiver, orange near the flame and falling to occlusion-
#     only within ~10 m. Round 2 gave it v = 0.045 everywhere, which against a
#     night sky is invisible in BOTH directions (too dark to add, too thin to
#     occlude). The ramp now opens at v 0.30 warm and decays — the standard
#     fire-lit-smoke gradient. That is what gives it a silhouette; the alpha
#     rises only modestly (0.115 -> 0.155 peak).
#  3. A SILHOUETTE, NOT A HAZE. `sharpen=(0.16,0.62)` on the cookie converts
#     the soft radial falloff into a defined, ragged billow edge, so 300 live
#     sprites read as lobes with boundaries instead of a gaussian smear. Rate
#     20 -> 34 (pool 500 covers 34*9 = 306 live).
###############################################################################

class BonfireSmoke(ParticleSystem):

  def __init__(self, *,
               base_r=1.80,
               base_y=8.80,          # AT the column top: smoke comes off the
                                     # flame tip, it does not stack over the fire
               size=2.90,
               rate=34.0,
               lifespan=9.00,        # engine clamps LifeSpan to 10 s
               velocity=0.60,
               dispersion=1.0,
               spin=2.70,
               rise=0.95,            # LOWER than the flame's: a lower rise
               rise_k=1.60,          # against the same wind IS the lean angle.
               rise_falloff=(10.0, 18.0), rise_falloff_amt=0.70,
               confine=0.0,          # smoke EXPANDS; the opposite of a chimney
               turbulence=1.10,
               curl_strength=1.30, curl_freq=0.30, curl_speed=0.05,
               wind=vec3(1.55, 0.0, 0.55),   # the lean is the whole read
               wind_k=1.80,          # tau 0.55 s: at wind speed almost at
               wind_shear=None,      # once, or the lean happens off-frame
               intensity=0.28,       # smoke is LIT-LOOKING, not emissive
               # SOFT FADE OFF — and this is a MEASURED perf decision, not taste.
               # A frame-time battery attributed 10.06 ms of the fire's 20.06 ms
               # close-camera cost to THIS LAYER ALONE — 63% of the whole fire,
               # more than the other eleven layers together. The cause is the
               # pairing: soft_fade 1.60 was the largest fade distance in the
               # asset riding the largest sprite (2.90 m, swelling to 9.3 m),
               # so every fragment of the biggest overlapping quads in the frame
               # did a depth-buffer fetch and a fade solve.
               #
               # AND IT HAD NOTHING TO FADE AGAINST. The soft-particle fade
               # exists to kill the hard intersection line where a sprite cuts
               # geometry; this layer is released at 8.8 m and drifts downwind
               # AT ALTITUDE — it never touches the hearth, the curb, the pyre
               # or the ground. The embers layer already ships 0.0 for exactly
               # the same reason (see its material call). Re-measured after the
               # change and checked for a hard plume/hillside edge in the
               # establishing and downwind views: none — the plume crosses the
               # ridge line 200+ m in front of it, where the fade distance would
               # be irrelevant anyway.
               soft_fade=0.0,
               pool_size=500,
               seed=73.0,
               noise_freq=2.4, noise_scroll=0.55, noise_octaves=4,
               noise_gain=1.15, noise_bias=0.26,
               sharpen=(0.16, 0.62), # billow EDGE — see the block above
               # PUFF, ARRIVING LATE. A billow leaving the tip at 8.8 m was
               # shed at the fuel bed h/rise = 8.8/3.4 = 2.6 s earlier, so the
               # smoke's release pulse carries that LAG in seconds rather than
               # a phase — 2.6 s at 0.67 Hz is 1.74 periods, i.e. a phase that
               # would be meaningless to author directly but is exact as a
               # transport time. This is what makes the smoke read as the same
               # breathing seen further downstream, not as a second metronome.
               puff_freq=0.67, puff_lag_s=2.60, puff_rate_amt=0.55,
               puff_shape=2,
               # RELEASE DISC, NOT RELEASE RING (see _sweep_radius): the
               # emission radius sweeps 0.54 <-> 3.06 m about its 1.80 m mean at
               # 1.37 Hz, so successive sprites walk a spiral through the disc
               # instead of stacking on one circle. 0 restores round 4's fixed
               # circle (the grey donut).
               ring_sweep=0.70, ring_sweep_hz=1.37,
               axis=(0.0, 0.0),
               offset=vec3(0, 0, 0)):
    super().__init__()

    self.pool = P.PoolData(size=pool_size, name="POOL")
    self.emit = P.RingEmitter(self.pool, name="EMIT",
                              LifeSpan=lifespan,
                              EmissionRate=_puff_mod(
                                  rate, puff_freq,
                                  -2.0 * _pi * puff_freq * puff_lag_s,
                                  puff_rate_amt, shape=puff_shape),
                              EmissionVelocity=velocity,
                              EmissionRadius=_sweep_radius(base_r, ring_sweep,
                                                           ring_sweep_hz),
                              DispersionAngle=dispersion,
                              EmitterSpinRate=spin,
                              Direction=vec3(0, 1, 0),
                              Tangent=vec3(1, 0, 0),
                              Offset=offset + vec3(0, HEARTH_Y + base_y, 0))
    self.servo = _servo(self.emit, name="SERV",
                        rise=rise, rise_k=rise_k,
                        rise_falloff=rise_falloff, rise_falloff_amt=rise_falloff_amt,
                        wind=wind, wind_k=wind_k, wind_shear=wind_shear,
                        confine=confine, axis=axis)
    self.turb = P.Turbulence(self.servo, name="TURB",
                             Amount=vec3(turbulence, turbulence * 0.4, turbulence))
    self.curl = P.CurlNoise(self.turb, name="CURL",
                            Strength=curl_strength, Frequency=curl_freq, Speed=curl_speed)

    # NO HEAT (aux channel): `heat` is left at the FlameFrag default of 0, so
    # ctx.is_heat is never queried, no heat technique pair is generated and the
    # aux pass skips this material. Three reasons, in order of weight:
    #   1. WRONG PLACE. Heat haze is a near-field effect over the flame tip.
    #      This layer is released AT 8.8 m and spends 9 s drifting ~20 m
    #      downwind while cooling; a shimmer over that whole region reads as a
    #      lens defect, not as hot air. The plume layer already covers 5.7-10 m,
    #      which is the band that should shimmer.
    #   2. NOTHING TO REFRACT. The postfx warps the composited frame by the
    #      heat gradient. Behind the smoke there is night sky and more smoke —
    #      a soft grey mass with no high-frequency detail, so the warp is
    #      invisible while still costing a full aux pass over the largest
    #      screen area in the fire (500 sprites swelling to 9.3 m).
    #   3. It would fight the smoke's own silhouette: sharpen=(0.16,0.62) is
    #      there to give the billow a defined edge, and a shimmer riding the
    #      same mask would smear the edge it was authored to create.
    #
    # FIRE-LIT AND ABSORPTIVE: warm rgb near the flame tip decaying to near
    # zero, with alpha carrying the body the whole way. The lit head is what
    # gives it a silhouette against a night sky; the tail occludes the stars,
    # which is what makes it read as matter rather than a grey glow.
    self.material = _material(
        FlameFrag,
        {0.00: hsv(26, 0.70, 0.065, 0.000),  # LIT by the flame tip it leaves
         0.10: hsv(20, 0.55, 0.052, 0.055),
         0.28: hsv(14, 0.36, 0.048, 0.108),  # rolling out of the firelight, but
         0.55: hsv(8,  0.22, 0.050, 0.120),  # SLOWLY: the visible part has to be
         0.80: hsv(0,  0.00, 0.032, 0.082),  # the DRIFTED part or there is no
         1.00: hsv(0,  0.00, 0.000, 0.000)}, # lean to see. Dim and long, not
                                             # bright and short.
        intensity=intensity, soft_fade=soft_fade,
        tint=vec3(0.20, 0.12, 0.06),        # near-black: smoke must not light
        smoothing=0.55, body_bias=0.3,
        freq=noise_freq, scroll=noise_scroll, octaves=noise_octaves,
        seed=seed, gain=noise_gain, bias=noise_bias, sharpen=sharpen,
        soft=(soft_fade > 0.0))

    self._size = _curve([(0.00, size * 0.60), (0.25, size),
                         (0.65, size * 1.9), (1.00, size * 3.2)])
    self.sprites = P.SpriteRenderer(self.curl, name="SPRI",
                                    material=self.material,
                                    Size=E.curve(E.ptc.unit_age, self._size))
    self.sprites.module.depth_sort = True
    self.render(self.sprites)


###############################################################################
# COMPOSITION — what a host actually builds.
#
# Each entry is ONE drawable and therefore ONE light. Core and plume run as
# several offset instances: that is how the fire gets a POOL of independently
# flickering lights (varied tint, radius, intensity, EMA time constant) instead
# of one metronome. The PLUME instances additionally sit on CONCENTRIC RINGS
# — since round 5 at 0.40 / 1.05 / 2.60 m and at three different HEIGHTS (the
# crib-vent seating, see PLUME_SEATING) -> a 5.2 m base disc that fills the
# pit's 6 m curb — with the INNER ring carrying the fattest sprites and the
# largest live population: a ring emitter only spawns on its radius, so one
# instance alone is a hollow tube that limb-brightens into two lobes with a
# hole down the middle.
#
# 7 drawables, 6 lights (streaks publish none) — deliberately under the 12-24
# guidance, because frustum culling is off and every light costs every pixel.
###############################################################################

###############################################################################
# ROUND-5 — SEATING THE COLUMN ON A BUILT PYRE (the crib-vent composition).
#
# bld_pyre now stands in the hearth: a 5.3 m hollow crib whose top is at 3.688 m
# with a 0.807 m clear flue, a skirt of cordwood out to a 2.805 m foot, and an
# ash bed of radius 1.85. Round 4's plume released ALL THREE rings at 0.05 m —
# i.e. INSIDE the timber — and opaque logs depth-test sprites away, which cost
# the visible column about a third of its height.
#
# THE RECIPE IS THE CHIMNEY-VENT SPLIT, the standard build for any fire with a
# structure in it (a crib, a brazier, a stove mouth): the flame is not one
# emitter inside the fuel, it is a VENT emitter that starts where the fuel ENDS
# plus a SKIRT emitter that runs up the OUTSIDE of it. Ring by ring:
#
#   ring 0  r 0.40  released 2.70 m   THE FLUE. Inside the crib's hollow shaft,
#           just under the 3.688 m crib top, so it vents out of the crown and
#           through the upper courses' slots instead of being born under a
#           metre of timber. 0.40 keeps it inside the 0.807 m clear flue.
#   ring 1  r 1.05  released 1.70 m   THE CRIB BODY. In the hollow above the two
#           solid courses; it comes out through the horizontal chimneys.
#   ring 2  r 2.60  released 0.70 m   THE SKIRT. OUTSIDE the mantle (skirt butts
#           at ~2.45, foot 2.805 is jitter+radius) and just above the fire
#           curb's 0.67 m top, so it licks up the outside of the stack and
#           backlights the timber. This is also the WIDTH fix: 2.60 + a 0.76 m
#           sprite reads to the curb's own 3.0 m centreline, so the fire finally
#           fills its 6 m curb instead of standing as a 4.7 m taper inside it.
#
# THE LIFESPAN IS DERIVED, NOT AUTHORED (plume_seating below). Raising a release
# raises the tip by the same amount unless the life comes down with it, and
# three rings released at three heights with one lifespan would end in three
# different tips — a frayed column. The servo's own identity h(L) = v*(L - tau)
# inverts to L = (tip - y0)/v + tau, so the AUTHORED number is the tip altitude
# (10.5 m, round 3's silhouette) and every ring's lifespan falls out of where it
# was seated. Likewise the authored number for density is the LIVE POPULATION
# per ring (742/663/546 — exactly round 4's, so this re-seat moves no photons),
# and the emission rate is live/lifespan. Both derivations are shared with the
# scene through this function, so the two files cannot drift.
###############################################################################

PLUME_TIP_M = 10.50      # authored flame-tip altitude, m above the hearth floor
                         # plane (the fire's y datum, HEARTH_Y below the slab)

# (base_r, base_y, live) per concentric ring — see the block above.
PLUME_SEATING = ((0.40, 2.55, 742.0),
                 (1.05, 1.55, 663.0),
                 (2.60, 0.55, 546.0))


def plume_seating(base_y, live, *, tip_m=PLUME_TIP_M, rise=3.40, rise_k=4.00):
  """(lifespan_s, rate_per_s) for a plume ring released `base_y` above the slab.

  Inverts the servo: h(L) = rise * (L - 1/rise_k), so a ring seated higher gets
  a proportionally SHORTER life and every ring's tip lands at the same authored
  altitude. `live` is the target live population (rate = live/lifespan), which
  is the number the pinned 900-slot pool and the optical depth both care about
  — not the rate."""
  life = (float(tip_m) - (HEARTH_Y + float(base_y))) / float(rise) + 1.0 / float(rise_k)
  if life <= 0.05:
    raise ValueError("bonfire.plume_seating: release %.3f m is at or above the "
                     "authored tip %.3f m" % (base_y, tip_m))
  return life, float(live) / life


def _drained(sysobj):
  """Build ONE system and close its trace IMMEDIATELY.

  ParticleSystem.__init__ opens a trace that only closes in generatedflow(),
  and generatedflow() no-ops unless the closing system is still the CURRENT
  trace. Constructing several systems before draining any of them therefore
  nests the traces and every system but the LAST silently loses its staged
  Expr bindings — here that is `Size`, so five of these seven layers rendered
  at default size, i.e. invisibly, with no error anywhere. Build-then-drain,
  one at a time, is the only safe order."""
  sysobj.generatedflow()
  return sysobj


def layers(*, wind=vec3(0.90, 0.0, 0.32), scale=1.0):
  """[(name, ParticleSystem instance, emitter_intensity, emitter_radius), ...].

  `wind` is a target VELOCITY in m/s (the servo target), not an acceleration.
  Each entry's graph is ALREADY drained (see _drained) — the host just reads
  `.graphdata`. The host creates one ParticlesDrawableData per entry, sets the
  two light knobs on it, and hangs it on the transparent layer at the pit's
  origin."""
  L = []

  # --- CORE x2: offset off-axis, different tints/flicker. The offsets are what
  # separate the two light centroids; identical offsets would collapse them
  # into one over-bright light in the middle (the co-located-slot trap).
  #
  # The two offsets also set the WIDTH of the white region, because each
  # instance carries its own near-white ball: round 2's +-0.45 m offsets alone
  # spread the clipped area to 0.9 m before a single sprite was drawn. Halved.
  # The published LIGHT intensity is NOT cut with the sprite intensity — the
  # fire must still throw the same warm flicker on the stone; only the pixels
  # it writes get contained.
  core_variants = [
      (vec3( 0.24, 0.0,  0.15), hsv(37, 0.32, 1.0), 0.035, 1.00, 1.00),
      (vec3(-0.27, 0.0, -0.17), hsv(31, 0.50, 1.0), 0.065, 0.86, 1.12),
  ]
  for i, (off, tint, smth, isc, rsc) in enumerate(core_variants):
    L.append(("core%d" % i,
              _drained(BonfireCore(offset=off * scale, wind=wind * 0.6,
                          base_r=0.46 * scale, size=0.40 * scale,
                          rise=2.20 * scale,
                          intensity=1.30 * isc,
                          light_tint=tint, light_smoothing=smth,
                          rate=520.0 / len(core_variants),
                          spin=3.10 + 0.7 * i,
                          pool_size=320, seed=41.0 + 13.0 * i)),
              8.5 * isc, 9.0 * rsc * scale))

  # --- PLUME x3 on CONCENTRIC RINGS: fills the base disc, and the varied curl
  # speed / seed keeps the three from moving as one body.
  #
  # Round 3: sprites 15% SMALLER and 33% MORE of them (rate 460 -> 610, pool
  # 620 -> 900 — the round-2 pool was already at its cap, so the rate rise
  # alone would have been silently dropped). Coverage is near enough constant
  # (0.85^2 * 1.33 = 0.96) so the column does not thin, but the sprite-to-
  # cookie-detail ratio drops and the individual blobs stop being legible.
  #
  # Round 5: the rings are RE-SEATED around the pyre (flue / crib / skirt — see
  # the PLUME_SEATING block) and their radii, release heights, lifespans and
  # rates all come out of that one table. The lifespans and rates are DERIVED
  # (plume_seating) so the three tips still meet at 10.5 m and the live
  # population per ring is unchanged from round 4.
  plume_variants = [
      # size  spin  offset                      tint             smth  int   rad
      (1.10, 2.30, vec3( 0.18, 0.0, -0.14), hsv(28, 0.62, 1.0), 0.12, 1.05, 0.85),
      (0.90, 2.90, vec3(-0.30, 0.0,  0.22), hsv(23, 0.74, 1.0), 0.18, 1.00, 1.00),
      (0.76, 1.90, vec3( 0.24, 0.0,  0.30), hsv(18, 0.84, 1.0), 0.26, 0.82, 1.15),
  ]
  for i, (sz, spin, off, tint, smth, isc, rsc) in enumerate(plume_variants):
    rr, by, live = PLUME_SEATING[i]
    life, rate = plume_seating(by, live)
    L.append(("plume%d" % i,
              _drained(BonfirePlume(offset=off * scale, wind=wind,
                           base_r=rr * scale,
                           base_y=by * scale,
                           size=sz * scale,
                           spin=spin,
                           rise=3.40 * scale,
                           lifespan=life,
                           intensity=0.24 * isc,
                           light_tint=tint, light_smoothing=smth,
                           rate=rate,
                           pool_size=900, seed=57.0 + 17.0 * i,
                           curl_speed=0.16 + 0.05 * i)),
              13.5 * isc, 16.0 * rsc * scale))

  # --- EMBERS: streaks publish NO light, so this entry's light knobs are zero
  # by contract, not by taste.
  L.append(("embers",
            _drained(BonfireEmbers(wind=wind * 1.8, base_r=1.35 * scale)), 0.0, 1.0))

  # --- SMOKE: absorptive, so its published emission colour is ~black anyway;
  # the intensity is set low rather than zero so a faint bounce survives.
  L.append(("smoke",
            _drained(BonfireSmoke(wind=wind * 2.6, base_r=1.80 * scale,
                                  base_y=8.80 * scale, size=2.90 * scale,
                                  rate=34.0)), 0.7, 9.0 * scale))
  return L


# `ork.particle.viewer.py bonfire` hosts exactly one class — give it the body of
# the fire. The full four-layer build goes through layers().
Bonfire = BonfirePlume


__all__ = ["BonfireCore", "BonfirePlume", "BonfireEmbers", "BonfireSmoke",
           "FlameFrag", "EmberFrag", "layers", "HEARTH_Y",
           "PLUME_TIP_M", "PLUME_SEATING", "plume_seating"]
