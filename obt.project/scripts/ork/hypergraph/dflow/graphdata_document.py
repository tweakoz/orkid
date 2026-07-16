###############################################################################
# ork.hypergraph.dflow.graphdata_document — the DEFAULT GraphDocument for
# doc-less families (particles first, hypermesh next).
#
# These families have no structured document layer: their serialized form already
# IS the reflection JSON of a dflow.GraphData. So the editor document just WRAPS a
# LIVE GraphData and edits it directly through the E1 introspection bindings +
# the GraphData mutation API. elaborate() returns that same graph — the IDENTITY:
# for these families the graph IS the document (JUL13_DFLOW E2). Almost no new
# machinery — this is family #2's (particles) editor document.
#
# MUST NOT import any family (terrain / particles / hypermesh): it edits any
# GraphData generically via reflection, so it stays family-neutral by construction.
###############################################################################

import json as _json

from orkengine.core import dataflow as _dflow
from orkengine.core import Object as _Object
from .document import GraphDocument, GraphDocumentError


class GraphDataDocument(GraphDocument):
    """A GraphDocument backed by a live dflow.GraphData. Node/edge enumeration,
    param read/write, add/connect/disconnect, positions and serialization all go
    through the reflection substrate (E1 introspection bindings + GraphData API), so
    it hosts particles, hypermesh, and any future GraphData-direct family unchanged."""

    def __init__(self, graph):
        super().__init__()
        if graph is None:
            raise GraphDocumentError(
                "GraphDataDocument requires a live dflow.GraphData to wrap")
        self._graph = graph
        # ordered {module_name: reflected_class_name}, the flat node list. GraphData
        # exposes no python module-by-index binding, so the family-neutral node list is
        # parsed from the graph's reflection JSON (the Modules object-map) — stable and
        # authoritative — then maintained across add_node. refresh() re-seeds it.
        self._nodes = self._read_nodes(graph)
        self._valid_classes = None      # lazily-built moduleClasses() name set (add_node)

    # ---- node enumeration ----------------------------------------------------

    @staticmethod
    def _read_nodes(graph):
        nodes = {}
        try:
            root = _json.loads(graph.serializeJson())
            mods = root["root"]["object"]["properties"].get("Modules", {})
            for name, entry in mods.items():
                nodes[name] = entry.get("object", {}).get("class", "")
        except Exception:
            pass
        return nodes

    def refresh(self):
        """Re-seed the node list from the live graph (after an external mutation)."""
        self._nodes = self._read_nodes(self._graph)

    def nodes(self):
        """[(name, reflected_class_name)] for every module, in enumeration order."""
        return list(self._nodes.items())

    def node_class(self, name):
        """Reflected class name of a node (live-authoritative via reflection)."""
        m = self._graph.findModule(name)
        if m is not None:
            return m.clazz.name
        return self._nodes.get(name)

    def tree_paths(self):
        # GraphData graphs are FLAT (no nested loop/group constructs) — key == module
        # name, obj == module name (the node handle these families take).
        return [("", name, name) for name in self._nodes]

    def edges(self):
        """Every typed edge as (out_module, out_plug, out_type, in_module, in_plug,
        in_type) dicts — the E1 GraphData.edges() introspection."""
        return self._graph.edges()

    # ---- editor presentation surface (badges / display name) -----------------
    # obj is a module NAME (the flat node handle); the state is read live from the graph
    # via reflection so it never drifts.

    def display_name(self, obj, key):
        cls = self.node_class(obj)
        return f"{obj}  [{cls}]" if cls else str(obj)

    def node_bypass_badge(self, obj):
        # bypassable iff the module has an input plug to pass through; active = its live
        # reflected _bypassed flag.
        mod = self._graph.findModule(obj)
        if mod is None:
            return None
        spec = _dflow.plugSpec(mod.clazz.name) or {"inputs": []}
        bypassable = len(spec.get("inputs", [])) > 0
        return (bypassable, bool(mod.bypassed))

    def node_output_badge(self, obj):
        # any module with an output plug can be marked as the display output.
        mod = self._graph.findModule(obj)
        if mod is None:
            return None
        spec = _dflow.plugSpec(mod.clazz.name) or {"outputs": []}
        return len(spec.get("outputs", [])) > 0

    # ---- node-level parameters ----------------------------------------------

    def editable_params(self, node):
        """Ordered [("inputs", plug_name, value)] for a node's input plugs. Values are
        read from the module's reflection JSON so transform (FloatXf/Vec*Xf) plug scalars
        surface too (the live plug .value binding decodes only plain scalar plugs)."""
        vals = self._module_input_values(node)
        return [("inputs", k, v) for (k, v) in vals]

    def get_param(self, node, kind, name):
        """Current value of a node param. kind='inputs' (a plug) or 'module' (a reflected
        prop). Read from the module's reflection JSON (robust across every plug type)."""
        if kind == "inputs":
            for (k, v) in self._module_input_values(node):
                if k == name:
                    return v
            raise GraphDocumentError(
                f"node {node!r} has no input plug {name!r}")
        if kind == "module":
            props = self._module_props(node)
            if name not in props:
                raise GraphDocumentError(
                    f"node {node!r} has no reflected property {name!r}")
            return props[name]
        raise GraphDocumentError(f"unknown param kind {kind!r} (want 'inputs' or 'module')")

    def set_param(self, node, kind, name, value):
        """Editor mutation (L2): write a node param through the plug/prop proxies."""
        mod = self._require_module(node)
        if kind == "inputs":
            setattr(mod.inputs, name, value)      # loud py error on unknown plug
        elif kind == "module":
            setattr(mod.properties, name, value)
        else:
            raise GraphDocumentError(
                f"unknown param kind {kind!r} (want 'inputs' or 'module')")
        return value

    # ---- structural mutations ------------------------------------------------

    def set_bypassed(self, node, flag):
        mod = self._require_module(node)
        mod.bypassed = bool(flag)
        return bool(flag)

    def select_output(self, node):
        if node is not None and node not in self._nodes:
            raise GraphDocumentError(f"select_output: no node named {node!r}")
        self._graph.output_node = node if node is not None else ""
        return node

    def add_node(self, module_clazz, name=None):
        """Add a node from a pybind-wrapped dflow module class (the same handle
        graph.create accepts, e.g. lev2.particles.Gravity). moduleClasses()-VALIDATED
        (drift-proof; no hand-maintained table) via a discarded scratch instance, then
        created + added to the live graph. Returns the assigned node name."""
        if not hasattr(module_clazz, "createShared"):
            raise GraphDocumentError(
                "add_node expects a dflow module class (one with a createShared factory)")
        scratch  = module_clazz.createShared()
        reflected = scratch.clazz.name
        if reflected not in self._valid_class_names():
            raise GraphDocumentError(
                f"class {reflected!r} is not a registered dflow module class (moduleClasses())")
        if name is None:
            name = self._auto_name(reflected)
        if name in self._nodes:
            raise GraphDocumentError(f"a node named {name!r} already exists in this graph")
        self._graph.create(name, module_clazz)    # createShared + addModule
        self._nodes[name] = reflected
        return name

    def delete_node(self, node):
        # GraphData::removeModule exists in C++ but is not exposed to Python; this slice
        # is python-only, so deletion is genuinely unavailable — fail LOUD (ops-self-defend)
        # rather than silently no-op. Rebuild the graph without the node, or add the binding.
        raise NotImplementedError(
            "GraphDataDocument.delete_node is unavailable: GraphData::removeModule is not "
            "bound to Python in this slice (needs a pyext binding).")

    def connect(self, dst_node, dst_plug, src_node, src_plug):
        """Editor mutation (L2, STRUCTURAL): create a typed edge (producer out-plug ->
        consumer in-plug). TYPED-CONNECT is enforced HERE at the document layer — a plug
        data-type mismatch raises a loud, CATCHABLE error (the E3 canvas's canConnect gate),
        never reaching C++ safeConnect's OrkAssert. Consistent with GraphData::canConnect
        (which compares the same GetDataTypeId the plug type_name renders)."""
        dmod = self._require_module(dst_node)
        smod = self._require_module(src_node)
        in_plug  = getattr(dmod.inputs, dst_plug)
        out_plug = getattr(smod.outputs, src_plug)
        if in_plug is None:
            raise GraphDocumentError(f"node {dst_node!r} has no input plug {dst_plug!r}")
        if out_plug is None:
            raise GraphDocumentError(f"node {src_node!r} has no output plug {src_plug!r}")
        if in_plug.type_name != out_plug.type_name:
            raise GraphDocumentError(
                f"typed-connect REJECTED: {src_node}.{src_plug} [{out_plug.type_name}] -> "
                f"{dst_node}.{dst_plug} [{in_plug.type_name}] — plug data types differ")
        self._graph.connect(in_plug, out_plug)
        return (src_node, src_plug, dst_node, dst_plug)

    def disconnect(self, dst_node, dst_plug):
        """Editor mutation (L2, STRUCTURAL): remove the edge feeding a consumer in-plug."""
        dmod = self._require_module(dst_node)
        in_plug = getattr(dmod.inputs, dst_plug)
        if in_plug is None:
            raise GraphDocumentError(f"node {dst_node!r} has no input plug {dst_plug!r}")
        self._graph.disconnect(in_plug)

    # ---- node positions (ride the C++ graph-level _editor_layout, E1) --------

    def node_pos(self, key):
        p = self._graph.nodePos(key)          # fvec2 or None
        return (p.x, p.y) if p is not None else None

    def set_node_pos(self, key, x, y):
        self._graph.setNodePos(key, float(x), float(y))

    @property
    def node_layout(self):
        return {k: (v.x, v.y) for (k, v) in self._graph.node_layout.items()}

    # ---- serialization -------------------------------------------------------

    def to_json(self):
        # The graph IS the document — serialize it via the object serializer (round-trips
        # modules + plug values + edges + positions). Document-level params (E0) are not yet
        # part of any GraphData-family wire form; refuse loudly rather than silently drop them.
        if len(self.params):
            raise GraphDocumentError(
                "GraphDataDocument document-param serialization is not wired in this slice "
                "(params non-empty) — the graph JSON alone would silently drop them.")
        return self._graph.serializeJson()

    @classmethod
    def from_json(cls, data):
        graph = _Object.deserializeJson(data)
        return cls(graph)

    # ---- elaboration = IDENTITY (the graph IS the document) ------------------

    def elaborate(self, *args, **kwargs):
        return self._graph

    # ---- internals -----------------------------------------------------------

    def _require_module(self, name):
        mod = self._graph.findModule(name)
        if mod is None:
            raise GraphDocumentError(
                f"no module named {name!r} in the graph; have {list(self._nodes)}")
        return mod

    def _valid_class_names(self):
        if self._valid_classes is None:
            self._valid_classes = {c["name"] for c in _dflow.moduleClasses()}
        return self._valid_classes

    def _auto_name(self, reflected):
        base = reflected.split("::")[-1]
        for suffix in ("ModuleData", "Data"):
            if base.endswith(suffix):
                base = base[: -len(suffix)]
                break
        base = base.lower() or "node"
        n = 0
        while f"{base}_{n}" in self._nodes:
            n += 1
        return f"{base}_{n}"

    def _module_props(self, node):
        """The module's reflected scalar properties (name -> value) from its JSON."""
        mod = self._require_module(node)
        try:
            mj = _json.loads(mod.serializeJson())
            return mj["root"]["object"]["properties"]
        except Exception:
            return {}

    def _module_input_values(self, node):
        """[(plug_name, value)] for a node's input plugs. Positional: the serialized
        input-plug array order matches reshapeIOs order == plugSpec() order (both iterate
        the same _inputs vector), so plugSpec names align index-for-index with the JSON."""
        mod  = self._require_module(node)
        spec = _dflow.plugSpec(mod.clazz.name) or {"inputs": []}
        try:
            mj  = _json.loads(mod.serializeJson())
            arr = mj["root"]["object"]["properties"].get("inputs", [])
        except Exception:
            arr = []
        out = []
        for i, p in enumerate(spec["inputs"]):
            val = None
            if i < len(arr):
                props = arr[i].get("object", {}).get("properties", {})
                val   = props.get("value")
            out.append((p["name"], val))
        return out
