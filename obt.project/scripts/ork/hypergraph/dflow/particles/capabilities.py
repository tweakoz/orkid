###############################################################################
# ork.hypergraph.dflow.particles.capabilities — the particles family's EDITOR
# CAPABILITY MASK: the family-specific knowledge the family-neutral dflow editor
# (the ork.dflow.edit.py shell + GraphDataDocument + GraphDataNodeGraphModel)
# consults through a small generic protocol, so the shell never special-cases the
# string "particles" (ECS-common-substrate law: families declare capabilities,
# the shell reads them generically).
#
# Two capabilities:
#
#  * DISPLAY FLAGS — supports_display_flags = False. A particles graph has NO
#    per-node display marker: render terminals are selected by BYPASS (bypass a
#    renderer omits it) and EVERY non-bypassed renderer draws simultaneously
#    (sprites + streaks + aux compose in one pass). So the canvas / propsheet must
#    NOT offer a display toggle on a particles node — there is no meaning to it.
#
#  * BYPASSABILITY (by node ROLE) + EFFECTIVE-GRAPH construction:
#      POOL      (psys::ParticlePoolData)   NOT bypassable — the pool SOURCE; there is
#                                           nothing upstream to pass through (refuse loud).
#      EMITTER   (psys::*EmitterData)        NOT bypassable — it POPULATES the pool; there
#                                           is no identity that keeps particles flowing.
#      SIDE      (globals / parameters …)    NOT bypassable — feeds a side input (RelTime /
#                                           params), not the particle chain (refuse loud).
#      OP        (forces / attractors /      bypassable → WIRE-AROUND: its pool-chain SOURCE
#                 colliders / modifiers)     is reconnected to whatever consumed its pool
#                                           output, and the op is dropped from the effective
#                                           graph — so its compute() never runs (the shared
#                                           pool is mutated in sequence, so merely flagging
#                                           _bypassed leaves its mutation live; the op must
#                                           actually leave the graph).
#      RENDERER  (psys::*RendererData)        bypassable → OMIT from the render set: a renderer
#                                           is a SINK (nothing downstream to wire), so it is
#                                           simply dropped — its render lambda never registers.
#                                           ALL renderers bypassed → an empty, honest render.
#
# The EFFECTIVE graph is built on a serialize/deserialize CLONE (same discipline as the
# host's compose clone) so the shared canvas / propsheet GraphData the user edits stays
# PRISTINE — the bypass never mutates it, it only shapes the transient sim graph.
###############################################################################

from orkengine.core import dataflow as _dflow
from orkengine.core import Object as _Object

# the family's particle-buffer chain plug is named "pool" on every ParticleModuleData
# (createDefaultIOs: an input "pool" + an output "pool"); the emitter populates it, each op
# mutates it in sequence, the renderer(s) read it. Bypass = identity on THIS primary input.
_CHAIN_PLUG = "pool"


def _pool_plugs(class_name):
  """(has_pool_input, has_pool_output) for a reflected module class, from the E1 plug
  schema — the structural test that tells a chain OP (both) from a SIDE provider (neither)."""
  spec = _dflow.plugSpec(class_name) or {}
  has_in = any(p.get("name") == _CHAIN_PLUG for p in spec.get("inputs", []))
  has_out = any(p.get("name") == _CHAIN_PLUG for p in spec.get("outputs", []))
  return has_in, has_out


def classify(class_name):
  """The node's ROLE in the particle graph, one of pool / emitter / renderer / op / side.
  Name first for the three named roles (a renderer carries a pool output too, so the plug
  schema alone can't tell it from an op), then the pool-plug schema separates a chain OP
  (pool in AND out) from a SIDE provider (globals / parameters)."""
  n = class_name or ""
  if "Renderer" in n:
    return "renderer"
  if "Emitter" in n:
    return "emitter"
  if "PoolData" in n:
    return "pool"
  has_in, has_out = _pool_plugs(n)
  if has_in and has_out:
    return "op"
  return "side"


# CANVAS CATEGORY ICON: the node's ROLE -> the standard_icons registry name of its
# category glyph. FORCES (the OP class), POOL, RENDERERS and EMITTERS each carry an
# icon (owner ruling: all emitters get one). SIDE (globals / parameters) carries no
# icon (absent from the map -> node_icon returns None -> the canvas draws no glyph).
_ICON_BY_ROLE = {
  "op":       "particle_force",
  "pool":     "particle_pool",
  "renderer": "particle_render",
  "emitter":  "particle_emitter",
}


