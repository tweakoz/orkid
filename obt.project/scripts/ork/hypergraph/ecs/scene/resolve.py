###############################################################################
# ork.ecs.scene.resolve — DSL bare-name resolution for Tier 3 Scene files.
#
# Mirrors ork.dflow.particles.resolve. Search path is colon-separated via
# ORK_SCENES_SEARCH_PATH; default is <ork.data>/scenes.
###############################################################################

import importlib.util
import inspect
import os
import sys
from pathlib import Path

from ork import path as ork_path
from . import Scene


def search_path():
  """Ordered list of directories searched for bare scene names. Sourced
  from ORK_SCENES_SEARCH_PATH (colon-separated, PATH-style); default is
  <ork.data>/scenes."""
  raw = os.environ.get("ORK_SCENES_SEARCH_PATH")
  if raw:
    return [Path(p).expanduser() for p in raw.split(":") if p]
  return [Path(ork_path.data) / "scenes"]


def portable_scene_path(scene_path):
  """The form a composed .ecs stores as its scene source: a PATH TOKEN the
  engine expands against the LIVE workspace at read time (see application.cpp's
  expander table), so the same .ecs is correct on a mac checkout and a linux one.

  <ork_data>/scenes/scn_x.py for the usual case, <ork_root>/... for anything
  else inside the workspace. A scene OUTSIDE the workspace has no token to hang
  on: it is stored absolute and warned about here, at the place the choice is
  made, rather than discovered as a missing file on another machine."""
  p = Path(scene_path).resolve()
  data = Path(str(ork_path.data)).resolve()
  root = Path(str(ork_path.root)).resolve()
  try:
    return "<ork_data>/%s" % p.relative_to(data).as_posix()
  except ValueError:
    pass
  try:
    return "<ork_root>/%s" % p.relative_to(root).as_posix()
  except ValueError:
    pass
  print("ork.scene: %s is outside ORKID_WORKSPACE_DIR — its path is stored ABSOLUTE "
        "in the composed .ecs, which is therefore not portable to another machine"
        % p, file=sys.stderr)
  return str(p)


def resolve_scene_file(arg):
  """Turn a CLI arg into an existing .py path. Direct paths win; otherwise
  search each dir in ORK_SCENES_SEARCH_PATH for `<name>` or `<name>.py`."""
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
    f"could not resolve scene file {arg!r}. searched:\n  {searched}\n"
    f"(set ORK_SCENES_SEARCH_PATH to override the default)")


def load_scene_class(scene_path, class_name=None):
  """Import `scene_path`, return the Scene subclass to host.
  Same auto-find / explicit-name semantics as the particles resolver."""
  scene_path = Path(scene_path)
  if not scene_path.exists():
    raise FileNotFoundError(f"Scene file not found: {scene_path}")

  spec = importlib.util.spec_from_file_location("scene_dsl_module", str(scene_path))
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)

  candidates = [
    cls for _, cls in inspect.getmembers(module, inspect.isclass)
    if issubclass(cls, Scene)
       and cls is not Scene
       and cls.__module__ == module.__name__
  ]

  if class_name is not None:
    for cls in candidates:
      if cls.__name__ == class_name:
        return cls
    raise ValueError(
      f"--class {class_name!r} not found in {scene_path.name}. "
      f"Available: {[c.__name__ for c in candidates]}")

  if not candidates:
    raise ValueError(
      f"No Scene subclass found in {scene_path.name}. "
      f"Define a class that subclasses ork.dflow.scene.Scene.")
  if len(candidates) > 1:
    names = [c.__name__ for c in candidates]
    raise ValueError(
      f"Multiple Scene subclasses in {scene_path.name}: {names}. "
      f"Pass --class <name> to disambiguate.")
  return candidates[0]


def list_scene_files(stream=None):
  """Print every .py file in the search path grouped by source directory."""
  out = stream if stream is not None else sys.stdout
  any_found = False
  for src in search_path():
    print(f"\n{src}:", file=out)
    if not src.is_dir():
      print(f"  (directory does not exist)", file=out)
      continue
    files = sorted(p for p in src.glob("*.py") if not p.name.startswith("_"))
    if not files:
      print(f"  (no .py files)", file=out)
      continue
    any_found = True
    stems = [f.stem for f in files]
    per_row = 6
    col_w = max(len(s) for s in stems) + 2
    for i in range(0, len(stems), per_row):
      row = stems[i:i + per_row]
      print("  " + "".join(s.ljust(col_w) for s in row).rstrip(), file=out)
  if not any_found:
    print("\n(no scene files found; set ORK_SCENES_SEARCH_PATH to override the default)",
          file=sys.stderr)
