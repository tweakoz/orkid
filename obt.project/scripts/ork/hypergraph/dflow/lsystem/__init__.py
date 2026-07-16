###############################################################################
# lsystem — the Python front-end for the reflected LRuleSet grammar (GRAMMARS GR-1).
#
# A grammar is authored here as a SERIALIZABLE COMBINATOR TREE (turtle ops + expr nodes
# + choose/fork/when), lowered to the six reflected C++ types (LExpr / LSymbolDef /
# LTurtleOp / LParamBinding / LRuleDef / LRuleSet), and derived (rewritten + turtle-
# interpreted) by the C++ evaluator at bake time into the XfNodeGraph -> LSweep pipeline.
# There is NO Python at runtime (A2): the grammar rides as reflected DATA on
# LSystemModuleData._grammar, which the derive() branch of _buildSkeleton consumes.
#
#   from ork.hypergraph.dflow import lsystem as L
#   class CaneCholla(L.Lsystem):
#     seed = "nm-cholla-07"
#     def grammar(self):
#       Shoot = self.symbol("Shoot", len=0.16, rad=0.020, gen=0)
#       @L.rule(Shoot, until=Shoot.gen >= 11)
#       def grow(S):
#         return [ L.segment(len=S.len, rad=S.rad, gid="cane"),
#                  L.choose((0.5, Shoot(len=S.len*0.96, gen=S.gen+1)), (0.5, L.bloom())) ]
#       self.axiom(Shoot()); self.iterate(depth=11, segment_budget=700)
#   ...
#   H.lsystem(grammar=CaneCholla)   # attach the derived LRuleSet to the module
#
# LOAD-BEARING (§17.1, T4): a production body is a serializable combinator tree, NOT
# arbitrary python. Comparisons build CMP nodes and Expr raises on __bool__/__iter__/__len__,
# so a stray `if S.gen > 3:` in a rule body fails LOUD at trace time (naming the rule).
###############################################################################

from orkengine import lev2 as _lev2

_hm = _lev2.hypermesh

###############################################################################
# named int constants — mirror lruleset.h (LExprKind / LExprOp / LTurtleKind). Keep in sync
# with ork.lev2/inc/ork/lev2/gfx/hypermesh/lruleset.h (the C++ evaluator reads these ints).
###############################################################################
CONST, PARAM, ENV, RNG, BINOP, CMP = 0, 1, 2, 3, 4, 5
ADD, SUB, MUL, DIV = 0, 1, 2, 3
LT, LE, GT, GE, EQ, NE = 10, 11, 12, 13, 14, 15
SEGMENT, PITCH, ROLL, YAW, TAPER, SLOT, FORK, CHOOSE, WHEN, CALL = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9

_NEGATE_CMP = {LT: GE, LE: GT, GT: LE, GE: LT, EQ: NE, NE: EQ}

# default turtle-string rotation magnitude (degrees) when a +/-/&/^// token carries no (paren).
_DEFAULT_ANGLE = 25.0


class LSystemAuthorError(Exception):
  """A grammar was authored with non-serializable python (an `if expr:`, iteration over an
  expr, or a malformed op) — the combinator-tree contract (§17.1 LOAD-BEARING) was broken."""


# module-level authoring context (set only while a grammar()/string-lower runs): `L.rule` +
# the __bool__ trap resolve the CURRENT Lsystem / rule from here (mirrors hypermesh's trace ctx).
_current_lsys = [None]
_current_rule = [None]


