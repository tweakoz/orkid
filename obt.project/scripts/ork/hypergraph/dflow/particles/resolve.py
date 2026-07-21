###############################################################################
# ork.dflow.particles.resolve — shared DSL bare-name resolution for callers
# that host particle DSL files (ork.particles.player.py, ECS demos, future
# editors).
#
# Resolution rules:
#  - explicit path (relative-to-cwd or absolute) → use as-is
#  - bare name → search each dir in ORK_PARTICLES_SEARCH_PATH (colon-separated)
#                for `<name>` or `<name>.py`
#  - default search path: <ork.data>/particles
#
# After resolution, load_dsl_class() imports the file and locates the single
# ParticleSystem subclass defined in it (or raises with a clear message;
# pass class_name= to disambiguate when a file declares multiple).
###############################################################################

import importlib.util
import inspect
import os
import sys
from pathlib import Path

from ork import path as ork_path
from . import ParticleSystem


def search_path():
  """Return the ordered list of directories searched for bare DSL names.
  Sourced from ORK_PARTICLES_SEARCH_PATH (colon-separated, PATH-style);
  default is <ork.data>/particles."""
  raw = os.environ.get("ORK_PARTICLES_SEARCH_PATH")
  if raw:
    return [Path(p).expanduser() for p in raw.split(":") if p]
  return [Path(ork_path.data) / "particles"]


def resolve_dsl_file(arg):
  """Turn a CLI arg into an existing .py path. See module docstring for rules.
  Raises FileNotFoundError listing the searched directories on miss."""
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
    f"could not resolve DSL file {arg!r}. searched:\n  {searched}\n"
    f"(set ORK_PARTICLES_SEARCH_PATH to override the default)")


def load_dsl_module(dsl_path):
  """Import `dsl_path` and return the loaded MODULE object. The editor's testbench seam
  needs the module (getattr(module, "TESTBENCH", ...)) — a module-level passive declaration
  that never rides the class. One exec_module per call; the returned handle is authoritative
  regardless of the sys.modules name (multi-doc loads never alias the wrong TESTBENCH)."""
  dsl_path = Path(dsl_path)
  if not dsl_path.exists():
    raise FileNotFoundError(f"DSL file not found: {dsl_path}")
  spec = importlib.util.spec_from_file_location("ptc_dsl_module", str(dsl_path))
  module = importlib.util.module_from_spec(spec)
  spec.loader.exec_module(module)
  return module


def class_in_module(module, dsl_path, class_name=None):
  """The single ParticleSystem subclass defined in an already-loaded `module`.
  `dsl_path` is used only for clear error messages."""
  candidates = [
    cls for _, cls in inspect.getmembers(module, inspect.isclass)
    if issubclass(cls, ParticleSystem)
       and cls is not ParticleSystem
       and cls.__module__ == module.__name__
  ]

  if class_name is not None:
    for cls in candidates:
      if cls.__name__ == class_name:
        return cls
    raise ValueError(
      f"--class {class_name!r} not found in {Path(dsl_path).name}. "
      f"Available: {[c.__name__ for c in candidates]}")

  if not candidates:
    raise ValueError(
      f"No ParticleSystem subclass found in {Path(dsl_path).name}. "
      f"Define a class that subclasses ork.dflow.particles.ParticleSystem.")
  if len(candidates) > 1:
    names = [c.__name__ for c in candidates]
    raise ValueError(
      f"Multiple ParticleSystem subclasses in {Path(dsl_path).name}: {names}. "
      f"Pass class_name=<name> to disambiguate.")
  return candidates[0]


def load_dsl_class(dsl_path, class_name=None):
  """Import `dsl_path`, return the ParticleSystem subclass to host.

  - class_name=None  → auto-find; raises if zero or multiple candidates
  - class_name=<str> → return the named class (must subclass ParticleSystem)
  """
  module = load_dsl_module(dsl_path)
  return class_in_module(module, dsl_path, class_name)


def list_dsl_files(stream=None):
  """Print every .py file in the search path grouped by source directory.
  Bare names (suitable for resolve_dsl_file) are shown alongside filename."""
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
    for f in files:
      print(f"  {f.stem}", file=out)
  if not any_found:
    print("\n(no DSL files found; set ORK_PARTICLES_SEARCH_PATH to override the default)",
          file=sys.stderr)
