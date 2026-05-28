###############################################################################
# ork.dflow — HyperSyn Python DSL root.
#
# Per the contract at ~/projects/orkid/.claude/skills/hypersyn/SKILL.md, this
# package hosts the trace-mode expression DSL used to author dataflow graphs
# in Python. Family vocabularies (particles, ptex2d, ptex3d, hypermesh, terrain,
# behavior, ...) live in sub-packages and are imported aliased — for example:
#
#   from orkengine.lev2 import ParticleSystem
#   from ork.dflow import particles as P
#
#   class FireExplosion(ParticleSystem):
#     def __init__(self):
#       self.pool = P.pool_data(size=4096)
#       ...
#
# M1 scope: trace machinery + one proof-of-pattern op in the particles family
# (pool_data). More ops + materializer wrappers land incrementally.
###############################################################################

from ._trace import (
    DslNode,
    current_graph,
    enter_trace,
    leave_trace,
)
from ._expr import Expr

__all__ = ["DslNode", "Expr", "current_graph", "enter_trace", "leave_trace"]