# EDITOR EXPRESSION FIELDS (E2.5 S7): reflected MODULE properties that store an ExprIR
# tree (JSON) and are edited AS SOURCE through the propsheet detail editor. Each entry maps a
# reflected class -> [(reflected_property, expression_context_name)]. ExprForce stores its
# per-particle force channels as three particles.force trees. (exprcolor bakes its color-ramp
# expressions directly into the gradient LUT with no stored source field, so it exposes no
# editable expr field here — editing it would require persisting the channel source first.)
_EXPR_FIELDS = {
  "psys::ExprForceModuleData": [
    ("force_x", "particles.force"),
    ("force_y", "particles.force"),
    ("force_z", "particles.force"),
  ],
}


class ParticlesEditorCapabilities:
  """The particles family's capability object, injected into the (family-neutral)
  GraphDataDocument + consumed by the ParticlesViewportHost. Stateless — one shared
  instance per document/host is enough."""

  # DISPLAY-FLAG capability: particles have no per-node display marker (see module docstring).
  supports_display_flags = False

  # ---- editor expression fields (consumed by the document's expr-row surface) ----

  def expr_fields(self, class_name):
    """[(reflected_property, context_name)] for a class's editable ExprIR fields, or []. The
    family-neutral property model surfaces one detail-editor row per entry."""
    return list(_EXPR_FIELDS.get(class_name or "", []))

  def print_expr(self, json_str, context_name):
    """Stored ExprIR JSON -> author SOURCE for the given context (the editor shows this)."""
    if context_name == "particles.force":
      from .exprir_particles import print_force
      return print_force(json_str)
    raise ValueError("no expression printer for context %r" % (context_name,))

  def parse_expr(self, source, context_name):
    """Author SOURCE -> validated canonical ExprIR JSON for the given context. Raises loudly
    (ExprParseError / ExprSignatureError / ParticleExprError) on any out-of-vocabulary or
    dishonest construct, so an invalid edit never reaches the C++ lowering."""
    if context_name == "particles.force":
      from .exprir_particles import parse_force
      return parse_force(source)
    raise ValueError("no expression parser for context %r" % (context_name,))

  # ---- canvas category icon (consumed by the document's node_icon accessor) ----

  def node_icon(self, class_name):
    """The standard_icons registry NAME of a node's category icon, or None. The
    family-neutral canvas reads this GENERICALLY (through the document) so it never
    checks a family or class name itself: a family without this accessor draws no
    icons. FORCES (op) -> 'particle_force', POOL -> 'particle_pool', RENDERERS ->
    'particle_render', EMITTERS -> 'particle_emitter'; SIDE maps to None."""
    return _ICON_BY_ROLE.get(classify(class_name))

  # ---- bypassability (consumed by the document's badge + refusal) ----------

  def bypass_kind(self, class_name):
    return classify(class_name)

  def bypassable(self, class_name):
    return classify(class_name) in ("op", "renderer")

  def refuse_bypass_reason(self, class_name):
    """None when `class_name` is bypassable; else a loud, human reason (ops-self-defend —
    the caller RAISES with this so a refused bypass never marks the flag)."""
    kind = classify(class_name)
    if kind == "pool":
      return "the POOL is the particle source — there is nothing upstream to pass through"
    if kind == "emitter":
      return ("an EMITTER populates the pool — bypassing it would leave the system with no "
              "particles (there is no pass-through identity)")
    if kind == "side":
      return ("this module feeds a SIDE input (globals / parameters), not the particle "
              "chain — there is nothing to pass through")
    return None

  # ---- effective-graph construction (consumed by the host on rebake) -------

  def render_terminals(self, graph):
    """The names of every RENDERER module in `graph` (each is a live render terminal unless
    bypassed) — the host uses the count to detect the all-terminals-bypassed empty render."""
    return [name for name, cls in _iter_modules(graph) if classify(cls) == "renderer"]

  def bypass_targets(self, graph):
    """(ops, renderers) = the names of the modules the LIVE graph currently has flagged
    _bypassed, split by role. pool/emitter/side flags are ignored defensively (they are
    refused at set time, so they should never appear — but a hand-authored .orj could carry
    one, and we must not act on it)."""
    ops, renderers = [], []
    for name, cls in _iter_modules(graph):
      mod = graph.findModule(name)
      if mod is None or not bool(mod.bypassed):
        continue
      kind = classify(cls)
      if kind == "op":
        ops.append(name)
      elif kind == "renderer":
        renderers.append(name)
    return ops, renderers

  def unwired_ops(self, graph):
    """The names of every chain OP that is FULLY FLOATING — its pool input AND its pool output
    are both unconnected (the just-added-not-yet-wired case). Excluding these keeps the preview
    LIVING while the user wires a new op up, rather than freezing the whole sim on the transient
    invalid topology. Scoped DELIBERATELY to fully-floating (not merely input-unconnected): a
    BROKEN chain — a mid-chain delete leaves a downstream op with a live pool CONSUMER but a dead
    source — stays INVALID (the S8.5 soft-fail freeze), never silently wired around. A floating op
    contributes nothing to the shared pool anyway, so dropping it is semantically a no-op."""
    edges = graph.edges()
    wired_in = {e["in_module"] for e in edges if e["in_plug"] == _CHAIN_PLUG}
    wired_out = {e["out_module"] for e in edges if e["out_plug"] == _CHAIN_PLUG}
    out = []
    for name, cls in _iter_modules(graph):
      if classify(cls) != "op":
        continue
      if name in wired_in or name in wired_out:
        continue
      out.append(name)
    return out

  def build_effective_graph(self, graph):
    """The EFFECTIVE sim graph for `graph`'s current bypass state: bypassed chain OPS are
    wired around (their pool source feeds their pool consumers) and dropped; bypassed
    RENDERERS are dropped (omitted from the render set); FULLY-FLOATING ops (a just-added,
    not-yet-wired op — see unwired_ops) are likewise dropped so the preview keeps living while
    the user wires up. Returns the LIVE graph UNCHANGED (byte-identical) when nothing is
    bypassed or floating; otherwise a serialize/deserialize CLONE that is mutated (the shared
    canvas graph stays pristine). Ops-self-defend: a clone failure falls back to the live graph
    rather than a black/empty viewport."""
    ops, renderers = self.bypass_targets(graph)
    floating = self.unwired_ops(graph)
    drop = list(dict.fromkeys(ops + renderers + floating))   # dedup (a floating op may be bypassed too)
    if not drop:
      return graph
    try:
      clone = _Object.deserializeJson(graph.serializeJson())
    except Exception:
      return graph
    # Drop every bypassed OP and RENDERER via the SAME wire-around-and-drop: an OP's pool
    # source feeds its pool consumers (the chain closes over it); a TERMINAL renderer has no
    # pool consumer so this is a plain remove (omit from the render set). Renderers are only
    # ever chained (the pool output is fan-out 1) when a graph carries multiple terminals —
    # a bypassed MIDDLE renderer then keeps the downstream terminal connected. A fully-floating
    # op has neither a pool source nor a pool consumer, so its wire-around is a plain remove.
    # Edges are re-read each iteration so a run of adjacent bypassed modules resolves in any order.
    for name in drop:
      _wire_around_and_drop(clone, name)
    return clone


