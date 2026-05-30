###############################################################################
# assets — concrete named asset wrappers, organized by output type (the
# subdirectory name). Each subdir houses one-class-per-file modules; this
# top-level __init__.py imports the category packages so authors can write
#    from ork.hypergraph.assets.sdf import SphereSdf
# or
#    from ork.hypergraph.assets.sdf.sphere import SphereSdf
###############################################################################
from . import sdf, mesh, materials
__all__ = ["sdf", "mesh", "materials"]
