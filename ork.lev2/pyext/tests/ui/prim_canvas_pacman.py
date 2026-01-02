#!/usr/bin/env ork.python

################################################################################
# PrimCanvas Pac-Man
# Uses textured quads with numpy-generated sprites
# Grid-based movement system
################################################################################

import signal
import math
import random
import numpy as np
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################
# Maze layout
################################################################################

MAZE_DATA = [
  "############################",
  "#............##............#",
  "#.####.#####.##.#####.####.#",
  "#o####.#####.##.#####.####o#",
  "#.####.#####.##.#####.####.#",
  "#..........................#",
  "#.####.##.########.##.####.#",
  "#.####.##.########.##.####.#",
  "#......##....##....##......#",
  "######.##### ## #####.######",
  "     #.##### ## #####.#     ",
  "     #.##          ##.#     ",
  "     #.## ###--### ##.#     ",
  "######.## #      # ##.######",
  "      .   #      #   .      ",
  "######.## #      # ##.######",
  "     #.## ######## ##.#     ",
  "     #.##          ##.#     ",
  "     #.## ######## ##.#     ",
  "######.## ######## ##.######",
  "#............##............#",
  "#.####.#####.##.#####.####.#",
  "#.####.#####.##.#####.####.#",
  "#o..##.......  .......##..o#",
  "###.##.##.########.##.##.###",
  "###.##.##.########.##.##.###",
  "#......##....##....##......#",
  "#.##########.##.##########.#",
  "#.##########.##.##########.#",
  "#..........................#",
  "############################",
]

# Direction constants
DIR_RIGHT = 0
DIR_DOWN = 1
DIR_LEFT = 2
DIR_UP = 3
DIR_NONE = -1

# Direction deltas: [dx, dy] for each direction
DIR_DELTA = {
  DIR_RIGHT: (1, 0),
  DIR_DOWN: (0, 1),
  DIR_LEFT: (-1, 0),
  DIR_UP: (0, -1),
}

def opposite_dir(d):
  if d == DIR_RIGHT: return DIR_LEFT
  if d == DIR_LEFT: return DIR_RIGHT
  if d == DIR_UP: return DIR_DOWN
  if d == DIR_DOWN: return DIR_UP
  return DIR_NONE

################################################################################
# Maze class
################################################################################

class Maze:
  def __init__(self):
    self.width = len(MAZE_DATA[0])
    self.height = len(MAZE_DATA)
    self.grid = []
    self.dots = set()
    self.power_pellets = set()
    self.pacman_start = (1, 1)
    self.ghost_house_pos = (13, 14)  # Where ghosts spawn

    # Parse maze
    for y, row in enumerate(MAZE_DATA):
      grid_row = []
      for x, cell in enumerate(row):
        if cell == '#':
          grid_row.append('#')
        elif cell == '-':
          grid_row.append('-')  # Ghost door
        elif cell == '.':
          grid_row.append(' ')
          self.dots.add((x, y))
        elif cell == 'o':
          grid_row.append(' ')
          self.power_pellets.add((x, y))
        else:
          grid_row.append(' ')  # Empty space

        # Find pacman start (center bottom area)
        if y == 23 and x == 13:
          self.pacman_start = (x, y)

      # Pad row to consistent width
      while len(grid_row) < self.width:
        grid_row.append(' ')
      self.grid.append(grid_row)

  def get_cell(self, x, y):
    """Get cell type at position, returns '#' for out of bounds"""
    if x < 0 or x >= self.width or y < 0 or y >= self.height:
      return '#'
    return self.grid[y][x]

  def can_enter(self, x, y, is_ghost=False):
    """Check if an entity can enter this cell"""
    cell = self.get_cell(x, y)
    if cell == '#':
      return False
    if cell == '-' and not is_ghost:
      return False
    return True

  def wrap_x(self, x):
    """Wrap x coordinate for tunnel"""
    if x < 0:
      return self.width - 1
    if x >= self.width:
      return 0
    return x

  def collect_dot(self, x, y):
    """Try to collect dot at position, returns points"""
    if (x, y) in self.dots:
      self.dots.remove((x, y))
      return 10
    if (x, y) in self.power_pellets:
      self.power_pellets.remove((x, y))
      return 50
    return 0

  def is_power_pellet(self, x, y):
    """Check if this position had a power pellet"""
    return (x, y) in self.power_pellets

  def bfs_direction(self, start_x, start_y, target_x, target_y, is_ghost=True):
    """
    BFS to find the best first direction to reach target.
    Returns the direction to take from start, or DIR_NONE if no path.
    """
    from collections import deque

    if start_x == target_x and start_y == target_y:
      return DIR_NONE

    # BFS queue: (x, y, first_direction)
    queue = deque()
    visited = set()
    visited.add((start_x, start_y))

    # Add initial moves
    for d in [DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT]:
      dx, dy = DIR_DELTA[d]
      nx = self.wrap_x(start_x + dx)
      ny = start_y + dy
      if self.can_enter(nx, ny, is_ghost):
        if nx == target_x and ny == target_y:
          return d
        queue.append((nx, ny, d))
        visited.add((nx, ny))

    # BFS
    while queue:
      x, y, first_dir = queue.popleft()

      for d in [DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT]:
        dx, dy = DIR_DELTA[d]
        nx = self.wrap_x(x + dx)
        ny = y + dy

        if (nx, ny) in visited:
          continue
        if not self.can_enter(nx, ny, is_ghost):
          continue

        if nx == target_x and ny == target_y:
          return first_dir

        visited.add((nx, ny))
        queue.append((nx, ny, first_dir))

    return DIR_NONE  # No path found

