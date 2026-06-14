###############################################################################
# ork.hypergraph.dflow.terrain.resolve — DSL bare-name resolution for terrain
# HeightField DSL files (used at AUTHORING time by the HeightField asset wrapper
# to run a DSL file once and embed its graph). Mirrors particles/resolve.py.
#
#  - explicit path (relative-to-cwd or absolute) -> use as-is
#  - bare name -> search ORK_TERRAIN_SEARCH_PATH (colon-separated); default
#                 <ork.data>/terrain
###############################################################################

import importlib.util
import inspect
import os
import sys
from pathlib import Path

from .base import HeightField

# terrain graph assets live in the hypergraph assets package (peer of
# assets/mesh). resolve.py is at ork/hypergraph/dflow/terrain/resolve.py, so
# parents[2] == ork/hypergraph.
_DEFAULT_TERRAIN_ASSETS = Path(__file__).resolve().parents[2] / "assets" / "terrain"


def search_path():
  """Ordered dirs searched for bare DSL names. From ORK_TERRAIN_SEARCH_PATH
  (colon-separated); default <hypergraph>/assets/terrain."""
  raw = os.environ.get("ORK_TERRAIN_SEARCH_PATH")
  if raw:
    return [Path(p).expanduser() for p in raw.split(":") if p]
  return [_DEFAULT_TERRAIN_ASSETS]


def resolve_dsl_file(arg):
  """Turn a name/path into an existing .py path; raises FileNotFoundError on miss."""
  direct = Path(arg).expanduser()
  if direct.is_file():
    return direct.resolve()
  if direct.suffix == "" and direct.with_suffix(".py").is_file():
    return direct.with_suffix(".py").resolve()
  for src in search_path():
    for candidate in (src / arg, src / f"{arg}.py"):
      if candidate.is_file():
        return candidate.resolve()
  searched = "\n  ".join(str(s) for s in search_path())
  raise FileNotFoundError(
    f"could not resolve terrain DSL file {arg!r}. searched:\n  {searched}\n"
    f"(set ORK_TERRAIN_SEARCH_PATH to override the default)")


def load_dsl_class(dsl_path, class_name=None):
  """Import dsl_path, return the HeightField subclass to host (auto-find, or
  class_name= to disambiguate multi-class files)."""
  dsl_path = Path(dsl_path)
  if not dsl_path.exists():
    raise FileNotFoundError(f"terrain DSL file not found: {dsl_path}")
  spec = importlib.util.spec_from_file_location("terrain_dsl_module", str(dsl_path))
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  candidates = [
    cls for _, cls in inspect.getmembers(module, inspect.isclass)
    if issubclass(cls, HeightField)
       and cls is not HeightField
       and cls.__module__ == module.__name__
  ]
  if class_name is not None:
    for cls in candidates:
      if cls.__name__ == class_name:
        return cls
    raise ValueError(
      f"--class {class_name!r} not found in {dsl_path.name}. "
      f"Available: {[c.__name__ for c in candidates]}")
  if not candidates:
    raise ValueError(
      f"No HeightField subclass found in {dsl_path.name}. "
      f"Define a class that subclasses ork.hypergraph.dflow.terrain.HeightField.")
  if len(candidates) > 1:
    raise ValueError(
      f"Multiple HeightField subclasses in {dsl_path.name}: "
      f"{[c.__name__ for c in candidates]}. Pass class_name=<name>.")
  return candidates[0]


def list_dsl_files(stream=None):
  out = stream if stream is not None else sys.stdout
  for src in search_path():
    print(f"\n{src}:", file=out)
    if not src.is_dir():
      print("  (directory does not exist)", file=out)
      continue
    for f in sorted(p for p in src.glob("*.py") if not p.name.startswith("_")):
      print(f"  {f.stem}", file=out)
