#!/usr/bin/env ork.python

################################################################################
# ECS Scene Editor Launcher
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
################################################################################

import argparse
from ork.editor.ecsedit import EcsEditor

################################################################################

parser = argparse.ArgumentParser(description="ECS Scene Editor")
parser.add_argument("--scene", "-s", type=str, help="Scene file to load on startup (.json)")
parser.add_argument("--fullscreen", "-f", action="store_true", help="Launch in fullscreen mode")
parser.add_argument("--ssaa", type=int, default=0, help="SSAA supersampling level (0=off, 1=2x, 2=3x, 3=4x)")
parser.add_argument("-W", "--width", type=int, default=0, help="initial window width when not fullscreen (0=default)")
parser.add_argument("-H", "--height", type=int, default=0, help="initial window height when not fullscreen (0=default)")
parser.add_argument("--hidpi", action="store_true", help="render at the display's backing (Retina) scale; default is LoDPI to save fillrate")
args = parser.parse_args()

################################################################################

ee_kwargs = dict(scene=args.scene, fullscreen=args.fullscreen, ssaa=args.ssaa, hidpi=args.hidpi)
if args.width > 0:
  ee_kwargs["width"] = args.width
if args.height > 0:
  ee_kwargs["height"] = args.height
app = EcsEditor(**ee_kwargs)
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
