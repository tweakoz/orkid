################################################################################
# GridMaze - Generic grid/maze class with BFS pathfinding
################################################################################

from collections import deque
from .grid_direction import DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT, DIR_NONE, DIR_DELTA

class GridMaze:
  """
  Generic 2D grid/maze for tile-based games.

  Usage:
    maze = GridMaze(width=28, height=31)
    maze.set_cell(x, y, '#')  # wall
    maze.set_cell(x, y, ' ')  # empty

    # Or parse from string data:
    maze = GridMaze.from_strings(["####", "#  #", "####"])

    # Check passability:
    if maze.can_enter(x, y):
      ...

    # BFS pathfinding:
    direction = maze.bfs_direction(start_x, start_y, target_x, target_y)
  """

  # Default cell types
  CELL_EMPTY = ' '
  CELL_WALL = '#'

  def __init__(self, width, height, default_cell=None):
    self.width = width
    self.height = height
    self._default_cell = default_cell if default_cell is not None else self.CELL_EMPTY
    self.grid = [[self._default_cell for _ in range(width)] for _ in range(height)]

    # Passability rules: set of cell types that block movement
    # Can be customized per entity type using can_enter's entity_type parameter
    self._blocked_cells = {'#'}
    self._conditional_cells = {}  # cell_type -> set of entity_types that CAN pass

    # Wrap settings for tunnel-like behavior
    self.wrap_horizontal = False
    self.wrap_vertical = False

  @classmethod
  def from_strings(cls, rows, cell_map=None):
    """
    Create maze from list of strings.

    Args:
      rows: List of strings, each representing a row
      cell_map: Optional dict mapping characters to cell values
    """
    if not rows:
      return cls(0, 0)

    height = len(rows)
    width = max(len(row) for row in rows)
    maze = cls(width, height)

    for y, row in enumerate(rows):
      for x, char in enumerate(row):
        cell = cell_map.get(char, char) if cell_map else char
        maze.grid[y][x] = cell

    return maze

  def set_cell(self, x, y, value):
    """Set cell value at position"""
    if 0 <= x < self.width and 0 <= y < self.height:
      self.grid[y][x] = value

  def get_cell(self, x, y):
    """Get cell value at position. Returns wall for out of bounds."""
    if x < 0 or x >= self.width or y < 0 or y >= self.height:
      return self.CELL_WALL
    return self.grid[y][x]

  def set_blocked_cells(self, cell_types):
    """Set which cell types block all movement"""
    self._blocked_cells = set(cell_types)

  def set_conditional_cell(self, cell_type, allowed_entity_types):
    """Set a cell type that only specific entity types can pass through"""
    self._conditional_cells[cell_type] = set(allowed_entity_types)

  def can_enter(self, x, y, entity_type=None):
    """
    Check if position is passable.

    Args:
      x, y: Grid coordinates
      entity_type: Optional string identifying entity type for conditional cells
    """
    cell = self.get_cell(x, y)

    if cell in self._blocked_cells:
      return False

    if cell in self._conditional_cells:
      if entity_type is None:
        return False
      return entity_type in self._conditional_cells[cell]

    return True

  def wrap_x(self, x):
    """Wrap x coordinate if horizontal wrapping enabled"""
    if not self.wrap_horizontal:
      return x
    if x < 0:
      return self.width - 1
    if x >= self.width:
      return 0
    return x

  def wrap_y(self, y):
    """Wrap y coordinate if vertical wrapping enabled"""
    if not self.wrap_vertical:
      return y
    if y < 0:
      return self.height - 1
    if y >= self.height:
      return 0
    return y

  def wrap_pos(self, x, y):
    """Wrap both coordinates"""
    return self.wrap_x(x), self.wrap_y(y)

  def bfs_direction(self, start_x, start_y, target_x, target_y, entity_type=None):
    """
    Use BFS to find the best first direction to reach target.

    Args:
      start_x, start_y: Starting position
      target_x, target_y: Target position
      entity_type: Entity type for passability checks

    Returns:
      Direction constant (DIR_UP, etc.) or DIR_NONE if no path
    """
    if start_x == target_x and start_y == target_y:
      return DIR_NONE

    queue = deque()
    visited = set()
    visited.add((start_x, start_y))

    # Add initial moves
    for d in [DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT]:
      dx, dy = DIR_DELTA[d]
      nx = self.wrap_x(start_x + dx)
      ny = self.wrap_y(start_y + dy)
      if self.can_enter(nx, ny, entity_type):
        if nx == target_x and ny == target_y:
          return d
        if (nx, ny) not in visited:
          queue.append((nx, ny, d))
          visited.add((nx, ny))

    # BFS
    while queue:
      x, y, first_dir = queue.popleft()

      for d in [DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT]:
        dx, dy = DIR_DELTA[d]
        nx = self.wrap_x(x + dx)
        ny = self.wrap_y(y + dy)

        if (nx, ny) in visited:
          continue
        if not self.can_enter(nx, ny, entity_type):
          continue

        if nx == target_x and ny == target_y:
          return first_dir

        visited.add((nx, ny))
        queue.append((nx, ny, first_dir))

    return DIR_NONE

  def bfs_path(self, start_x, start_y, target_x, target_y, entity_type=None):
    """
    Use BFS to find complete path to target.

    Returns:
      List of (x, y) positions from start to target, or empty list if no path
    """
    if start_x == target_x and start_y == target_y:
      return [(start_x, start_y)]

    queue = deque()
    visited = {(start_x, start_y): None}

    # Add initial positions
    for d in [DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT]:
      dx, dy = DIR_DELTA[d]
      nx = self.wrap_x(start_x + dx)
      ny = self.wrap_y(start_y + dy)
      if self.can_enter(nx, ny, entity_type) and (nx, ny) not in visited:
        visited[(nx, ny)] = (start_x, start_y)
        if nx == target_x and ny == target_y:
          return self._reconstruct_path(visited, (nx, ny))
        queue.append((nx, ny))

    while queue:
      x, y = queue.popleft()

      for d in [DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT]:
        dx, dy = DIR_DELTA[d]
        nx = self.wrap_x(x + dx)
        ny = self.wrap_y(y + dy)

        if (nx, ny) in visited:
          continue
        if not self.can_enter(nx, ny, entity_type):
          continue

        visited[(nx, ny)] = (x, y)
        if nx == target_x and ny == target_y:
          return self._reconstruct_path(visited, (nx, ny))
        queue.append((nx, ny))

    return []

  def _reconstruct_path(self, visited, end):
    """Reconstruct path from BFS visited dict"""
    path = []
    current = end
    while current is not None:
      path.append(current)
      current = visited[current]
    path.reverse()
    return path

  def find_cells(self, cell_type):
    """Find all positions with given cell type"""
    positions = []
    for y in range(self.height):
      for x in range(self.width):
        if self.grid[y][x] == cell_type:
          positions.append((x, y))
    return positions

  def __repr__(self):
    return f"GridMaze({self.width}x{self.height})"
