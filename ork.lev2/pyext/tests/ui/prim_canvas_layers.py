#!/usr/bin/env ork.python
################################################################################
# PrimCanvas Layer Transform Test - Shadow of the Beast Style Parallax
# Demonstrates layer transforms with multi-layer parallax scrolling:
#   1. Sky layer - slowest scroll (far background)
#   2. Mountains layer - medium scroll
#   3. Ground layer - faster scroll
#   4. Sprites layer - foreground animated objects
################################################################################

import math, random
from orkengine.core import vec2, vec3, vec4, mtx4, quat, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.frame_profiler import FrameProfilerComponent

tokens = CrcStringProxy()

################################################################################
# Color palette
################################################################################

COLORS = {
  # Sky colors
  'A': vec4(0.1, 0.1, 0.3, 1.0),   # Dark sky
  'B': vec4(0.2, 0.3, 0.5, 1.0),   # Mid sky
  'C': vec4(0.4, 0.5, 0.7, 1.0),   # Light sky
  'W': vec4(1.0, 1.0, 1.0, 1.0),   # White (stars/clouds)
  'Y': vec4(1.0, 0.9, 0.5, 1.0),   # Yellow (sun/moon)
  # Mountain colors
  'M': vec4(0.15, 0.1, 0.2, 1.0),  # Dark mountain
  'm': vec4(0.25, 0.2, 0.3, 1.0),  # Mid mountain
  'P': vec4(0.35, 0.25, 0.4, 1.0), # Purple mountain
  # Ground colors
  'G': vec4(0.2, 0.4, 0.15, 1.0),  # Dark grass
  'g': vec4(0.3, 0.55, 0.2, 1.0),  # Light grass
  'T': vec4(0.35, 0.2, 0.1, 1.0),  # Tree trunk
  'L': vec4(0.15, 0.35, 0.1, 1.0), # Dark leaves
  'l': vec4(0.25, 0.5, 0.15, 1.0), # Light leaves
  # Sprite colors
  'R': vec4(1.0, 0.3, 0.2, 1.0),   # Red
  'O': vec4(1.0, 0.6, 0.2, 1.0),   # Orange
}

################################################################################
# Tile patterns (16x16 for better detail)
################################################################################

TILES = {
  # Gradient sky tile (top - dark)
  'sky_top': """
AAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAA
AAAAAAAWAAAAAAAA
AAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAA
AAAAAAAAAAAWAAAA
AAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAA
AAAWAAAAAAAAAWAA
AAAAAAAAAAAAAAAA
AAAAAAAAAAAAAAAA
""",
  # Sky mid section
  'sky_mid': """
BBBBBBBBBBBBBBBB
BBBBBBBBBBBBBBBB
BBBBBWBBBBBBBBBB
BBBBBBBBBBBBBBBB
BBBBBBBBBBBBBBBB
BBBBBBBBBBWBBBBB
BBBBBBBBBBBBBBBB
BBBBBBBBBBBBBBBB
BBBBBBBBBBBBBBBB
BBBWBBBBBBBBBBBB
BBBBBBBBBBBBBBBB
BBBBBBBBBBBBBBBB
BBBBBBBBBBBWBBBB
BBBBBBBBBBBBBBBB
BBBBBBBBBBBBBBBB
BBBBWBBBBBBBBBBB
""",
  # Ground with grass
  'ground': """
gGgGgGgGgGgGgGgG
GgGgGgGgGgGgGgGg
gGgGgGgGgGgGgGgG
GgGgGgGgGgGgGgGg
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
""",
  # Tree on ground
  'tree': """
GGGGGGllllGGGGGG
GGGGGllllllGGGGG
GGGGllllllllGGGG
GGGllllllllllGGG
GGllllllllllllGG
GGGllLLLLLLllGGG
GGGGLLLLLLLlGGGG
GGGGGLLTTLlGGGGG
GGGGGGTTTGGGGGGG
GGGGGGTTTGGGGGGG
GGGGGGTTTGGGGGGG
GgGgGgTTTgGgGgGg
GgGgGgGgGgGgGgGg
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
GGGGGGGGGGGGGGGG
""",
}