def _iter_modules(graph):
  """[(module_name, reflected_class_name)] for every module in `graph`, parsed from the
  reflection JSON (the family-neutral node walk the GraphDataDocument uses)."""
  import json as _json
  try:
    root = _json.loads(graph.serializeJson())
    mods = root["root"]["object"]["properties"].get("Modules", {})
    return [(name, entry.get("object", {}).get("class", "")) for name, entry in mods.items()]
  except Exception:
    return []


def _wire_around_and_drop(graph, opname):
  """Reconnect every consumer of `opname`'s pool output to `opname`'s pool SOURCE, then
  remove `opname` from `graph`. Edges are re-read from the live clone so a chain of adjacent
  bypassed ops resolves to the first surviving producer no matter the processing order."""
  mod = graph.findModule(opname)
  if mod is None:
    return
  edges = graph.edges()
  src = None
  for e in edges:
    if e["in_module"] == opname and e["in_plug"] == _CHAIN_PLUG:
      src = (e["out_module"], e["out_plug"])
      break
  consumers = [(e["in_module"], e["in_plug"]) for e in edges
               if e["out_module"] == opname and e["out_plug"] == _CHAIN_PLUG]
  if src is not None:
    smod = graph.findModule(src[0])
    out_plug = getattr(smod.outputs, src[1]) if smod is not None else None
    if out_plug is not None:
      for dst, dp in consumers:
        dmod = graph.findModule(dst)
        if dmod is None:
          continue
        in_plug = getattr(dmod.inputs, dp)
        if in_plug is None:
          continue
        graph.disconnect(in_plug)
        graph.connect(in_plug, out_plug)
  # drop the op (also severs its now-dangling upstream edge)
  graph.removeModule(mod)
