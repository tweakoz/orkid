################################################################################
# TransformEdit - Composite widget for editing DecompTransform
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import math
from orkengine.core import vec3, vec4, quat, Transform, CrcStringProxy
from orkengine import lev2
from ork.ui import icon_library

################################################################################

def quat_to_axis_angle(q):
  """Convert quaternion to axis-angle representation.
  Returns (axis_x, axis_y, axis_z, angle_degrees)
  """
  # Quaternion components (assuming q has x,y,z,w or similar access)
  # For identity quat, w=1, x=y=z=0
  # angle = 2 * acos(w)
  # axis = (x,y,z) / sin(angle/2)

  # Get quaternion components - try different access patterns
  try:
    w = q.w
    x = q.x
    y = q.y
    z = q.z
  except:
    # If direct access fails, assume it's identity
    return (0.0, 1.0, 0.0, 0.0)

  # Normalize quaternion
  length = math.sqrt(w*w + x*x + y*y + z*z)
  if length > 0.0001:
    w /= length
    x /= length
    y /= length
    z /= length

  # Clamp w to [-1, 1] to avoid acos domain errors
  w = max(-1.0, min(1.0, w))

  angle_rad = 2.0 * math.acos(w)
  angle_deg = math.degrees(angle_rad)

  # Calculate axis
  s = math.sqrt(1.0 - w*w)
  if s < 0.0001:
    # Angle is close to 0, axis doesn't matter
    return (0.0, 1.0, 0.0, angle_deg)

  axis_x = x / s
  axis_y = y / s
  axis_z = z / s

  return (axis_x, axis_y, axis_z, angle_deg)

################################################################################

