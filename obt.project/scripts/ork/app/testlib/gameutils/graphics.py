################################################################################
# Graphics utilities for game development
################################################################################

import numpy as np

def hsv_to_rgb(h, s, v):
  """
  Convert HSV color to RGB.

  Args:
    h: Hue (0.0 to 1.0)
    s: Saturation (0.0 to 1.0)
    v: Value/brightness (0.0 to 1.0)

  Returns:
    Tuple of (r, g, b) each in range 0.0 to 1.0
  """
  if s == 0.0:
    return v, v, v
  i = int(h * 6.0)
  f = (h * 6.0) - i
  p = v * (1.0 - s)
  q = v * (1.0 - s * f)
  t = v * (1.0 - s * (1.0 - f))
  i = i % 6
  if i == 0: return v, t, p
  if i == 1: return q, v, p
  if i == 2: return p, v, t
  if i == 3: return p, q, v
  if i == 4: return t, p, v
  if i == 5: return v, p, q
  return v, v, v

def hsv_to_rgb255(h, s, v):
  """
  Convert HSV color to RGB with 0-255 range.

  Returns:
    Tuple of (r, g, b) each in range 0 to 255
  """
  r, g, b = hsv_to_rgb(h, s, v)
  return int(r * 255), int(g * 255), int(b * 255)


def create_texture_from_numpy(ctx, np_img, name, flip_y=True, auto_alpha=True):
  """
  Create an Orkid texture from a numpy array.

  Args:
    ctx: Orkid graphics context
    np_img: Numpy array of shape (H, W, 3) or (H, W, 4) with uint8 values
    name: Texture name string
    flip_y: If True, flip image vertically (OpenGL convention)
    auto_alpha: If True and input is RGB, generate alpha from non-black pixels

  Returns:
    lev2.Texture object
  """
  from orkengine import lev2
  from orkengine.core import CrcStringProxy
  tokens = CrcStringProxy()

  txi = ctx.TXI

  if flip_y:
    np_img = np.flipud(np_img).copy()
  else:
    np_img = np_img.copy()

  h, w = np_img.shape[:2]

  # Handle RGB vs RGBA input
  if np_img.ndim == 2:
    # Grayscale - expand to RGBA
    rgba = np.zeros((h, w, 4), dtype=np.uint8)
    rgba[:, :, 0] = np_img
    rgba[:, :, 1] = np_img
    rgba[:, :, 2] = np_img
    rgba[:, :, 3] = np.where(np_img > 0, 255, 0) if auto_alpha else 255
  elif np_img.shape[2] == 3:
    # RGB - add alpha channel
    rgba = np.zeros((h, w, 4), dtype=np.uint8)
    rgba[:, :, :3] = np_img
    if auto_alpha:
      rgba[:, :, 3] = np.where(np.any(np_img > 0, axis=2), 255, 0)
    else:
      rgba[:, :, 3] = 255
  else:
    # Already RGBA
    rgba = np_img

  img = lev2.Image.createFromBuffer(w, h, tokens.RGBA8, rgba)
  tex = lev2.Texture(name)
  txi.updateTexture(tex, img)
  return tex


def create_circle_sprite(size, color, filled=True):
  """
  Create a circle sprite as numpy array.

  Args:
    size: Image size (width and height)
    color: RGB tuple (0-255 each)
    filled: If True, fill the circle; otherwise just outline

  Returns:
    Numpy array of shape (size, size, 3)
  """
  img = np.zeros((size, size, 3), dtype=np.uint8)
  center = size // 2
  radius = size // 2 - 2

  for y in range(size):
    for x in range(size):
      dx = x - center
      dy = y - center
      dist = (dx * dx + dy * dy) ** 0.5
      if filled:
        if dist <= radius:
          img[y, x] = color
      else:
        if radius - 1 <= dist <= radius + 1:
          img[y, x] = color
  return img


def create_rect_sprite(size, color, border=0):
  """
  Create a rectangle sprite as numpy array.

  Args:
    size: Image size (width and height) or (width, height) tuple
    color: RGB tuple (0-255 each)
    border: Border width in pixels (0 = filled)

  Returns:
    Numpy array of shape (size, size, 3) or (height, width, 3)
  """
  if isinstance(size, tuple):
    w, h = size
  else:
    w = h = size

  img = np.zeros((h, w, 3), dtype=np.uint8)

  if border == 0:
    img[:, :] = color
  else:
    # Top and bottom borders
    img[:border, :] = color
    img[-border:, :] = color
    # Left and right borders
    img[:, :border] = color
    img[:, -border:] = color

  return img


def parse_ascii_sprite(sprite_str, char='*', color_map=None):
  """
  Parse an ASCII art sprite into pixel positions.

  Args:
    sprite_str: Multi-line string with char marking pixels
    char: Character that represents a pixel (default '*'), ignored if color_map provided
    color_map: Optional dict mapping characters to colors (e.g., {'R': vec4(1,0,0,1)})
               When provided, returns pixels with per-pixel colors

  Returns:
    Tuple of (pixels, width, height) where pixels is list of:
      - (x, y) offsets from center if no color_map
      - (x, y, color) if color_map provided
  """
  pixels = []
  lines = sprite_str.lstrip('\n').rstrip().split('\n')
  height = len(lines)
  width = max(len(line) for line in lines) if lines else 0
  for row, line in enumerate(lines):
    for col, c in enumerate(line):
      if color_map:
        if c in color_map:
          pixels.append((col - width / 2, row - height / 2, color_map[c]))
      elif c == char:
        pixels.append((col - width / 2, row - height / 2))
  return pixels, width, height