###############################################################################
# Expr — the bake-time symbol-decision expression tree (S.len * 0.96, S.gen >= 11, S.rnd(a,b)).
# A pure-python AST that lowers to LExpr; operator overloads build BINOP/CMP nodes. The __bool__/
# __iter__/__len__ traps (T4) turn the most likely authoring error into a loud trace-time failure.
###############################################################################
class Expr:
  __slots__ = ("_kind", "_c", "_ref", "_op", "_args")

  def __init__(self, kind, c=0.0, ref="", op=0, args=()):
    self._kind = kind
    self._c    = float(c)
    self._ref  = ref
    self._op   = op
    self._args = list(args)

  # ---- leaf constructors ----
  @staticmethod
  def const(v): return Expr(CONST, c=float(v))

  @staticmethod
  def param(name): return Expr(PARAM, ref=str(name))

  @staticmethod
  def env(name): return Expr(ENV, ref=str(name))

  @staticmethod
  def rng(a, b): return Expr(RNG, args=[_ce(a), _ce(b)])

  # ---- lower to the reflected LExpr (fresh nodes -> TREE, T6) ----
  def build(self):
    if self._kind == CONST:
      return _hm.LExpr(kind=CONST, constant=self._c)
    if self._kind in (PARAM, ENV):
      return _hm.LExpr(kind=self._kind, ref=self._ref)
    return _hm.LExpr(kind=self._kind, op=self._op, args=[a.build() for a in self._args])

  # ---- arithmetic -> BINOP ----
  def _bin(self, op, o, rev=False):
    a, b = (_ce(o), self) if rev else (self, _ce(o))
    return Expr(BINOP, op=op, args=[a, b])

  def __add__(self, o):      return self._bin(ADD, o)
  def __radd__(self, o):     return self._bin(ADD, o, rev=True)
  def __sub__(self, o):      return self._bin(SUB, o)
  def __rsub__(self, o):     return self._bin(SUB, o, rev=True)
  def __mul__(self, o):      return self._bin(MUL, o)
  def __rmul__(self, o):     return self._bin(MUL, o, rev=True)
  def __truediv__(self, o):  return self._bin(DIV, o)
  def __rtruediv__(self, o): return self._bin(DIV, o, rev=True)
  def __neg__(self):         return Expr(BINOP, op=SUB, args=[Expr.const(0.0), self])

  # ---- comparisons -> CMP ----
  def _cmp(self, op, o): return Expr(CMP, op=op, args=[self, _ce(o)])
  def __lt__(self, o): return self._cmp(LT, o)
  def __le__(self, o): return self._cmp(LE, o)
  def __gt__(self, o): return self._cmp(GT, o)
  def __ge__(self, o): return self._cmp(GE, o)
  def __eq__(self, o): return self._cmp(EQ, o)
  def __ne__(self, o): return self._cmp(NE, o)
  __hash__ = None  # unhashable (we never key on an Expr); overriding __eq__ already drops it

  # ---- the serializable-tree traps (T4) ----
  def _trap(self, what):
    rule = _current_rule[0]
    where = (" in rule %r" % rule) if rule else ""
    raise LSystemAuthorError(
        "L-system grammar expression used in a python %s context%s.\n"
        "Production bodies must be SERIALIZABLE COMBINATOR TREES — a runtime condition is a "
        "GUARD (`@L.rule(Sym, when=/until=Expr)`) or `L.when(pred, a, b)`, never a python "
        "`if expr:` / `for x in expr` / `list(expr)`. (offending expr kind=%d)" % (what, where, self._kind))

  def __bool__(self): self._trap("boolean/if")
  def __iter__(self): self._trap("iteration")
  def __len__(self):  self._trap("len()")

  def negate(self):
    """logical NOT — for `until=`. Flips a CMP op; wraps anything else as (expr < 0.5) (i.e. not-truthy)."""
    if self._kind == CMP:
      return Expr(CMP, op=_NEGATE_CMP[self._op], args=list(self._args))
    return Expr(CMP, op=LT, args=[self, Expr.const(0.5)])

  def __repr__(self):
    return "Expr(kind=%d, op=%d, ref=%r, c=%r, args=%d)" % (self._kind, self._op, self._ref, self._c, len(self._args))


