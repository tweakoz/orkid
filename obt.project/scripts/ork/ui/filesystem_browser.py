################################################################################
# FilesystemBrowser - Composite filesystem browser widget
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import os
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.ui import standard_icons, icon_library

################################################################################

class FilesystemBrowser:
  """
  Composite filesystem browser widget with:
  - Toolbar (navigation, view mode, favorites)
  - Filter bar with hold/clear buttons
  - Favorites panel (left)
  - Filesystem view (right)
  """

  def __init__(self, container, name, initial_path=None, default_filter="", bg_color=vec3(0.1, 0.1, 0.1), mode="load"):
    self.container = container
    self.name = name
    self.bg_color = bg_color
    self.default_filter = default_filter
    self.mode = mode  # "load" or "save"

    # Callbacks (set by user)
    self.onSelect = None
    self.onActivate = None
    self.onDirectoryChanged = None
    self.onCancel = None

    icon_size = 20
    tokens = CrcStringProxy()

    ############################################################################
    # Main VPack - contains top hpack and content hpack
    ############################################################################

    self.main_vpack = container.makeChild(uiclass=lev2.ui.VerticalPack, args=[f"{name}_vpack"])
    self.main_vpack.margin = 4
    self.main_vpack.item_height = 36
    self.main_vpack.fill = True
    self.main_vpack.bg_color = vec4(0, 0, 0, 1)

    ############################################################################
    # Top HPack - toolbar + filter edit + hold/clear buttons
    ############################################################################

    self.top_hpack = self.main_vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=[f"{name}_top"])
    self.top_hpack.margin = 4
    self.top_hpack.bg_color = vec4(0, 0, 0, 1)

    self.toolbar = self.top_hpack.makeChild(uiclass=lev2.ui.Toolbar, args=[f"{name}_toolbar"])
    self.toolbar.fixed_width = 350

    # Style toolbar
    self.toolbar.bgcolor = vec4(0.12, 0.12, 0.15, 1)
    self.toolbar.button_hover_color = vec4(0.25, 0.25, 0.3, 1)
    self.toolbar.button_pressed_color = vec4(0.2, 0.4, 0.6, 1)
    self.toolbar.button_toggled_color = vec4(0.25, 0.45, 0.65, 1)
    self.toolbar.separator_color = vec4(0.3, 0.3, 0.35, 1)
    self.toolbar.icon_size = icon_size
    self.toolbar.button_padding = 4
    self.toolbar.item_spacing = 2
    self.toolbar.edge_padding = 6

    # Navigation buttons
    self.btn_home = self.toolbar.addButton("home", standard_icons.get('home', icon_size, icon_size), "Home Directory")
    self.btn_parent = self.toolbar.addButton("parent", standard_icons.get('parent', icon_size, icon_size), "Parent Directory")
    self.btn_refresh = self.toolbar.addButton("refresh", standard_icons.get('refresh', icon_size, icon_size), "Refresh")

    self.toolbar.addSeparator()

    # View mode buttons
    self.btn_list = self.toolbar.addButton("list", standard_icons.get('file_text', icon_size, icon_size), "List View")
    self.btn_list.toggle_mode = True
    self.btn_list.toggled = True

    self.btn_icons = self.toolbar.addButton("icons", standard_icons.get('folder', icon_size, icon_size), "Icon View")
    self.btn_icons.toggle_mode = True

    self.toolbar.addSeparator()

    # Hidden files button (eye icon)
    eye_svg = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
      <ellipse cx="12" cy="12" rx="10" ry="6" fill="none" stroke="#E6E6E6" stroke-width="2"/>
      <circle cx="12" cy="12" r="3" fill="#E6E6E6"/>
    </svg>'''
    icon_eye = icon_library.from_svg_string(eye_svg, icon_size, icon_size)
    self.btn_hidden = self.toolbar.addButton("hidden", icon_eye, "Show Hidden Files")
    self.btn_hidden.toggle_mode = True
    self.btn_hidden.toggled = True

    self.toolbar.addSeparator()

    # Favorites button
    star_svg = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
      <polygon points="12,2 15,9 22,9 17,14 19,21 12,17 5,21 7,14 2,9 9,9" fill="#E6E6E6" stroke="#888" stroke-width="0.5"/>
    </svg>'''
    icon_star = icon_library.from_svg_string(star_svg, icon_size, icon_size)
    self.btn_add_fav = self.toolbar.addButton("add_fav", icon_star, "Add to Favorites")

    self.toolbar.addSeparator()

    # Icon size +/- buttons (folder with +/- overlay)
    def make_folder_size_icon(symbol):
      return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
        <path d="M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z" fill="#4D99CC"/>
        <text x="12" y="14" text-anchor="middle" dominant-baseline="central" font-family="sans-serif" font-size="18" font-weight="bold" fill="#FFFFFF" stroke="#000000" stroke-width="0.5">{symbol}</text>
      </svg>'''

    icon_plus = icon_library.from_svg_string(make_folder_size_icon("+"), icon_size, icon_size)
    icon_minus = icon_library.from_svg_string(make_folder_size_icon("-"), icon_size, icon_size)

    self.btn_size_up = self.toolbar.addButton("size_up", icon_plus, "Increase Icon Size")
    self.btn_size_down = self.toolbar.addButton("size_down", icon_minus, "Decrease Icon Size")

    # Filter LineEdit
    self.filter_edit = self.top_hpack.makeChild(uiclass=lev2.ui.LineEdit, args=["Filter:", "", vec3(0.12, 0.12, 0.15)])
    self.top_hpack.fill_widget = self.filter_edit

    # Hold/Clear icons
    def make_text_icon(text, color="#E6E6E6"):
      w = max(24, len(text) * 6 + 4)
      cx = w / 2
      return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} 24">
        <text x="{cx}" y="12" text-anchor="middle" dominant-baseline="central" font-family="sans-serif" font-size="9" font-weight="bold" fill="{color}">{text}</text>
      </svg>'''

    icon_hold = icon_library.from_svg_string(make_text_icon("HOLD"), icon_size*2, icon_size*2)
    icon_clr = icon_library.from_svg_string(make_text_icon("CLR"), icon_size*2, icon_size*2)

    # Hold button
    self.btn_hold = self.top_hpack.makeChild(uiclass=lev2.ui.ImageButton, args=[f"{name}_hold"])
    self.btn_hold.inactive_image = icon_hold
    self.btn_hold.bgcolor = vec4(0.15, 0.15, 0.18, 1)
    self.btn_hold.inactive_blend_mode = tokens.ALPHA
    self.btn_hold.fixed_width = 36
    self.hold_filter = False

    # Clear button
    self.btn_clear = self.top_hpack.makeChild(uiclass=lev2.ui.ImageButton, args=[f"{name}_clear"])
    self.btn_clear.inactive_image = icon_clr
    self.btn_clear.bgcolor = vec4(0.15, 0.15, 0.18, 1)
    self.btn_clear.inactive_blend_mode = tokens.ALPHA
    self.btn_clear.fixed_width = 36

    ############################################################################
    # Content HPack - favorites panel + filesystem view
    ############################################################################

    self.content_hpack = self.main_vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=[f"{name}_content"])
    self.content_hpack.margin = 4
    self.content_hpack.fill = True
    self.content_hpack.bg_color = vec4(0, 0, 0, 1)
    self.content_hpack.item_width = 200

    # Favorites panel
    self.favorites_panel = self.content_hpack.makeChild(uiclass=lev2.ui.VerticalPack, args=[f"{name}_favorites"])
    self.favorites_panel.margin = 1
    self.favorites_panel.item_height = 28
    self.favorites_panel.fill = False
    self.favorites_panel.bg_color = vec4(0, 0, 0, 1)

    # Filesystem view
    self.fs_view = self.content_hpack.makeChild(uiclass=lev2.ui.FilesystemView, args=[f"{name}_fsview"])

    # Create model
    home_dir = os.path.expanduser("~")
    start_path = initial_path if initial_path else home_dir
    self.model = lev2.ui.LocalFilesystemModel(start_path)
    self.model.show_hidden = True
    self.model.directories_first = True
    self.model.sort_field = lev2.ui.FilesystemSortField.Name
    self.model.sort_order = lev2.ui.FilesystemSortOrder.Ascending

    self.fs_view.model = self.model
    self.fs_view.view_mode = lev2.ui.FilesystemViewMode.List
    self.fs_view.allow_multiselect = True

    # Apply default filter if specified
    if self.default_filter:
      self.filter_edit.text = self.default_filter
      if ';' in self.default_filter:
        # Multi-pattern: use model.filter which supports semicolon-separated globs
        self.model.filter = self.default_filter
      elif '*' not in self.default_filter and '?' not in self.default_filter:
        self.model.name_filter = f"*{self.default_filter}*"
      else:
        self.model.name_filter = self.default_filter

    # Style filesystem view
    self.fs_view.bgcolor = vec4(0.15, 0.15, 0.15, 1)
    self.fs_view.text_color = vec4(0.9, 0.9, 0.9, 1)
    self.fs_view.selected_color = vec4(0.2, 0.4, 0.6, 1)
    self.fs_view.hover_color = vec4(0.25, 0.25, 0.3, 1)
    self.fs_view.directory_color = vec4(0.7, 0.85, 1.0, 1)
    self.fs_view.header_bgcolor = vec4(0.12, 0.12, 0.15, 1)
    self.fs_view.item_height = 24

    # Set default folder and file icons (C++ converts to texture lazily)
    folder_svg = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
      <path d="M10 4H4c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z" fill="#4D99CC"/>
    </svg>'''
    file_svg = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
      <path d="M14 2H6c-1.1 0-2 .9-2 2v16c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V8l-6-6z" fill="#808080"/>
      <path d="M14 2v6h6" fill="#606060"/>
    </svg>'''
    self.fs_view.folder_icon = icon_library.from_svg_string(folder_svg, 64, 64)
    self.fs_view.file_icon = icon_library.from_svg_string(file_svg, 64, 64)

    # Favorites manager
    self.favorites_mgr = lev2.ui.FavoritesManager.instance()
    self.favorite_widgets = []

    # Set content as the fill widget (takes remaining space)
    self.main_vpack.fill_widget = self.content_hpack

    ############################################################################
    # Bottom bar - filename input (save only) + action buttons
    ############################################################################

    self.bottom_hpack = self.main_vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=[f"{name}_bottom"])
    self.bottom_hpack.margin = 4
    self.bottom_hpack.item_width = 80
    self.bottom_hpack.bg_color = vec4(0.12, 0.12, 0.15, 1)
    self.bottom_hpack.fixed_height = 36

    # Filename input (save mode only)
    if self.mode == "save":
      self.filename_edit = self.bottom_hpack.makeChild(uiclass=lev2.ui.LineEdit, args=["Filename:", "", vec3(0.15, 0.15, 0.18)])
      self.bottom_hpack.fill_widget = self.filename_edit
    else:
      self.filename_edit = None

    # Action button (Save or Load)
    action_text = "SAVE" if self.mode == "save" else self.mode.upper()
    action_color = vec4(0.15, 0.25, 0.15, 1) if self.mode == "save" else vec4(0.15, 0.15, 0.25, 1)
    icon_action = icon_library.from_svg_string(make_text_icon(action_text), icon_size*2, icon_size*2)
    self.btn_action = self.bottom_hpack.makeChild(uiclass=lev2.ui.ImageButton, args=[f"{name}_action"])
    self.btn_action.inactive_image = icon_action
    self.btn_action.bgcolor = action_color
    self.btn_action.inactive_blend_mode = tokens.ALPHA

    # Cancel button
    icon_cancel = icon_library.from_svg_string(make_text_icon("CANCEL", "#FF8888"), icon_size*2, icon_size*2)
    self.btn_cancel = self.bottom_hpack.makeChild(uiclass=lev2.ui.ImageButton, args=[f"{name}_cancel"])
    self.btn_cancel.inactive_image = icon_cancel
    self.btn_cancel.bgcolor = vec4(0.25, 0.15, 0.15, 1)
    self.btn_cancel.inactive_blend_mode = tokens.ALPHA

    # Wire up callbacks
    self._setupCallbacks(home_dir)
    self._refresh_favorites_panel()

  def _setupCallbacks(self, home_dir):
    """Wire up all internal callbacks."""

    # Navigation
    self.btn_home.onPressed(lambda: self.navigateTo(home_dir))
    self.btn_parent.onPressed(lambda: self.navigateUp())
    self.btn_refresh.onPressed(lambda: self.refresh())

    # View mode
    def set_list_view(toggled):
      if toggled:
        self.fs_view.view_mode = lev2.ui.FilesystemViewMode.List
        self.btn_icons.toggled = False

    def set_icon_view(toggled):
      if toggled:
        self.fs_view.view_mode = lev2.ui.FilesystemViewMode.Icon
        self.btn_list.toggled = False

    self.btn_list.onToggled(set_list_view)
    self.btn_icons.onToggled(set_icon_view)

    # Hidden files
    def toggle_hidden(toggled):
      self.model.show_hidden = toggled
      self.fs_view.refresh()

    self.btn_hidden.onToggled(toggle_hidden)

    # Add favorite
    def add_favorite():
      self.fs_view.addCurrentAsFavorite()
      self._refresh_favorites_panel()

    self.btn_add_fav.onPressed(add_favorite)

    # Icon size +/-
    def increase_icon_size():
      self.fs_view.icon_size = min(256, self.fs_view.icon_size + 16)
      self.fs_view.refresh()

    def decrease_icon_size():
      self.fs_view.icon_size = max(32, self.fs_view.icon_size - 16)
      self.fs_view.refresh()

    self.btn_size_up.onPressed(increase_icon_size)
    self.btn_size_down.onPressed(decrease_icon_size)

    # Hold filter toggle
    def toggle_hold(btn):
      self.hold_filter = not self.hold_filter
      if self.hold_filter:
        self.btn_hold.bgcolor = vec4(0.25, 0.35, 0.25, 1)
      else:
        self.btn_hold.bgcolor = vec4(0.15, 0.15, 0.18, 1)

    self.btn_hold.onPressed = toggle_hold

    # Clear filter
    def clear_filter(btn):
      self.model.filter = ""
      self.model.name_filter = ""
      self.filter_edit.text = ""
      self.fs_view.refresh()

    self.btn_clear.onPressed = clear_filter

    # Filter committed
    def on_filter_committed(text):
      if text:
        if '*' not in text and '?' not in text:
          pattern = f"*{text}*"
        else:
          pattern = text
        self.model.name_filter = pattern
      else:
        self.model.name_filter = ""
      self.fs_view.refresh()

    self.filter_edit.onTextCommitted(on_filter_committed)

    # Selection callback
    def on_select(path):
      # In save mode, populate filename field with selected file's name
      if self.mode == "save" and self.filename_edit and path:
        if os.path.isfile(path):
          self.filename_edit.text = os.path.basename(path)
      if self.onSelect:
        self.onSelect(path)

    self.fs_view.onSelect(on_select)

    # Activation callback
    def on_activate(path):
      if self.onActivate:
        self.onActivate(path)

    self.fs_view.onActivate(on_activate)

    # Directory changed callback
    def on_dir_changed(path):
      if not self.hold_filter:
        self.model.filter = ""
        self.model.name_filter = ""
        self.filter_edit.text = ""
      if self.onDirectoryChanged:
        self.onDirectoryChanged(path)

    self.fs_view.onDirectoryChanged(on_dir_changed)

    # Action button (Save/Load)
    def on_action(btn):
      if self.mode == "save" and self.filename_edit:
        filename = self.filename_edit.text.strip()
        if filename:
          path = os.path.join(self.model.getCurrentPath(), filename)
          if self.onActivate:
            self.onActivate(path)
      else:
        path = self.fs_view.selected_path
        if path and self.onActivate:
          self.onActivate(path)

    self.btn_action.onPressed = on_action

    # Cancel button
    def on_cancel(btn):
      if self.onCancel:
        self.onCancel()

    self.btn_cancel.onPressed = on_cancel

    # Filename edit enter key (save mode)
    if self.filename_edit:
      def on_filename_committed(text):
        if text.strip():
          path = os.path.join(self.model.getCurrentPath(), text.strip())
          if self.onActivate:
            self.onActivate(path)
      self.filename_edit.onTextCommitted(on_filename_committed)

  def _refresh_favorites_panel(self):
    """Refresh the favorites panel content."""
    model_id = self.model.modelIdentifier()
    entries = self.favorites_mgr.getFavoriteEntries(model_id)
    tokens = CrcStringProxy()

    # SVG icons
    x_svg = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
      <line x1="6" y1="6" x2="18" y2="18" stroke="#E6E6E6" stroke-width="2" stroke-linecap="round"/>
      <line x1="18" y1="6" x2="6" y2="18" stroke="#E6E6E6" stroke-width="2" stroke-linecap="round"/>
    </svg>'''
    arrow_svg = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
      <path d="M5,12 L19,12 M13,6 L19,12 L13,18" fill="none" stroke="#E6E6E6" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
    </svg>'''
    icon_x = icon_library.from_svg_string(x_svg, 16, 16)
    icon_arrow = icon_library.from_svg_string(arrow_svg, 16, 16)

    for i, entry in enumerate(entries):
      if i < len(self.favorite_widgets):
        fav_hpack, fav_edit, btn_del, btn_apply = self.favorite_widgets[i]
        fav_edit.text = entry.displayName()

        fav_entry = entry
        def on_commit(text, e=fav_entry, mid=model_id):
          if text != e.displayName():
            e.name = text
            self.favorites_mgr.updateFavoriteEntry(mid, e)
        fav_edit.onTextCommitted(on_commit)

        btn_del.onPressed = lambda btn, e=fav_entry, mid=model_id: (
          self.favorites_mgr.removeFavoriteEntry(mid, e.uuid),
          self._refresh_favorites_panel()
        )
        def apply_fav(btn, e=fav_entry):
          self.fs_view.applyFavorite(e)
          self.filter_edit.text = e.name_filter if e.name_filter else ""
        btn_apply.onPressed = apply_fav
      else:
        fav_hpack = self.favorites_panel.makeChild(uiclass=lev2.ui.HorizontalPack, args=["fav_hpack"])
        fav_hpack.margin = 2
        fav_hpack.item_width = 24
        fav_hpack.bg_color = vec4(0, 0, 0, 1)

        fav_bgcolor = vec3(0.18, 0.18, 0.22)
        fav_edit = fav_hpack.makeChild(uiclass=lev2.ui.LineEdit, args=["", entry.displayName(), fav_bgcolor])
        fav_edit.input_color = vec4(fav_bgcolor.x * 0.9, fav_bgcolor.y * 0.9, fav_bgcolor.z * 0.9, 1)
        fav_hpack.fill_widget = fav_edit

        btn_del = fav_hpack.makeChild(uiclass=lev2.ui.ImageButton, args=["btn_del"])
        btn_del.inactive_image = icon_x
        btn_del.bgcolor = vec4(0.3, 0.15, 0.15, 1)
        btn_del.inactive_blend_mode = tokens.ALPHA

        btn_apply = fav_hpack.makeChild(uiclass=lev2.ui.ImageButton, args=["btn_apply"])
        btn_apply.inactive_image = icon_arrow
        btn_apply.bgcolor = vec4(0.15, 0.3, 0.15, 1)
        btn_apply.inactive_blend_mode = tokens.ALPHA

        self.favorite_widgets.append((fav_hpack, fav_edit, btn_del, btn_apply))

        fav_entry = entry
        def on_commit(text, e=fav_entry, mid=model_id):
          if text != e.displayName():
            e.name = text
            self.favorites_mgr.updateFavoriteEntry(mid, e)
        fav_edit.onTextCommitted(on_commit)

        btn_del.onPressed = lambda btn, e=fav_entry, mid=model_id: (
          self.favorites_mgr.removeFavoriteEntry(mid, e.uuid),
          self._refresh_favorites_panel()
        )
        def apply_fav_new(btn, e=fav_entry):
          self.fs_view.applyFavorite(e)
          self.filter_edit.text = e.name_filter if e.name_filter else ""
        btn_apply.onPressed = apply_fav_new

    for i in range(len(entries), len(self.favorite_widgets)):
      fav_hpack, fav_edit, btn_del, btn_apply = self.favorite_widgets[i]
      fav_edit.text = ""

  # Public API

  def navigateTo(self, path):
    """Navigate to the specified path."""
    return self.fs_view.navigateTo(path)

  def navigateUp(self):
    """Navigate to parent directory."""
    return self.fs_view.navigateUp()

  def refresh(self):
    """Refresh the current directory."""
    self.fs_view.refresh()

  def getCurrentPath(self):
    """Get the current directory path."""
    return self.model.getCurrentPath()

  def getSelectedPath(self):
    """Get the currently selected path."""
    return self.fs_view.selected_path

  def getSelectedPaths(self):
    """Get all selected paths."""
    return self.fs_view.selected_paths

  ###########################################################################
  # Factory methods
  ###########################################################################

  @staticmethod
  def uifactory(parent_layoutgroup, args):
    """
    UI factory for use with layoutgroup.makeChild

    Args:
      parent_layoutgroup: Parent LayoutGroup
      args: [name, initial_path, default_filter, bg_color, mode]
            mode: "load" (default) or "save"
    """
    name = args[0]
    initial_path = args[1] if len(args) > 1 else None
    default_filter = args[2] if len(args) > 2 else ""
    bg_color = args[3] if len(args) > 3 else vec3(0.1, 0.1, 0.1)
    mode = args[4] if len(args) > 4 else "load"

    container_item = parent_layoutgroup.makeChild(uiclass=lev2.ui.VerticalPack, args=[name])
    container = container_item.widget
    container.margin = 0
    container.fill = True
    container.bg_color = vec4(bg_color.x, bg_color.y, bg_color.z, 1)

    browser = FilesystemBrowser(container, name, initial_path, default_filter, bg_color, mode)
    container.uservars.filesystem_browser = browser

    return container_item

  @staticmethod
  def wfactory(args):
    """
    Widget factory for use with widget.makeChild

    Args:
      args: [name, initial_path, default_filter, bg_color, mode]
            mode: "load" (default) or "save"
    """
    name = args[0]
    initial_path = args[1] if len(args) > 1 else None
    default_filter = args[2] if len(args) > 2 else ""
    bg_color = args[3] if len(args) > 3 else vec3(0.1, 0.1, 0.1)
    mode = args[4] if len(args) > 4 else "load"

    container = lev2.ui.VerticalPack.wfactory([name])
    container.margin = 0
    container.fill = True
    container.bg_color = vec4(bg_color.x, bg_color.y, bg_color.z, 1)

    browser = FilesystemBrowser(container, name, initial_path, default_filter, bg_color, mode)
    container.uservars.filesystem_browser = browser

    return container
