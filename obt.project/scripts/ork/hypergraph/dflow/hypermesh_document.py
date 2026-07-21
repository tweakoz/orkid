###############################################################################
# ork.hypergraph.dflow.hypermesh_document — the HYPERMESH GraphDocument
# specialization (JUL13_DFLOW E2).
#
# Hypermesh is a doc-less GraphData-direct family: its serialized form already IS
# the reflection JSON of a dflow.GraphData (Hypermesh.save() writes exactly that).
# So the document layer inherits GraphDataDocument WHOLE — node/edge enumeration,
# param read/write, positions, the E0 doc-params envelope, elaborate()=identity —
# and adds only the thin family identity it has EARNED: a family tag and the proven
# save/load file path (Hypermesh.save's serializeJson + load_graphdata's deserialize),
# so an editor document round-trips against the SAME on-disk .hmgraph.json artifact.
#
# Import-cheap by construction (no lev2 / hypermesh-package import at load time): it
# wraps any live GraphData generically, exactly like its base.
###############################################################################

from orkengine.core import Object as _Object
from .graphdata_document import GraphDataDocument
from .document import GraphDocumentError


# EDITOR EXPRESSION FIELDS (E2.5 S6/S7): the reflected `*_tree` properties on the hypermesh
# select / extrude modules that store a canonical hypermesh.selexpr ExprIR tree (JSON) and are
# edited AS SOURCE through the propsheet detail editor. Each maps a reflected class ->
# [(reflected_property, expression_context_name)]. The GLSL eval form (`_predicate` /
# `_dist_pred` / ...) stays the shader's compiled form — S6 keeps the tree as the STORAGE /
# cook-hash identity — so editing a tree re-keys the cook cache but does NOT re-emit shader
# (there is no tree -> GLSL lowerer in v1; see exprir_selexpr's deferred-text-round-trip note).
_EXPR_FIELDS = {
    "hypermesh::SelectData": [
        ("predicate_tree", "hypermesh.selexpr"),
    ],
    "hypermesh::ExtrudeFacesData": [
        ("dist_tree",  "hypermesh.selexpr"),
        ("inset_tree", "hypermesh.selexpr"),
        ("dir_tree",   "hypermesh.selexpr"),
        ("twist_tree", "hypermesh.selexpr"),
        ("scale_tree", "hypermesh.selexpr"),
    ],
}


class HypermeshDocument(GraphDataDocument):
    """A GraphDataDocument specialized for the hypermesh family. Wraps a live hypermesh
    dflow.GraphData; save()/load() ride the SAME plain reflection-JSON artifact that
    Hypermesh.save() / load_graphdata() produce and the C++ host reads, so a document
    round-trips against on-disk hypermesh assets unchanged."""

    FAMILY = "hypermesh"

    @property
    def family(self):
        """The graph-family tag this document hosts — 'hypermesh'."""
        return self.FAMILY

    def save(self, path):
        """Write the wrapped graph as the portable hypermesh artifact — the EXACT bytes
        Hypermesh.save() writes (graphdata.serializeJson()), so load_graphdata() and the
        C++ host read it unchanged. Document params (E0) do NOT ride this interop artifact;
        if any are present save refuses LOUDLY (ops self-defend) and directs the caller to
        the document snapshot path (to_json / capture_checkpoint carries the envelope)."""
        if len(self.params):
            raise GraphDocumentError(
                "HypermeshDocument.save writes the plain interop artifact, which cannot "
                "carry document params — use to_json()/capture_checkpoint() (the E0 "
                "envelope) for a param-bearing document snapshot.")
        with open(path, "w") as f:
            f.write(self._graph.serializeJson())
        return path

    @classmethod
    def load(cls, path):
        """Load a Hypermesh.save() / load_graphdata() artifact -> HypermeshDocument (no
        Python DSL re-run; the deserialized graph IS the document)."""
        with open(path) as f:
            graph = _Object.deserializeJson(f.read())
        return cls(graph)

    # ---- editor EXPRESSION-TREE fields (E2.5 S6/S7 — hypermesh SelExpr) --------
    # Overridden DIRECTLY here (not via a capability mask): the base capability protocol also
    # carries bypass / display-flag behavior, and hypermesh handles those at the node-model
    # layer — so injecting a caps object would either re-implement that protocol or perturb the
    # current badge behavior. These three overrides add ONLY the expr surface, leaving bypass /
    # display / icons untouched (the family-neutral GraphDataDocument defaults).

    def expr_fields(self, node):
        """[(reflected_property, context_name)] for a select/extrude module's editable
        hypermesh.selexpr ExprIR-tree fields, or []. Only fields that CURRENTLY hold a tree
        are surfaced (an unset `*_tree` string is not an editable expression)."""
        fields = _EXPR_FIELDS.get(self.node_class(node) or "", [])
        if not fields:
            return []
        props = self._module_props(node)
        return [(f, ctx) for (f, ctx) in fields if props.get(f)]

    def expr_field_source(self, node, field):
        """Author SOURCE of a `*_tree` field: the stored canonical ExprIR JSON pretty-printed
        against the hypermesh.selexpr context. '' when the field is unset."""
        js = self._module_props(node).get(field, "")
        if not js:
            return ""
        from ..exprir import decode_json, pretty_print
        from .hypermesh.exprir_selexpr import SELEXPR_CTX
        return pretty_print(decode_json(js), SELEXPR_CTX)

    def set_expr_field(self, node, field, source):
        """Editor mutation (L2): PARSE + VALIDATE author SOURCE against the hypermesh.selexpr
        vocabulary, then write the canonical (sorted, byte-stable) ExprIR JSON back to the
        reflected `*_tree` property. Raises loudly (ExprParseError / ExprSignatureError) on an
        out-of-vocabulary / dishonest expression — the document is UNCHANGED (the write never
        happens). The serialization FORM is unchanged (encode_json == capture_json's form), so
        no cook-cache salt bump is needed — the salt already covers tree-content changes."""
        self._require_selexpr_field(node, field)
        from ..exprir import parse, encode_json
        from .hypermesh.exprir_selexpr import SELEXPR_CTX
        tree = parse(source, SELEXPR_CTX)     # loud on invalid, before any mutation
        js = encode_json(tree)
        mod = self._require_module(node)
        setattr(mod.properties, field, js)
        return js

    def _require_selexpr_field(self, node, field):
        fields = [f for (f, _c) in _EXPR_FIELDS.get(self.node_class(node) or "", [])]
        if field not in fields:
            raise GraphDocumentError(
                f"node {node!r} [{self.node_class(node)}] has no hypermesh.selexpr field "
                f"{field!r}; have {fields}")
