#!/usr/bin/env ork.python
################################################################################
# PrimCanvas Space Invaders - Pixel art sprites with keyboard input
################################################################################

import random
from orkengine.core import vec2, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.frame_profiler import FrameProfilerComponent
from ork.app.testlib.gameutils import parse_ascii_sprite

tokens = CrcStringProxy()

################################################################################
# Sprites - Galaxian style with 4 colors per invader
################################################################################

# Color palette: R=Red, B=Blue, Y=Yellow, W=White
COLORS = {
  'R': vec4(1.0, 0.2, 0.2, 1.0),  # Red
  'B': vec4(0.2, 0.4, 1.0, 1.0),  # Blue
  'Y': vec4(1.0, 1.0, 0.2, 1.0),  # Yellow
  'W': vec4(1.0, 1.0, 1.0, 1.0),  # White
  'G': vec4(0.2, 0.9, 0.3, 1.0),  # Green (player)
}

SPRITES = {
  'inv1a': """
  YY
 RRRR
RRRRRR
BW BW B
BBBBBB
 B  B
Y    Y
""",
  'inv1b': """
  YY
 RRRR
RRRRRR
BW BW B
BBBBBBB
B BB B
 Y  Y
""",
  'inv2a': """
 Y   Y
  R R
 RRRRR
BB R BB
BBBBBBB
B WWW B
B     B
  Y Y
""",
  'inv2b': """
 Y   Y
R R R R
 RRRRR
BB R BB
BBBBBBB
  WWW
 B   B
Y     Y
""",
  'inv3a': """
   RR
  RRRR
 YYYYYY
BB YY BB
BBBBBBBB
  W  W
 B    B
Y  BB  Y
""",
  'inv3b': """
   RR
  RRRR
 YYYYYY
BB YY BB
BBBBBBBB
 B YY B
Y      Y
 B    B
""",
  'player': """
    G
   GGG
   GGG
GGGGGGGGG
WWWWWWWWW
""",
}

################################################################################
# Game
################################################################################

