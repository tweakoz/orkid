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
args = parser.parse_args()

################################################################################

app = EcsEditor(scene=args.scene, fullscreen=args.fullscreen, ssaa=args.ssaa)
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
