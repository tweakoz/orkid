###############################################################################
# ork.hypergraph.dflow.document — the family-neutral BASE graph-document class.
#
# One editor core, N document specializations (JUL13_DFLOW E2, adjudication 2).
# The base defines the editor-facing SURFACE every editor consumer binds — the
# canvas, the outliner, the property sheet, the undo stack, and the MCP tools —
# WITHOUT knowing which concrete graph family it hosts. Specializations:
#
#   TerrainDoc         (terrain/doc.py)  — the structured un-unrolled program:
#                       loops / groups / switch, captures, pywriter; elaborate()
#                       DERIVES a fresh dflow.GraphData (owner law L2).
#   GraphDataDocument  (graphdata_document.py) — the default for doc-less
#                       families (particles, hypermesh): wraps a LIVE GraphData;
#                       elaborate() is the IDENTITY (the graph IS the document).
#
# This module MUST NOT import any family (terrain / particles / hypermesh) or any
# gfx-requiring engine surface: the base is what those families depend ON, never
# the reverse, and it must import cheaply anywhere.
#
# Owner laws honored here: ops self-defend (every unimplemented operation raises
# LOUDLY and named — never a silent no-op); the document is the single source of
# truth (L2); parametric document values ride the params table, never inlined.
###############################################################################


class GraphDocumentError(ValueError):
    """Raised on an illegal editor document mutation — an unknown param, an
    unsupported operation, an out-of-range value. Loud by design (ops-self-defend):
    the editor never silently drops an edit. Family error types subclass this
    (TerrainDoc's TerrainDocParamError) so a family `except` still catches base
    machinery raises while callers can catch the family-neutral base."""


# --- document parameters (E0) — the family-neutral params table ---------------
# HOISTED from terrain's _ParamTable (JUL13_DFLOW E2: "HOIST it here if it is
# genuinely family-neutral"). It is: name -> value + an OPTIONAL unit TAG (E0
# part-2 typed literals) + a per-name structural flag + declaration (signature)
# order. Values stay PLAIN numeric (the tag is metadata, never carried on the
# value) so any symbolic leaf reading the CURRENT value reads a plain number and
# an elaborated graph is tag-free — unchanged values bake byte-identically.
# Every family gets "asset-level params" as document data for free (the editor's
# param panel is uniform; the demoscene macro-knob rides the same mechanism).

class ParamTable:
    """Document-parameter table. Non-empty ONLY on an editor trace path; plain
    instantiation (viewer / scenes) leaves it empty and the document behaves as it
    did before E0. Subclasses may set `_error_cls` to raise a family-specific error
    type from set()."""

    _error_cls = GraphDocumentError

    def __init__(self):
        self._values = {}
        self._order = []
        self._structural = set()
        self._tags = {}          # name -> unit tag (E0 part 2); absent = plain literal

    def declare(self, name, value, tag=None):
        if name not in self._values:
            self._order.append(name)
        self._values[name] = value
        if tag is not None:
            self._tags[name] = tag

    def tag_of(self, name):
        return self._tags.get(name)

    def get(self, name):
        return self._values[name]

    def set(self, name, value):
        if name not in self._values:
            raise self._error_cls(
                f"unknown document parameter {name!r}; have {self._order}")
        self._values[name] = value

    def mark_structural(self, name):
        self._structural.add(name)

    def is_structural(self, name):
        return name in self._structural

    def names(self):
        return list(self._order)

    def __contains__(self, name):
        return name in self._values

    def __len__(self):
        return len(self._values)


###############################################################################

