#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import argparse, time, os

from obt import host

from ork.app.application import ComponentizedApplication
from ork.app.movie_capture import MovieCaptureComponent
from ork.app.testlib.multiscene1 import MultiScene1Component
from ork.app.testlib.lfodrone import LfoDroneComponent

from orkengine import core, lev2

################################################################################

parser = argparse.ArgumentParser()
parser.add_argument('--freerun', '-f', action='store_true', help='Enable freerun mode (async), no movie generated..')
parser.add_argument("--fps", "-F", type=float, default=60.0, help="Set target FPS")
parser.add_argument("--length", "-l", type=float, default=10.0, help="length of movie in seconds")
parser.add_argument("--preset", "-p", type=str, default="high", help="Encoder preset (fast, medium, high, ultra)")
parser.add_argument("--outputpath", "-o", type=str, default="/tmp/str_audio_test_movie.mp4", help="Output path for movie")
args = parser.parse_args()

################################################################################

class ComplexMovieApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()

    self.absolutetime = 0.0
    self.freerun = args.freerun

    if self.freerun:
      self.FPS = 120.0
      self.UPS = 360.0
    else:
      # these need to match for lockstep mode (for now)
      self.FPS = args.fps     # frames per second
      self.UPS = args.fps     # frames per second

    ########################################
    # multiscene component (4 viewports, 3 scenes, 1 ui)
    ########################################

    self.multiscene = self.addComponent("multiscene1", MultiScene1Component, show_ui_panel=True )

    ########################################
    # lfo drone synth component (for audio test tone)
    ########################################

    self.lfodrone = self.addComponent("lfodrone", LfoDroneComponent )

    ########################################
    # movie component (for movie capture)
    ########################################

    if not self.freerun:
      self.mov = self.addComponent("movie",
                                   MovieCaptureComponent,
                                   OUTPATH = args.outputpath,
                                   LEN = args.length,  # seconds
                                   FPS = self.FPS,     # frames per second
                                   NUMFRAMES = int(self.FPS * args.length))


    ########################################
    # lockstep mode ?, use STREAM audio device (for movie capture)
    ########################################

    W = 1280 if self.freerun else 1920
    H = 720  if self.freerun else 1080

    self.ezapp = lev2.OrkEzApp.create(
        self,
        enable_lockstep_ups = True,
        enable_lockstep_fps = True,
        enable_freerun_ups = True,
        enable_freerun_fps = True,
        enable_audio_synth=True,
        audio_stream_sync=not self.freerun,
        enable_graphics=True,
        freerun=self.freerun,
        target_ups = self.UPS,
        target_fps = self.FPS,
        width=W,
        height=H
    )
    
    
###############################################################################

app = ComplexMovieApp()
app.ezapp.mainThreadLoop(on_iter=lambda : False)

if host.IsOsx and not app.freerun:
  time.sleep(1)
  os.system(f"open {args.outputpath}")
