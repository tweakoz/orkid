###############################################################################
# _forest — shared instanced-forest LAYOUT (not an asset; underscore = helper, the
# resolver skips it). Any instanced-tree asset (ls_anim_inst, ...) imports
# forest_matrices() from HERE, so no forest depends on another forest.
#
# A forest is grid×grid copies of ONE shared mesh placed by per-instance matrices,
# deterministic per `seed` (same seed -> same forest, no global RNG). Each matrix's
# bottom-row free floats carry per-tree (phase, amp_delta, freq_delta) -> the VS
# wind reads them via _instance_attrs.xyz, so every tree sways on its own.
###############################################################################
import math
import numpy as np


def _h(i, stream):
  """Stateless hash -> [0,1): deterministic per (instance i, stream) so a layout is REPRODUCIBLE
  (same seed -> same forest) with no global RNG state. Distinct `stream` per jitter channel."""
  x = (int(i) * 2654435761 + int(stream) * 2246822519 + 2166136261) & 0xFFFFFFFF
  x ^= x >> 15; x = (x * 2246822519) & 0xFFFFFFFF
  x ^= x >> 13; x = (x * 3266489917) & 0xFFFFFFFF
  x ^= x >> 16
  return (x & 0xFFFFFF) / float(0x1000000)


def _inst_mtx(tx, tz, s, yaw, tilt, phi, d0, d1, d2):
  # rotation = Ry(phi) @ Rx(tilt) @ Ry(yaw): yaw spins the tree about its trunk, tilt leans it from
  # vertical, phi aims the lean azimuth. Scaled by s, translated to (tx, 0, tz).
  cy, sy = math.cos(yaw),  math.sin(yaw)
  ct, st = math.cos(tilt), math.sin(tilt)
  cp, sp = math.cos(phi),  math.sin(phi)
  Ry = np.array([[ cy, 0.0, sy], [0.0, 1.0, 0.0], [-sy, 0.0, cy]])
  Rx = np.array([[1.0, 0.0, 0.0], [0.0,  ct, -st], [0.0,  st, ct]])
  Rp = np.array([[ cp, 0.0, sp], [0.0, 1.0, 0.0], [-sp, 0.0, cp]])
  R  = Rp @ Rx @ Ry
  # COLUMN-MAJOR mat4: numpy row i = glm column i (so .reshape(-1) is column-major). m[0:3,0:3] = R.T*s
  # (each glm basis column = the matching column of R, scaled). The basis columns' .w (m[0..2,3]) carry
  # 3 free per-instance floats -> _instance_attrs.xyz (setupMeshRender splits them out + zeroes them in
  # the transform). Here: (phase01, amp_delta, freq_delta) the VS wind reads for per-tree sway.
  m = np.zeros((4, 4), dtype="float32")
  m[0:3, 0:3] = R.T * s
  m[0, 3], m[1, 3], m[2, 3] = d0, d1, d2
  m[3] = (tx, 0.0, tz, 1.0)
  return m


def forest_matrices(grid, spacing, scale, pos_jitter, rot_jitter, tilt_jitter,
                    phase_jitter, amp_jitter, freq_jitter, seed):
  """grid×grid per-instance matrices with deterministic jitter + per-tree wind (phase/amp/freq packed
  into the matrix bottom-row free floats -> _instance_attrs.xyz the VS wind reads). The single forest
  layout shared by every instanced-tree asset. Jitter channels (all amount-controlled, 0 = none):
    pos_jitter   — random XZ offset off the lattice (breaks grid regularity)
    rot_jitter   — random yaw spin (1.0 = fully random heading)
    tilt_jitter  — random lean from vertical (~14deg max at 1.0)
    phase_jitter — per-tree wind PHASE (1.0 = fully de-synced sway)
    amp_jitter   — per-tree sway amplitude delta about the material base
    freq_jitter  — per-tree sway frequency delta about the material base"""
  mats = []
  half = (grid - 1) * 0.5
  i = 0
  for gz in range(grid):
    for gx in range(grid):
      idx  = seed * 9176 + i               # per-tree seed (reproducible; `seed` reshuffles the forest)
      jx   = (_h(idx, 0) * 2.0 - 1.0) * pos_jitter * spacing * 0.5   # +/- half-cell at amount 1.0
      jz   = (_h(idx, 1) * 2.0 - 1.0) * pos_jitter * spacing * 0.5
      tx   = (gx - half) * spacing + jx
      tz   = (gz - half) * spacing + jz
      yaw  = _h(idx, 2) * 2.0 * math.pi * rot_jitter                 # random heading (full at 1.0)
      tilt = _h(idx, 3) * 0.25 * tilt_jitter                         # lean: 0.25 rad ~= 14deg max
      phi  = _h(idx, 4) * 2.0 * math.pi                              # lean azimuth (always random)
      s    = scale * (0.85 + 0.30 * _h(idx, 5))                      # modest natural size variation
      ph   = _h(idx, 6) * phase_jitter                              # phase01: 1.0 => fully de-synced sway
      amp  = (_h(idx, 7) * 2.0 - 1.0) * amp_jitter                  # amp delta  (+/- about the base)
      frq  = (_h(idx, 8) * 2.0 - 1.0) * freq_jitter                 # freq delta (+/- about the base)
      mats.append(_inst_mtx(tx, tz, s, yaw, tilt, phi, ph, amp, frq))
      i += 1
  return mats


__all__ = ["forest_matrices"]
