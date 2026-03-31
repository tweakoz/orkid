"""
ork.ui.figma — Figma JSON → SVG conversion and structured design access.

Two main entry points:

  FigmaDesign(path)
      Load a Figma REST API JSON export and query pages, frames,
      sections, and elements by name.  Convert any node or region
      to SVG deterministically.

  figma_node_to_svg(node)
      Low-level: convert a raw Figma JSON node dict to SVG markup.
"""

from ork.ui.figma.design import FigmaDesign, FigmaNode, FigmaPage, FigmaSection, FigmaFrame, CardRegion, PanelSpec
from ork.ui.figma.converter import figma_node_to_svg, figma_children_to_svg_fragment
from ork.ui.figma.shapes import person_silhouette_svg, multi_person_svg, circle_icon_svg
from ork.ui.figma.card import build_card_svg
