###############################################################################
# assets/hypermesh — concrete named hypermesh assets (GPU mesh dataflow graphs).
#
# Mirrors assets/terrain, assets/materials, assets/mesh: this folder houses recognizable,
# user-authored hypermesh definitions (a RippleGrid, a future Torus, Metaball, LSystem, ...);
# the generic DSL + ops live in ork.hypergraph.dflow.hypermesh.
###############################################################################
from ork.hypergraph.assets.hypermesh.ripplegrid import RippleGrid
from ork.hypergraph.assets.hypermesh.box import Box
from ork.hypergraph.assets.hypermesh.uvsphere import UvSphere
from ork.hypergraph.assets.hypermesh.icosphere import IcoSphere
from ork.hypergraph.assets.hypermesh.cone import Cone
from ork.hypergraph.assets.hypermesh.uvsphere_subdiv import UvSphereSubdiv
from ork.hypergraph.assets.hypermesh.subdiv_demo import SubdivDemo
from ork.hypergraph.assets.hypermesh.subdiv_mask_demo import SubdivMaskDemo
from ork.hypergraph.assets.hypermesh.transform_demo import TransformDemo
from ork.hypergraph.assets.hypermesh.delete_demo import DeleteDemo
from ork.hypergraph.assets.hypermesh.mirror_demo import MirrorDemo
from ork.hypergraph.assets.hypermesh.compact_demo import CompactDemo
from ork.hypergraph.assets.hypermesh.racer import Racer
from ork.hypergraph.assets.hypermesh.sort_test import SortTestDemo
from ork.hypergraph.assets.hypermesh.edge_test import EdgeTestDemo
from ork.hypergraph.assets.hypermesh.edge_select_test import EdgeSelectTest
from ork.hypergraph.assets.hypermesh.point_select_test import PointSelectTest
from ork.hypergraph.assets.hypermesh.bevel_demo import BevelDemo
from ork.hypergraph.assets.hypermesh.extrude_demo import ExtrudeDemo
from ork.hypergraph.assets.hypermesh.extrude_expr_demo import ExtrudeExprDemo
from ork.hypergraph.assets.hypermesh.droop_demo import DroopDemo
from ork.hypergraph.assets.hypermesh.droop_demo_oct import DroopDemoOct
from ork.hypergraph.assets.hypermesh.inset_demo import InsetDemo
from ork.hypergraph.assets.hypermesh.normals_demo import NormalsDemo
from ork.hypergraph.assets.hypermesh.select_demo import SelectDemo
from ork.hypergraph.assets.hypermesh.bitop_demo import BitOpDemo
from ork.hypergraph.assets.hypermesh.genmask_demo import GenMaskDemo

__all__ = ["RippleGrid", "Box", "UvSphere", "IcoSphere", "Cone", "UvSphereSubdiv", "SubdivDemo", "SubdivMaskDemo",
           "TransformDemo", "DeleteDemo", "MirrorDemo", "CompactDemo", "Racer", "SortTestDemo", "EdgeTestDemo", "EdgeSelectTest", "PointSelectTest", "BevelDemo",
           "ExtrudeDemo", "ExtrudeExprDemo", "DroopDemo", "DroopDemoOct", "InsetDemo", "NormalsDemo",
           "SelectDemo", "BitOpDemo", "GenMaskDemo"]
