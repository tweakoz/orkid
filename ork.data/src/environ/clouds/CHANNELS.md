# Cloud-layer texture contract (SKYLIGHT lane B4)

Binding spec for the textures consumed by the `FWD_SKYBOX_PROC` cloud shells
(pbrtools.i2 "CLOUD LAYER SEAM (slice B3)") and by the ground cloud-shadow
projection (spec §4.B step 6). Baked by `bake_cloud_layers.py` in this
directory (deterministic, seeded; rebake = rerun the script).

## Shipped textures (ork.data/platform_lev2/textures/)

| file | layer | suggested shell altitude | morphology |
|---|---|---|---|
| `clouds_cirrus_1024.png` | high cirrus | 8–11 km | fibrous wind-aligned strands (LIC smear + wavy warp) |
| `clouds_altocumulus_1024.png` | mid altocumulus | 3–5 km | cellular mackerel cloudlets (periodic Worley + undulatus rows) |
| `clouds_cumulus_1024.png` | low broken cumulus | 1–2 km | billowy lobed masses (2-stage domain-warped fBm + Worley puff erosion) |
| `clouds_cumulus_2048.png` | same layer, 2K variant | 1–2 km | identical params, finer sampling — for owner A/B |

All textures tile seamlessly (periodic synthesis by construction; verified
numerically and visually — see the contact sheets).

## Channel semantics (RGBA8)

- **R — coverage rank field.** EXACTLY histogram-equalized: uniform marginal
  distribution. Therefore the artist coverage-threshold knob `t` is linear in
  sky fraction: thresholding at `t` covers exactly `(1 - t)` of the tile.
  `t=1` clear sky, `t=0` full overcast, morphology (level sets) fixed across
  the whole sweep. This is a RANK field, not physical density — opacity/
  density should be derived in-shader from the remapped value, e.g.
  `cov = smoothstep(t, t + softness, R_eroded)`.
- **G — core-ness / optical-thickness proxy.** 0 at feathered edges, 1 at the
  densest cores (periodic gaussian blur of R, normalized). Use for per-layer
  sun-transmittance tinting (darker/warmer cores), cumulus base darkening,
  and as the inverse driver of the silver-lining term (silver lining peaks
  where G is low and coverage is marginal).
- **B — high-frequency erosion detail.** Tileable HF field (per-layer
  character: anisotropic ridged for cirrus, fine Worley for alto/cumulus).
  Intended use: erode the coverage boundary before thresholding, e.g.
  `R_eroded = R - erode_amt * B * edge_proximity` — gives crinkly silhouettes
  that animate when B is scrolled at a slightly different rate than R
  (recommend ~1.1–1.3x the layer wind rate for cheap evolution).
- **A — coverage-rank LOW byte (jul25; was 255/reserved).** R+A pack the rank
  at 16 bits: `rank = (256*R + A)/257` in-shader. The pack is LINEAR in both
  channels, so bilinear filtering reconstructs the 16-bit field exactly —
  eliminates the 8-bit threshold terracing the owner flagged. Consumers
  reading R alone still get the top 8 bits (backward compatible).

## Conventions and suggested per-layer params

- **Wind axis:** cirrus strands are aligned to **+U**. The layer's UV scroll
  (wind) vector should run along +U for cirrus, or the UV frame rotated so
  texture-U tracks the wind direction. Alto/cumulus are near-isotropic.
- **Tile world-size** (sets cloudlet scale; tune to taste): cirrus ~30 km,
  altocumulus ~8 km (cells ≈ 350 m), cumulus ~12 km (masses ≈ 1.5–3 km,
  puff lobes ≈ 300–800 m).
- **Threshold sweep previews** in the contact sheets use
  `soft = 0.035`, `erode = 0.10` — good starting shader defaults.
- The SAME R channel (same threshold remap, same wind offset) is the ground
  cloud-shadow mask; G can attenuate shadow density.

## Rebake

```
~/.staging-jul24/pyvenv/bin/python bake_cloud_layers.py \
    --outdir <ork.data/platform_lev2/textures> \
    --review <contact-sheet dir> --cumulus2048
```

All look decisions are named params in `PARAMS` at the top of the script
(A8: nothing constant-folded). Same params + seeds = byte-identical bake.
When the hypersyn `ptex2d` family lands, each layer graph ports 1:1
(periodic fBm / Worley / domain-warp / equalize ops — see feature-request
note in the lane report).
