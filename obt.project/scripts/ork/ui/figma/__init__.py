"""
ork.ui.figma — Figma JSON design access and conversion.

  FigmaDesign(path)         Generic Figma JSON loader and navigator.
  OrkidFigmaDesign(path)    Orkid card UI: detect_cards, hover SVGs, etc.
  SwiftFigmaDesign(path)    macOS/Swift: build_page, dmg_layout, etc.
  figma_node_to_svg(node)   Low-level node → SVG conversion.
"""

from ork.ui.figma.design import FigmaDesign, FigmaNode, FigmaPage, FigmaSection
from ork.ui.figma.converter import figma_node_to_svg, figma_children_to_svg_fragment
from ork.ui.figma.orkid_design import OrkidFigmaDesign, CardRegion, PanelSpec
from ork.ui.figma.swift_design import SwiftFigmaDesign
from ork.ui.figma.shapes import person_silhouette_svg, multi_person_svg, circle_icon_svg
from ork.ui.figma.card import build_card_svg
from ork.ui.figma import constants