def _ce(x):
  """coerce a python number OR an Expr into an Expr (number -> CONST). Anything else is an author error."""
  if isinstance(x, Expr):
    return x
  if isinstance(x, bool):
    return Expr.const(1.0 if x else 0.0)
  if isinstance(x, (int, float)):
    return Expr.const(float(x))
  raise LSystemAuthorError("expected a number or grammar expression (S.*, S.rnd(...), arithmetic), got %s"
                           % type(x).__name__)


###############################################################################
# Symbol — a grammar non-terminal (alphabet entry). Attribute access builds PARAM exprs
# (S.len -> PARAM("len")); calling it builds a CALL op (Shoot(len=S.len*0.96) -> LTurtleOp CALL).
# It doubles as the symbolic state `S` passed to a production (S.len / S.rnd / S.env.moisture).
###############################################################################
class _EnvProxy:
  def __getattr__(self, k):
    if k.startswith("_"):
      raise AttributeError(k)
    return Expr.env(k)


class Symbol:
  def __init__(self, lsys, name, defaults):
    self._lsys     = lsys
    self._name     = name
    self._defaults = {str(k): float(v) for k, v in defaults.items()}

  @property
  def name(self):
    return self._name

  def build(self):  # -> LSymbolDef
    return _hm.LSymbolDef(name=self._name, defaults=dict(self._defaults))

  def rnd(self, a, b):
    """a stochastic draw in [a, b) — RNG(a, b). Deterministic per (grammar, seed) via the C++ counter-hash."""
    return Expr.rng(a, b)

  @property
  def env(self):
    """environment-field proxy — S.env.moisture -> ENV("moisture"). Evaluates to 0.0 in GR-1 (field bridge is GR-6)."""
    return _EnvProxy()

  def __call__(self, **params):  # -> a CALL op instantiating this non-terminal
    return OpSpec(CALL, symbol=self._name, params=[(k, _ce(v)) for k, v in params.items()])

  def __getattr__(self, k):  # S.len / S.gen / ... -> PARAM expr (open alphabet)
    if k.startswith("_"):
      raise AttributeError(k)
    return Expr.param(k)

  def __repr__(self):
    return "Symbol(%r, defaults=%r)" % (self._name, self._defaults)


###############################################################################
# OpSpec — a python turtle-op node. The whole grammar authors as an OpSpec tree, then derive()
# resolves gid strings -> ids and materializes each node into a fresh LTurtleOp (no aliasing:
# build() mints a new reflected object every call, so a shared OpSpec still serializes as a tree).
###############################################################################
class OpSpec:
  __slots__ = ("kind", "params", "gid", "symbol", "children", "weights", "guard", "every")

  def __init__(self, kind, params=None, gid=None, symbol="", children=None, weights=None, guard=None, every=None):
    self.kind     = kind
    self.params   = params or []       # list of (key:str, Expr)
    self.gid      = gid                 # str (deferred -> id) | int | None
    self.symbol   = symbol             # CALL target
    self.children = children or []     # list of OpSpec
    self.weights  = weights or []      # CHOOSE weights
    self.guard    = guard              # WHEN predicate (Expr | None)
    self.every    = every             # SLOT spacing (recorded; multi-emit is GR-2)

  def gids(self):
    out = set()
    if isinstance(self.gid, str):
      out.add(self.gid)
    for c in self.children:
      out |= c.gids()
    return out

  def build(self, gid_map):
    op = _hm.LTurtleOp(kind=self.kind)
    if self.params:
      op.params = [_hm.LParamBinding(key=k, value=v.build()) for k, v in self.params]
    if self.gid is not None:
      op.gid = gid_map[self.gid] if isinstance(self.gid, str) else (int(self.gid) & 0xFFF)
    if self.symbol:
      op.symbol = self.symbol
    if self.children:
      op.children = [c.build(gid_map) for c in self.children]
    if self.weights:
      op.weights = [float(w) for w in self.weights]
    if self.guard is not None:
      op.guard = self.guard.build()
    return op


