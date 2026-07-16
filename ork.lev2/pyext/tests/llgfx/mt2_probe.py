#!/usr/bin/env ork.python
import os; os.environ["PYTHONUNBUFFERED"]="1"
import sys; sys.stdout.reconfigure(line_buffering=True)
from orkengine import core, lev2, ecs
import time
SRC=os.environ.get("MT2_SRC_HDR","")  # any equirect .hdr envmap source
if not SRC: sys.exit("mt2_probe: set MT2_SRC_HDR to an .hdr envmap path")
MODE=sys.argv[1] if len(sys.argv)>1 else "burst"
ez=ecs.headless_appinit(use_subsystems=['opq','core','gpu','lev2']); ez.mainThreadBegin()
ctx=ez.bindGfxToCurrentThread(); assert ctx
print(f"MODE={MODE} ctx ok",flush=True)
t0=time.time()
if MODE=="burst":
  ok=lev2.EnvMapProcessor.processToXIR(SRC,"/tmp/mt2_burst.xir")
  print(f"BURST done ok={ok} t={time.time()-t0:.1f}s bytes={os.path.getsize('/tmp/mt2_burst.xir') if os.path.exists('/tmp/mt2_burst.xir') else 0}",flush=True)
else:
  ok=lev2.EnvMapProcessor.processToXIRViaMicrotask(SRC,"/tmp/mt2_micro.xir")
  print(f"MICRO done ok={ok} t={time.time()-t0:.1f}s bytes={os.path.getsize('/tmp/mt2_micro.xir') if os.path.exists('/tmp/mt2_micro.xir') else 0}",flush=True)
print("teardown",flush=True)
ez.mainThreadEnd(); ecs.headless_exit()
