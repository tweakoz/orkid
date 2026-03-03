#!/usr/bin/env ork.python
################################################################################
# PrimCanvas Sprite Torture Test
# Stress tests SpritePrimitive and SpriteInstance with many sprites,
# transforms, rotations, scales, tints, visibility, and animation.
################################################################################

import math, random
from orkengine.core import vec2, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication

tokens = CrcStringProxy()

################################################################################
# Sprite Definitions - Various shapes for testing
################################################################################

COLORS = {
  'R': vec4(1.0, 0.2, 0.2, 1.0),
  'G': vec4(0.2, 1.0, 0.3, 1.0),
  'B': vec4(0.2, 0.4, 1.0, 1.0),
  'Y': vec4(1.0, 1.0, 0.2, 1.0),
  'W': vec4(1.0, 1.0, 1.0, 1.0),
  'C': vec4(0.2, 1.0, 1.0, 1.0),
  'M': vec4(1.0, 0.2, 1.0, 1.0),
  'O': vec4(1.0, 0.6, 0.2, 1.0),
}

# Various sprite shapes for testing (same colors, different patterns per frame)
SPRITES = {
  # Simple cross - expands/contracts
  'cross1': """
  W
 WWW
  W
""",
  'cross2': """
 WWW
WWWWW
 WWW
""",
  # Diamond - pulses
  'diamond1': """
  B
 BBB
BBBBB
 BBB
  B
""",
  'diamond2': """
 BBB
BBBBB
BBBBB
BBBBB
 BBB
""",
  # Arrow - stretches
  'arrow1': """
G
GG
GGG
GG
G
""",
  'arrow2': """
G
GG
GGG
GGGG
GGGGG
GGGG
GGG
GG
G
""",
  # Square - blinks inner
  'square1': """
MMMM
MWWM
MWWM
MMMM
""",
  'square2': """
MMMM
MMMM
MMMM
MMMM
""",
  # Star - twinkles
  'star1': """
  R
 RRR
RRRRR
 RRR
RR RR
""",
  'star2': """
R R R
 RRR
RRRRR
 RRR
R R R
""",
  # Spaceship - thruster animation
  'ship1': """
   G
  GGG
 GGGGG
GWWWWWG
 GGGGG
  G G
""",
  'ship2': """
   G
  GGG
 GGGGG
GWWWWWG
 GGGGG
  GWG
   W
""",
}

def parse_ascii_sprite(sprite_str, color_map):
  """Parse ASCII art sprite into pixel positions with colors."""
  pixels = []
  lines = sprite_str.lstrip('\n').rstrip().split('\n')
  height = len(lines)
  width = max(len(line) for line in lines) if lines else 0
  for row, line in enumerate(lines):
    for col, c in enumerate(line):
      if c in color_map:
        pixels.append((col - width / 2, row - height / 2, color_map[c]))
  return pixels, width, height

################################################################################
# Sprite Torture Test
################################################################################