class RuleSpec:
  __slots__ = ("lhs", "guard", "weight", "rhs")

  def __init__(self, lhs, guard, weight, rhs):
    self.lhs    = lhs
    self.guard  = guard    # Expr | None
    self.weight = float(weight)
    self.rhs    = rhs      # list of OpSpec

  def gids(self):
    out = set()
    for o in self.rhs:
      out |= o.gids()
    return out

  def build(self, gid_map):
    rd = _hm.LRuleDef(lhs=self.lhs, weight=self.weight, rhs=[o.build(gid_map) for o in self.rhs])
    if self.guard is not None:
      rd.guard = self.guard.build()
    return rd


###############################################################################
# op builders (the @op surface, §17.4). Each returns an OpSpec; params accept a number OR an Expr.
###############################################################################
def segment(len=None, rad=None, gid=None):
  """one turtle joint -> an XfNode edge. len/rad flow through the PARAM env (A8, live-pokeable);
  gid stamps the face material band (A1 tags[20:32))."""
  p = []
  if len is not None:
    p.append(("len", _ce(len)))
  if rad is not None:
    p.append(("rad", _ce(rad)))
  return OpSpec(SEGMENT, params=p, gid=gid)


def pitch(deg): return OpSpec(PITCH, params=[("angle", _ce(deg))])   # rotate frame about local X
def yaw(deg):   return OpSpec(YAW,   params=[("angle", _ce(deg))])   # rotate frame about local Y
def roll(deg):  return OpSpec(ROLL,  params=[("angle", _ce(deg))])   # rotate frame about local Z (heading)
def taper(factor=1.0): return OpSpec(TAPER, params=[("amount", _ce(factor))])  # scale the radius accumulator


def slot(tag=None, gid=None, every=None):
  """emit an XfSlot at the current node (an attach point GR-2 consumes as an instance). `tag`/`gid`
  is the slot band; `every` (spacing) is recorded but a single slot is emitted in GR-1 (multi-emit is GR-2)."""
  g = gid if gid is not None else tag
  return OpSpec(SLOT, gid=g, every=every)


def spines(every=None, gid="spine"):
  """sugar over slot — areole/spine attach points down a joint (a single XfSlot in GR-1; GR-2 places clusters)."""
  return slot(tag=gid, gid=gid, every=every)


def bloom(gid="flower"):
  """sugar over slot — a terminal flower attach point (bloom & stop)."""
  return slot(tag=gid, gid=gid)


def choose(*branches):
  """weighted stochastic pick of ONE branch: choose((0.5, a), (0.3, b), (0.2, c)). Each branch is a
  single op (or a list -> a groupless WHEN sequence). Resolved in the rewrite pass by the counter-hash RNG."""
  weights, children = [], []
  for pair in branches:
    w, op = pair
    weights.append(float(w))
    children.append(_as_single_op(op))
  return OpSpec(CHOOSE, weights=weights, children=children)


def fork(nmin, nmax=None, lean=None, then=None):
  """push N in [nmin, nmax] child frames, each running `then` (a serializable combinator body). lean =
  a fixed degree OR an (a,b) range (a stochastic pitch off the parent). Each child rewrites independently."""
  if nmax is None:
    nmax = nmin
  p = []
  if int(nmin) == int(nmax):
    p.append(("count", Expr.const(int(nmin))))
  else:
    # RNG(nmin-0.5, nmax+0.5) rounds to a uniform integer in [nmin, nmax] (the evaluator lrounds `count`).
    p.append(("count", Expr.rng(int(nmin) - 0.4999, int(nmax) + 0.4999)))
  if lean is not None:
    if isinstance(lean, (tuple, list)):
      p.append(("lean", Expr.rng(lean[0], lean[1])))
    else:
      p.append(("lean", _ce(lean)))
  return OpSpec(FORK, params=p, children=_as_op_list(then))


