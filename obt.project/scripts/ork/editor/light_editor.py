################################################################################
# Light Property Editor - MVC controller for light properties
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

from orkengine.core import vec3, vec4

################################################################################

class LightPropertyEditor:
  """MVC controller that mediates between light model data and UI views.

  Connects a ColorPicker and intensity F32Edit to a light's data,
  handling bidirectional synchronization.
  """

  def __init__(self, color_picker, intensity_edit):
    """Initialize the light property editor.

    Args:
      color_picker: ColorPicker widget instance
      intensity_edit: F32Edit widget for intensity
    """
    self._color_picker = color_picker
    self._intensity_edit = intensity_edit
    self._light_data = None
    self._syncing_from_model = False

    # Connect view callbacks to controller
    self._color_picker.onColorChanged = self._onViewChanged
    self._intensity_edit.onValueChanged(lambda v: self._onViewChanged())

  def bind(self, light_data):
    """Bind to a light's data and sync view from model.

    Args:
      light_data: LightData instance (e.g., PointLightData)
    """
    self._light_data = light_data
    self._syncViewFromModel()

  def unbind(self):
    """Unbind from current light."""
    self._light_data = None

  @property
  def is_bound(self):
    """Check if currently bound to a light."""
    return self._light_data is not None

  def _syncViewFromModel(self):
    """Update view to reflect model state. Does not trigger write-back."""
    if not self._light_data:
      return
    self._syncing_from_model = True
    c = self._light_data.color
    intensity = max(c.x, c.y, c.z, 0.001)
    self._color_picker.current_color = vec4(c.x / intensity, c.y / intensity, c.z / intensity, 1.0)
    self._intensity_edit.value = intensity
    self._syncing_from_model = False

  def _onViewChanged(self):
    """Called when user edits the view. Writes to model."""
    if self._syncing_from_model or not self._light_data:
      return
    c = self._color_picker.current_color
    i = self._intensity_edit.value
    self._light_data.color = vec3(c.x * i, c.y * i, c.z * i)
