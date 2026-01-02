#!/usr/bin/env ork.python
################################################################################
# PrimCanvas Pac-Man - Grid-based movement using gameutils library
################################################################################

import signal, math, random
import numpy as np
from orkengine.core import vec2, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.testlib.gameutils.grid2d import (
  DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP, DIR_NONE, DIR_DELTA,
  opposite_dir, GridMaze, GridEntity
)
from ork.app.testlib.gameutils import create_texture_from_numpy

tokens = CrcStringProxy()

################################################################################
# Constants
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

GHOST_START_POS = [(12, 14), (13, 14), (14, 14), (15, 14)]
GHOST_COLORS = [[255, 0, 0], [255, 184, 255], [0, 255, 255], [255, 184, 82]]
PACMAN_START = (13, 23)
MOUTH_ANGLES = [5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60]
ANIM_CYCLE = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1]

################################################################################
# PacMan Maze
################################################################################

class PacManMaze(GridMaze):
  def __init__(self):
    super().__init__(len(MAZE_DATA[0]), len(MAZE_DATA))
    self.wrap_horizontal = True
    self.set_blocked_cells({'#'})
    self.set_conditional_cell('-', {'ghost'})
    self.dots, self.power_pellets = set(), set()

    for y, row in enumerate(MAZE_DATA):
      for x, cell in enumerate(row):
        if cell == '.':
          self.dots.add((x, y))
          cell = ' '
        elif cell == 'o':
          self.power_pellets.add((x, y))
          cell = ' '
        self.set_cell(x, y, cell if cell in '#-' else ' ')

  def collect_dot(self, x, y):
    if (x, y) in self.dots:
      self.dots.remove((x, y))
      return 10
    if (x, y) in self.power_pellets:
      self.power_pellets.remove((x, y))
      return 50
    return 0

################################################################################
# Sprite Generation
################################################################################

def create_circle(size, color, radius_frac=0.5):
  img = np.zeros((size, size, 3), dtype=np.uint8)
  center, radius = size // 2, int(size * radius_frac) - 2
  for y in range(size):
    for x in range(size):
      if (x - center)**2 + (y - center)**2 <= radius**2:
        img[y, x] = color
  return img

def create_pacman_sprite(size, mouth_angle, direction=0):
  img = np.zeros((size, size, 3), dtype=np.uint8)
  center, radius = size // 2, size // 2 - 2
  for y in range(size):
    for x in range(size):
      dx, dy = x - center, y - center
      if dx*dx + dy*dy <= radius*radius:
        angle = (math.degrees(math.atan2(dy, dx)) - direction * 90 + 180) % 360 - 180
        if abs(angle) > mouth_angle:
          img[y, x] = [255, 255, 0]
  return img

def create_ghost_sprite(size, color):
  img = np.zeros((size, size, 3), dtype=np.uint8)
  center, radius = size // 2, size // 2 - 2
  for y in range(size):
    for x in range(size):
      dx, dy = x - center, y - center
      dist = math.sqrt(dx*dx + dy*dy)
      # Body
      if (y <= center and dist <= radius) or (y > center and abs(dx) <= radius and y < size - 2 + int(2 * math.sin(x * math.pi / 4))):
        img[y, x] = color
      # Eyes
      for ex in [center - 4, center + 4]:
        ed = math.sqrt((x - ex)**2 + (y - center + 2)**2)
        if ed <= 3: img[y, x] = [255, 255, 255]
        if ed <= 1.5: img[y, x] = [0, 0, 255]
  return img

def create_wall_sprite(size):
  img = np.zeros((size, size, 3), dtype=np.uint8)
  img[:, :] = [0, 0, 180]
  img[:2, :] = img[-2:, :] = img[:, :2] = img[:, -2:] = [0, 0, 100]
  return img

################################################################################
# Ghost AI
################################################################################

class Ghost(GridEntity):
  def __init__(self, x, y, color_idx):
    super().__init__(x, y, entity_type='ghost')
    self.color_idx = color_idx
    self.mode = 'scatter'
    self.speed = 4.0

  def update_movement(self, dt, maze, pacman):
    speed = 2.5 if self.mode == 'frightened' else self.speed
    if self.move_progress < 1.0:
      self.move_progress = min(1.0, self.move_progress + speed * dt)

    if self.is_at_cell():
      if self.mode == 'frightened':
        # Random valid direction (not reverse)
        dirs = [d for d in [DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT]
                if d != opposite_dir(self.direction) and
                maze.can_enter(maze.wrap_x(self.grid_x + DIR_DELTA[d][0]),
                               maze.wrap_y(self.grid_y + DIR_DELTA[d][1]), 'ghost')]
        self.next_direction = random.choice(dirs) if dirs else opposite_dir(self.direction)
      else:
        self.next_direction = maze.bfs_direction(self.grid_x, self.grid_y,
                                                  pacman.grid_x, pacman.grid_y, 'ghost')
      if self.next_direction != DIR_NONE:
        self.try_move(maze, self.next_direction)

