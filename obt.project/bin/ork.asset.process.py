#!/usr/bin/env python3

import sys, time, signal
import os, argparse
from orkengine.core import *
from orkengine.lev2 import *

tokens = CrcStringProxy()

ezapp = lev2appinit()
#ctx = GfxEnv.loadingContext()
#ctx.makeCurrent()

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
envmap_assets_to_process = [
  "src://envmaps/blender_city.dds",
  "src://envmaps/blender_courtyard.dds",
  "src://envmaps/blender_forest.dds",
  "src://envmaps/blender_interior.dds",
  "src://envmaps/blender_night.dds",
  "src://envmaps/blender_studio.dds",
  "src://envmaps/blender_sunrise.dds",
  "src://envmaps/blender_sunset.dds",
  "src://envmaps/tozenv_basic.dds",
  "src://envmaps/tozenv_caustic1.png",
  "src://envmaps/tozenv_nebula.png",
]
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
irr_maps = dict()
for item in envmap_assets_to_process:
  X = PbrCommon.requestIrradianceMaps(str(item))
  irr_maps[item] = X
#######################################
ok_to_exit = False

def onCtrlC(signum, frame):
  global ok_to_exit
  print("exiting....")
  ok_to_exit = True
  
#######################################

signal.signal(signal.SIGINT, onCtrlC)
previous_time = time.time()
while ( (not ok_to_exit) ):
  time.sleep(0.1)
  ezapp.processMainSerialQueue()
  current_time = time.time()
  delta_time = current_time - previous_time
  if delta_time > 3.0:
    print("asset_count<%d>" % asset_count)
    previous_time = current_time
