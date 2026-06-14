###############################################################################
# Shared hypermesh-asset resolver + validator exit protocol — used by both
# obt.project/bin/ork.hypermesh.viewer.py and obt.project/bin/_ork.hypermesh.validate.py so they agree
# on how an asset NAME (the .py filename stem) maps to a Hypermesh subclass, and on the validator's
# exit codes / stdout result token. Leading-underscore filename so the viewer's asset lister ignores it.
###############################################################################
import os, sys, importlib

# ---- validator exit protocol (parent reads subprocess.returncode) ----
EXIT_OK         = 0   # rendered + content present  -> safe to (re)load
EXIT_BLANK      = 2   # rendered but no content      -> do NOT reload (NOT 1: that's Python's uncaught-exc code)
EXIT_LOAD_ERROR = 3   # exception loading/materializing/rendering -> do NOT reload
EXIT_TIMEOUT    = 4   # GPU capture timed out         -> do NOT reload
EXIT_VETFAIL    = 5   # rendered fine but the MESH failed the meshvet geometric tier -> do NOT reload
# Printed to stdout just before teardown so the decision is readable even if teardown SIGABRTs (the
# exit-134 history): RESULT_TOKEN + one of PASS / BLANK / ERROR / TIMEOUT / VETFAIL. The parent prefers
# this token and falls back to returncode; VETFAIL is printed AFTER the render-tier PASS — last token wins.
RESULT_TOKEN = "HMVALIDATE_RESULT="


def assets_dir():
  # the assets/hypermesh package dir, WITHOUT importing it (find_spec doesn't execute the package).
  import importlib.util
  return os.path.dirname(importlib.util.find_spec("ork.hypergraph.assets.hypermesh").origin)


def list_asset_names():
  # asset FILENAME stems in assets/hypermesh/ (underscore-prefixed files excluded).
  return sorted(f[:-3] for f in os.listdir(assets_dir())
                if f.endswith(".py") and not f.startswith("_"))


def class_from_module(mod):
  """pick the Hypermesh asset class from a loaded module: __all__[0], else the first Hypermesh subclass."""
  exported = getattr(mod, "__all__", None)
  if exported:
    return getattr(mod, exported[0])
  from ork.hypergraph.dflow.hypermesh import Hypermesh
  for v in vars(mod).values():
    if isinstance(v, type) and issubclass(v, Hypermesh) and v is not Hypermesh:
      return v
  raise RuntimeError("no Hypermesh asset class found in %s" % getattr(mod, "__file__", mod))


def resolve_asset(name):
  """SEARCH mode: name = asset filename stem; return its Hypermesh subclass. Raises on unknown."""
  if name not in list_asset_names():
    raise KeyError("unknown hypermesh asset %r (have: %s)" % (name, ", ".join(list_asset_names())))
  return class_from_module(importlib.import_module("ork.hypergraph.assets.hypermesh.%s" % name))


# ---- explicit-path (-i) mode: load/reload a Hypermesh asset from an arbitrary .py file ----
PATH_MODNAME = "_hmwatch_asset"   # synthetic module name a path-loaded asset is registered under

def load_asset_from_path(path):
  """load (or RE-load, fresh) the Hypermesh asset class from an arbitrary .py file path. Re-calling it
  re-executes the file, so it doubles as the reload for -i mode (picks up edits)."""
  import importlib.util
  spec = importlib.util.spec_from_file_location(PATH_MODNAME, path)
  mod  = importlib.util.module_from_spec(spec)
  sys.modules[PATH_MODNAME] = mod
  spec.loader.exec_module(mod)
  return class_from_module(mod)


# stock starter asset written when `-i <path>` points at a non-existent file (a basic cube to iterate from).
STOCK_TEMPLATE = '''\
#!/usr/bin/env ork.python
###############################################################################
# A hypermesh test asset. Edit + save; the viewer (run with -w) live-reloads it
# after an offscreen validation passes. The class may be named anything — it's
# found via __all__ (or as the first Hypermesh subclass in the file).
###############################################################################
from orkengine.core import vec3
from ork.hypergraph.dflow.hypermesh import Hypermesh


class Asset(Hypermesh):
  def __init__(self):
    super().__init__()
    self.output(self.box(size=1.5))

  # optional — animate live (drives plugs each frame):
  # def onUpdate(self, updinfo):
  #   pass


__all__ = ["Asset"]
'''

def ensure_asset_file(path):
  """create `path` from the stock cube template if it doesn't exist. Returns True if it created it."""
  if os.path.exists(path):
    return False
  d = os.path.dirname(os.path.abspath(path))
  if d and not os.path.isdir(d):
    os.makedirs(d, exist_ok=True)
  with open(path, "w") as f:
    f.write(STOCK_TEMPLATE)
  return True