################################################################################
# Main Game
################################################################################

class PacManGame:
  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=True, disable_mouse_cursor=True)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg = self.ezapp.topLayoutGroup
    lg.clearColorStd = vec4(0, 0, 0, 1)
    cl = lg.makeChild(uiclass=lev2.ui.PrimCanvas, args=["pacman"])
    self.canvas = cl.widget
    for edge in ['top', 'left', 'bottom', 'right']:
      getattr(cl.layout, edge).anchorTo(getattr(lg.layout, edge))
    self.canvas.bg_color = vec4(0, 0, 0, 1)
    self.canvas.draw_background = True

    self._init_game()
    signal.signal(signal.SIGINT, lambda *_: self.ezapp.signalExit())

  def _init_game(self):
    self.maze = PacManMaze()
    self.time = self.score = self.pacman_anim = 0.0
    self.lives, self.frightened_time = 3, 0.0
    self.game_over = self.you_win = False
    self.pacman = GridEntity(*PACMAN_START, entity_type='pacman')
    self.pacman.speed = 6.0
    self.ghosts = [Ghost(x, y, i) for i, (x, y) in enumerate(GHOST_START_POS)]
    self.input_direction = DIR_NONE

  def _make_quad_prim(self, tex, count):
    prim = lev2.ui.QuadPrimitive(pipeline=self.canvas.pipelineTextured, texture=tex)
    quads = [lev2.ui.QuadData() for _ in range(count)]
    for q in quads: prim.addQuad(q)
    self.canvas.addPrimitive(prim)
    return prim, quads

  def onGpuInit(self, ctx):
    self.canvas.gpuInit(ctx)
    self.font = lev2.FontManager.fontForId("i18")
    self.font_large = lev2.FontManager.fontForId("i32")
    self.canvas.pipelineTextured.rasterstate.setBlendingMacro(tokens.ALPHA)

    ss = 32
    tex = lambda img, name: create_texture_from_numpy(ctx, img, name)

    # Textures
    self.tex = {
      'wall': tex(create_wall_sprite(ss), 'wall'),
      'dot': tex(create_circle(ss, [255, 200, 150], 0.17), 'dot'),
      'power': tex(create_circle(ss, [255, 200, 150], 0.33), 'power'),
    }
    for i, c in enumerate(GHOST_COLORS):
      self.tex[f'ghost_{i}'] = tex(create_ghost_sprite(ss, c), f'ghost_{i}')
    self.tex['ghost_scared'] = tex(create_ghost_sprite(ss, [0, 0, 200]), 'ghost_scared')
    for d in range(4):
      for m, a in enumerate(MOUTH_ANGLES):
        self.tex[f'pac_{d}_{m}'] = tex(create_pacman_sprite(ss, a, d), f'pac_{d}_{m}')

    # Primitives
    wall_count = sum(row.count('#') for row in self.maze.grid)
    self.wall_prim, self.wall_quads = self._make_quad_prim(self.tex['wall'], wall_count)
    self.dot_prim, self.dot_quads = self._make_quad_prim(self.tex['dot'], len(self.maze.dots))
    self.power_prim, self.power_quads = self._make_quad_prim(self.tex['power'], len(self.maze.power_pellets))

    self.pac_prims, self.pac_quads = {}, {}
    for d in range(4):
      for m in range(12):
        k = f'pac_{d}_{m}'
        self.pac_prims[k], q = self._make_quad_prim(self.tex[k], 1)
        self.pac_quads[k] = q[0]

    self.ghost_prims = []
    for i in range(4):
      _, q = self._make_quad_prim(self.tex[f'ghost_{i}'], 1)
      self.ghost_prims.append(q[0])

    self.score_prim = lev2.ui.TextPrimitive(font=self.font, color=vec4(1, 1, 1, 1))
    self.canvas.addPrimitive(self.score_prim)
    self.msg_prim = lev2.ui.TextPrimitive(font=self.font_large, color=vec4(1, 1, 0, 1))
    self.canvas.addPrimitive(self.msg_prim)
    self._render()

  def _update(self, dt):
    if self.game_over:
      return self._render()

    if self.frightened_time > 0:
      self.frightened_time -= dt
      if self.frightened_time <= 0:
        for g in self.ghosts: g.mode = 'chase'

    self.pacman_anim += dt * 80
    self.pacman.update(dt)

    if self.pacman.is_at_cell():
      px, py = self.pacman.grid_x, self.pacman.grid_y
      was_power = (px, py) in self.maze.power_pellets
      self.score += self.maze.collect_dot(px, py)
      if was_power and self.score:
        self.frightened_time = 8.0
        for g in self.ghosts: g.mode = 'frightened'

      moved = self.input_direction != DIR_NONE and self.pacman.try_move(self.maze, self.input_direction)
      if not moved and self.pacman.direction != DIR_NONE:
        self.pacman.try_move(self.maze, self.pacman.direction)

    for g in self.ghosts:
      g.update_movement(dt, self.maze, self.pacman)

    # Collision check
    prx, pry = self.pacman.get_render_pos()
    for g in self.ghosts:
      gx, gy = g.get_render_pos()
      if (gx - prx)**2 + (gy - pry)**2 < 0.8:
        if g.mode == 'frightened':
          g.teleport(13, 14)
          g.mode = 'chase'
          self.score += 200
        else:
          self.lives -= 1
          if self.lives <= 0:
            self.game_over = True
          else:
            self.pacman.teleport(*PACMAN_START)
            for i, g2 in enumerate(self.ghosts):
              g2.teleport(*GHOST_START_POS[i])
              g2.mode = 'scatter'

    if not self.maze.dots and not self.maze.power_pellets:
      self.game_over = self.you_win = True
    self._render()

  def _set_quad(self, qd, x, y, size, color=None):
    qd.setPosition(x, y)
    qd.setSize(size, size)
    qd.setUV(0, 0, 1, 1)
    qd.setColor(color or vec4(1, 1, 1, 1))

  def _hide_quad(self, qd):
    qd.setPosition(-100, -100)
    qd.setSize(0, 0)

  def _render(self):
    w, h = self.canvas.width, self.canvas.height
    if w < 1 or h < 1: return

    cell = min(w / self.maze.width, h / self.maze.height)
    ox = (w - self.maze.width * cell) / 2
    oy = (h - self.maze.height * cell) / 2
    def screen_pos(gx, gy): return ox + gx * cell, h - (oy + gy * cell) - cell

    # Walls
    qi = 0
    for y in range(self.maze.height):
      for x in range(self.maze.width):
        if self.maze.grid[y][x] == '#':
          self._set_quad(self.wall_quads[qi], *screen_pos(x, y), cell)
          qi += 1

    # Dots & power pellets
    for i, (x, y) in enumerate(self.maze.dots):
      self._set_quad(self.dot_quads[i], *screen_pos(x, y), cell) if i < len(self.dot_quads) else None
    for i in range(len(self.maze.dots), len(self.dot_quads)):
      self._hide_quad(self.dot_quads[i])

    pulse = vec4(0.6 + 0.4 * math.sin(self.time * 6), 0.6 + 0.4 * math.sin(self.time * 6), 0.6 + 0.4 * math.sin(self.time * 6), 1)
    for i, (x, y) in enumerate(self.maze.power_pellets):
      self._set_quad(self.power_quads[i], *screen_pos(x, y), cell, pulse) if i < len(self.power_quads) else None
    for i in range(len(self.maze.power_pellets), len(self.power_quads)):
      self._hide_quad(self.power_quads[i])

    # Pacman
    d = self.pacman.direction if self.pacman.direction != DIR_NONE else DIR_RIGHT
    active = f'pac_{d}_{ANIM_CYCLE[int(self.pacman_anim) % 22]}'
    prx, pry = self.pacman.get_render_pos()
    for k, qd in self.pac_quads.items():
      if k == active:
        self._set_quad(qd, *screen_pos(prx, pry), cell)
      else:
        self._hide_quad(qd)

    # Ghosts
    for i, g in enumerate(self.ghosts):
      gx, gy = g.get_render_pos()
      color = vec4(0.3, 0.3, 1, 1) if g.mode == 'frightened' and not (self.frightened_time < 2 and int(self.time * 5) % 2) else vec4(1, 1, 1, 1)
      self._set_quad(self.ghost_prims[i], *screen_pos(gx, gy), cell, color)

    # UI
    self.score_prim.clearItems()
    self.score_prim.addItem(f"SCORE: {self.score}  LIVES: {self.lives}", vec2(10, 10))
    self.msg_prim.clearItems()
    if self.game_over:
      msg = "YOU WIN!" if self.you_win else "GAME OVER"
      self.msg_prim.addItem(msg, vec2(w/2 - 80, h/2))
      self.msg_prim.addItem("PRESS SPACE TO RESTART", vec2(w/2 - 180, h/2 + 50))

    self.canvas.markDirty()

  def onUpdate(self, updinfo):
    self.time += updinfo.deltatime
    self._update(updinfo.deltatime)

  def onUiEvent(self, ev):
    if ev.code == tokens.KEY_DOWN.hashed:
      dirs = {263: DIR_LEFT, 262: DIR_RIGHT, 265: DIR_UP, 264: DIR_DOWN}
      if ev.keycode in dirs:
        self.input_direction = dirs[ev.keycode]
      elif ev.keycode == 32 and self.game_over:
        self._init_game()
    return lev2.ui.HandlerResult()

PacManGame().ezapp.mainThreadLoop()
