################################################################################
# Grid Direction utilities for 2D maze-based games
################################################################################

# Direction constants
DIR_RIGHT = 0
DIR_DOWN = 1
DIR_LEFT = 2
DIR_UP = 3
DIR_NONE = -1

# Direction deltas: (dx, dy) for each direction
DIR_DELTA = {
  DIR_RIGHT: (1, 0),
  DIR_DOWN: (0, 1),
  DIR_LEFT: (-1, 0),
  DIR_UP: (0, -1),
}

# Direction names for debugging
DIR_NAMES = {
  DIR_RIGHT: "RIGHT",
  DIR_DOWN: "DOWN",
  DIR_LEFT: "LEFT",
  DIR_UP: "UP",
  DIR_NONE: "NONE",
}

def opposite_dir(d):
  """Return the opposite direction"""
  if d == DIR_RIGHT: return DIR_LEFT
  if d == DIR_LEFT: return DIR_RIGHT
  if d == DIR_UP: return DIR_DOWN
  if d == DIR_DOWN: return DIR_UP
  return DIR_NONE

def rotate_dir_cw(d):
  """Rotate direction 90 degrees clockwise"""
  if d == DIR_NONE: return DIR_NONE
  return (d + 1) % 4

def rotate_dir_ccw(d):
  """Rotate direction 90 degrees counter-clockwise"""
  if d == DIR_NONE: return DIR_NONE
  return (d - 1) % 4

def dir_from_delta(dx, dy):
  """Get direction constant from delta values"""
  for d, (ddx, ddy) in DIR_DELTA.items():
    if dx == ddx and dy == ddy:
      return d
  return DIR_NONE

__all__ = [
  'DIR_RIGHT', 'DIR_DOWN', 'DIR_LEFT', 'DIR_UP', 'DIR_NONE',
  'DIR_DELTA', 'DIR_NAMES',
  'opposite_dir', 'rotate_dir_cw', 'rotate_dir_ccw', 'dir_from_delta',
]