class SpriteTortureTest(ComponentizedApplication):
  # Configuration
  NUM_SPRITES = 16384  # Number of sprite instances
  SPRITE_TYPES = ['cross', 'diamond', 'arrow', 'square', 'star', 'ship']

  def __init__(self):
    super().__init__()

    self.createEzApp(fullscreen=True)

    lg = self.ezapp.topLayoutGroup
    lg.clearColorStd = vec4(0.05, 0.05, 0.1, 1)
    cl = lg.makeChild(uiclass=lev2.ui.PrimCanvas, args=["sprites"])
    self.canvas = cl.widget
    for edge in ['top', 'left', 'bottom', 'right']:
      getattr(cl.layout, edge).anchorTo(getattr(lg.layout, edge))
    self.canvas.bg_color = vec4(0.05, 0.05, 0.1, 1)
    self.canvas.draw_background = True

    self.time = 0
    self.paused = False

  def _create_sprite(self, sprite_str):
    """Create a SpritePrimitive from ASCII art."""
    pixels, width, height = parse_ascii_sprite(sprite_str, COLORS)
    sprite = lev2.ui.SpritePrimitive(pipeline=self.canvas.pipelineSpriteSolid)
    for px, py, color in pixels:
      qd = lev2.ui.QuadData()
      qd.setPosition(px, py)
      qd.setSize(1, 1)
      qd.setColor(color)
      sprite.addQuad(qd)
    return sprite

  def _onGpuInit(self, ctx):
    self.canvas.gpuInit(ctx)
    self.font = lev2.FontManager.fontForId("i14")

    # Create layers
    self.templates_layer = self.canvas.createLayer("templates")  # For SSBO allocation
    self.sprites_layer = self.canvas.createLayer("sprites")
    self.ui_layer = self.canvas.createLayer("ui")

    # Create sprite templates (2 frames each for animation)
    self.sprite_templates = {}
    for stype in self.SPRITE_TYPES:
      self.sprite_templates[stype] = [
        self._create_sprite(SPRITES[f'{stype}1']),
        self._create_sprite(SPRITES[f'{stype}2']),
      ]

    # Add all sprite templates to templates layer for SSBO allocation
    for stype, frames in self.sprite_templates.items():
      for sprite in frames:
        self.templates_layer.addPrimitive(sprite)

    # Create sprite instances with random properties
    self.instances = []
    random.seed(42)  # Reproducible randomness
    for i in range(self.NUM_SPRITES):
      stype = self.SPRITE_TYPES[i % len(self.SPRITE_TYPES)]
      sprite = self.sprite_templates[stype][0]

      inst = lev2.ui.SpriteInstance(sprite=sprite)

      # Random initial state
      data = {
        'instance': inst,
        'type': stype,
        'x': random.uniform(0.1, 0.9),
        'y': random.uniform(0.1, 0.9),
        'vx': random.uniform(-0.1, 0.1),
        'vy': random.uniform(-0.1, 0.1),
        'rotation': random.uniform(0, math.pi * 2),
        'rot_speed': random.uniform(-2, 2),
        'scale': random.uniform(2, 6),
        'scale_phase': random.uniform(0, math.pi * 2),
        'tint_r_phase': random.uniform(0, math.pi * 2),
        'tint_g_phase': random.uniform(0, math.pi * 2),
        'tint_b_phase': random.uniform(0, math.pi * 2),
        'tint_speed': random.uniform(0.3, 1.2),  # Vary color cycling speed
        'anim_offset': random.uniform(0, 1),  # Desync animation timing
        'visible_phase': random.uniform(0, math.pi * 2),
      }

      self.instances.append(data)
      self.sprites_layer.addPrimitive(inst)

    # Text for stats
    self.stats_prim = lev2.ui.TextPrimitive(font=self.font, color=vec4(1, 1, 1, 1))
    self.ui_layer.addPrimitive(self.stats_prim)

    self._update_sprites(0)

  def _update_sprites(self, dt):
    w, h = self.canvas.width, self.canvas.height
    if w < 1:
      return

    for data in self.instances:
      inst = data['instance']

      # Movement with bounce
      data['x'] += data['vx'] * dt
      data['y'] += data['vy'] * dt

      if data['x'] < 0.05 or data['x'] > 0.95:
        data['vx'] *= -1
        data['x'] = max(0.05, min(0.95, data['x']))
      if data['y'] < 0.05 or data['y'] > 0.95:
        data['vy'] *= -1
        data['y'] = max(0.05, min(0.95, data['y']))

      # Rotation
      data['rotation'] += data['rot_speed'] * dt

      # Pulsing scale
      scale = data['scale'] * (1.0 + 0.3 * math.sin(self.time * 2 + data['scale_phase']))

      # Color tint cycling (independent phases per channel for full desync)
      t = self.time * data['tint_speed']
      r = 0.7 + 0.3 * math.sin(t + data['tint_r_phase'])
      g = 0.7 + 0.3 * math.sin(t + data['tint_g_phase'])
      b = 0.7 + 0.3 * math.sin(t + data['tint_b_phase'])
      inst.tint = vec4(r, g, b, 1.0)

      # Visibility toggling (some sprites blink)
      if data['visible_phase'] < 1.0:  # Only some sprites blink
        inst.visible = math.sin(self.time * 3 + data['visible_phase'] * 10) > 0
      else:
        inst.visible = True

      # Animation frame swapping (per-sprite timing for desync)
      anim_time = self.time * 4 + data['anim_offset'] * 10  # offset by up to 10 frames
      frame = int(anim_time) % 2
      inst.sprite = self.sprite_templates[data['type']][frame]

      # Apply transform
      sx = data['x'] * w
      sy = data['y'] * h
      inst.setTransform(sx, sy, data['rotation'], scale)

    # Update stats
    visible_count = sum(1 for d in self.instances if d['instance'].visible)
    self.stats_prim.clearItems()
    self.stats_prim.addItem(f"Sprites: {self.NUM_SPRITES} total, {visible_count} visible", vec2(10, 10))
    self.stats_prim.addItem(f"Time: {self.time:.1f}s  [SPACE=pause, Q=quit]", vec2(10, 28))

    self.canvas.markDirty()

  def _onUpdate(self, updinfo):
    # Visual feedback for pause state
    if not self.paused:
      self.time += updinfo.deltatime
      self._update_sprites(updinfo.deltatime)

  def _onUiEvent(self, ev):
    # Only react to KEY_DOWN, not KEY_REPEAT
    if ev.code == tokens.KEY_DOWN.hashed:
      if ev.keycode == 32:  # Space
        self.paused = not self.paused
      elif ev.keycode == ord('Q') or ev.keycode == 256:  # Q or Escape
        self.ezapp.signalExit()
    return lev2.ui.HandlerResult()

###############################################################################

app = SpriteTortureTest()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
