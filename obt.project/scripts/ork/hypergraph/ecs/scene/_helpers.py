###############################################################################
# scene helpers — pure Transform/orientation utilities shared by the Scene core
# (__init__.py) and the feature mixins (_terrain/_walker/_projectiles). Kept in
# their own module so the mixins can import them without a cycle back to __init__.
###############################################################################

from orkengine.core import Transform as _CoreTransform


def Transform(**kwargs):
  """Kwargs-style constructor for lev2.Transform.

  Author writes:
      transform=Transform(translation=vec3(0, 5, 0))

  Equivalent dict form, used inline at the entity/spawner boundary:
      transform={"translation": vec3(0, 5, 0)}

  Implementation: constructs the underlying reflected Transform via the
  no-arg ctor and applies each kwarg via setattr."""
  xf = _CoreTransform()
  for k, v in kwargs.items():
    setattr(xf, k, v)
  return xf


def axis_angle(axis, angle):
  """Orientation DSL constructor — axis-angle → quat.

  axis : fvec3 (any non-zero rotation axis; auto-normalized by the C++ side)
  angle: radians

  Returns a quat, ready to drop into transform={"orientation": axis_angle(...)}.
  Thin alias for quat.createFromAxisAngle — short name reads better in
  transform dicts than the long static-method form."""
  from orkengine.core import quat
  return quat.createFromAxisAngle(axis, float(angle))


def _coerce_transform(t):
  """Accept Transform, dict, or None → Transform or None.

  Used at every entity/spawner boundary so authors can drop in either:
      transform=Transform(translation=vec3(-5, 0, 0))
  or:
      transform={"translation": vec3(-5, 0, 0),
                 "orientation": axis_angle(vec3(0, 1, 0), math.pi/4),
                 "scale": 1.5}
  or simply omit the kwarg (None → no transform decl)."""
  if t is None or isinstance(t, _CoreTransform):
    return t
  if isinstance(t, dict):
    return Transform(**t)
  raise TypeError(
    f"transform must be a Transform, dict, or None; got {type(t).__name__}")