def branch(*ops):
  """a single bracket push `[...]` — one child frame running `ops` (a FORK with count defaulted to 1).
  The combinator peer of the turtle-string `[ ]`."""
  return OpSpec(FORK, children=_as_op_list(list(ops)))


def when(pred, a, b=None):
  """a serializable conditional: run `a` where `pred` holds, else `b` (both optional bodies). Resolved
  in the rewrite pass. `b` lowers to a second guarded body over the negated predicate."""
  wa = OpSpec(WHEN, guard=_ce(pred), children=_as_op_list(a))
  if b is None:
    return wa
  wb = OpSpec(WHEN, guard=_ce(pred).negate(), children=_as_op_list(b))
  return OpSpec(WHEN, children=[wa, wb])  # groupless WHEN (null guard) = a sequence of the two


def _as_op_list(x):
  """normalize an op / list-of-ops / None into a flat list of OpSpec; reject an Expr (a common error:
  returning `S.len*2` where a turtle op was expected)."""
  if x is None:
    return []
  if isinstance(x, OpSpec):
    return [x]
  if isinstance(x, Expr):
    raise LSystemAuthorError("expected a turtle op (L.segment/choose/fork/... or Sym(...)), got a grammar "
                             "Expr — an expression is a PARAM/number, not an op")
  if isinstance(x, (list, tuple)):
    out = []
    for e in x:
      out += _as_op_list(e)
    return out
  raise LSystemAuthorError("expected a turtle op or list of ops, got %s" % type(x).__name__)


def _as_single_op(op):
  """a CHOOSE branch must be ONE op; wrap a list in a groupless WHEN (a sequence)."""
  if isinstance(op, OpSpec):
    return op
  ops = _as_op_list(op)
  if len(ops) == 1:
    return ops[0]
  return OpSpec(WHEN, children=ops)


###############################################################################
# @L.rule — the production decorator. Used inside grammar(); binds the decorated function's returned
# combinator body to the current Lsystem as a rule for `symbol`, with an optional serializable guard.
###############################################################################
def rule(symbol, when=None, until=None):
  if when is not None and until is not None:
    raise LSystemAuthorError("L.rule: specify when= OR until=, not both")
  guard = None
  if when is not None:
    guard = _ce(when)
  elif until is not None:
    guard = _ce(until).negate()   # the rule fires UNTIL the predicate becomes true (i.e. while NOT it)

  def deco(fn):
    lsys = _current_lsys[0]
    if lsys is None:
      raise LSystemAuthorError("@L.rule used outside an Lsystem.grammar() body")
    prev = _current_rule[0]
    _current_rule[0] = "%s (symbol %r)" % (getattr(fn, "__name__", "<production>"), symbol.name)
    try:
      body = fn(symbol)   # call the production ONCE with the symbolic state S (= the Symbol)
    finally:
      _current_rule[0] = prev
    lsys._rulespecs.append(RuleSpec(symbol.name, guard, 1.0, _as_op_list(body)))
    return fn

  return deco


