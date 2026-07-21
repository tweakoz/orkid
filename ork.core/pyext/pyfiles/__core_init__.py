from ._core import *
from ._core import dataflow as _dataflow


def _dataflow_to_json_schema():
    """ONE json-ready document describing every registered DgModuleData subclass —
    the E8/MCP schema feed. Per module class: reflected name, family tag, CLASS
    annotations (the reflection-carried add palette: dsl.verb / editor.palette.*),
    reflected properties (type label, enum choice lists, property annotations e.g.
    editor.range.min/max), and the plug schema (name/type/rate + min/max range +
    display_only markers). Assembled purely from the live reflection surface
    (moduleClasses + plugSpec) — no hand tables, so the schema always tracks the
    code. Call AFTER engine init (module classes register at appinit); the result
    is plain dict/list/scalar, so json.dumps(...) serializes it directly."""
    modules = []
    for c in _dataflow.moduleClasses():
        spec = _dataflow.plugSpec(c["name"]) or {}
        modules.append({
            "name":        c["name"],
            "family":      c["family"],
            "annotations": dict(c.get("annotations", {})),
            "properties":  [dict(p) for p in c.get("properties", ())],
            "inputs":      [dict(p) for p in spec.get("inputs", ())],
            "outputs":     [dict(p) for p in spec.get("outputs", ())],
        })
    modules.sort(key=lambda m: m["name"])
    return {"schema": "dflow.modules", "version": 1, "modules": modules}


# python-side assembler attached onto the compiled submodule so callers reach it as
# core.dataflow.to_json_schema() — the same surface as moduleClasses()/plugSpec().
_dataflow.to_json_schema = _dataflow_to_json_schema
