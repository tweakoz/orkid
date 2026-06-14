#!/usr/bin/env ork.python
###############################################################################
# ork.terrain.sweep.py — bake a PARAM SWEEP of a terrain DSL in ONE process and
# assemble a labeled contact sheet. Because the erox shaders read their scalars
# from a params SSBO (the text is param-independent), the disk shader cache hits
# after the first bake -> sweeping a knob does NOT recompile, only re-dispatches.
#
#   ork.terrain.sweep.py erox --param capacity_Kc --values 0.5,1,2,4
#   ork.terrain.sweep.py erox --param sim_time_s  --values 30,60,120,240 \
#                             --dim 384 --height-scale 1024 -p capacity_Kc=2
###############################################################################
import argparse, sys, math, os
from pathlib import Path

from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2
from orkengine import ecs

from ork.hypergraph.ecs.scene.assets import HeightField


def _parse_param(s):
    if "=" not in s:
        raise ValueError(f"bad -p {s!r}; want KEY=VALUE")
    k, v = s.split("=", 1)
    try:
        v = int(v)
    except ValueError:
        try:
            v = float(v)
        except ValueError:
            pass
    return k.strip(), v


def parse_args():
    p = argparse.ArgumentParser(description="terrain DSL param sweep -> contact sheet")
    p.add_argument("dsl_file", help="terrain DSL .py name (e.g. erox)")
    p.add_argument("--class", dest="class_name", default=None, help="explicit HeightField subclass")
    p.add_argument("--param", required=True, help="the DSL kwarg to sweep")
    p.add_argument("--values", required=True, help="comma-separated values (e.g. 0.5,1,2,4)")
    p.add_argument("--dim", "-d", type=int, default=384, help="bake grid resolution (default 384 for speed)")
    p.add_argument("--mpt", type=float, default=8.0, help="horizontal meters per texel (default 8); extent=dim*mpt unless --extent")
    p.add_argument("--extent", type=float, default=None, help="horizontal world size (meters); overrides --mpt")
    p.add_argument("--height-scale", dest="height_scale", type=float, default=1024.0,
                   help="meters that normalized height 1.0 represents (default 1024 so erosion is visible)")
    p.add_argument("-p", "--fixed", action="append", dest="fixed", default=[], metavar="KEY=VALUE",
                   help="fixed DSL kwarg held across the sweep (repeatable)")
    p.add_argument("--thumb", type=int, default=320, help="contact-sheet thumbnail size")
    p.add_argument("--out", default=None, help="contact-sheet output path (png)")
    p.add_argument("--no-open", action="store_true", help="don't open the contact sheet")
    return p.parse_args()


def _coerce(v):
    try:
        return int(v)
    except ValueError:
        return float(v)


def _thumb_from_png(path, sz):
    """16-bit height PNG -> per-image-normalized 8-bit thumbnail (numpy + PIL)."""
    import numpy as np
    from PIL import Image
    a = np.asarray(Image.open(path)).astype(np.float64)
    if a.ndim == 3:
        a = a[:, :, 0]
    lo, hi = a.min(), a.max()
    a = (a - lo) / (hi - lo + 1e-9)
    im = Image.fromarray((a * 255).astype("uint8")).resize((sz, sz), Image.BILINEAR)
    return im.convert("RGB")


def _contact_sheet(thumbs, labels, sz, out_path):
    from PIL import Image, ImageDraw
    n = len(thumbs)
    cols = int(math.ceil(math.sqrt(n)))
    rows = int(math.ceil(n / cols))
    barh = 22
    pad = 6
    cw, ch = sz + pad, sz + barh + pad
    sheet = Image.new("RGB", (cols * cw + pad, rows * ch + pad), (24, 24, 28))
    drw = ImageDraw.Draw(sheet)
    for i, (im, lab) in enumerate(zip(thumbs, labels)):
        r, c = divmod(i, cols)
        x, y = pad + c * cw, pad + r * ch
        sheet.paste(im, (x, y + barh))
        drw.rectangle([x, y, x + sz, y + barh], fill=(40, 40, 48))
        drw.text((x + 5, y + 5), lab, fill=(230, 230, 120))
    sheet.save(out_path)


def main():
    args = parse_args()
    values = [_coerce(v.strip()) for v in args.values.split(",") if v.strip()]
    try:
        fixed = dict(_parse_param(s) for s in args.fixed)
    except ValueError as e:
        print(f"terrain sweep: {e}", file=sys.stderr)
        return 2
    name = Path(args.dsl_file).stem

    ezapp = ecs.headless_appinit(use_subsystems=['opq', 'core', 'gpu', 'lev2'])
    ezapp.mainThreadBegin()
    ctx = ezapp.bindGfxToCurrentThread()
    assert ctx, "bindGfxToCurrentThread() returned null"

    thumbs, labels = [], []
    print(f"terrain sweep: {name}.{args.param} over {values} (dim={args.dim})", flush=True)
    for v in values:
        kwargs = dict(fixed)
        kwargs[args.param] = v
        print(f"  bake {args.param}={v} {fixed or ''}", flush=True)
        hf = HeightField(dsl_file=args.dsl_file, dsl_class=args.class_name,
                         dimension=args.dim, extent_m=(args.extent if args.extent is not None else args.dim * args.mpt),
                         height_scale_m=args.height_scale, ctx=ctx, **kwargs)
        hf.gendata.asset_name = f"sweep_{name}"
        art = hf.build(ext="png")
        thumbs.append(_thumb_from_png(art["height"], args.thumb))
        labels.append(f"{args.param}={v}")

    ezapp.mainThreadEnd()
    ecs.headless_exit()

    outdir = str(core.Path.expandPathString(f"<assetcache>/terrain/sweep_{name}"))
    out = args.out or os.path.join(outdir, f"sheet_{args.param}.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    _contact_sheet(thumbs, labels, args.thumb, out)
    print(f"terrain sweep: wrote {out}", flush=True)
    if not args.no_open:
        from obt import command
        command.run(["open", out])
    return 0


if __name__ == "__main__":
    sys.exit(main())
