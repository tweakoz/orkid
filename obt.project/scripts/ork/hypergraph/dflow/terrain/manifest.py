###############################################################################
# ork.hypergraph.dflow.terrain.manifest — the terrain bake SCALE CONTRACT.
#
# A terrain bake writes one  <asset>.terrain.json  next to its channel images. It
# is the AUTHORITATIVE, self-describing scale + normalization for every consumer
# (viewer, CPU mask sampler, physics, segmentation) so nothing hardcodes the
# DEFAULT_* constants the viewer used to own.
#
# Conventions (locked):
#   * Horizontal: X,Z in [-extent_m/2, +extent_m/2], centered at origin. The uv map
#     is  uv = xz/extent_m + 0.5  with TEXEL-CENTER sampling (matches the GPU
#     sampler2D LINEAR convention and the viewer's CPU bilinear).
#   * Vertical (v2 — NATURAL UNITS): stored heights ARE world meters (Y_m = h).
#     There is no vertical scale constant; exaggeration is authored in the graph
#     (remap nodes) and lands in the stored meters like everything else.
#   * Per-channel (min,max,mean) are the raw FieldStats of the stored field —
#     provenance: compare bakes, frame cameras/spawns, drive segmentation metrics.
#
# A measurement (normal/slope/curvature/segmentation) reads `extent_m` here; heights
# arrive in meters, so measurements match what the consumer renders by construction.
###############################################################################

import json
import os
from dataclasses import dataclass, field

_VERSION = 2  # v2: NATURAL UNITS (stored heights are meters; no height_m key)


@dataclass
class ChannelInfo:
    file: str                    # basename, resolved relative to the manifest dir
    semantic: str = ""           # "height_meters" | "normal_world" | mask name | ...
    min: float = 0.0             # raw FieldStats over the captured field
    max: float = 0.0
    mean: float = 0.0


@dataclass
class TerrainManifest:
    """The persisted scale contract for one terrain bake. Load with `.load(path)`;
    consumers read `extent_m`/`dim` + per-channel ranges and use the world<->uv
    helpers instead of hardcoding scale. Stored heights are TRUE METERS (v2)."""
    extent_m: float
    dim: int
    format: str = "exr"                      # channel image extension
    origin_m: tuple = (0.0, 0.0, 0.0)
    up_axis: str = "y"
    channels: dict = field(default_factory=dict)   # name -> ChannelInfo
    provenance: dict = field(default_factory=dict)
    version: int = _VERSION
    _dir: str = ""                           # directory the manifest was loaded from (not serialized)

    # ---- write -------------------------------------------------------------
    @classmethod
    def write(cls, path, *, extent_m, dim, channels, format="exr",
              origin_m=(0.0, 0.0, 0.0), up_axis="y", provenance=None):
        """Serialize a manifest JSON to `path`. `channels` maps name -> dict with
        keys file/semantic/min/max/mean (file is stored as a basename). Returns path."""
        chans = {}
        for name, c in channels.items():
            chans[name] = {
                "file":     os.path.basename(c["file"]),
                "semantic": c.get("semantic", name),
                "min":      float(c.get("min", 0.0)),
                "max":      float(c.get("max", 0.0)),
                "mean":     float(c.get("mean", 0.0)),
            }
        doc = {
            "version":  _VERSION,
            "scale": {
                "extent_m":  float(extent_m),
                "dim":       int(dim),
                "origin_m":  list(origin_m),
                "up_axis":   up_axis,
                "height_anchor": "meters",        # v2: stored heights ARE world meters
            },
            "format":     format,
            "channels":   chans,
            "provenance": dict(provenance or {}),
        }
        with open(path, "w") as f:
            json.dump(doc, f, indent=2)
        return path

    # ---- read --------------------------------------------------------------
    @classmethod
    def load(cls, path):
        """Load a manifest JSON. Channel `file`s resolve relative to the manifest dir."""
        with open(path, "r") as f:
            doc = json.load(f)
        sc = doc.get("scale", {})
        chans = {n: ChannelInfo(file=c["file"], semantic=c.get("semantic", n),
                                min=c.get("min", 0.0), max=c.get("max", 0.0),
                                mean=c.get("mean", 0.0))
                 for n, c in doc.get("channels", {}).items()}
        m = cls(
            extent_m=float(sc.get("extent_m", 1.0)),
            dim=int(sc.get("dim", 0)),
            format=doc.get("format", "exr"),
            origin_m=tuple(sc.get("origin_m", (0.0, 0.0, 0.0))),
            up_axis=sc.get("up_axis", "y"),
            channels=chans,
            provenance=doc.get("provenance", {}),
            version=int(doc.get("version", _VERSION)),
        )
        m._dir = os.path.dirname(os.path.abspath(path))
        return m

    # ---- consumer helpers --------------------------------------------------
    def channel_path(self, name):
        """Absolute path to a channel image (resolved relative to the manifest dir)."""
        return os.path.join(self._dir, self.channels[name].file)

    def world_to_uv(self, x, z):
        """World XZ -> [0,1] uv (origin-centered, the bake's sampling convention)."""
        ox, _, oz = self.origin_m
        return ((x - ox) / self.extent_m + 0.5, (z - oz) / self.extent_m + 0.5)

    def uv_to_world(self, u, v):
        ox, _, oz = self.origin_m
        return (ox + (u - 0.5) * self.extent_m, oz + (v - 0.5) * self.extent_m)

    def graph_value(self, h, channel="height"):
        """v2 identity: stored values ARE the graph values (true meters); kept for the
        cross-bake metric callers that predate natural units."""
        return h


__all__ = ["TerrainManifest", "ChannelInfo"]
