#!/usr/bin/env ork.python

################################################################################
# Toolbar Widget Test
# Demonstrates toolbar with image buttons and separators
################################################################################

import signal
import os
from orkengine.core import vec2, vec3, vec4, Path
from orkengine import lev2
from ork.ui import standard_icons

################################################################################

class ToolbarTest:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.1, 0.1, 0.1, 1)

    # Create toolbar widget
    # Uses anchor layout to position toolbar at top of window
    toolbar_layout = lg_group.makeChild(uiclass=lev2.ui.Toolbar, args=["toolbar"])
    self.toolbar = toolbar_layout.widget

    # Anchor toolbar to top, left, right edges with fixed height via bottom guide
    root_layout = lg_group.layout
    toolbar_layout.layout.top.anchorTo(root_layout.top)
    toolbar_layout.layout.left.anchorTo(root_layout.left)
    toolbar_layout.layout.right.anchorTo(root_layout.right)
    # Create a fixed guide 48 pixels from top for the bottom edge
    bottom_guide = root_layout.fixedHorizontalGuide(48)
    toolbar_layout.layout.bottom.anchorTo(bottom_guide)

    # Icon size
    icon_size = 24

    # File operation buttons
    btn_new = self.toolbar.addButton("new", standard_icons.get('new', icon_size, icon_size), "New File")
    btn_new.onPressed(lambda: print("New clicked!"))

    btn_open = self.toolbar.addButton("open", standard_icons.get('open', icon_size, icon_size), "Open File")
    btn_open.onPressed(lambda: print("Open clicked!"))

    btn_save = self.toolbar.addButton("save", standard_icons.get('save', icon_size, icon_size), "Save File")
    btn_save.onPressed(lambda: print("Save clicked!"))

    self.toolbar.addSeparator()

    # Navigation buttons
    btn_home = self.toolbar.addButton("home", standard_icons.get('home', icon_size, icon_size), "Home")
    btn_home.onPressed(lambda: print("Home clicked!"))

    btn_parent = self.toolbar.addButton("parent", standard_icons.get('parent', icon_size, icon_size), "Parent Directory")
    btn_parent.onPressed(lambda: print("Parent clicked!"))

    btn_refresh = self.toolbar.addButton("refresh", standard_icons.get('refresh', icon_size, icon_size), "Refresh")
    btn_refresh.onPressed(lambda: print("Refresh clicked!"))

    self.toolbar.addSeparator()

    # Transport controls
    btn_skip_back = self.toolbar.addButton("skip_back", standard_icons.get('skip_back', icon_size, icon_size), "Skip Back")
    btn_skip_back.onPressed(lambda: print("Skip Back clicked!"))

    btn_rewind = self.toolbar.addButton("rewind", standard_icons.get('rewind', icon_size, icon_size), "Rewind")
    btn_rewind.onPressed(lambda: print("Rewind clicked!"))

    btn_play = self.toolbar.addButton("play", standard_icons.get('play', icon_size, icon_size), "Play")
    btn_play.toggle_mode = True
    btn_play.onToggled(lambda toggled: print(f"Play toggled: {toggled}"))

    btn_pause = self.toolbar.addButton("pause", standard_icons.get('pause', icon_size, icon_size), "Pause")
    btn_pause.toggle_mode = True
    btn_pause.onToggled(lambda toggled: print(f"Pause toggled: {toggled}"))

    btn_stop = self.toolbar.addButton("stop", standard_icons.get('stop', icon_size, icon_size), "Stop")
    btn_stop.onPressed(lambda: print("Stop clicked!"))

    btn_ff = self.toolbar.addButton("fast_forward", standard_icons.get('fast_forward', icon_size, icon_size), "Fast Forward")
    btn_ff.onPressed(lambda: print("Fast Forward clicked!"))

    btn_skip_fwd = self.toolbar.addButton("skip_forward", standard_icons.get('skip_forward', icon_size, icon_size), "Skip Forward")
    btn_skip_fwd.onPressed(lambda: print("Skip Forward clicked!"))

    btn_loop = self.toolbar.addButton("loop", standard_icons.get('loop', icon_size, icon_size), "Loop")
    btn_loop.toggle_mode = True
    btn_loop.onToggled(lambda toggled: print(f"Loop toggled: {toggled}"))

    self.toolbar.addSeparator()

    # Utility buttons
    btn_search = self.toolbar.addButton("search", standard_icons.get('search', icon_size, icon_size), "Search")
    btn_search.onPressed(lambda: print("Search clicked!"))

    btn_settings = self.toolbar.addButton("settings", standard_icons.get('settings', icon_size, icon_size), "Settings")
    btn_settings.onPressed(lambda: print("Settings clicked!"))

    btn_info = self.toolbar.addButton("info", standard_icons.get('info', icon_size, icon_size), "Info")
    btn_info.onPressed(lambda: print("Info clicked!"))

    self.toolbar.addSeparator()

    btn_close = self.toolbar.addButton("close", standard_icons.get('close', icon_size, icon_size), "Close")
    btn_close.onPressed(lambda: print("Close clicked!"))

    # Style toolbar
    self.toolbar.bgcolor = vec4(0.18, 0.18, 0.18, 1)
    self.toolbar.button_hover_color = vec4(0.3, 0.3, 0.35, 1)
    self.toolbar.button_pressed_color = vec4(0.25, 0.45, 0.65, 1)
    self.toolbar.button_toggled_color = vec4(0.35, 0.55, 0.75, 1)
    self.toolbar.separator_color = vec4(0.35, 0.35, 0.35, 1)
    self.toolbar.icon_size = icon_size
    self.toolbar.button_padding = 6
    self.toolbar.item_spacing = 4
    self.toolbar.edge_padding = 8
    self.toolbar.show_tooltips = True
    self.toolbar.tooltip_delay_ms = 400

    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

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

ToolbarTest().ezapp.mainThreadLoop()