###############################################################################
# Lsystem — the family base class (mirrors the other DSL families). A subclass authors either a
# grammar() method (combinator form) OR string-form class attrs (axiom / rules / legend / iterate).
# derive() lowers whichever to the SAME reflected LRuleSet.
###############################################################################
class Lsystem:
  seed = 0
  depth = 8
  segment_budget = 4096

  def __init__(self):
    self._symbols   = []          # list of Symbol
    self._axiom     = []          # list of OpSpec
    self._rulespecs = []          # list of RuleSpec
    self._depth     = int(type(self).depth)
    self._budget    = int(type(self).segment_budget)
    self._seed_val  = _seed_to_u32(type(self).seed)
    self._gid_map   = {}
    self._ruleset   = None

  # ---- authoring API (combinator form) ----
  def symbol(self, name, **defaults):
    s = Symbol(self, name, defaults)
    self._symbols.append(s)
    return s

  def axiom(self, *ops):
    self._axiom = _as_op_list(list(ops))

  def iterate(self, depth=None, segment_budget=None):
    if depth is not None:
      self._depth = int(depth)
    if segment_budget is not None:
      self._budget = int(segment_budget)

  def grammar(self):
    raise NotImplementedError("Lsystem subclass must define grammar() (combinator form) or "
                              "string-form class attrs (rules=/axiom=/legend=)")

  # ---- lowering ----
  def _run_authoring(self):
    cls = type(self)
    string_form = (cls.grammar is Lsystem.grammar) and isinstance(cls.__dict__.get("rules"), dict)
    if string_form:
      self._lower_string()
      return
    prev = _current_lsys[0]
    _current_lsys[0] = self
    try:
      self.grammar()
    finally:
      _current_lsys[0] = prev

  def _lower_string(self):
    cls    = type(self)
    rules  = cls.__dict__.get("rules", {})
    legend = cls.__dict__.get("legend", {}) or {}
    itr    = cls.__dict__.get("iterate", {})
    itr    = itr if isinstance(itr, dict) else {}
    names  = list(rules.keys())
    self._symbols = [Symbol(self, nm, {}) for nm in names]
    symset = set(names)
    self._axiom = _tokenize(str(cls.__dict__.get("axiom", "")), legend, symset)
    self._rulespecs = [RuleSpec(nm, None, 1.0, _tokenize(str(body), legend, symset))
                       for nm, body in rules.items()]
    if "depth" in itr:
      self._depth = int(itr["depth"])
    if "segment_budget" in itr:
      self._budget = int(itr["segment_budget"])

  def derive(self):
    """lower this grammar to a reflected LRuleSet (the artifact serialized on LSystemModuleData._grammar)."""
    self._symbols, self._axiom, self._rulespecs = [], [], []
    self._run_authoring()
    # gid ids: sorted-by-name within THIS grammar -> stable across runs (cook-hash safe), id base 1.
    allgids = set()
    for o in self._axiom:
      allgids |= o.gids()
    for r in self._rulespecs:
      allgids |= r.gids()
    self._gid_map = {name: i + 1 for i, name in enumerate(sorted(allgids))}
    self._ruleset = _hm.LRuleSet(
        symbols=[s.build() for s in self._symbols],
        axiom=[o.build(self._gid_map) for o in self._axiom],
        rules=[r.build(self._gid_map) for r in self._rulespecs],
        depth=self._depth,
        segment_budget=self._budget,
        seed=self._seed_val)
    return self._ruleset

  def build(self):
    """the family build seam — GR-1 derives the grammar; the multi-sink mesh/instances/collider
    (§17.2) is GR-2. Subclasses that only author a grammar work through H.lsystem(grammar=...)."""
    return self.derive()

  def gid_of(self, tag):
    """the integer gid a tag/slot name resolved to (valid after derive())."""
    return self._gid_map.get(tag)