class SpaceInvaders(ComponentizedApplication):
  # Grid config
  COLS, ROWS = 11, 5
  SPACING = (0.07, 0.075)
  START_POS = (0.12, 0.12)

  def __init__(self):
    super().__init__()

    self.profiler = self.addComponent("profiler", FrameProfilerComponent,
                                      gpu_filter=["*", "-fwd:total"])

    self.createEzApp(fullscreen=True, disable_mouse_cursor=True)

    lg = self.ezapp.topLayoutGroup
    lg.clearColorStd = vec4(0, 0, 0.02, 1)
    cl = lg.makeChild(uiclass=lev2.ui.PrimCanvas, args=["invaders"])
    self.canvas = cl.widget
    for edge in ['top', 'left', 'bottom', 'right']:
      getattr(cl.layout, edge).anchorTo(getattr(lg.layout, edge))
    self.canvas.bg_color = vec4(0, 0, 0.02, 1)
    self.canvas.draw_background = True

    # Parse sprites with color map
    self.sprite_frames = [
      (parse_ascii_sprite(SPRITES['inv1a'], color_map=COLORS),
       parse_ascii_sprite(SPRITES['inv1b'], color_map=COLORS)),
      (parse_ascii_sprite(SPRITES['inv2a'], color_map=COLORS),
       parse_ascii_sprite(SPRITES['inv2b'], color_map=COLORS)),
      (parse_ascii_sprite(SPRITES['inv3a'], color_map=COLORS),
       parse_ascii_sprite(SPRITES['inv3b'], color_map=COLORS)),
    ]
    self.player_sprite = parse_ascii_sprite(SPRITES['player'], color_map=COLORS)

    self._init_game()

  def _init_game(self):
    self.time = self.anim_time = self.score = 0
    self.anim_frame = 0
    self.game_over = self.you_win = False
    self.player_x, self.player_y = 0.5, 0.92
    self.move_left = self.move_right = False
    self.fire_cooldown = self.enemy_fire_cooldown = 0.5
    self.invader_dir = 1

    # Create invaders
    self.invaders = []
    for row in range(self.ROWS):
      for col in range(self.COLS):
        self.invaders.append({
          'x': self.START_POS[0] + col * self.SPACING[0],
          'y': self.START_POS[1] + row * self.SPACING[1],
          'alive': True, 'row': row, 'type': row % 3
        })

    # Projectiles
    self.bullets = [{'x': 0, 'y': 0, 'active': False} for _ in range(3)]
    self.missiles = [{'x': 0, 'y': 0, 'active': False} for _ in range(5)]

  def _make_quad_prim(self, layer, pipeline, count):
    prim = lev2.ui.QuadPrimitive(pipeline=pipeline)
    quads = [lev2.ui.QuadData() for _ in range(count)]
    for q in quads: prim.addQuad(q)
    layer.addPrimitive(prim)
    return quads

  def _onGpuInit(self, ctx):
    self.canvas.gpuInit(ctx)
    self.font = lev2.FontManager.fontForId("i18")
    self.font_large = lev2.FontManager.fontForId("i32")

    # Create layers
    self.game_layer = self.canvas.createLayer("game")
    self.ui_layer = self.canvas.createLayer("ui")

    # Calculate max pixels needed
    max_inv_px = max(max(len(f[0][0]), len(f[1][0])) for f in self.sprite_frames)
    total_px = self.COLS * self.ROWS * max_inv_px + len(self.player_sprite[0])

    self.pixel_quads = self._make_quad_prim(self.game_layer, self.canvas.pipelineSolid, total_px)
    self.bullet_quads = self._make_quad_prim(self.game_layer, self.canvas.pipelineSolid, 3)
    self.missile_quads = self._make_quad_prim(self.game_layer, self.canvas.pipelineSolid, 5)

    self.score_prim = lev2.ui.TextPrimitive(font=self.font, color=vec4(0.2, 1, 0.2, 1))
    self.ui_layer.addPrimitive(self.score_prim)
    self.msg_prim = lev2.ui.TextPrimitive(font=self.font_large, color=vec4(1, 0.2, 0.2, 1))
    self.ui_layer.addPrimitive(self.msg_prim)
    self._render()

  def _fire(self):
    if self.game_over or self.fire_cooldown > 0: return
    for b in self.bullets:
      if not b['active']:
        b['x'], b['y'], b['active'] = self.player_x, self.player_y - 0.03, True
        self.fire_cooldown = 0.2
        break

  def _update(self, dt):
    if self.game_over: return self._render()

    # Animation & invader movement (synced)
    self.anim_time += dt
    if self.anim_time > 0.5:
      self.anim_time = 0
      self.anim_frame = 1 - self.anim_frame

      # Check edge collision
      edge_hit = any(i['alive'] and ((self.invader_dir > 0 and i['x'] > 0.92) or
                     (self.invader_dir < 0 and i['x'] < 0.08)) for i in self.invaders)
      if edge_hit:
        self.invader_dir *= -1
        for i in self.invaders:
          i['y'] += 0.035
          if i['alive'] and i['y'] > self.player_y - 0.08:
            self.game_over = True
      else:
        for i in self.invaders:
          i['x'] += self.invader_dir * 0.0144

    # Cooldowns
    self.fire_cooldown = max(0, self.fire_cooldown - dt)
    self.enemy_fire_cooldown = max(0, self.enemy_fire_cooldown - dt)

    # Player movement
    if self.move_left: self.player_x -= 0.3 * dt
    if self.move_right: self.player_x += 0.3 * dt
    self.player_x = max(0.05, min(0.95, self.player_x))

    # Bullet movement & collision
    for b in self.bullets:
      if b['active']:
        b['y'] -= 0.8 * dt
        if b['y'] < 0: b['active'] = False
        else:
          for inv in self.invaders:
            if inv['alive'] and abs(b['x'] - inv['x']) < 0.025 and abs(b['y'] - inv['y']) < 0.025:
              inv['alive'] = b['active'] = False
              self.score += 10 * (self.ROWS - inv['row'])
              break

    # Enemy firing
    if self.enemy_fire_cooldown <= 0:
      bottom = {}
      for inv in self.invaders:
        if inv['alive']:
          col = int((inv['x'] - self.START_POS[0] + 0.01) / self.SPACING[0])
          if col not in bottom or inv['y'] > bottom[col]['y']:
            bottom[col] = inv
      best = min((abs(i['x'] - self.player_x), i) for i in bottom.values())[1] if bottom else None
      if best and abs(best['x'] - self.player_x) < 0.05:
        for m in self.missiles:
          if not m['active']:
            m['x'], m['y'], m['active'] = best['x'], best['y'] + 0.03, True
            self.enemy_fire_cooldown = 1.2 + random.uniform(-0.3, 0.3)
            break

    # Missile movement & collision
    for m in self.missiles:
      if m['active']:
        m['y'] += 0.4 * dt
        if m['y'] > 1.0: m['active'] = False
        elif abs(m['x'] - self.player_x) < 0.03 and abs(m['y'] - self.player_y) < 0.03:
          m['active'] = False
          self.game_over = True

    # Win check
    if all(not i['alive'] for i in self.invaders):
      self.game_over = self.you_win = True

    self._render()

  def _set_quad(self, qd, x, y, w, h, color):
    qd.setPosition(x, y)
    qd.setSize(w, h)
    qd.setColor(color)

  def _hide_quad(self, qd):
    qd.setPosition(-100, -100)
    qd.setSize(0, 0)

  def _render(self):
    w, h = self.canvas.width, self.canvas.height
    if w < 1: return

    scale = min(w / 800, h / 600)
    ps = max(2, int(4 * scale))
    bw, bh = max(3, int(5 * scale)), max(10, int(18 * scale))

    qi = 0
    # Invaders
    for inv in self.invaders:
      sprite = self.sprite_frames[inv['type']][self.anim_frame]
      pixels, _, _ = sprite
      max_px = max(len(self.sprite_frames[inv['type']][0][0]),
                   len(self.sprite_frames[inv['type']][1][0]))
      if inv['alive']:
        sx, sy = inv['x'] * w, inv['y'] * h
        for px, py, color in pixels:
          self._set_quad(self.pixel_quads[qi], sx + px * ps, h - sy - py * ps, ps, ps, color)
          qi += 1
        for _ in range(max_px - len(pixels)):
          self._hide_quad(self.pixel_quads[qi])
          qi += 1
      else:
        for _ in range(max_px):
          self._hide_quad(self.pixel_quads[qi])
          qi += 1

    # Player
    pixels, _, _ = self.player_sprite
    sx, sy = self.player_x * w, self.player_y * h
    for px, py, color in pixels:
      if qi < len(self.pixel_quads):
        self._set_quad(self.pixel_quads[qi], sx + px * ps, h - sy - py * ps, ps, ps, color)
        qi += 1

    while qi < len(self.pixel_quads):
      self._hide_quad(self.pixel_quads[qi])
      qi += 1

    # Bullets & missiles
    for i, b in enumerate(self.bullets):
      if b['active']:
        self._set_quad(self.bullet_quads[i], b['x'] * w - bw / 2, h - b['y'] * h, bw, bh, vec4(1, 1, 1, 1))
      else:
        self._hide_quad(self.bullet_quads[i])

    for i, m in enumerate(self.missiles):
      if m['active']:
        self._set_quad(self.missile_quads[i], m['x'] * w - bw / 2, h - m['y'] * h, bw, bh, vec4(1, 0.3, 0.1, 1))
      else:
        self._hide_quad(self.missile_quads[i])

    # UI
    self.score_prim.clearItems()
    self.score_prim.addItem(f"SCORE: {self.score}", vec2(10, 10))
    self.msg_prim.clearItems()
    if self.game_over:
      msg = "YOU WIN!" if self.you_win else "GAME OVER"
      self.msg_prim.addItem(msg, vec2(w / 2 - 80, h / 2))
      self.msg_prim.addItem("PRESS SPACE TO RESTART", vec2(w / 2 - 180, h / 2 + 50))

    self.canvas.markDirty()

  def _onUpdate(self, updinfo):
    self.time += updinfo.deltatime
    self._update(updinfo.deltatime)

  def _onUiEvent(self, ev):
    if ev.code == tokens.KEY_DOWN.hashed:
      if ev.keycode == 263: self.move_left = True
      elif ev.keycode == 262: self.move_right = True
      elif ev.keycode == 32:
        if self.game_over: self._init_game()
        else: self._fire()
    elif ev.code == tokens.KEY_UP.hashed:
      if ev.keycode == 263: self.move_left = False
      elif ev.keycode == 262: self.move_right = False
    return lev2.ui.HandlerResult()

###############################################################################

app = SpaceInvaders()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
