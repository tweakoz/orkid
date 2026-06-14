###############################################################################
# materials.hypermesh — HYPERMESH-coupled ptex3d materials.
#
# Materials authored to read the hypermesh render's per-triangle/per-face side data (filled by the
# render triangulator). TopoView shades every FACE distinctly for topology inspection; GroupView shades
# every FACE by its selection-group working bits (__tags & 0x1F).
###############################################################################
from .topoview import TopoView
from .groupview import GroupView

__all__ = ["TopoView", "GroupView"]
