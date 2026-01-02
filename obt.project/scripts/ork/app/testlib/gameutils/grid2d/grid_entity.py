################################################################################
# GridEntity - Grid-based entity with smooth movement interpolation
################################################################################

from .grid_direction import DIR_NONE, DIR_DELTA

class GridEntity:
  """
  Entity that moves on a grid with smooth interpolation between cells.

  Features:
  - Integer grid positions (grid_x, grid_y)
  - Smooth animation via move_progress interpolation
  - Direction tracking
  - Collision-checked movement via try_move()

  Usage:
    entity = GridEntity(start_x, start_y)
    entity.speed = 5.0  # cells per second

    # In update loop:
    entity.update(dt)
    if entity.is_at_cell():
      entity.try_move(maze, DIR_RIGHT)

    # For rendering, get interpolated position:
    rx, ry = entity.get_render_pos()
  """

  def __init__(self, x, y, entity_type=None):
    """
    Initialize entity at grid position.

    Args:
      x, y: Starting grid coordinates (integers)
      entity_type: Optional string for maze passability checks
    """
    self.grid_x = x
    self.grid_y = y
    self.prev_x = x
    self.prev_y = y
    self.move_progress = 1.0  # 1.0 = at cell, 0.0 = just started moving
    self.direction = DIR_NONE
    self.next_direction = DIR_NONE
    self.speed = 5.0  # Cells per second
    self.entity_type = entity_type

  def get_render_pos(self):
    """Get interpolated position for smooth rendering"""
    t = self.move_progress
    rx = self.prev_x + (self.grid_x - self.prev_x) * t
    ry = self.prev_y + (self.grid_y - self.prev_y) * t
    return rx, ry

  def is_at_cell(self):
    """Check if entity has reached its target cell"""
    return self.move_progress >= 1.0

  def is_moving(self):
    """Check if entity is currently moving between cells"""
    return self.move_progress < 1.0

  def try_move(self, maze, direction):
    """
    Try to start moving in a direction.

    Args:
      maze: GridMaze instance for collision checking
      direction: Direction constant (DIR_UP, etc.)

    Returns:
      True if move started, False if blocked
    """
    if direction == DIR_NONE:
      return False

    if direction not in DIR_DELTA:
      return False

    dx, dy = DIR_DELTA[direction]
    new_x = maze.wrap_x(self.grid_x + dx)
    new_y = maze.wrap_y(self.grid_y + dy)

    if maze.can_enter(new_x, new_y, self.entity_type):
      self.prev_x = self.grid_x
      self.prev_y = self.grid_y
      self.grid_x = new_x
      self.grid_y = new_y
      self.direction = direction
      self.move_progress = 0.0
      return True

    return False

  def teleport(self, x, y):
    """Instantly move to position without animation"""
    self.grid_x = x
    self.grid_y = y
    self.prev_x = x
    self.prev_y = y
    self.move_progress = 1.0
    self.direction = DIR_NONE

  def update(self, dt):
    """
    Update movement animation.

    Args:
      dt: Delta time in seconds
    """
    if self.move_progress < 1.0:
      self.move_progress += self.speed * dt
      if self.move_progress >= 1.0:
        self.move_progress = 1.0

  def distance_to(self, other):
    """Calculate grid distance to another entity"""
    dx = self.grid_x - other.grid_x
    dy = self.grid_y - other.grid_y
    return (dx * dx + dy * dy) ** 0.5

  def distance_to_pos(self, x, y):
    """Calculate grid distance to a position"""
    dx = self.grid_x - x
    dy = self.grid_y - y
    return (dx * dx + dy * dy) ** 0.5

  def render_distance_to(self, other):
    """Calculate render distance to another entity (uses interpolated positions)"""
    rx1, ry1 = self.get_render_pos()
    rx2, ry2 = other.get_render_pos()
    dx = rx1 - rx2
    dy = ry1 - ry2
    return (dx * dx + dy * dy) ** 0.5

  def same_cell_as(self, other):
    """Check if on same grid cell as another entity"""
    return self.grid_x == other.grid_x and self.grid_y == other.grid_y

  def at_position(self, x, y):
    """Check if at specific grid position"""
    return self.grid_x == x and self.grid_y == y

  def __repr__(self):
    return f"GridEntity(pos=({self.grid_x},{self.grid_y}), dir={self.direction}, progress={self.move_progress:.2f})"
