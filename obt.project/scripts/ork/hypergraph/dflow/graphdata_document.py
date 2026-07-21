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

    def __init__(self, graph, capabilities=None):
        super().__init__()
        if graph is None:
            raise GraphDocumentError(
                "GraphDataDocument requires a live dflow.GraphData to wrap")
        self._graph = graph
        # OPTIONAL family capability mask (injected by the family loader; the document
        # itself imports NO family). None = the family-neutral defaults below (bypassable
        # iff a module has an input plug; display flags supported). A family with a
        # capability object (particles: no display flags + role-based bypassability) has
        # its badges + bypass refusal + display affordance deferred to that object, so the
        # shell / node model stay family-neutral.
        self._caps = capabilities
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

    def node_class_name(self, node):
        """The reflected class name for the E1 property-metadata surface (enum choices,
        editor.range, describeX prop_meta). Same as node_class — enables reflection-derived
        editors for a GraphData family's module properties + input plugs."""
        return self.node_class(node)

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
        # bypassable + active(_bypassed). A family capability mask (particles) classifies
        # bypassability by node ROLE; the family-neutral default is "bypassable iff the
        # module has an input plug to pass through".
        mod = self._graph.findModule(obj)
        if mod is None:
            return None
        if self._caps is not None:
            return (bool(self._caps.bypassable(mod.clazz.name)), bool(mod.bypassed))
        spec = _dflow.plugSpec(mod.clazz.name) or {"inputs": []}
        bypassable = len(spec.get("inputs", [])) > 0
        return (bypassable, bool(mod.bypassed))

    def node_output_badge(self, obj):
        # A family with NO per-node display marker (particles: render terminals are chosen by
        # bypass, not a display flag) declares supports_display_flags=False — the display
        # affordance is then ABSENT (never offered), not merely inert. Default: any module
        # with an output plug can be marked as the display output.
        if self._caps is not None and not self._caps.supports_display_flags:
            return False
        mod = self._graph.findModule(obj)
        if mod is None:
            return None
        spec = _dflow.plugSpec(mod.clazz.name) or {"outputs": []}
        return len(spec.get("outputs", [])) > 0

    def node_icon(self, obj):
        # CATEGORY ICON (family capability): the standard_icons registry name of a node's
        # category glyph, or None. A family capability object may classify nodes into
        # categories that carry a canvas icon (particles: force / pool / render); the
        # family-neutral default (no capabilities, or a capabilities object without a
        # node_icon accessor) is None — no icon, unchanged rendering.
        if self._caps is None:
            return None
        fn = getattr(self._caps, "node_icon", None)
        if fn is None:
            return None
        cls = self.node_class(obj)
        return fn(cls) if cls else None

    # ---- node-level parameters ----------------------------------------------

    # reflected MODULE properties the propsheet must NOT surface as free-edit rows: the plug
    # arrays, the editor-infra flags (bypass rides the outliner badge; cachepoint/viewable are
    # cook/display infra). Expr-tree fields + sub-object/array values are excluded dynamically.
    _MODULE_PROP_EXCLUDE = frozenset({"inputs", "outputs", "bypassed", "cachepoint", "viewable"})

    def editable_params(self, node):
        """Ordered editable params: the input plugs FIRST, then the module's reflected NON-plug
        scalar/bool/enum properties (e.g. SpriteRenderer.sort / draw_order). Plug values are
        read from the module's reflection JSON so transform (FloatXf/Vec*Xf) plug scalars
        surface too; module props route through get_param/set_param('module', ...) with the
        reflection-derived enum/range editors the property model already applies."""
        rows = [("inputs", k, v) for (k, v) in self._module_input_values(node)]
        rows += self._module_editable_props(node)
        return rows

    def _module_editable_props(self, node):
        """[("module", name, value)] for a node's reflected NON-plug scalar/bool/enum
        properties — the tweakables edited alongside plugs. Excludes plug arrays + editor-infra
        flags (_MODULE_PROP_EXCLUDE), the expr-tree fields (S7 detail editor, surfaced
        separately), and sub-object/array values (a nested-object editor is out of scope; a
        material sub-object is not a scalar tweakable)."""
        exclude = set(self._MODULE_PROP_EXCLUDE)
        exclude |= {f for (f, _c) in self.expr_fields(node)}
        out = []
        for name, val in self._module_props(node).items():
            if name in exclude or isinstance(val, (dict, list)):
                continue
            out.append(("module", name, val))
        return out

    def plug_is_connected(self, node, name):
        """Whether input plug `name` is fed by an edge (live reflection). Drives the
        propsheet value-ghosting for connected plugs (owner requirement 1)."""
        mod = self._require_module(node)
        plug = getattr(mod.inputs, name)
        return bool(plug is not None and plug.is_connected)

    def plug_transformer(self, node, name):
        """The floatxf.floatxfdata transformer on a FloatXf-typed input plug `name`, or
        None (plain plug / no transformer). Live handle — editing its reflected items
        round-trips to the serialized GraphData and re-parameterizes the bake."""
        mod = self._require_module(node)
        plug = getattr(mod.inputs, name)
        if plug is None:
            return None
        return getattr(plug, "transformer", None)   # only FloatXfInPlugData carries it

    # ---- editor expression fields (E2.5 S7) ----------------------------------
    # A reflected MODULE property that stores an ExprIR tree (JSON) and is edited AS SOURCE
    # through the propsheet detail editor. The family capability object declares which
    # properties are expr fields + their context; parse/print route through it (family owns
    # its vocabularies). Family-neutral: a document with no such capability exposes none.

    def expr_fields(self, node):
        """[(reflected_property, context_name)] for a node's editable ExprIR fields, or []."""
        if self._caps is None:
            return []
        fn = getattr(self._caps, "expr_fields", None)
        if fn is None:
            return []
        return fn(self.node_class(node))

    def expr_field_source(self, node, field):
        """The author SOURCE of an expr field (the stored tree pretty-printed via the field's
        context). '' when the field is unset."""
        ctx = self._expr_field_context(node, field)
        js = self._module_props(node).get(field, "")
        return self._caps.print_expr(js, ctx)

    def set_expr_field(self, node, field, source):
        """Editor mutation (L2): parse+validate author SOURCE against the field's context and
        write the canonical ExprIR JSON to the reflected property. Raises loudly on an
        invalid/dishonest expression (the document is UNCHANGED — the write never happens)."""
        ctx = self._expr_field_context(node, field)
        js = self._caps.parse_expr(source, ctx)   # loud on invalid, before any mutation
        mod = self._require_module(node)
        setattr(mod.properties, field, js)
        return js

    def _expr_field_context(self, node, field):
        for (f, ctx) in self.expr_fields(node):
            if f == field:
                return ctx
        raise GraphDocumentError(
            f"node {node!r} has no expr field {field!r}; have {[f for (f, _c) in self.expr_fields(node)]}")

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
        # ops-self-defend: a family that classifies bypassability by role (particles:
        # POOL / EMITTER / side providers are NOT bypassable) refuses a bad bypass LOUDLY
        # so the flag is never marked (the node model surfaces the reason in the status line).
        if flag and self._caps is not None:
            reason = self._caps.refuse_bypass_reason(mod.clazz.name)
            if reason is not None:
                raise GraphDocumentError(
                    f"cannot bypass {node!r} [{mod.clazz.name}]: {reason}")
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

    def add_node_by_class_name(self, class_name, name=None):
        """Add a node from a REFLECTED CLASS NAME (the editor add-menu path). moduleClasses()-
        VALIDATED (drift-proof), then created via the engine's rtti shared factory
        (GraphData.createByClassName) — no per-class pybind wrapper needed, so any registered
        DgModuleData subclass adds. Returns the assigned node name."""
        if class_name not in self._valid_class_names():
            raise GraphDocumentError(
                f"class {class_name!r} is not a registered dflow module class (moduleClasses())")
        if name is None:
            name = self._auto_name(class_name)
        if name in self._nodes:
            raise GraphDocumentError(f"a node named {name!r} already exists in this graph")
        self._graph.createByClassName(name, class_name)   # rtti factory + addModule
        self._nodes[name] = class_name
        return name

    def plugs_compatible(self, src_node, src_plug, dst_node, dst_plug):
        """Engine connectability verdict (GraphData.plugsCompatible: strict flow-type + fan-out)
        for a producer out-plug -> consumer in-plug. Raises GraphDocumentError if either
        endpoint plug is unknown (the canvas pre-connect gate composes the human reason)."""
        smod = self._require_module(src_node)
        dmod = self._require_module(dst_node)
        out_plug = getattr(smod.outputs, src_plug)
        in_plug  = getattr(dmod.inputs, dst_plug)
        if out_plug is None:
            raise GraphDocumentError(f"node {src_node!r} has no output plug {src_plug!r}")
        if in_plug is None:
            raise GraphDocumentError(f"node {dst_node!r} has no input plug {dst_plug!r}")
        return bool(self._graph.plugsCompatible(in_plug, out_plug))

    def delete_node(self, node):
        """Editor mutation (L2, STRUCTURAL): remove a module and every edge touching it
        (both directions) via GraphData.removeModule. removeModule also drops the module's
        editor-layout entry and clears the output marker if it named this node, so the doc
        mirror only has to forget the name. Loud (GraphDocumentError) on an unknown node."""
        mod = self._require_module(node)
        self._graph.removeModule(mod)
        self._nodes.pop(node, None)
        return node

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
        # modules + plug values + edges + positions). With NO document params this returns
        # the PLAIN reflection JSON string byte-identically (a doc-less .orj). With E0
        # document params present it returns the E0 ENVELOPE string {"params":..,
        # "graphdata":..} so a doc-less family can still carry asset-level params — nothing
        # is silently dropped. Always a JSON STRING (the GraphData-family wire contract).
        graph_js = self._graph.serializeJson()
        if not len(self.params):
            return graph_js
        return _json.dumps({
            "params": self._encode_params(),
            "graphdata": _json.loads(graph_js),
        })

    @classmethod
    def from_json(cls, data):
        # Shape-detect the E0 envelope vs a plain reflection JSON (NOT a legacy shim — a
        # doc-less .orj simply carries no document params). An envelope is the only dict form
        # with BOTH "params" and "graphdata" keys; the graph reflection JSON is keyed on
        # "root". Accepts a JSON string OR an already-parsed dict either way.
        envelope = cls._as_envelope(data)
        if envelope is not None:
            gdata = envelope["graphdata"]
            graph = _Object.deserializeJson(
                gdata if isinstance(gdata, str) else _json.dumps(gdata))
            doc = cls(graph)
            cls._decode_params_into(doc.params, envelope["params"])
            return doc
        graph = _Object.deserializeJson(data if isinstance(data, str) else _json.dumps(data))
        return cls(graph)

    # ---- E0 document-params envelope (round-trips the base ParamTable) --------
    # Encoded purely through the ParamTable PUBLIC surface (names/get/tag_of/is_structural
    # + declare/mark_structural) so nothing here reaches into the base table internals.

    @staticmethod
    def _as_envelope(data):
        if isinstance(data, dict):
            d = data
        else:
            try:
                d = _json.loads(data)
            except (ValueError, TypeError):
                return None
        if isinstance(d, dict) and "params" in d and "graphdata" in d:
            return d
        return None

    def _encode_params(self):
        pt = self.params

        def _enc(v):
            # JSON has no tuple type — tag vec/tuple values so decode restores them (the
            # property sheet keys tuple vs scalar rows off the python type).
            return {"__tuple__": list(v)} if isinstance(v, tuple) else v

        return {n: {"value": _enc(pt.get(n)),
                    "tag": pt.tag_of(n),
                    "structural": pt.is_structural(n)}
                for n in pt.names()}

    @staticmethod
    def _decode_params_into(pt, params_dict):
        def _dec(v):
            return tuple(v["__tuple__"]) if isinstance(v, dict) and "__tuple__" in v else v

        for name, entry in params_dict.items():
            pt.declare(name, _dec(entry.get("value")), entry.get("tag"))
            if entry.get("structural"):
                pt.mark_structural(name)

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