class GraphDocument:
    """Base editor document. The surface below is what a single editor core binds
    across every graph family; a specialization implements what its family supports
    and inherits a LOUD, NAMED refusal for what it does not (ops-self-defend).

    A document owns:
      - the family-neutral document `params` table (E0), and
      - a family-decided node-position backing store (the base default is an
        in-memory dict keyed by the family's node key; GraphData-direct families
        override to ride the C++ graph-level `_editor_layout`).
    """

    def __init__(self):
        self.params = ParamTable()
        self._node_positions = {}          # key -> (x, y) — default backing store

    # ---- helpers -------------------------------------------------------------

    def _unsupported(self, op):
        raise NotImplementedError(
            f"{type(self).__name__} does not implement {op!r} — this graph family "
            f"does not support that editor operation.")

    # ---- node / tree enumeration --------------------------------------------

    def tree_paths(self):
        """Deterministic editor paths for every document object — the SINGLE keying
        source shared by the outliner, the canvas, and session references. Returns
        [(parent_key, key, obj)] in tree order. Family-specific."""
        self._unsupported("tree_paths")

    def find_by_path(self, key):
        """The document object at a tree_paths() key, or None."""
        for (_pk, k, obj) in self.tree_paths():
            if k == key:
                return obj
        return None

    # ---- node-level parameters ----------------------------------------------

    def editable_params(self, node):
        """Ordered [(kind, name, value)] the editor may set_param on `node` (a
        family node handle). Family-specific."""
        self._unsupported("editable_params")

    def get_param(self, node, kind, name):
        """Current value of a node param. `kind` is 'inputs' (a plug) or 'module' (a
        reflected scalar). The base default scans editable_params (which returns the
        EFFECTIVE value); families with a richer read path override."""
        for (k, n, v) in self.editable_params(node):
            if k == kind and n == name:
                return v
        raise GraphDocumentError(
            f"node {node!r} has no editable param {name!r} (kind={kind!r})")

    def set_param(self, node, kind, name, value):
        """Editor mutation (L2): set a node param. `kind` is 'inputs' (a plug) or
        'module' (a reflected scalar). Family-specific."""
        self._unsupported("set_param")

    # ---- editor presentation surface (what the outliner renders per object) --
    # STATE, not UI — the model decides colors/glyphs; the document answers "what is
    # true about this object". Defaults are the safe no-badge / no-name generic answer
    # so a family opts in only where it has the concept.

    def display_name(self, obj, key):
        """A family-neutral display string for the document object `obj` at tree-path
        `key`, or None to fall back to the key's last segment. Default None."""
        return None

    def node_bypass_badge(self, obj):
        """(enabled, active) for the bypass badge on `obj`, or None if `obj` carries no
        bypass badge (a family opts in for its bypassable nodes AND structural constructs).
        Default None."""
        return None

    def node_output_badge(self, obj):
        """enabled-state (bool) for the display/select-as-output badge on `obj`, or None
        if `obj` carries no output badge (leaf nodes that can drive the display opt in; a
        disabled-but-shown badge returns False). Default None."""
        return None

    # ---- structural mutations (families NotImplement what they lack) ---------

    def set_bypassed(self, obj, flag):
        """Editor mutation (L2, STRUCTURAL): toggle a node/construct bypass flag."""
        self._unsupported("set_bypassed")

    def select_output(self, target):
        """Mark `target` (a family node handle, or None to clear) as the document's
        display output."""
        self._unsupported("select_output")

    def add_node(self, *args, **kwargs):
        """Editor mutation (L2, STRUCTURAL): add a node to the document."""
        self._unsupported("add_node")

    def delete_node(self, node):
        """Editor mutation (L2, STRUCTURAL): remove a node from the document."""
        self._unsupported("delete_node")

    def connect(self, *args, **kwargs):
        """Editor mutation (L2, STRUCTURAL): create a typed edge."""
        self._unsupported("connect")

    def disconnect(self, *args, **kwargs):
        """Editor mutation (L2, STRUCTURAL): remove an edge."""
        self._unsupported("disconnect")

    # ---- node positions (family decides the backing store) -------------------

    def node_pos(self, key):
        """The (x, y) canvas position stored for the node at tree-path `key`, or
        None. The base default is an in-memory dict; GraphData-direct families
        override to read the graph-level `_editor_layout`."""
        return self._node_positions.get(key)

    def set_node_pos(self, key, x, y):
        """Store the canvas position of the node at tree-path `key`."""
        self._node_positions[key] = (float(x), float(y))

    @property
    def node_layout(self):
        """key -> (x, y) for every positioned node."""
        return dict(self._node_positions)

    # ---- serialization framing (family owns its wire format) -----------------

    def to_json(self):
        """Serialize the document. Terrain returns its lossless doc-JSON dict;
        GraphData-direct families return the reflection JSON string. Family-specific."""
        self._unsupported("to_json")

    @classmethod
    def from_json(cls, data):
        """Reconstruct a document from to_json() output. Family-specific."""
        raise NotImplementedError(
            f"{cls.__name__} does not implement from_json.")

    # ---- undo checkpoint contract (composes with editor/undo_stack.py) -------
    # The host records an ALREADY-APPLIED mutation with UndoStack.record(pre, post,
    # restore=...). capture_checkpoint() supplies an OPAQUE snapshot for pre/post;
    # restore_checkpoint() turns a snapshot back into a document the host installs.

    def capture_checkpoint(self):
        """An opaque, restorable snapshot of the whole document (its to_json())."""
        return self.to_json()

    @classmethod
    def restore_checkpoint(cls, snapshot):
        """Rebuild a document from a capture_checkpoint() snapshot (the host swaps
        the live document for the returned one)."""
        return cls.from_json(snapshot)

    # ---- elaboration (the ONLY constructor of the runnable GraphData) --------

    def elaborate(self, *args, **kwargs):
        """Derive / return the runnable dflow.GraphData this document represents.
        ABSTRACT — every specialization implements it (terrain DERIVES a fresh graph;
        GraphData-direct families return the wrapped graph, the identity).

        The canonical product is the GraphData. A family MAY return it directly
        (GraphDataDocument) or as the first element of a family-specific tuple where the
        graph carries a side-channel — terrain returns `(graph, capture_map)`, its display-
        capture map — so a generic consumer takes `g[0] if isinstance(g, tuple) else g`."""
        raise NotImplementedError(
            f"{type(self).__name__} must implement elaborate() -> GraphData.")
