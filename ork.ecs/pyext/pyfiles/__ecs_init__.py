# Force orkengine.core and orkengine.lev2 to load (in that order) before
# _ecs.so so the pybind11 type registry has every dependent type available
# when ecs's bindings register defaults / cross-module casts. Without this,
# `from orkengine import ecs` raises a misleading
#   "arg(): could not convert default argument ... (type not registered yet?)"
# whenever an ecs binding refers to a core/lev2 type as a default arg.
from orkengine import core as _ensure_core_loaded   # noqa: F401
from orkengine import lev2 as _ensure_lev2_loaded   # noqa: F401
from ._ecs import *
