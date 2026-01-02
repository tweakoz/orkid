#!/usr/bin/env ork.python

################################################################################
# PrimCanvas Composite Test - Space Invaders
# Uses quads for sprites, text for score, keyboard input
################################################################################

import signal
import math
import random
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################
# Pixel art sprites (2 animation frames each)
################################################################################

INVADER_TYPE1_A = """
  **
 ****
******
** **
******
 *  *
*    *
"""

INVADER_TYPE1_B = """
  **
 ****
******
** **
******
* ** *
 *  *
"""

INVADER_TYPE2_A = """
 *   *
  * *
 *****
** * **
*******
* *** *
*     *
  * *
"""

INVADER_TYPE2_B = """
 *   *
* * * *
 *****
** * **
*******
  ***
 *   *
*     *
"""

INVADER_TYPE3_A = """
   **
  ****
 ******
** ** **
********
  *  *
 *    *
*  **  *
"""

INVADER_TYPE3_B = """
   **
  ****
 ******
** ** **
********
 * ** *
*      *
 *    *
"""

PLAYER_SPRITE = """
    *
   ***
   ***
*********
*********
"""

def parse_sprite(sprite_str):
  """Parse a sprite string into a list of (col, row) pixel positions"""
  pixels = []
  # lstrip('\n') removes only leading newlines, preserving leading spaces on first row
  lines = sprite_str.lstrip('\n').rstrip().split('\n')
  height = len(lines)
  width = max(len(line) for line in lines) if lines else 0
  for row, line in enumerate(lines):
    for col, char in enumerate(line):
      if char == '*':
        pixels.append((col - width/2, row - height/2))
  return pixels, width, height

################################################################################

