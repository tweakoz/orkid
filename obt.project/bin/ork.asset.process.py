#!/usr/bin/env python3

import sys, time
import os, argparse
from orkengine.core import *
from orkengine.lev2 import *

tokens = CrcStringProxy()

ezapp = lev2appinit()
ctx = GfxEnv.loadingContext()
ctx.makeCurrent()

#######################################
# todo : dynamic enumeration of assets
########################################
assets_to_process = [
  "lev2://textures/Inconsolata12.png",
  "lev2://textures/Inconsolata13.png",
  "lev2://textures/Inconsolata14.png",
  "lev2://textures/Inconsolata15.png",
  "lev2://textures/Inconsolata16.png",
  "lev2://textures/Inconsolata24.png",
  "lev2://textures/Inconsolata32.png",
  "lev2://textures/Inconsolata48.png",
  "lev2://textures/dflayer.png",
  "lev2://textures/dflayersel.png",
  "data://tests/misc_gltf_samples/DamagedHelmet.glb",
]
asset_count = len(assets_to_process)
#######################################
def on_event(loadreq,evcode,data):
  if evcode == tokens.loadComplete.hashed:
    global asset_count
    asset_count -= 1
    if loadreq.assetPath in assets_to_process:
      assets_to_process.remove(loadreq.assetPath)
    print("rem<%d> COMPLETE: %s" % (asset_count,loadreq.assetPath))
    print(assets_to_process)
#######################################
for item in assets_to_process:
  asset.enqueueLoad(
    path = item,
    vars = {},
    onEvent = on_event
  )
#######################################
while asset_count>0:
  lev2apppoll()