################################################################################
# Animated sprites
################################################################################

SPRITES = {
  'bird1': """
 W   W
 WW WW
WWWWWWW
  WWW
""",
  'bird2': """
  WWW
 WWWWW
WWWWWWW
 W   W
""",
  'bat1': """
MM   MM
MMMMMMM
 MMMMM
  MMM
""",
  'bat2': """
 M   M
MMMMMMM
MMMMMMM
  MMM
""",
  'runner1': """
  RR
 RRRR
  RR
 R  R
R    R
""",
  'runner2': """
  RR
 RRRR
  RR
  RR
 R  R
""",
}

################################################################################
# Helpers
################################################################################

def parse_ascii(ascii_str, color_map):
  """Parse ASCII art into pixel positions with colors."""
  pixels = []
  lines = ascii_str.lstrip('\n').rstrip().split('\n')
  height = len(lines)
  width = max(len(line) for line in lines) if lines else 0
  for row, line in enumerate(lines):
    for col, c in enumerate(line):
      if c in color_map:
        pixels.append((col, row, color_map[c]))
  return pixels, width, height

################################################################################
# Shadow of the Beast Parallax Demo
################################################################################

class ParallaxDemo(ComponentizedApplication):
  TILE_SIZE = 32

  # Parallax scroll speeds (pixels per second)
  SKY_SPEED = 8
  MOUNTAIN_SPEED = 20
  GROUND_SPEED = 60

  def __init__(self):
    super().__init__()

    self.profiler = self.addComponent("profiler", FrameProfilerComponent,
                                      gpu_filter=["*", "-fwd:total"])

    self.createEzApp(fullscreen=True)

    lg = self.ezapp.topLayoutGroup
    lg.clearColorStd = vec4(0.05, 0.05, 0.1, 1)
    cl = lg.makeChild(uiclass=lev2.ui.PrimCanvas, args=["parallax"])
    self.canvas = cl.widget
    for edge in ['top', 'left', 'bottom', 'right']:
      getattr(cl.layout, edge).anchorTo(getattr(lg.layout, edge))
    self.canvas.bg_color = vec4(0.05, 0.05, 0.1, 1)
    self.canvas.draw_background = True

    self.time = 0
    self.scroll_pos = 0  # Master scroll position
    self.paused = False
    self.scroll_input = 0  # -1 = left, 0 = none, 1 = right
    self.key_left = False
    self.key_right = False

  def _create_tile_sprite(self, tile_str):
    """Create a SpritePrimitive from ASCII tile art."""
    pixels, width, height = parse_ascii(tile_str, COLORS)
    sprite = lev2.ui.SpritePrimitive(pipeline=self.canvas.pipelineSpriteSolid)
    pixel_size = self.TILE_SIZE / max(width, height)
    for px, py, color in pixels:
      qd = lev2.ui.QuadData()
      # Position from top-left (0,0) - no centering
      qd.setPosition(px * pixel_size, py * pixel_size)
      qd.setSize(pixel_size, pixel_size)
      qd.setColor(color)
      sprite.addQuad(qd)
    return sprite

  def _create_sprite(self, sprite_str, scale=4.0):
    """Create animated sprite from ASCII art."""
    pixels, width, height = parse_ascii(sprite_str, COLORS)
    sprite = lev2.ui.SpritePrimitive(pipeline=self.canvas.pipelineSpriteSolid)
    for px, py, color in pixels:
      qd = lev2.ui.QuadData()
      qd.setPosition((px - width/2) * scale, (py - height/2) * scale)
      qd.setSize(scale, scale)
      qd.setColor(color)
      sprite.addQuad(qd)
    return sprite

  def _create_wave_strip(self, layer, sample_count, total_width, amplitude, base_y, color, phase_offset=0):
    """Create a wave tristrip like prim_canvas_tristrip.py."""
    strip = lev2.ui.TriStripPrimitive(pipeline=self.canvas.pipelineVtxSolid)
    vertices = []
    for i in range(sample_count * 2):
      vd = lev2.ui.VertexData()
      vertices.append(vd)
      strip.addVertex(vd)
    layer.addPrimitive(strip)
    return strip, vertices

  def _update_wave_strip(self, vertices, sample_count, total_width, amplitude, base_y, color, time_offset):
    """Update wave strip vertices - same formula as tristrip test."""
    thickness = 4.0
    for i in range(sample_count):
      x = (i / (sample_count - 1)) * total_width
      phase = (i / sample_count) * math.pi * 8
      wave = (
        math.sin(phase + time_offset * 0.1) * 0.5 +
        math.sin(phase * 2.1 + time_offset * 0.15) * 0.3 +
        math.sin(phase * 0.5 + time_offset * 0.05) * 0.4 +
        math.sin(phase * 4.7 + time_offset * 0.2) * 0.15
      )
      wave = wave / 1.35
      y = base_y + wave * amplitude
      # Bottom vertex
      vertices[i * 2].setPosition(x, y - thickness)
      vertices[i * 2].setColor(color)
      # Top vertex
      vertices[i * 2 + 1].setPosition(x, y + thickness)
      vertices[i * 2 + 1].setColor(color)

  def _create_mountains(self, screen_width):
    """Create 3 wave layers for parallax mountains."""
    sample_count = 256
    total_width = self.layer_width * 2
    mtn_height = 2 * self.TILE_SIZE

    # Back wave - tallest, lightest (distant)
    self.mtn_back_layer = self.canvas.createLayer("mtn_back")
    self.mtn_back_strip, self.mtn_back_verts = self._create_wave_strip(
      self.mtn_back_layer, sample_count, total_width,
      mtn_height * 0.8, mtn_height * 0.6,
      vec4(0.25, 0.20, 0.35, 1.0)
    )

    # Mid wave
    self.mtn_mid_layer = self.canvas.createLayer("mtn_mid")
    self.mtn_mid_strip, self.mtn_mid_verts = self._create_wave_strip(
      self.mtn_mid_layer, sample_count, total_width,
      mtn_height * 0.6, mtn_height * 0.4,
      vec4(0.15, 0.12, 0.22, 1.0)
    )

    # Front wave - shortest, darkest (closest)
    self.mtn_front_layer = self.canvas.createLayer("mtn_front")
    self.mtn_front_strip, self.mtn_front_verts = self._create_wave_strip(
      self.mtn_front_layer, sample_count, total_width,
      mtn_height * 0.4, mtn_height * 0.25,
      vec4(0.08, 0.06, 0.12, 1.0)
    )

    self.mtn_sample_count = sample_count
    self.mtn_total_width = total_width
    self.mtn_height = mtn_height

  def _create_layer_tiles(self, layer, tile_pattern, num_tiles_x, num_tiles_y, y_offset):
    """Create a horizontal strip of tiles for parallax scrolling."""
    instances = []
    for row in range(num_tiles_y):
      for col in range(num_tiles_x):
        # Select tile based on pattern
        if callable(tile_pattern):
          tile_name = tile_pattern(col, row)
        else:
          tile_name = tile_pattern

        if tile_name and tile_name in self.tile_sprites:
          inst = lev2.ui.SpriteInstance(sprite=self.tile_sprites[tile_name])
          # Position at grid cell (no centering - tiles start at 0,0)
          x = col * self.TILE_SIZE
          y = y_offset + row * self.TILE_SIZE
          inst.setTransform(x, y, 0, 1.0)
          layer.addPrimitive(inst)
          instances.append(inst)
    return instances

  def _onGpuInit(self, ctx):
    self.canvas.gpuInit(ctx)
    self.font = lev2.FontManager.fontForId("i14")

    w, h = self.canvas.width, self.canvas.height
    if w < 100: w = 1920
    if h < 100: h = 1080

    # Calculate how many tiles we need (2x width for seamless wrap)
    self.tiles_per_row = (w // self.TILE_SIZE) + 4
    self.layer_width = self.tiles_per_row * self.TILE_SIZE

    ############################################################################
    # Create layers (back to front)
    ############################################################################
    self.sky_layer = self.canvas.createLayer("sky")
    # Mountain layers created by _create_mountains()
    self.ground_layer = self.canvas.createLayer("ground")
    self.sprites_layer = self.canvas.createLayer("sprites")
    self.ui_layer = self.canvas.createLayer("ui")

    ############################################################################
    # Create tile sprites
    ############################################################################
    self.tile_sprites = {}
    for name, tile_str in TILES.items():
      self.tile_sprites[name] = self._create_tile_sprite(tile_str)

    # Add to a dummy layer for SSBO allocation
    for sprite in self.tile_sprites.values():
      self.sky_layer.addPrimitive(sprite)

    ############################################################################
    # Sky layer - gradient with stars (slowest scroll)
    ############################################################################
    def sky_pattern(col, row):
      if row < 2:
        return 'sky_top'
      else:
        return 'sky_mid'

    self.sky_instances = self._create_layer_tiles(
      self.sky_layer, sky_pattern,
      self.tiles_per_row * 2, 4, 0  # Double width for wrap
    )

    ############################################################################
    # Mountain layers - 3 wave layers with parallax (created between sky and ground)
    ############################################################################
    self._create_mountains(w)
    # Reorder: move ground, sprites, ui after mountains
    self.canvas.removeLayer(self.ground_layer)
    self.canvas.removeLayer(self.sprites_layer)
    self.canvas.removeLayer(self.ui_layer)
    self.canvas.addLayer(self.ground_layer)
    self.canvas.addLayer(self.sprites_layer)
    self.canvas.addLayer(self.ui_layer)

    ############################################################################
    # Ground layer - terrain with trees (fastest scroll)
    ############################################################################
    def ground_pattern(col, row):
      # Trees every few tiles
      if col % 7 == 3:
        return 'tree'
      return 'ground'

    self.ground_instances = self._create_layer_tiles(
      self.ground_layer, ground_pattern,
      self.tiles_per_row * 2, 3, 0
    )

    ############################################################################
    # Sprites layer - animated creatures
    ############################################################################
    self.anim_sprites = {
      'bird': [self._create_sprite(SPRITES['bird1']), self._create_sprite(SPRITES['bird2'])],
      'bat': [self._create_sprite(SPRITES['bat1']), self._create_sprite(SPRITES['bat2'])],
      'runner': [self._create_sprite(SPRITES['runner1']), self._create_sprite(SPRITES['runner2'])],
    }

    # Add for SSBO
    for frames in self.anim_sprites.values():
      for sprite in frames:
        self.sky_layer.addPrimitive(sprite)

    # Create sprite instances
    self.sprite_data = []
    random.seed(42)

    # Birds in sky area
    for i in range(4):
      inst = lev2.ui.SpriteInstance(sprite=self.anim_sprites['bird'][0])
      self.sprite_data.append({
        'instance': inst,
        'type': 'bird',
        'x': random.uniform(100, w - 100),
        'vx': random.uniform(20, 60),
        'phase': random.uniform(0, 10),
      })
      self.sprites_layer.addPrimitive(inst)

    # Bats in mountain area
    for i in range(3):
      inst = lev2.ui.SpriteInstance(sprite=self.anim_sprites['bat'][0])
      self.sprite_data.append({
        'instance': inst,
        'type': 'bat',
        'x': random.uniform(100, w - 100),
        'vx': random.uniform(-40, -20),
        'phase': random.uniform(0, 10),
      })
      self.sprites_layer.addPrimitive(inst)

    # Runners on ground
    for i in range(3):
      inst = lev2.ui.SpriteInstance(sprite=self.anim_sprites['runner'][0])
      self.sprite_data.append({
        'instance': inst,
        'type': 'runner',
        'x': random.uniform(100, w - 100),
        'vx': random.uniform(60, 120),
        'phase': random.uniform(0, 10),
      })
      self.sprites_layer.addPrimitive(inst)

    ############################################################################
    # UI layer
    ############################################################################
    self.stats_prim = lev2.ui.TextPrimitive(font=self.font, color=vec4(1, 1, 1, 1))
    self.ui_layer.addPrimitive(self.stats_prim)

    self.canvas.markDirty()

  def _update(self, dt):
    w, h = self.canvas.width, self.canvas.height
    if w < 1 or h < 1:
      return

    # Update scroll input from held keys
    self.scroll_input = 0
    if self.key_right:
      self.scroll_input += 1
    if self.key_left:
      self.scroll_input -= 1

    # Advance master scroll (base speed + input)
    scroll_speed = self.GROUND_SPEED + self.scroll_input * 200
    self.scroll_pos += scroll_speed * dt

    # Wrap scroll position
    wrap_width = self.layer_width / 2
    if self.scroll_pos > wrap_width:
      self.scroll_pos -= wrap_width

    ############################################################################
    # Layer layout - proportional heights
    # NOTE: Y appears inverted - Y=0 is BOTTOM, Y=h is TOP
    #   Ground:    bottom 35% (y = 0)
    #   Mountains: next 25%
    #   Sky:       top 40%
    ############################################################################
    ground_screen_y = 0
    ground_screen_h = h * 0.35
    mtn_screen_y = ground_screen_h
    mtn_screen_h = h * 0.25
    sky_screen_y = mtn_screen_y + mtn_screen_h
    sky_screen_h = h * 0.40

    # Local coordinate heights for each layer
    sky_local_h = 4 * self.TILE_SIZE
    mtn_local_h = 2 * self.TILE_SIZE
    ground_local_h = 3 * self.TILE_SIZE

    # Sky - slowest scroll, at top of screen
    sky_scroll = (self.scroll_pos * self.SKY_SPEED / self.GROUND_SPEED) % wrap_width
    sky_scale_y = sky_screen_h / sky_local_h
    sky_transform = mtx4()
    sky_transform.setColumn(0, vec4(1.0, 0, 0, 0))
    sky_transform.setColumn(1, vec4(0, sky_scale_y, 0, 0))
    sky_transform.setColumn(2, vec4(0, 0, 1, 0))
    sky_transform.setColumn(3, vec4(-sky_scroll, sky_screen_y, 0, 1))
    self.sky_layer.transform = sky_transform

    # Mountains - 3 layers at different speeds (parallax)
    mtn_scale_y = mtn_screen_h / mtn_local_h

    # Update wave vertices
    self._update_wave_strip(self.mtn_back_verts, self.mtn_sample_count, self.mtn_total_width,
                            self.mtn_height * 0.8, self.mtn_height * 0.6,
                            vec4(0.25, 0.20, 0.35, 1.0), self.time * 0.5)
    self._update_wave_strip(self.mtn_mid_verts, self.mtn_sample_count, self.mtn_total_width,
                            self.mtn_height * 0.6, self.mtn_height * 0.4,
                            vec4(0.15, 0.12, 0.22, 1.0), self.time * 0.8)
    self._update_wave_strip(self.mtn_front_verts, self.mtn_sample_count, self.mtn_total_width,
                            self.mtn_height * 0.4, self.mtn_height * 0.25,
                            vec4(0.08, 0.06, 0.12, 1.0), self.time * 1.2)

    # Back mountain - slowest
    mtn_back_scroll = (self.scroll_pos * 0.15) % wrap_width
    mtn_back_transform = mtx4()
    mtn_back_transform.setColumn(0, vec4(1.0, 0, 0, 0))
    mtn_back_transform.setColumn(1, vec4(0, mtn_scale_y, 0, 0))
    mtn_back_transform.setColumn(2, vec4(0, 0, 1, 0))
    mtn_back_transform.setColumn(3, vec4(-mtn_back_scroll, mtn_screen_y, 0, 1))
    self.mtn_back_layer.transform = mtn_back_transform

    # Mid mountain - medium
    mtn_mid_scroll = (self.scroll_pos * 0.25) % wrap_width
    mtn_mid_transform = mtx4()
    mtn_mid_transform.setColumn(0, vec4(1.0, 0, 0, 0))
    mtn_mid_transform.setColumn(1, vec4(0, mtn_scale_y, 0, 0))
    mtn_mid_transform.setColumn(2, vec4(0, 0, 1, 0))
    mtn_mid_transform.setColumn(3, vec4(-mtn_mid_scroll, mtn_screen_y, 0, 1))
    self.mtn_mid_layer.transform = mtn_mid_transform

    # Front mountain - fastest of the 3
    mtn_front_scroll = (self.scroll_pos * 0.4) % wrap_width
    mtn_front_transform = mtx4()
    mtn_front_transform.setColumn(0, vec4(1.0, 0, 0, 0))
    mtn_front_transform.setColumn(1, vec4(0, mtn_scale_y, 0, 0))
    mtn_front_transform.setColumn(2, vec4(0, 0, 1, 0))
    mtn_front_transform.setColumn(3, vec4(-mtn_front_scroll, mtn_screen_y, 0, 1))
    self.mtn_front_layer.transform = mtn_front_transform

    # Ground - fastest scroll, bottom of screen
    ground_scroll = self.scroll_pos % wrap_width
    ground_scale_y = ground_screen_h / ground_local_h
    ground_transform = mtx4()
    ground_transform.setColumn(0, vec4(1.0, 0, 0, 0))
    ground_transform.setColumn(1, vec4(0, ground_scale_y, 0, 0))
    ground_transform.setColumn(2, vec4(0, 0, 1, 0))
    ground_transform.setColumn(3, vec4(-ground_scroll, ground_screen_y, 0, 1))
    self.ground_layer.transform = ground_transform

    ############################################################################
    # Sprites - positioned in screen coordinates
    ############################################################################
    self.sprites_layer.transform = mtx4.composed(vec3(0, 0, 0), quat(), 1.0)

    for data in self.sprite_data:
      inst = data['instance']

      # Move sprite
      data['x'] += data['vx'] * dt

      # Wrap horizontally
      if data['x'] > w + 50:
        data['x'] = -50
      elif data['x'] < -50:
        data['x'] = w + 50

      # Animate
      frames = self.anim_sprites[data['type']]
      frame_idx = int((self.time * 8 + data['phase'])) % len(frames)
      inst.sprite = frames[frame_idx]

      # Position sprites in screen coordinates (Y=0 at bottom)
      if data['type'] == 'runner':
        sprite_y = ground_screen_y + ground_screen_h * 0.7  # On ground (near top of ground area)
      elif data['type'] == 'bird':
        sprite_y = sky_screen_y + sky_screen_h * 0.5 + 30 * math.sin(self.time * 2 + data['phase'])
      elif data['type'] == 'bat':
        sprite_y = mtn_screen_y + mtn_screen_h * 0.5 + 20 * math.sin(self.time * 3 + data['phase'])
      else:
        sprite_y = h / 2

      inst.setTransform(data['x'], sprite_y, 0, 1.0)

    ############################################################################
    # UI
    ############################################################################
    self.stats_prim.clearItems()
    self.stats_prim.addItem("Shadow of the Beast Style Parallax", vec2(10, 10))
    self.stats_prim.addItem(f"Scroll: {self.scroll_pos:.0f}  Speed: {scroll_speed:.0f}  Time: {self.time:.1f}s", vec2(10, 28))
    self.stats_prim.addItem("[SPACE=pause, HOLD LEFT/RIGHT=scroll, Q=quit]", vec2(10, 46))

    self.canvas.markDirty()

  def _onUpdate(self, updinfo):
    if not self.paused:
      self.time += updinfo.deltatime
      self._update(updinfo.deltatime)

  def _onUiEvent(self, ev):
    if ev.code == tokens.KEY_DOWN.hashed:
      if ev.keycode == 32:  # Space
        self.paused = not self.paused
      elif ev.keycode == 262:  # Right arrow
        self.key_right = True
      elif ev.keycode == 263:  # Left arrow
        self.key_left = True
      elif ev.keycode == ord('Q') or ev.keycode == 256:
        self.ezapp.signalExit()
    elif ev.code == tokens.KEY_UP.hashed:
      if ev.keycode == 262:  # Right arrow
        self.key_right = False
      elif ev.keycode == 263:  # Left arrow
        self.key_left = False
    return lev2.ui.HandlerResult()

###############################################################################

app = ParallaxDemo()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