class SpaceInvaders:

  def __init__(self):
    super().__init__()

    self.ezapp = lev2.OrkEzApp.create(self, fullscreen=True, disable_mouse_cursor=True)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.clearColorStd = vec4(0.0, 0.0, 0.02, 1)

    # Create PrimCanvas widget
    canvas_layout = lg_group.makeChild(uiclass=lev2.ui.PrimCanvas, args=["game_canvas"])
    self.canvas = canvas_layout.widget

    # Anchor to fill parent
    root_layout = lg_group.layout
    canvas_layout.layout.top.anchorTo(root_layout.top)
    canvas_layout.layout.left.anchorTo(root_layout.left)
    canvas_layout.layout.bottom.anchorTo(root_layout.bottom)
    canvas_layout.layout.right.anchorTo(root_layout.right)

    self.canvas.bg_color = vec4(0.0, 0.0, 0.02, 1)
    self.canvas.draw_background = True
    self.canvas.onUiEvent = self._onCanvasEvent

    # Parse sprites
    self.sprite_types = [
      (parse_sprite(INVADER_TYPE1_A), parse_sprite(INVADER_TYPE1_B)),
      (parse_sprite(INVADER_TYPE2_A), parse_sprite(INVADER_TYPE2_B)),
      (parse_sprite(INVADER_TYPE3_A), parse_sprite(INVADER_TYPE3_B)),
    ]
    self.player_sprite = parse_sprite(PLAYER_SPRITE)

    # Game state
    self.time = 0.0
    self.anim_time = 0.0
    self.anim_frame = 0
    self.score = 0
    self.game_over = False
    self.you_win = False

    # All positions in normalized coordinates (0-1)
    # Player
    self.player_x = 0.5  # Center horizontally
    self.player_y = 0.92  # Near bottom
    self.player_speed = 0.3  # Fraction of screen per second

    # Input state
    self.move_left = False
    self.move_right = False

    # Invaders grid (normalized coordinates)
    self.invader_cols = 11
    self.invader_rows = 5
    self.invader_spacing_x = 0.07
    self.invader_spacing_y = 0.075
    self.invader_start_x = 0.12
    self.invader_start_y = 0.12
    self.invader_direction = 1  # 1 = right, -1 = left
    self.invader_step = 0.0144  # Fraction of screen per step (20% faster)
    self.invader_drop = 0.035
    self.invaders = []  # List of dicts

    # Player bullets (normalized)
    self.bullets = []
    self.bullet_speed = 0.8  # Fraction of screen per second
    self.max_bullets = 3
    self.fire_cooldown = 0

    # Enemy missiles (normalized)
    self.enemy_missiles = []
    self.enemy_missile_speed = 0.4  # Slower than player bullets
    self.max_enemy_missiles = 5
    self.enemy_fire_cooldown = 0
    self.enemy_fire_interval = 1.2  # Seconds between enemy shots

    # Primitives - will be created in onGpuInit
    self.pixel_prim = None
    self.pixel_quads = []
    self.bullet_prim = None
    self.bullet_quads = []
    self.enemy_missile_prim = None
    self.enemy_missile_quads = []
    self.score_prim = None
    self.gameover_prim = None

    # Signal handling
    def onCtrlC(signum, frame):
      print("signalling EXIT")
      self.ezapp.signalExit()

    signal.signal(signal.SIGINT, onCtrlC)

  def onGpuInit(self, ctx):
    self.canvas.gpuInit(ctx)
    self.font = lev2.FontManager.fontForId("i18")
    self.font_large = lev2.FontManager.fontForId("i32")

    # Initialize invaders (normalized coordinates 0-1)
    for row in range(self.invader_rows):
      for col in range(self.invader_cols):
        x = self.invader_start_x + col * self.invader_spacing_x
        y = self.invader_start_y + row * self.invader_spacing_y
        sprite_type = row % 3  # Cycle through sprite types
        self.invaders.append({'x': x, 'y': y, 'alive': True, 'row': row, 'type': sprite_type})

    # Calculate max pixels needed
    max_invader_pixels = max(
      max(len(self.sprite_types[i][0][0]), len(self.sprite_types[i][1][0]))
      for i in range(3)
    )
    player_pixels = len(self.player_sprite[0])
    total_pixels = (self.invader_cols * self.invader_rows * max_invader_pixels) + player_pixels

    # Single primitive for all pixel quads
    self.pixel_prim = lev2.ui.QuadPrimitive(pipeline=self.canvas.pipelineSolid)
    for i in range(total_pixels):
      qd = lev2.ui.QuadData()
      self.pixel_quads.append(qd)
      self.pixel_prim.addQuad(qd)
    self.canvas.addPrimitive(self.pixel_prim)

    # Player bullet primitive
    self.bullet_prim = lev2.ui.QuadPrimitive(pipeline=self.canvas.pipelineSolid)
    for i in range(self.max_bullets):
      qd = lev2.ui.QuadData()
      self.bullet_quads.append(qd)
      self.bullet_prim.addQuad(qd)
      self.bullets.append({'x': 0, 'y': 0, 'active': False})
    self.canvas.addPrimitive(self.bullet_prim)

    # Enemy missile primitive
    self.enemy_missile_prim = lev2.ui.QuadPrimitive(pipeline=self.canvas.pipelineSolid)
    for i in range(self.max_enemy_missiles):
      qd = lev2.ui.QuadData()
      self.enemy_missile_quads.append(qd)
      self.enemy_missile_prim.addQuad(qd)
      self.enemy_missiles.append({'x': 0, 'y': 0, 'active': False})
    self.canvas.addPrimitive(self.enemy_missile_prim)

    # Score text
    self.score_prim = lev2.ui.TextPrimitive(font=self.font, color=vec4(0.2, 1.0, 0.2, 1.0))
    self.score_prim.addItem("SCORE: 0", vec2(10, 10))
    self.canvas.addPrimitive(self.score_prim)

    # Game over text (initially empty)
    self.gameover_prim = lev2.ui.TextPrimitive(font=self.font_large, color=vec4(1.0, 0.2, 0.2, 1.0))
    self.canvas.addPrimitive(self.gameover_prim)

    self._updateGame(0)

  def _onCanvasEvent(self, ev):
    return lev2.ui.HandlerResult()

  def _fire(self):
    if self.game_over:
      return
    if self.fire_cooldown > 0:
      return
    # Find inactive bullet
    for bullet in self.bullets:
      if not bullet['active']:
        bullet['x'] = self.player_x
        bullet['y'] = self.player_y - 0.03  # Slightly above player (normalized)
        bullet['active'] = True
        self.fire_cooldown = 0.2
        break

  def _updateGame(self, dt):
    if self.game_over:
      self._renderGame()
      return

    canvas_w = self.canvas.width
    canvas_h = self.canvas.height
    if canvas_w < 1:
      return

    # Update animation and quantized invader movement
    self.anim_time += dt
    if self.anim_time > 0.5:
      self.anim_time = 0
      self.anim_frame = 1 - self.anim_frame

      # Move invaders in sync with animation (quantized step, normalized coords)
      move_down = False
      for inv in self.invaders:
        if inv['alive']:
          if self.invader_direction > 0 and inv['x'] > 0.92:
            move_down = True
            break
          elif self.invader_direction < 0 and inv['x'] < 0.08:
            move_down = True
            break

      if move_down:
        self.invader_direction *= -1
        for inv in self.invaders:
          inv['y'] += self.invader_drop
          # Check if invaders reached player
          if inv['alive'] and inv['y'] > self.player_y - 0.08:
            self.game_over = True
      else:
        for inv in self.invaders:
          inv['x'] += self.invader_direction * self.invader_step

    # Update cooldown
    self.fire_cooldown = max(0, self.fire_cooldown - dt)

    # Move player (normalized coords)
    if self.move_left:
      self.player_x -= self.player_speed * dt
    if self.move_right:
      self.player_x += self.player_speed * dt
    self.player_x = max(0.05, min(0.95, self.player_x))

    # Move bullets (normalized coords)
    for bullet in self.bullets:
      if bullet['active']:
        bullet['y'] -= self.bullet_speed * dt
        if bullet['y'] < 0:
          bullet['active'] = False

    # Collision detection - player bullets vs invaders (normalized hit box)
    hit_size = 0.025
    for bullet in self.bullets:
      if not bullet['active']:
        continue
      bx, by = bullet['x'], bullet['y']
      for inv in self.invaders:
        if not inv['alive']:
          continue
        ix, iy = inv['x'], inv['y']
        if abs(bx - ix) < hit_size and abs(by - iy) < hit_size:
          inv['alive'] = False
          bullet['active'] = False
          self.score += 10 * (self.invader_rows - inv['row'])
          break

    # Enemy firing logic
    self.enemy_fire_cooldown = max(0, self.enemy_fire_cooldown - dt)
    if self.enemy_fire_cooldown <= 0:
      # Find bottom-most alive invader in each column
      bottom_invaders = {}
      for inv in self.invaders:
        if inv['alive']:
          col = int((inv['x'] - self.invader_start_x + 0.01) / self.invader_spacing_x)
          if col not in bottom_invaders or inv['y'] > bottom_invaders[col]['y']:
            bottom_invaders[col] = inv

      # Find invader best aligned with player (within firing range)
      best_invader = None
      best_alignment = 0.05  # Max horizontal distance to consider
      for inv in bottom_invaders.values():
        alignment = abs(inv['x'] - self.player_x)
        if alignment < best_alignment:
          best_alignment = alignment
          best_invader = inv

      # Fire from the best aligned invader
      if best_invader:
        for missile in self.enemy_missiles:
          if not missile['active']:
            missile['x'] = best_invader['x']
            missile['y'] = best_invader['y'] + 0.03  # Below invader
            missile['active'] = True
            self.enemy_fire_cooldown = self.enemy_fire_interval + random.uniform(-0.3, 0.3)
            break

    # Move enemy missiles (downward)
    for missile in self.enemy_missiles:
      if missile['active']:
        missile['y'] += self.enemy_missile_speed * dt
        if missile['y'] > 1.0:
          missile['active'] = False

    # Collision detection - enemy missiles vs player
    player_hit_size = 0.03
    for missile in self.enemy_missiles:
      if not missile['active']:
        continue
      if abs(missile['x'] - self.player_x) < player_hit_size and abs(missile['y'] - self.player_y) < player_hit_size:
        missile['active'] = False
        self.game_over = True

    # Check win condition
    all_dead = all(not inv['alive'] for inv in self.invaders)
    if all_dead:
      self.game_over = True
      self.you_win = True

    self._renderGame()

  def _renderGame(self):
    canvas_w = self.canvas.width
    canvas_h = self.canvas.height

    # Scale pixel size based on screen size (reference: 800x600)
    scale = min(canvas_w / 800, canvas_h / 600)
    ps = max(2, int(4 * scale))  # Pixel size scales with screen
    bullet_w = max(3, int(5 * scale))
    bullet_h = max(10, int(18 * scale))

    quad_idx = 0

    # Row colors for invaders
    row_colors = [
      vec4(1.0, 0.2, 0.2, 1.0),  # Red
      vec4(1.0, 0.5, 0.2, 1.0),  # Orange
      vec4(1.0, 1.0, 0.2, 1.0),  # Yellow
      vec4(0.2, 1.0, 0.5, 1.0),  # Green
      vec4(0.2, 0.5, 1.0, 1.0),  # Blue
    ]

    # Render invaders as pixel art (convert normalized to screen coords)
    for inv in self.invaders:
      sprite_type = inv['type']
      sprite_data = self.sprite_types[sprite_type][self.anim_frame]
      pixels, width, height = sprite_data
      color = row_colors[inv['row'] % len(row_colors)]

      # Convert normalized position to screen pixels
      screen_x = inv['x'] * canvas_w
      screen_y = inv['y'] * canvas_h

      if inv['alive']:
        for px, py in pixels:
          qd = self.pixel_quads[quad_idx]
          x = screen_x + px * ps
          y = screen_y + py * ps
          qd.setPosition(x, canvas_h - y)
          qd.setSize(ps, ps)
          qd.setColor(color)
          quad_idx += 1
        # Fill remaining slots for this invader with hidden quads
        max_pixels = max(len(self.sprite_types[sprite_type][0][0]), len(self.sprite_types[sprite_type][1][0]))
        while quad_idx % max_pixels != 0 or quad_idx == 0:
          if quad_idx >= len(self.pixel_quads):
            break
          qd = self.pixel_quads[quad_idx]
          qd.setPosition(-100, -100)
          qd.setSize(0, 0)
          quad_idx += 1
      else:
        # Hide dead invader's pixels
        max_pixels = max(len(self.sprite_types[sprite_type][0][0]), len(self.sprite_types[sprite_type][1][0]))
        for _ in range(max_pixels):
          if quad_idx >= len(self.pixel_quads):
            break
          qd = self.pixel_quads[quad_idx]
          qd.setPosition(-100, -100)
          qd.setSize(0, 0)
          quad_idx += 1

    # Render player as pixel art (convert normalized to screen coords)
    player_pixels, pw, ph = self.player_sprite
    player_color = vec4(0.2, 0.9, 0.3, 1.0)
    player_screen_x = self.player_x * canvas_w
    player_screen_y = self.player_y * canvas_h
    for px, py in player_pixels:
      if quad_idx >= len(self.pixel_quads):
        break
      qd = self.pixel_quads[quad_idx]
      x = player_screen_x + px * ps
      y = player_screen_y + py * ps
      qd.setPosition(x, canvas_h - y)
      qd.setSize(ps, ps)
      qd.setColor(player_color)
      quad_idx += 1

    # Hide any remaining quads
    while quad_idx < len(self.pixel_quads):
      qd = self.pixel_quads[quad_idx]
      qd.setPosition(-100, -100)
      qd.setSize(0, 0)
      quad_idx += 1

    # Update player bullet quads (convert normalized to screen coords)
    for i, bullet in enumerate(self.bullets):
      qd = self.bullet_quads[i]
      if bullet['active']:
        bx = bullet['x'] * canvas_w
        by = bullet['y'] * canvas_h
        qd.setPosition(bx - bullet_w / 2, canvas_h - by)
        qd.setSize(bullet_w, bullet_h)
        qd.setColor(vec4(1.0, 1.0, 1.0, 1.0))
      else:
        qd.setPosition(-100, -100)
        qd.setSize(0, 0)

    # Update enemy missile quads (red/orange color)
    for i, missile in enumerate(self.enemy_missiles):
      qd = self.enemy_missile_quads[i]
      if missile['active']:
        mx = missile['x'] * canvas_w
        my = missile['y'] * canvas_h
        qd.setPosition(mx - bullet_w / 2, canvas_h - my)
        qd.setSize(bullet_w, bullet_h)
        qd.setColor(vec4(1.0, 0.3, 0.1, 1.0))  # Orange-red
      else:
        qd.setPosition(-100, -100)
        qd.setSize(0, 0)

    # Update score
    self.score_prim.clearItems()
    self.score_prim.addItem(f"SCORE: {self.score}", vec2(10, 10))

    # Game over text
    self.gameover_prim.clearItems()
    if self.game_over:
      if self.you_win:
        self.gameover_prim.addItem("YOU WIN!", vec2(canvas_w / 2 - 80, canvas_h / 2))
      else:
        self.gameover_prim.addItem("GAME OVER", vec2(canvas_w / 2 - 100, canvas_h / 2))
      self.gameover_prim.addItem("PRESS SPACE TO RESTART", vec2(canvas_w / 2 - 180, canvas_h / 2 + 50))

    self.canvas.markDirty()

  def onUpdate(self, updinfo):
    self.time += updinfo.deltatime
    self._updateGame(updinfo.deltatime)

  def _restart(self):
    # Reset game state
    self.score = 0
    self.game_over = False
    self.you_win = False
    self.player_x = 0.5
    self.anim_time = 0.0
    self.anim_frame = 0
    self.fire_cooldown = 0.5  # Prevent immediate fire after restart
    self.enemy_fire_cooldown = 1.0

    # Reset invaders
    self.invaders.clear()
    for row in range(self.invader_rows):
      for col in range(self.invader_cols):
        x = self.invader_start_x + col * self.invader_spacing_x
        y = self.invader_start_y + row * self.invader_spacing_y
        sprite_type = row % 3
        self.invaders.append({'x': x, 'y': y, 'alive': True, 'row': row, 'type': sprite_type})
    self.invader_direction = 1

    # Reset bullets and missiles
    for bullet in self.bullets:
      bullet['active'] = False
    for missile in self.enemy_missiles:
      missile['active'] = False

  def onUiEvent(self, uievent):
    if uievent.code == tokens.KEY_DOWN.hashed:
      if uievent.keycode == 263:  # Left arrow
        self.move_left = True
      elif uievent.keycode == 262:  # Right arrow
        self.move_right = True
      elif uievent.keycode == 32:  # Space
        if self.game_over:
          self._restart()
        elif self.fire_cooldown <= 0:
          self._fire()
    elif uievent.code == tokens.KEY_UP.hashed:
      if uievent.keycode == 263:
        self.move_left = False
      elif uievent.keycode == 262:
        self.move_right = False
    return lev2.ui.HandlerResult()

###############################################################################

SpaceInvaders().ezapp.mainThreadLoop()