###############################################################################
# string dual form (§17.2) — a turtle-string tokenizer lowering to the SAME OpSpec tree the
# combinator form builds. F=segment, +/- =yaw, &/^ =pitch, / \ =roll, [ ]=branch, !=taper (legend),
# UPPERCASE=CALL, (x)=param for the preceding op. Gate: string vs combinator == uuid-stripped JSON.
###############################################################################
def _tokenize(s, legend, symbols):
  ops = []
  i, n = 0, len(s)
  while i < n:
    c = s[i]
    i += 1
    if c.isspace():
      continue
    if c == "[":
      depth, j = 1, i
      while j < n and depth > 0:
        if s[j] == "[":
          depth += 1
        elif s[j] == "]":
          depth -= 1
        j += 1
      if depth != 0:
        raise LSystemAuthorError("string grammar: unbalanced '[' in %r" % s)
      ops.append(OpSpec(FORK, children=_tokenize(s[i:j - 1], legend, symbols)))
      i = j
      continue
    if c == "]":
      raise LSystemAuthorError("string grammar: unbalanced ']' in %r" % s)
    # optional (param) immediately following the op token
    param = None
    if i < n and s[i] == "(":
      k = s.find(")", i)
      if k < 0:
        raise LSystemAuthorError("string grammar: unclosed '(' in %r" % s)
      param = float(s[i + 1:k])
      i = k + 1
    if c == "+":
      ops.append(yaw(param if param is not None else _DEFAULT_ANGLE))
    elif c == "-":
      ops.append(yaw(-(param if param is not None else _DEFAULT_ANGLE)))
    elif c == "&":
      ops.append(pitch(param if param is not None else _DEFAULT_ANGLE))
    elif c == "^":
      ops.append(pitch(-(param if param is not None else _DEFAULT_ANGLE)))
    elif c == "/":
      ops.append(roll(param if param is not None else _DEFAULT_ANGLE))
    elif c == "\\":
      ops.append(roll(-(param if param is not None else _DEFAULT_ANGLE)))
    elif c in legend:
      ops.append(_apply_legend(legend[c], param))
    elif c in symbols:
      ops.append(OpSpec(CALL, symbol=c))
    else:
      raise LSystemAuthorError("string grammar: unknown token %r in %r (legend=%s symbols=%s)"
                               % (c, s, sorted(legend), sorted(symbols)))
  return ops


def _apply_legend(entry, param):
  """a legend entry is a pre-built OpSpec (used directly — build() re-mints it per occurrence) OR a
  builder function (called with the (paren) param, e.g. legend={'!': L.taper} -> taper(0.8))."""
  if isinstance(entry, OpSpec):
    if param is not None:
      raise LSystemAuthorError("string grammar: a (param) on a pre-built legend op is ambiguous — put a "
                               "builder (e.g. L.taper) in the legend for parametrized tokens")
    return entry
  if callable(entry):
    return entry() if param is None else entry(param)
  raise LSystemAuthorError("string grammar: legend entry must be an op or a builder, got %s" % type(entry).__name__)


###############################################################################
# helpers + Hypermesh integration
###############################################################################
def _seed_to_u32(seed):
  """a stable uint32 seed. int -> masked; str -> FNV-1a 32 (so seed='nm-cholla-07' is deterministic)."""
  if isinstance(seed, bool):
    seed = int(seed)
  if isinstance(seed, int):
    return seed & 0xFFFFFFFF
  h = 2166136261
  for b in str(seed).encode("utf-8"):
    h = ((h ^ b) * 16777619) & 0xFFFFFFFF
  return h


def resolve_grammar(grammar):
  """accept an LRuleSet | an Lsystem instance | an Lsystem subclass | a 0-arg builder -> LRuleSet.
  Used by Hypermesh.lsystem(grammar=...) to attach the derived grammar to LSystemModuleData._grammar."""
  if isinstance(grammar, _hm.LRuleSet):
    return grammar
  if isinstance(grammar, Lsystem):
    return grammar.derive()
  if isinstance(grammar, type) and issubclass(grammar, Lsystem):
    return grammar().derive()
  if callable(grammar):
    return resolve_grammar(grammar())
  raise TypeError("H.lsystem(grammar=...) expects an LRuleSet, an Lsystem subclass/instance, or a "
                  "builder returning one; got %s" % type(grammar).__name__)


__all__ = [
    "Lsystem", "Symbol", "Expr", "OpSpec", "LSystemAuthorError",
    "rule", "segment", "pitch", "roll", "yaw", "taper", "slot", "spines", "bloom",
    "choose", "fork", "branch", "when", "resolve_grammar",
    "CONST", "PARAM", "ENV", "RNG", "BINOP", "CMP",
    "ADD", "SUB", "MUL", "DIV", "LT", "LE", "GT", "GE", "EQ", "NE",
    "SEGMENT", "PITCH", "ROLL", "YAW", "TAPER", "SLOT", "FORK", "CHOOSE", "WHEN", "CALL",
]
