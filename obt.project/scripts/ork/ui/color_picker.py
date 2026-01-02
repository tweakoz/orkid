################################################################################
# ColorPicker - Composite widget combining ColorEdit, RGB sliders, and presets
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

from orkengine.core import vec3, vec4
from orkengine import lev2

################################################################################

class ColorPicker:
    """
    Composite color picker widget with:
    - RGB sliders (left side)
    - Preset save/load controls (left bottom)
    - Color wheel editor (right side, square)
    """

    ###########################################################################

    def __init__(self, container, name, initial_color,bg_color):
        #print("container:", container)
        #print("name:", name)
        #print("initial_color:", initial_color)

        self.container = container
        self.name = name
        self.current_color = initial_color
        self._updating = False  # Prevent feedback loops

        #################################################
        # Left side: VPack with sliders and preset controls
        #################################################

        self.vpack = self.container.makeChild(uiclass=lev2.ui.VerticalPack, args=[f"{name}_vpack"])
        self.vpack.margin = 1
        self.vpack.bg_color = vec4(bg_color,1)
        self.vpack.item_height = 20

        #################################################
        # Right side: ColorEdit (square)
        #################################################

        self.coloredit = self.container.makeChild(
            uiclass=lev2.ui.ColorEdit,
            args=[f"{name}_coloredit", initial_color]
        )

        self.coloredit.onColorChanged = lambda w: self._onColorEditChanged()

        #################################################
        # RGB Sliders
        #################################################

        self.r_slider = self.vpack.makeChild(
            uiclass=lev2.ui.FloatSlider,
            args=["R", vec3(0.5, 0.2, 0.2), 0.0, 1.0, initial_color.x]
        )
        self.r_slider.update_on_drag = True
        self.r_slider.onValueChanged = lambda w: self._onSliderChanged()

        self.g_slider = self.vpack.makeChild(
            uiclass=lev2.ui.FloatSlider,
            args=["G", vec3(0.2, 0.5, 0.2), 0.0, 1.0, initial_color.y]
        )
        self.g_slider.update_on_drag = True
        self.g_slider.onValueChanged = lambda w: self._onSliderChanged()

        self.b_slider = self.vpack.makeChild(
            uiclass=lev2.ui.FloatSlider,
            args=["B", vec3(0.2, 0.2, 0.5), 0.0, 1.0, initial_color.z]
        )
        self.b_slider.update_on_drag = True
        self.b_slider.onValueChanged = lambda w: self._onSliderChanged()

        #################################################
        # Commit/Cancel Buttons
        #################################################

        self.action_hpack = self.vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=[f"{name}_actions"])
        self.action_hpack.margin = 2
        self.action_hpack.uniform = True

        # Commit button
        self.commit_btn = self.action_hpack.makeChild(
            uiclass=lev2.ui.Button,
            args=["Commit", vec3(0.3, 0.5, 0.3)]
        )
        self.commit_btn.onPressed = lambda w: self._onCommit()

        # Cancel button
        self.cancel_btn = self.action_hpack.makeChild(
            uiclass=lev2.ui.Button,
            args=["Cancel", vec3(0.5, 0.3, 0.3)]
        )
        self.cancel_btn.onPressed = lambda w: self._onCancel()

        # Callbacks (set by user)
        self.onCommit = None   # Called with final color when committed
        self.onCancel = None   # Called when cancelled
        self.onColorChanged = None  # Called during live editing


    ###########################################################################

    def _onSliderChanged(self):
        """RGB sliders changed - update color and ColorEdit"""
        if self._updating:
            return

        self._updating = True
        try:
            r = self.r_slider.value
            g = self.g_slider.value
            b = self.b_slider.value
            self.current_color = vec4(r, g, b, self.current_color.w)
            self.coloredit.currentColor = self.current_color
        finally:
            self._updating = False

    ###########################################################################

    def _onColorEditChanged(self):
        """ColorEdit changed - update sliders"""
        if self._updating:
            return

        self._updating = True
        try:
            color = self.coloredit.currentColor
            self.current_color = color
            self.r_slider.value = color.x
            self.g_slider.value = color.y
            self.b_slider.value = color.z
        finally:
            self._updating = False

    ###########################################################################

    def _onCommit(self):
        """Commit the current color and close"""
        if self.onCommit:
            self.onCommit(self.current_color)

    ###########################################################################

    def _onCancel(self):
        """Cancel and close without saving"""
        if self.onCancel:
            self.onCancel()

    ###########################################################################

    @staticmethod
    def uifactory(parent_layoutgroup, args):
        """
        UI factory for use with layoutgroup makeChild

        Args:
            parent_layoutgroup: Parent LayoutGroup
            args: [name, initial_color, height]

        Returns:
            uilayoutitem_ptr_t (the container's layout item)
        """
        name = args[0]
        initial_color = args[1]

        # Create horizontal pack container with fixed height
        container_item = parent_layoutgroup.makeChild(uiclass=lev2.ui.HorizontalPack, args=[name])
        container = container_item.widget
        container.margin = 4
        container.uniform = True
        
        # Create ColorPicker and store it in the container's uservars
        picker = ColorPicker(container, name, initial_color)

        # Store picker instance in container's uservars for later access
        container.uservars.color_picker = picker

        return container_item

    ###########################################################################

    @staticmethod
    def wfactory(args):
        """
        Widget factory for use with widget.makeChild (e.g., vpack.makeChild)

        Args:
            args: [name, initial_color, height]

        Returns:
            HorizontalPack widget containing the ColorPicker
        """
        name = args[0]
        bg_color = args[1]
        initial_color = args[2]
        
        # increment static counter (int)
        
        hpack = lev2.ui.HorizontalPack.wfactory([name])
        hpack.bg_color = vec4(bg_color,1)
        hpack.margin = 2
        hpack.uniform = True
        
        # Create ColorPicker and store it in the hpack's uservars
        picker = ColorPicker(hpack, name, initial_color,bg_color)

        # Store picker instance in hpack's uservars for later access
        hpack.uservars.color_picker = picker

        return hpack
