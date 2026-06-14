###############################################################################
# ork.hypergraph.ptex3d.resolve — bare-name resolution for ptex3d MATERIAL files
# (Ptex3d subclasses). Mirrors dflow/terrain/resolve.py.
#
# A viewer/tool resolves a material a terrain DSL names (its MATERIAL attr, or a
# -M flag). The search order puts the CALLER's extra dirs FIRST — the viewer passes
# the terrain asset's own folder — so a bespoke material can live RIGHT NEXT TO its
# terrain. Then ORK_PTEX3D_SEARCH_PATH, then the shared <hypergraph>/assets/materials.
#   resolve_material("hmview")                 -> assets/materials/hmview.py
#   resolve_material("xxx2_mat", [terrain_dir]) -> terrain_dir/xxx2_mat.py  (co-located)
###############################################################################

import importlib.util
import inspect
import os
import sys
from pathlib import Path

# resolve.py is at ork/hypergraph/ptex3d/resolve.py -> parents[1] == ork/hypergraph.
_DEFAULT_MATERIALS = Path(__file__).resolve().parents[1] / "assets" / "materials"


def search_path(extra_dirs=()):
  """Ordered dirs searched for a bare material name: caller's extra_dirs first (e.g.
  the terrain asset folder), then ORK_PTEX3D_SEARCH_PATH (colon-sep), then materials/."""
  dirs = [Path(d) for d in extra_dirs]
  raw = os.environ.get("ORK_PTEX3D_SEARCH_PATH")
  if raw:
    dirs += [Path(p).expanduser() for p in raw.split(":") if p]
  dirs.append(_DEFAULT_MATERIALS)
  return dirs


def resolve_material(arg, extra_dirs=()):
  """Turn a name/path into an existing .py; raises FileNotFoundError on miss."""
  direct = Path(arg).expanduser()
  if direct.is_file():
    return direct.resolve()
  if direct.suffix == "" and direct.with_suffix(".py").is_file():
    return direct.with_suffix(".py").resolve()
  for src in search_path(extra_dirs):
    for candidate in (src / arg, src / f"{arg}.py"):
      if candidate.is_file():
        return candidate.resolve()
  searched = "\n  ".join(str(s) for s in search_path(extra_dirs))
  raise FileNotFoundError(
    f"could not resolve ptex3d material {arg!r}. searched:\n  {searched}\n"
    f"(set ORK_PTEX3D_SEARCH_PATH to add dirs)")


def load_material_class(path, class_name=None):
  """Import path, return the Ptex3d subclass to materialize (auto-find, or class_name=
  to disambiguate a multi-class file)."""
  from ork.hypergraph.ptex3d import Ptex3d   # lazy: avoid import cycle at module load
  path = Path(path)
  if not path.exists():
    raise FileNotFoundError(f"ptex3d material file not found: {path}")
  spec = importlib.util.spec_from_file_location("ptex3d_material_module", str(path))
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  candidates = [
    cls for _, cls in inspect.getmembers(module, inspect.isclass)
    if issubclass(cls, Ptex3d)
       and cls is not Ptex3d
       and cls.__module__ == module.__name__
  ]
  if class_name is not None:
    for cls in candidates:
      if cls.__name__ == class_name:
        return cls
    raise ValueError(
      f"--class {class_name!r} not found in {path.name}. "
      f"Available: {[c.__name__ for c in candidates]}")
  if not candidates:
    raise ValueError(
      f"No Ptex3d subclass found in {path.name}. "
      f"Define a class that subclasses ork.hypergraph.ptex3d.Ptex3d.")
  if len(candidates) > 1:
    raise ValueError(
      f"Multiple Ptex3d subclasses in {path.name}: "
      f"{[c.__name__ for c in candidates]}. Pass class_name=<name>.")
  return candidates[0]