################################################################################
# Entity class for grid-based movement
################################################################################

class Entity:
  def __init__(self, x, y):
    self.grid_x = x
    self.grid_y = y
    self.prev_x = x
    self.prev_y = y
    self.move_progress = 1.0  # 1.0 = at grid position, 0.0 = just started moving
    self.direction = DIR_NONE
    self.next_direction = DIR_NONE
    self.speed = 5.0  # Cells per second

  def get_render_pos(self):
    """Get interpolated position for rendering"""
    t = self.move_progress
    rx = self.prev_x + (self.grid_x - self.prev_x) * t
    ry = self.prev_y + (self.grid_y - self.prev_y) * t
    return rx, ry

  def is_at_cell(self):
    """Check if entity has reached its target cell"""
    return self.move_progress >= 1.0

  def try_move(self, maze, direction, is_ghost=False):
    """Try to start moving in a direction. Returns True if successful."""
    if direction == DIR_NONE:
      return False

    dx, dy = DIR_DELTA[direction]
    new_x = maze.wrap_x(self.grid_x + dx)
    new_y = self.grid_y + dy

    if maze.can_enter(new_x, new_y, is_ghost):
      self.prev_x = self.grid_x
      self.prev_y = self.grid_y
      self.grid_x = new_x
      self.grid_y = new_y
      self.direction = direction
      self.move_progress = 0.0
      return True
    return False

  def update(self, dt):
    """Update movement animation"""
    if self.move_progress < 1.0:
      self.move_progress += self.speed * dt
      if self.move_progress >= 1.0:
        self.move_progress = 1.0

################################################################################
# Sprite generation
################################################################################

def create_pacman_texture(size, mouth_angle, direction=0):
  """Create pac-man sprite with given mouth opening and direction"""
  img = np.zeros((size, size, 3), dtype=np.uint8)
  center = size // 2
  radius = size // 2 - 2

  for y in range(size):
    for x in range(size):
      dx = x - center
      dy = y - center
      dist = math.sqrt(dx*dx + dy*dy)

      if dist <= radius:
        angle = math.atan2(dy, dx)
        angle = math.degrees(angle)
        # Rotate based on direction
        angle -= direction * 90
        angle = (angle + 180) % 360 - 180

        if abs(angle) > mouth_angle:
          img[y, x] = [255, 255, 0]

  return img

