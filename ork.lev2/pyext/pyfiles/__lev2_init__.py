# Force orkengine.core to load before _lev2.so so its pybind11 type registry
# (fvec4, fmtx4, etc.) is populated before lev2's bindings try to convert
# default-arg values like `fvec4(1,1,1,1)` into Python objects. Without
# this, `from orkengine import lev2` raises a misleading
#   "arg(): could not convert default argument ... (type not registered yet?)"
# unless the user happens to import orkengine.core first.
from orkengine import core as _ensure_core_loaded  # noqa: F401
from ._lev2 import *