class TransformEdit:
  """
  Composite widget for editing a DecompTransform (transform_ptr_t).

  In Python bindings, DecompTransform is called "Transform" with properties:
    - translation (vec3)
    - orientation (quat)
    - scale (float) - uniform scale
    - nonUniformScale (vec3)

  Layout:
    Row 1: Translation [X] [Y] [Z]
    Row 2: Orientation (axis-angle) [X] [Y] [Z] [Ang]
    Row 3: Scale [U] [Scale] or [U] [X] [Y] [Z]
    Row 4: [Set to Identity]
  """

  def __init__(self, vpack, transform=None):
    """
    Initialize the transform editor.

    Args:
      vpack: The VerticalPack widget containing all rows
      transform: Optional Transform (transform_ptr_t) to bind to
    """
    self.vpack = vpack
    self._transform = transform
    self._uniform_scale = True
    self._updating_ui = False  # Prevent feedback loops

    # Widget references (set during creation)
    self.trans_x = None
    self.trans_y = None
    self.trans_z = None
    self.axis_x = None
    self.axis_y = None
    self.axis_z = None
    self.angle = None
    self.uniform_btn = None
    self.scale_uniform = None
    self.scale_x = None
    self.scale_y = None
    self.scale_z = None
    self.identity_btn = None

    # Scale row widgets for toggling visibility
    self.hpack_scale = None
    self.hpack_scale_uniform = None
    self.hpack_scale_xyz = None

  @property
  def transform(self):
    return self._transform

  @transform.setter
  def transform(self, value):
    self._transform = value
    self._refreshFromData()

  def _refreshFromData(self):
    """Update UI from the bound transform data."""
    if self._transform is None:
      return

    self._updating_ui = True
    try:
      # Translation
      t = self._transform.translation
      if self.trans_x: self.trans_x.value = t.x
      if self.trans_y: self.trans_y.value = t.y
      if self.trans_z: self.trans_z.value = t.z

      # Orientation (convert quaternion to axis-angle)
      q = self._transform.orientation
      ax, ay, az, ang = quat_to_axis_angle(q)
      if self.axis_x: self.axis_x.value = ax
      if self.axis_y: self.axis_y.value = ay
      if self.axis_z: self.axis_z.value = az
      if self.angle: self.angle.value = ang

      # Scale
      us = self._transform.scale
      ns = self._transform.nonUniformScale
      if self.scale_uniform: self.scale_uniform.value = us
      if self.scale_x: self.scale_x.value = ns.x
      if self.scale_y: self.scale_y.value = ns.y
      if self.scale_z: self.scale_z.value = ns.z
    finally:
      self._updating_ui = False

  def _onTranslationChanged(self, val):
    """Called when any translation field changes."""
    if self._updating_ui or self._transform is None:
      return
    t = vec3(self.trans_x.value, self.trans_y.value, self.trans_z.value)
    self._transform.translation = t

  def _onOrientationChanged(self, val):
    """Called when any orientation field changes."""
    if self._updating_ui or self._transform is None:
      return
    # Convert axis-angle back to quaternion
    axis = vec3(self.axis_x.value, self.axis_y.value, self.axis_z.value)
    angle_rad = math.radians(self.angle.value)
    # quat(axis, angle) constructor
    q = quat(axis, angle_rad)
    self._transform.orientation = q

  def _onUniformScaleChanged(self, val):
    """Called when uniform scale field changes."""
    if self._updating_ui or self._transform is None:
      return
    self._transform.scale = val

  def _onNonUniformScaleChanged(self, val):
    """Called when any non-uniform scale field changes."""
    if self._updating_ui or self._transform is None:
      return
    s = vec3(self.scale_x.value, self.scale_y.value, self.scale_z.value)
    self._transform.nonUniformScale = s

  def _onUniformToggle(self, btn):
    """Toggle between uniform and non-uniform scale."""
    self._uniform_scale = not self._uniform_scale
    self._updateScaleRowVisibility()

    # When switching to uniform, copy X to uniform scale
    if self._uniform_scale and self._transform:
      self._transform.scale = self._transform.nonUniformScale.x
      self._refreshFromData()
    # When switching to non-uniform, copy uniform to all axes
    elif not self._uniform_scale and self._transform:
      us = self._transform.scale
      self._transform.nonUniformScale = vec3(us, us, us)
      self._refreshFromData()

  def _updateScaleRowVisibility(self):
    """Swap scale hpacks based on uniform mode."""
    if not self.hpack_scale:
      return
    # Swap the inner hpacks
    if self._uniform_scale:
      self.hpack_scale.removeChild(self.hpack_scale_xyz, False)
      self.hpack_scale.addChild(self.hpack_scale_uniform, True)
    else:
      self.hpack_scale.removeChild(self.hpack_scale_uniform, False)
      self.hpack_scale.addChild(self.hpack_scale_xyz, True)

  def _onSetIdentity(self, btn):
    """Reset transform to identity."""
    if self._transform is None:
      return
    self._transform.translation = vec3(0, 0, 0)
    self._transform.orientation = quat()  # Identity quaternion
    self._transform.scale = 1.0
    self._transform.nonUniformScale = vec3(1, 1, 1)
    self._refreshFromData()

  ###########################################################################
  # Factory methods
  ###########################################################################

  @staticmethod
  def _build_ui(vpack, editor, transform):
    """
    Build the UI widgets inside the vpack.

    Args:
      vpack: The VerticalPack widget to populate
      editor: The TransformEdit instance
      transform: Optional Transform to bind to
    """
    MARGIN = 2
    LABEL_WIDTH = 32
    btn_color = vec3(0.3, 0.35, 0.4)
    action_color = vec3(0.35, 0.28, 0.32)
    tokens = CrcStringProxy()

    # Row 1: Translation
    hpack_trans = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["trans_row"])
    hpack_trans.margin = MARGIN
    hpack_trans.item_width = LABEL_WIDTH
    hpack_trans.fill = True

    trans_label = hpack_trans.makeChild(uiclass=lev2.ui.Button, args=["Pos", btn_color])

    hpack_trans_edits = hpack_trans.makeChild(uiclass=lev2.ui.HorizontalPack, args=["trans_edits"])
    hpack_trans_edits.margin = MARGIN
    hpack_trans_edits.uniform = True

    editor.trans_x = hpack_trans_edits.makeChild(uiclass=lev2.ui.F32Edit, args=["tx", "X", 0.0])
    editor.trans_y = hpack_trans_edits.makeChild(uiclass=lev2.ui.F32Edit, args=["ty", "Y", 0.0])
    editor.trans_z = hpack_trans_edits.makeChild(uiclass=lev2.ui.F32Edit, args=["tz", "Z", 0.0])

    editor.trans_x.onValueChanged(editor._onTranslationChanged)
    editor.trans_y.onValueChanged(editor._onTranslationChanged)
    editor.trans_z.onValueChanged(editor._onTranslationChanged)

    # Row 2: Orientation (axis-angle)
    hpack_orient = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["orient_row"])
    hpack_orient.margin = MARGIN
    hpack_orient.item_width = LABEL_WIDTH
    hpack_orient.fill = True

    orient_label = hpack_orient.makeChild(uiclass=lev2.ui.Button, args=["Rot", btn_color])

    hpack_orient_inner = hpack_orient.makeChild(uiclass=lev2.ui.HorizontalPack, args=["orient_inner"])
    hpack_orient_inner.margin = 0
    hpack_orient_inner.uniform = True

    # Axis section
    hpack_axis = hpack_orient_inner.makeChild(uiclass=lev2.ui.HorizontalPack, args=["axis_section"])
    hpack_axis.margin = MARGIN
    hpack_axis.item_width = 48
    hpack_axis.fill = True

    axis_label = hpack_axis.makeChild(uiclass=lev2.ui.TextBox, args=["axis_label", vec4(btn_color.x, btn_color.y, btn_color.z, 1), "Axis"])

    hpack_axis_edits = hpack_axis.makeChild(uiclass=lev2.ui.HorizontalPack, args=["axis_edits"])
    hpack_axis_edits.margin = MARGIN
    hpack_axis_edits.uniform = True

    editor.axis_x = hpack_axis_edits.makeChild(uiclass=lev2.ui.F32Edit, args=["ax", "X", 0.0, -1.0, 1.0])
    editor.axis_y = hpack_axis_edits.makeChild(uiclass=lev2.ui.F32Edit, args=["ay", "Y", 1.0, -1.0, 1.0])
    editor.axis_z = hpack_axis_edits.makeChild(uiclass=lev2.ui.F32Edit, args=["az", "Z", 0.0, -1.0, 1.0])

    # Angle section
    hpack_angle = hpack_orient_inner.makeChild(uiclass=lev2.ui.HorizontalPack, args=["angle_section"])
    hpack_angle.margin = MARGIN
    hpack_angle.item_width = 48
    hpack_angle.fill = True

    angle_label = hpack_angle.makeChild(uiclass=lev2.ui.TextBox, args=["angle_label", vec4(btn_color.x, btn_color.y, btn_color.z, 1), "Angle"])

    editor.angle = hpack_angle.makeChild(uiclass=lev2.ui.F32Edit, args=["ang", "deg", 0.0, -360.0, 360.0])

    editor.axis_x.onValueChanged(editor._onOrientationChanged)
    editor.axis_y.onValueChanged(editor._onOrientationChanged)
    editor.axis_z.onValueChanged(editor._onOrientationChanged)
    editor.angle.onValueChanged(editor._onOrientationChanged)

    # Row 3: Scale (with uniform toggle)
    hpack_scale = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["scale_row"])
    hpack_scale.margin = MARGIN
    hpack_scale.item_width = LABEL_WIDTH
    hpack_scale.fill = True
    editor.hpack_scale = hpack_scale

    scale_label = hpack_scale.makeChild(uiclass=lev2.ui.Button, args=["Sca", btn_color])

    editor.uniform_btn = hpack_scale.makeChild(uiclass=lev2.ui.ImageButton, args=["uniform_btn"])
    editor.uniform_btn.inactive_image = icon_library.text_icon("U", 24, 24)
    editor.uniform_btn.bgcolor = vec4(btn_color.x, btn_color.y, btn_color.z, 1)
    editor.uniform_btn.hover_color = vec4(btn_color.x + 0.1, btn_color.y + 0.1, btn_color.z + 0.1, 1)
    editor.uniform_btn.pressed_color = vec4(0.2, 0.4, 0.6, 1)
    editor.uniform_btn.inactive_blend_mode = tokens.ALPHA
    editor.uniform_btn.onPressed = editor._onUniformToggle

    # Uniform scale hpack (swappable)
    editor.hpack_scale_uniform = lev2.ui.HorizontalPack.wfactory(["scale_uniform_container"])
    editor.hpack_scale_uniform.margin = 0
    editor.hpack_scale_uniform.uniform = True

    editor.scale_uniform = editor.hpack_scale_uniform.makeChild(uiclass=lev2.ui.F32Edit, args=["su", "Scale", 1.0, 0.001, 1000.0])
    editor.scale_uniform.onValueChanged(editor._onUniformScaleChanged)

    # Non-uniform scale hpack (swappable)
    editor.hpack_scale_xyz = lev2.ui.HorizontalPack.wfactory(["scale_xyz_container"])
    editor.hpack_scale_xyz.margin = MARGIN
    editor.hpack_scale_xyz.uniform = True

    editor.scale_x = editor.hpack_scale_xyz.makeChild(uiclass=lev2.ui.F32Edit, args=["sx", "X", 1.0, 0.001, 1000.0])
    editor.scale_y = editor.hpack_scale_xyz.makeChild(uiclass=lev2.ui.F32Edit, args=["sy", "Y", 1.0, 0.001, 1000.0])
    editor.scale_z = editor.hpack_scale_xyz.makeChild(uiclass=lev2.ui.F32Edit, args=["sz", "Z", 1.0, 0.001, 1000.0])

    editor.scale_x.onValueChanged(editor._onNonUniformScaleChanged)
    editor.scale_y.onValueChanged(editor._onNonUniformScaleChanged)
    editor.scale_z.onValueChanged(editor._onNonUniformScaleChanged)

    hpack_scale.addChild(editor.hpack_scale_uniform)

    # Row 4: Identity button
    hpack_identity = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["identity_row"])
    hpack_identity.margin = MARGIN
    hpack_identity.item_width = 24
    hpack_identity.fill = True

    identity_label = hpack_identity.makeChild(uiclass=lev2.ui.TextBox, args=["identity_label", vec4(action_color.x, action_color.y, action_color.z, 1), "Reset to Identity"])
    identity_label.fixed_width = 168

    editor.identity_btn = hpack_identity.makeChild(uiclass=lev2.ui.ImageButton, args=["identity_btn"])
    editor.identity_btn.inactive_image = icon_library.crosshairs_icon(20, 20)
    editor.identity_btn.bgcolor = vec4(action_color.x, action_color.y, action_color.z, 1)
    editor.identity_btn.hover_color = vec4(action_color.x + 0.1, action_color.y + 0.1, action_color.z + 0.1, 1)
    editor.identity_btn.pressed_color = vec4(0.5, 0.35, 0.4, 1)
    editor.identity_btn.inactive_blend_mode = tokens.ALPHA
    editor.identity_btn.onPressed = editor._onSetIdentity

    # Refresh from data if provided
    if transform:
      editor._refreshFromData()

    # Store editor in vpack's uservars
    vpack.uservars.transform_edit = editor

  @staticmethod
  def uifactory(parent_layoutgroup, args):
    """
    UI factory for use with layoutgroup.makeChild

    Args:
      parent_layoutgroup: Parent LayoutGroup
      args: [name] or [name, transform]

    Returns:
      uilayoutitem_ptr_t (the vpack's layout item)
    """
    name = args[0]
    transform = args[1] if len(args) > 1 else None

    vpack_item = parent_layoutgroup.makeChild(uiclass=lev2.ui.VerticalPack, args=[name])
    vpack = vpack_item.widget
    vpack.margin = 2
    vpack.item_height = 24
    vpack.fill = False
    vpack.fixed_height = 108

    editor = TransformEdit(vpack, transform)
    TransformEdit._build_ui(vpack, editor, transform)

    return vpack_item

  @staticmethod
  def wfactory(args):
    """
    Widget factory for standalone creation.

    Args:
      args: [name] or [name, transform]

    Returns:
      VerticalPack widget containing the TransformEdit
    """
    name = args[0]
    transform = args[1] if len(args) > 1 else None

    vpack = lev2.ui.VerticalPack.wfactory([name])
    vpack.margin = 2
    vpack.item_height = 24
    vpack.fill = False
    vpack.fixed_height = 108

    editor = TransformEdit(vpack, transform)
    TransformEdit._build_ui(vpack, editor, transform)

    return vpack
