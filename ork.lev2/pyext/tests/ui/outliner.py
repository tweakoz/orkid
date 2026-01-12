#!/usr/bin/env ork.python

################################################################################
# Outliner Widget Test
# Demonstrates tree view with VarMap-based hierarchical data
################################################################################

import signal
from orkengine.core import vec2, vec3, vec4, VarMap
from orkengine import lev2
from ork.ui.test import outliner_data

################################################################################

class OutlinerTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.1, 0.1, 0.1, 1)

    # Create outliner widget
    outliner_layout = lg_group.makeChild(uiclass=lev2.ui.Outliner, args=["outliner"])
    self.outliner = outliner_layout.widget

    # Set up layout - anchor to parent
    root_layout = lg_group.layout
    outliner_layout.layout.top.anchorTo(root_layout.top)
    outliner_layout.layout.left.anchorTo(root_layout.left)
    outliner_layout.layout.bottom.anchorTo(root_layout.bottom)
    outliner_layout.layout.right.anchorTo(root_layout.right)

    # Use VarMap data (simple approach)
    self.outliner.data = outliner_data.test_data()
    self.outliner.model.allow_rename = True  # Enable rename support
    self.outliner.model.allow_delete = True  # Enable delete support
    self.outliner.model.allow_add = True     # Enable add support
    self.outliner.model.allow_multiselect = True     # Enable add support
    self.outliner.expandAll()

    # Set selection callback
    def on_select(key):
      print(f"Selected: {key}")
      value = self._getValueByKey(key)
      if value:
        print(f"  Value: {value}")

    self.outliner.onSelect(on_select)

    # Set delete callback
    def on_delete(key):
      print(f"Deleted: {key}")

    self.outliner.onDelete(on_delete)

    # Set add callback
    def on_add(key):
      print(f"Added: {key}")

    self.outliner.onAdd(on_add)

    # Style
    self.outliner.bgcolor = vec4(0.15, 0.15, 0.15, 1)
    self.outliner.text_color = vec4(0.9, 0.9, 0.9, 1)
    self.outliner.selected_color = vec4(0.2, 0.4, 0.6, 1)
    self.outliner.hover_color = vec4(0.25, 0.25, 0.3, 1)
    self.outliner.item_height = 24

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  def _getValueByKey(self, key):
    """Navigate to a value by slash-separated key path."""
    parts = key.split("/")
    current = self.outliner.data
    for part in parts:
      if part in current:
        val = current[part]
        if isinstance(val, VarMap):
          current = val
        else:
          return val
      else:
        return None
    return current

  def onGpuInit(self, ctx):
    # Set up theme engine for SDF rendering
    self.uicontext = self.ezapp.uicontext
    self.base_db = lev2.ui.createDefaultStyleDatabase()
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)
    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    self.uicontext.theme_engine = custom_theme

  def onUpdate(self, updinfo):
    pass

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

OutlinerTest().ezapp.mainThreadLoop()
