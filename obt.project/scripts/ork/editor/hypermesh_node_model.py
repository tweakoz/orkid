################################################################################
# hypermesh_node_model — HypermeshNodeGraphModel: bind a HypermeshDocument (a
# GraphData-direct family document, E2) to the generic GPU node editor
# (ork.ui.node_editor.NodeEditor), as the standalone dflow editor's canvas for the
# HYPERMESH family (roads / buildings / box / extrude / L-system meshes).
#
# Hypermesh is a doc-less GraphData-direct family: its serialized form IS the reflection
# JSON of a dflow.GraphData, and HypermeshDocument inherits GraphDataDocument whole. So the
# canvas adapter inherits GraphDataNodeGraphModel whole too — nodes, ports (from
# dflow.plugSpec), edges (GraphData.edges()), positions (the graph-level _editor_layout),
# add/delete/connect (createByClassName / removeModule / plugsCompatible — all family-generic
# engine pybinds) and BYPASS behave identically to the particles/.orj adapter. The ONE
# family-honest override is the DISPLAY flag: see is_output/has_display_flag below.
#
# WHY A SUBCLASS (not a bare reuse): (1) the family identity the shell prints/routes on,
# (2) the display-flag honesty, and (3) a single place to hang any future hypermesh-only
# canvas affordance. node_types() (the Tab-add palette) already filters to the family of the
# modules present — hypermesh module classes are reflected under the `hypermesh::` namespace,
# so the base's namespace-prefix family filter lists exactly the hypermesh (+ roads, which
# are hypermesh-family C++) module classes with no per-family code.
################################################################################

from ork.editor.graphdata_node_model import GraphDataNodeGraphModel


class HypermeshNodeGraphModel(GraphDataNodeGraphModel):
  """The hypermesh family's C0 NodeGraphModel over a HypermeshDocument (a live hypermesh
  dflow.GraphData). Topology / ports / edges / positions / add-delete-connect / BYPASS all
  resolve LIVE through the reflection substrate exactly as the generic GraphData adapter, so a
  mutation never drifts from the serialized graph.

  BYPASS is REAL for hypermesh: the shared DgSorter/GraphInst bypass-resolver routes a
  consumer THROUGH any _bypassed pass-through producer, so a bypassed module drops out of the
  materialized mesh (the viewport host re-materializes on a bypass toggle).

  DISPLAY (the _output_node marker) is INERT for hypermesh: hypermesh materialize takes the
  LAST producing module as the terminal mesh (hmdflow.cpp), not the graph's _output_node — so
  a display selection would change no pixels. We report NO display flag rather than offer a
  no-op badge (owner law: be honest about what is inert)."""

  # ---- display flag: inert for hypermesh -> not offered (honest) --------------

  def has_display_flag(self, nid):
    return False

  def is_output(self, nid):
    return False

  def set_output(self, nid, on):
    # never reached (has_display_flag is False so the canvas offers no display badge); if a
    # caller pokes it anyway, refuse LOUDLY rather than silently mark an inert marker.
    return ("refused", "display/output marker is inert for the hypermesh family "
                       "(materialize uses the last-producing module as the terminal)")
