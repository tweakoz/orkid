#!/usr/bin/env ork.python
################################################################################
# STORED-ATLAS bake-cache key gate — the key must track BAKED PIXELS, not the
# render technique.
#
# The terrain's stored atlas is baked once through ONE forced technique
# (FWD_SSBO_CUSTOM_CAPTURE, terrain_chunk_drawable.cpp) and cached on disk under
# a dir named from a material digest + a terrain-content hash
# (ork.hypergraph.ecs.scene._terrain). That material digest USED to be the
# compiled .fxv2's filename — a whole-file sha1 — so ORKID_TERRAIN_MESHSHADER,
# which only APPENDS a render technique, renamed the cache dir and forced a full
# 8192^2 re-bake (multi-second launch stall) for pixels that cannot differ.
#
# This gate is pure text (no GPU, no window, no assets):
#
#   1. LEAK WITNESS — the mesh env really does change the compiled artifact's
#      digest. If this ever stops being true the regression can't recur, but the
#      exclusion below is still correct; the witness documents WHY it exists.
#   2. KEY IMMUNITY — texbake_material_digest() is IDENTICAL across mesh modes
#      0/1/2 (computed with the env actually set, so a future env read anywhere
#      inside the key path trips this), and is NOT the compiled digest.
#   3. SENSITIVITY — the key still tracks what the pixels depend on: a material
#      SOURCE edit, a material PARAM change, or a capture-TARGET change each
#      produce a different key (else a real edit would bind a stale atlas).
#   4. DSL-EMBEDDED MATERIAL — a terrain DSL declares its MATERIAL_CLASS inside
#      the DSL file, which load_dsl_class execs as `terrain_dsl_module` with no
#      __file__; inspect can retrieve NO source for it. The key must then take
#      the caller's DSL text (and track edits to it), never quietly fall back to
#      the class NAME — that fallback is source-blind and would pin a stale
#      atlas across every material edit.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
sys.stdout.reconfigure(line_buffering=True)
import hashlib

# repo root = five levels up; prepend THIS checkout's scripts dir so ork.testing +
# the scene/ptex3d modules resolve from the same tree as this test.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

from orkengine import core   # core before lev2
from ork.testing import verdict
from ork.hypergraph.ptex3d.fxv2_template import (generate_surface_fxv2, CODEGEN_VERSION)
from ork.hypergraph.ecs.scene._terrain import texbake_material_digest, texbake_material_source

BODY     = "o.albedo = vec3(uv.x, uv.y, 0.5);"
CAP_BODY = "c.base = vec4(uv, 0.0, 1.0);\nc.nrmao = vec4(wnrm, 1.0);"
TARGETS  = ("base", "nrmao")


# Two materials that differ by ONE authored constant — stand-ins for "the artist
# edited the surface". Only their SOURCE text matters here (never instantiated).
class MatA:
  TINT = 0.25

  def surface(self, ctx):
    return ctx.color * self.TINT


class MatB:
  TINT = 0.75

  def surface(self, ctx):
    return ctx.color * self.TINT


def compiled_digest(meshmode):
  """The .fxv2 filename digest the ptex3d codegen produces under this mesh mode —
  computed exactly as fxv2_template.materialize_surface_fxv2 does."""
  os.environ["ORKID_TERRAIN_MESHSHADER"] = str(meshmode)
  from ork.hypergraph.dflow.terrain.gpu_chunk import TerrainChunkVertexSource
  vs = TerrainChunkVertexSource(dim=1024, extent_m=1000.0, chunk=128)  # mesh= from the env
  text = generate_surface_fxv2(BODY, **vs.as_material_kwargs(),
                               wants_capture=True, capture_targets=TARGETS, capture_body=CAP_BODY)
  return hashlib.sha1((CODEGEN_VERSION + "\n" + text).encode("utf-8")).hexdigest()[:16]


def key_under(meshmode, mat_cls=MatA, params=None, targets=TARGETS):
  os.environ["ORKID_TERRAIN_MESHSHADER"] = str(meshmode)
  return texbake_material_digest(mat_cls, params if params is not None else {"tint": 0.5}, list(targets))


def main():
  fails = []
  ambient = os.environ.get("ORKID_TERRAIN_MESHSHADER")

  # 1. LEAK WITNESS
  comp = {m: compiled_digest(m) for m in (0, 1, 2)}
  print("compiled fxv2 digest: mode0=%s mode1=%s mode2=%s" % (comp[0], comp[1], comp[2]))
  if comp[0] == comp[1]:
    fails.append("mesh mode no longer changes the compiled fxv2 digest (witness stale)")

  # 2. KEY IMMUNITY
  keys = {m: key_under(m) for m in (0, 1, 2)}
  print("bake cache key      : mode0=%s mode1=%s mode2=%s" % (keys[0], keys[1], keys[2]))
  if len(set(keys.values())) != 1:
    fails.append("bake cache key varies with ORKID_TERRAIN_MESHSHADER: %r" % (keys,))
  if any(k.endswith(c) for k in keys.values() for c in comp.values()):
    fails.append("bake cache key is derived from the compiled artifact digest")

  # 3. SENSITIVITY — what the pixels DO depend on
  base = keys[0]
  cases = [("material source", key_under(0, mat_cls=MatB)),
           ("material params", key_under(0, params={"tint": 0.9})),
           ("capture targets", key_under(0, targets=("base", "nrmao", "wm")))]
  for label, k in cases:
    print("sensitivity %-16s -> %s (%s)" % (label, k, "changed" if k != base else "UNCHANGED"))
    if k == base:
      fails.append("bake cache key ignores a %s change" % label)

  # 4. DSL-EMBEDDED MATERIAL (no retrievable source)
  embedded = {}
  exec(compile("class Embedded:\n  TINT = 0.25\n", "<terrain_dsl_module>", "exec"), embedded)
  emb = embedded["Embedded"]
  emb.__module__ = "terrain_dsl_module"        # exactly what load_dsl_class produces
  try:
    texbake_material_source(emb)
    fails.append("source-less material class silently keyed (no fallback demanded)")
  except RuntimeError as e:
    print("embedded no-fallback -> raises: %s" % str(e).split(" — ")[0])
  emb_a = texbake_material_digest(emb, {}, TARGETS, fallback_src="class Embedded:\n  TINT = 0.25\n")
  emb_b = texbake_material_digest(emb, {}, TARGETS, fallback_src="class Embedded:\n  TINT = 0.75\n")
  print("embedded fallback   : a=%s b=%s (%s)" % (emb_a, emb_b, "changed" if emb_a != emb_b else "UNCHANGED"))
  if emb_a == emb_b:
    fails.append("DSL-embedded material key ignores the DSL source it lives in")

  if ambient is None:
    os.environ.pop("ORKID_TERRAIN_MESHSHADER", None)
  else:
    os.environ["ORKID_TERRAIN_MESHSHADER"] = ambient

  detail = "texbake cache key: mesh-immune=%s key=%s" % (len(set(keys.values())) == 1, base)
  sys.exit(verdict(not fails, detail if not fails else detail + " | " + "; ".join(fails)))


main()