def create_ghost_texture(size, color_rgb):
  """Create ghost sprite"""
  img = np.zeros((size, size, 3), dtype=np.uint8)
  center = size // 2
  radius = size // 2 - 2

  for y in range(size):
    for x in range(size):
      dx = x - center
      dy = y - center
      dist = math.sqrt(dx*dx + dy*dy)

      # Top half is rounded
      if y <= center:
        if dist <= radius:
          img[y, x] = color_rgb
      else:
        # Bottom with wavy edge
        if abs(dx) <= radius:
          wave = int(2 * math.sin(x * math.pi / 4))
          if y < size - 2 + wave:
            img[y, x] = color_rgb

      # Eyes
      eye_y = center - 2
      for eye_x in [center - 4, center + 4]:
        edx = x - eye_x
        edy = y - eye_y
        eye_dist = math.sqrt(edx*edx + edy*edy)
        if eye_dist <= 3:
          img[y, x] = [255, 255, 255]
        if eye_dist <= 1.5:
          img[y, x] = [0, 0, 255]

  return img

def create_dot_texture(size):
  """Create small dot"""
  img = np.zeros((size, size, 3), dtype=np.uint8)
  center = size // 2
  radius = size // 6

  for y in range(size):
    for x in range(size):
      dx = x - center
      dy = y - center
      if dx*dx + dy*dy <= radius*radius:
        img[y, x] = [255, 200, 150]
  return img

def create_power_pellet_texture(size):
  """Create power pellet"""
  img = np.zeros((size, size, 3), dtype=np.uint8)
  center = size // 2
  radius = size // 3

  for y in range(size):
    for x in range(size):
      dx = x - center
      dy = y - center
      if dx*dx + dy*dy <= radius*radius:
        img[y, x] = [255, 200, 150]
  return img

def create_wall_texture(size):
  """Create wall tile"""
  img = np.zeros((size, size, 3), dtype=np.uint8)
  for y in range(size):
    for x in range(size):
      if x < 2 or x >= size-2 or y < 2 or y >= size-2:
        img[y, x] = [0, 0, 100]
      else:
        img[y, x] = [0, 0, 180]
  return img

################################################################################
# Ghost AI
################################################################################

class Ghost(Entity):
  def __init__(self, x, y, color_idx):
    super().__init__(x, y)
    self.color_idx = color_idx
    self.mode = 'scatter'  # scatter, chase, frightened
    self.speed = 4.0
    self.frightened_speed = 2.5

  def choose_direction(self, maze, pacman):
    """Choose next direction at intersection using BFS pathfinding"""
    if not self.is_at_cell():
      return

    if self.mode == 'frightened':
      # Random movement when frightened
      possible = []
      for d in [DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT]:
        if d == opposite_dir(self.direction):
          continue
        dx, dy = DIR_DELTA[d]
        nx = maze.wrap_x(self.grid_x + dx)
        ny = self.grid_y + dy
        if maze.can_enter(nx, ny, is_ghost=True):
          possible.append(d)
      if not possible:
        rev = opposite_dir(self.direction)
        if rev != DIR_NONE:
          possible = [rev]
      if possible:
        self.next_direction = random.choice(possible)
    else:
      # Use BFS to find path to pacman
      bfs_dir = maze.bfs_direction(
        self.grid_x, self.grid_y,
        pacman.grid_x, pacman.grid_y,
        is_ghost=True
      )
      if bfs_dir != DIR_NONE:
        self.next_direction = bfs_dir
      else:
        # Fallback: pick any valid direction
        for d in [DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT]:
          if d == opposite_dir(self.direction):
            continue
          dx, dy = DIR_DELTA[d]
          nx = maze.wrap_x(self.grid_x + dx)
          ny = self.grid_y + dy
          if maze.can_enter(nx, ny, is_ghost=True):
            self.next_direction = d
            break

  def update_movement(self, dt, maze, pacman):
    """Update ghost movement"""
    # Update animation
    current_speed = self.frightened_speed if self.mode == 'frightened' else self.speed
    if self.move_progress < 1.0:
      self.move_progress += current_speed * dt
      if self.move_progress >= 1.0:
        self.move_progress = 1.0

    # At cell center, choose and execute next move
    if self.is_at_cell():
      self.choose_direction(maze, pacman)
      if self.next_direction != DIR_NONE:
        self.try_move(maze, self.next_direction, is_ghost=True)

################################################################################
# Main Game
################################################################################

class PacManGame:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=True, disable_mouse_cursor=True)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.0, 0.0, 0.0, 1)

    canvas_layout = lg_group.makeChild(uiclass=lev2.ui.PrimCanvas, args=["pacman_canvas"])
    self.canvas = canvas_layout.widget

    root_layout = lg_group.layout
    canvas_layout.layout.top.anchorTo(root_layout.top)
    canvas_layout.layout.left.anchorTo(root_layout.left)
    canvas_layout.layout.bottom.anchorTo(root_layout.bottom)
    canvas_layout.layout.right.anchorTo(root_layout.right)

    self.canvas.bg_color = vec4(0.0, 0.0, 0.0, 1)
    self.canvas.draw_background = True

    # Game state
    self.maze = Maze()
    self.time = 0.0
    self.score = 0
    self.lives = 3
    self.game_over = False
    self.you_win = False

    # Pacman
    px, py = self.maze.pacman_start
    self.pacman = Entity(px, py)
    self.pacman.speed = 6.0
    self.pacman_anim = 0.0

    # Ghosts - start in ghost house area
    self.ghosts = []
    ghost_colors = [
      [255, 0, 0],      # Red
      [255, 184, 255],  # Pink
      [0, 255, 255],    # Cyan
      [255, 184, 82],   # Orange
    ]
    self.ghost_colors = ghost_colors
    ghost_start_positions = [(12, 14), (13, 14), (14, 14), (15, 14)]
    for i, (gx, gy) in enumerate(ghost_start_positions):
      self.ghosts.append(Ghost(gx, gy, i))

    self.frightened_time = 0.0

    # Input
    self.input_direction = DIR_NONE

    # Textures
    self.textures = {}
    self.sprite_size = 32

    # Primitives
    self.wall_prim = None
    self.wall_quads = []
    self.dot_prim = None
    self.dot_quads = []
    self.power_prim = None
    self.power_quads = []
    self.pacman_prim = None
    self.pacman_quad = None
    self.ghost_prims = []
    self.score_prim = None
    self.gameover_prim = None

    def onCtrlC(signum, frame):
      self.ezapp.signalExit()
    signal.signal(signal.SIGINT, onCtrlC)

  def _createTexture(self, ctx, np_img, name):
    """Create texture from numpy array"""
    txi = ctx.TXI
    np_img = np.flipud(np_img).copy()
    h, w = np_img.shape[:2]
    rgba = np.zeros((h, w, 4), dtype=np.uint8)
    rgba[:, :, :3] = np_img
    rgba[:, :, 3] = np.where(np.any(np_img > 0, axis=2), 255, 0)
    img = lev2.Image.createFromBuffer(w, h, tokens.RGBA8, rgba)
    tex = lev2.Texture(name)
    txi.updateTexture(tex, img)
    return tex

  def onGpuInit(self, ctx):
    self.canvas.gpuInit(ctx)
    self.font = lev2.FontManager.fontForId("i18")
    self.font_large = lev2.FontManager.fontForId("i32")

    # Enable alpha blending
    self.canvas.pipelineTextured.rasterstate.setBlendingMacro(tokens.ALPHA)

    ss = self.sprite_size

    # Pacman textures (4 directions x 12 mouth states)
    for d in range(4):
      for m, angle in enumerate([5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60]):
        key = f"pacman_{d}_{m}"
        self.textures[key] = self._createTexture(ctx, create_pacman_texture(ss, angle, d), key)

    # Ghost textures
    for i, color in enumerate(self.ghost_colors):
      self.textures[f"ghost_{i}"] = self._createTexture(ctx, create_ghost_texture(ss, color), f"ghost_{i}")
    self.textures["ghost_frightened"] = self._createTexture(ctx, create_ghost_texture(ss, [0, 0, 200]), "ghost_frightened")

    # Other textures
    self.textures["dot"] = self._createTexture(ctx, create_dot_texture(ss), "dot")
    self.textures["power"] = self._createTexture(ctx, create_power_pellet_texture(ss), "power")
    self.textures["wall"] = self._createTexture(ctx, create_wall_texture(ss), "wall")

    # Create wall quads
    wall_count = sum(1 for row in self.maze.grid for cell in row if cell == '#')
    self.wall_prim = lev2.ui.QuadPrimitive(
      pipeline=self.canvas.pipelineTextured,
      texture=self.textures["wall"]
    )
    for _ in range(wall_count):
      qd = lev2.ui.QuadData()
      self.wall_quads.append(qd)
      self.wall_prim.addQuad(qd)
    self.canvas.addPrimitive(self.wall_prim)

    # Create dot quads
    max_dots = len(self.maze.dots)
    self.dot_prim = lev2.ui.QuadPrimitive(
      pipeline=self.canvas.pipelineTextured,
      texture=self.textures["dot"]
    )
    for _ in range(max_dots):
      qd = lev2.ui.QuadData()
      self.dot_quads.append(qd)
      self.dot_prim.addQuad(qd)
    self.canvas.addPrimitive(self.dot_prim)

    # Create power pellet quads
    max_power = len(self.maze.power_pellets)
    self.power_prim = lev2.ui.QuadPrimitive(
      pipeline=self.canvas.pipelineTextured,
      texture=self.textures["power"]
    )
    for _ in range(max_power):
      qd = lev2.ui.QuadData()
      self.power_quads.append(qd)
      self.power_prim.addQuad(qd)
    self.canvas.addPrimitive(self.power_prim)

    # Create pacman primitives - one for each direction/mouth combo
    # We'll position the active one on-screen, others off-screen
    self.pacman_prims = {}
    self.pacman_quads = {}
    for d in range(4):
      for m in range(12):
        key = f"pacman_{d}_{m}"
        prim = lev2.ui.QuadPrimitive(
          pipeline=self.canvas.pipelineTextured,
          texture=self.textures[key]
        )
        qd = lev2.ui.QuadData()
        prim.addQuad(qd)
        self.pacman_prims[key] = prim
        self.pacman_quads[key] = qd
        self.canvas.addPrimitive(prim)

    # Create ghost quads
    for i in range(4):
      prim = lev2.ui.QuadPrimitive(
        pipeline=self.canvas.pipelineTextured,
        texture=self.textures[f"ghost_{i}"]
      )
      qd = lev2.ui.QuadData()
      prim.addQuad(qd)
      self.ghost_prims.append((prim, qd))
      self.canvas.addPrimitive(prim)

    # Text
    self.score_prim = lev2.ui.TextPrimitive(font=self.font, color=vec4(1, 1, 1, 1))
    self.score_prim.addItem("SCORE: 0", vec2(10, 10))
    self.canvas.addPrimitive(self.score_prim)

    self.gameover_prim = lev2.ui.TextPrimitive(font=self.font_large, color=vec4(1, 1, 0, 1))
    self.canvas.addPrimitive(self.gameover_prim)

    self._render()

  def _update(self, dt):
    if self.game_over:
      self._render()
      return

    # Update frightened timer
    if self.frightened_time > 0:
      self.frightened_time -= dt
      if self.frightened_time <= 0:
        for ghost in self.ghosts:
          ghost.mode = 'chase'

    # Pacman animation (fast chomping)
    self.pacman_anim += dt * 80

    # Update pacman movement
    self.pacman.update(dt)

    # At cell center: collect dots FIRST, then move (try_move resets move_progress)
    if self.pacman.is_at_cell():
      # Collect dots at current position
      px, py = self.pacman.grid_x, self.pacman.grid_y
      was_power = (px, py) in self.maze.power_pellets
      points = self.maze.collect_dot(px, py)
      self.score += points
      if was_power and points > 0:
        self.frightened_time = 8.0
        for ghost in self.ghosts:
          ghost.mode = 'frightened'

      # Try queued direction first
      if self.input_direction != DIR_NONE:
        if self.pacman.try_move(self.maze, self.input_direction):
          pass  # Successfully changed direction
        elif self.pacman.direction != DIR_NONE:
          # Try to continue in current direction
          self.pacman.try_move(self.maze, self.pacman.direction)
      elif self.pacman.direction != DIR_NONE:
        # Continue in current direction
        self.pacman.try_move(self.maze, self.pacman.direction)

    # Update ghosts
    for ghost in self.ghosts:
      ghost.update_movement(dt, self.maze, self.pacman)

    # Check ghost collision
    pac_rx, pac_ry = self.pacman.get_render_pos()
    for ghost in self.ghosts:
      gx, gy = ghost.get_render_pos()
      dist = (gx - pac_rx)**2 + (gy - pac_ry)**2
      if dist < 0.8:
        if ghost.mode == 'frightened':
          # Eat ghost
          ghost.grid_x, ghost.grid_y = 13, 14
          ghost.prev_x, ghost.prev_y = 13, 14
          ghost.move_progress = 1.0
          ghost.mode = 'chase'
          self.score += 200
        else:
          # Die
          self.lives -= 1
          if self.lives <= 0:
            self.game_over = True
          else:
            self._reset_positions()

    # Check win
    if len(self.maze.dots) == 0 and len(self.maze.power_pellets) == 0:
      self.game_over = True
      self.you_win = True

    self._render()

  def _reset_positions(self):
    """Reset pacman and ghosts after death"""
    px, py = self.maze.pacman_start
    self.pacman.grid_x = px
    self.pacman.grid_y = py
    self.pacman.prev_x = px
    self.pacman.prev_y = py
    self.pacman.move_progress = 1.0
    self.pacman.direction = DIR_NONE

    ghost_positions = [(12, 14), (13, 14), (14, 14), (15, 14)]
    for i, ghost in enumerate(self.ghosts):
      gx, gy = ghost_positions[i]
      ghost.grid_x = gx
      ghost.grid_y = gy
      ghost.prev_x = gx
      ghost.prev_y = gy
      ghost.move_progress = 1.0
      ghost.direction = DIR_NONE
      ghost.mode = 'scatter'

  def _render(self):
    canvas_w = self.canvas.width
    canvas_h = self.canvas.height
    if canvas_w < 1 or canvas_h < 1:
      return

    # Calculate cell size
    cell_w = canvas_w / self.maze.width
    cell_h = canvas_h / self.maze.height
    cell_size = min(cell_w, cell_h)

    offset_x = (canvas_w - self.maze.width * cell_size) / 2
    offset_y = (canvas_h - self.maze.height * cell_size) / 2

    # Render walls
    quad_idx = 0
    for y in range(self.maze.height):
      for x in range(self.maze.width):
        if self.maze.grid[y][x] == '#':
          qd = self.wall_quads[quad_idx]
          sx = offset_x + x * cell_size
          sy = offset_y + y * cell_size
          qd.setPosition(sx, canvas_h - sy - cell_size)
          qd.setSize(cell_size, cell_size)
          qd.setUV(0, 0, 1, 1)
          qd.setColor(vec4(1, 1, 1, 1))
          quad_idx += 1

    # Render dots
    quad_idx = 0
    for (x, y) in list(self.maze.dots):
      if quad_idx < len(self.dot_quads):
        qd = self.dot_quads[quad_idx]
        sx = offset_x + x * cell_size
        sy = offset_y + y * cell_size
        qd.setPosition(sx, canvas_h - sy - cell_size)
        qd.setSize(cell_size, cell_size)
        qd.setUV(0, 0, 1, 1)
        qd.setColor(vec4(1, 1, 1, 1))
        quad_idx += 1
    while quad_idx < len(self.dot_quads):
      self.dot_quads[quad_idx].setPosition(-100, -100)
      self.dot_quads[quad_idx].setSize(0, 0)
      quad_idx += 1

    # Render power pellets
    quad_idx = 0
    for (x, y) in list(self.maze.power_pellets):
      if quad_idx < len(self.power_quads):
        qd = self.power_quads[quad_idx]
        sx = offset_x + x * cell_size
        sy = offset_y + y * cell_size
        pulse = 0.6 + 0.4 * math.sin(self.time * 6)
        qd.setPosition(sx, canvas_h - sy - cell_size)
        qd.setSize(cell_size, cell_size)
        qd.setUV(0, 0, 1, 1)
        qd.setColor(vec4(pulse, pulse, pulse, 1))
        quad_idx += 1
    while quad_idx < len(self.power_quads):
      self.power_quads[quad_idx].setPosition(-100, -100)
      self.power_quads[quad_idx].setSize(0, 0)
      quad_idx += 1

    # Render pacman with animation and direction
    pac_rx, pac_ry = self.pacman.get_render_pos()
    sx = offset_x + pac_rx * cell_size
    sy = offset_y + pac_ry * cell_size

    # Select pacman texture based on direction and animation frame
    pac_dir = self.pacman.direction if self.pacman.direction != DIR_NONE else DIR_RIGHT
    # Mouth animation: cycle through 0-11-0 for smooth open/close (22 frames total)
    anim_cycle = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1]
    mouth_frame = anim_cycle[int(self.pacman_anim) % 22]
    active_key = f"pacman_{pac_dir}_{mouth_frame}"

    # Position active pacman on-screen, all others off-screen
    for key, qd in self.pacman_quads.items():
      if key == active_key:
        qd.setPosition(sx, canvas_h - sy - cell_size)
        qd.setSize(cell_size, cell_size)
        qd.setUV(0, 0, 1, 1)
        qd.setColor(vec4(1, 1, 1, 1))
      else:
        qd.setPosition(-100, -100)
        qd.setSize(0, 0)

    # Render ghosts
    for i, ghost in enumerate(self.ghosts):
      prim, qd = self.ghost_prims[i]
      gx, gy = ghost.get_render_pos()
      sx = offset_x + gx * cell_size
      sy = offset_y + gy * cell_size
      qd.setPosition(sx, canvas_h - sy - cell_size)
      qd.setSize(cell_size, cell_size)
      qd.setUV(0, 0, 1, 1)

      if ghost.mode == 'frightened':
        if self.frightened_time < 2 and int(self.time * 5) % 2 == 0:
          qd.setColor(vec4(1, 1, 1, 1))
        else:
          qd.setColor(vec4(0.3, 0.3, 1, 1))
      else:
        qd.setColor(vec4(1, 1, 1, 1))

    # Score
    self.score_prim.clearItems()
    self.score_prim.addItem(f"SCORE: {self.score}  LIVES: {self.lives}", vec2(10, 10))

    # Game over
    self.gameover_prim.clearItems()
    if self.game_over:
      if self.you_win:
        self.gameover_prim.addItem("YOU WIN!", vec2(canvas_w/2 - 80, canvas_h/2))
      else:
        self.gameover_prim.addItem("GAME OVER", vec2(canvas_w/2 - 100, canvas_h/2))
      self.gameover_prim.addItem("PRESS SPACE TO RESTART", vec2(canvas_w/2 - 180, canvas_h/2 + 50))

    self.canvas.markDirty()

  def _restart(self):
    self.maze = Maze()
    self.score = 0
    self.lives = 3
    self.game_over = False
    self.you_win = False
    self.frightened_time = 0

    px, py = self.maze.pacman_start
    self.pacman = Entity(px, py)
    self.pacman.speed = 6.0

    self.ghosts = []
    ghost_positions = [(12, 14), (13, 14), (14, 14), (15, 14)]
    for i, (gx, gy) in enumerate(ghost_positions):
      self.ghosts.append(Ghost(gx, gy, i))

    self.input_direction = DIR_NONE

  def onUpdate(self, updinfo):
    self.time += updinfo.deltatime
    self._update(updinfo.deltatime)

  def onUiEvent(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed:
      if uievent.keycode == 263:  # Left
        self.input_direction = DIR_LEFT
      elif uievent.keycode == 262:  # Right
        self.input_direction = DIR_RIGHT
      elif uievent.keycode == 265:  # Up
        self.input_direction = DIR_UP
      elif uievent.keycode == 264:  # Down
        self.input_direction = DIR_DOWN
      elif uievent.keycode == 32:  # Space
        if self.game_over:
          self._restart()
      return lev2.ui.HandlerResult()
    return lev2.ui.HandlerResult()

###############################################################################

PacManGame().ezapp.mainThreadLoop()
