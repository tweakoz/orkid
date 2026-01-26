#!/usr/bin/env ork.python

################################################################################
# componentized application which captures a movie of a multi-scene setup with audio
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import argparse, time, os, math

from obt import host

from ork.app.application import ComponentizedApplication
from ork.app.movie_capture import MovieCaptureComponent
from ork.app.testlib.multiscene1 import MultiScene1Component
from ork.app.testlib.lfodrone import LfoDroneComponent

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

parser = argparse.ArgumentParser()
parser.add_argument('--freerun', '-f', action='store_true', help='Enable freerun mode (async), no movie generated..')
parser.add_argument("--fps", "-F", type=float, default=60.0, help="Set target FPS")
parser.add_argument("--length", "-l", type=float, default=10.0, help="length of movie in seconds")
parser.add_argument("--preset", "-p", type=str, default="high", help="Encoder preset (fast, medium, high, ultra)")
parser.add_argument("--outputpath", "-o", type=str, default="/tmp/str_audio_test_movie.mp4", help="Output path for movie")
parser.add_argument("--hires", "-H", action='store_true', help="4K render, 8k textures")
args = parser.parse_args()

################################################################################

class ComplexMovieApp(ComponentizedApplication):

  #########################################################
  
  def __init__(self):
    super().__init__()

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

    self.multiscene = self.addComponent("multiscene1", 
                                        MultiScene1Component,
                                        use_8k_textures = args.hires )

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
                                   NUMFRAMES = int(self.FPS * args.length),
                                   PRESET = args.preset )


    ########################################
    # lockstep mode ?, use STREAM audio device (for movie capture)
    ########################################

    W = 1600 if self.freerun else (3840 if args.hires else 1920)
    H = 900  if self.freerun else (2160 if args.hires else 1080)

    self.createEzApp(
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
        height=H,
        use_subsystems=['opq', 'core', 'gpu', 'lev2']
    )
    
  #########################################################
  # hook up audio analyzer to main bus
  #   and display in panel 0
  #########################################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    lg_group = self.multiscene.lg_group
    mainbus = self.lfodrone.synth.outputBus("main")
    mainbus_source = mainbus.createScopeSource()

    analyzer_lgroup = lg_group.makeChild( uiclass = lev2.singularity.SpectrumAnalyzer,
                                          args = ["MAINBUS"] )

    analyzer = lg_group.getUserVar("analyzers.MAINBUS")
    mainbus_source.connect(analyzer.sink)

    panel0_layout = self.multiscene.panels[0].griditem.layout
    lg_group.replaceChild(panel0_layout,analyzer_lgroup)

    analyzer_layout = analyzer_lgroup.layout

    panel1_layout = self.multiscene.panels[1].griditem.layout
    panel3 = self.multiscene.panels[3].griditem
    panel3_layout = panel3.layout

    lg_group.findGuideBetween(analyzer_layout,panel1_layout).proportion = 0.65
    lg_group.findGuideBetween(panel3_layout,panel1_layout).proportion = 0.65

    g_top = panel3_layout.top
    g_bot = panel3_layout.bottom
    g_lft = panel3_layout.left
    g_rht = panel3_layout.right
    g_top2 = panel3_layout.offsetHorizontalGuide(g_bot, -128, locked=True )
    g_bot2 = panel3_layout.offsetHorizontalGuide(g_bot, -8, locked=True )    
    g_lft2 = panel3_layout.offsetVerticalGuide(g_lft, 8, locked=True )
    g_rhr2 = panel3_layout.offsetVerticalGuide(g_lft, 256, locked=True )    
    lg_panel3 = lg_group.makeChild( uiclass = lev2.ui.EvTestBox, args = ["PANEL3LG",vec4(1)] )
    #lg_panel3.layout.setProportionalRect(panel3_layout,0.25,0.25,0.25,0.25)
    lg_panel3.layout.setRect(left=g_lft2,
                             right=g_rhr2,
                             top=g_top2,
                             bottom=g_bot2)
    lg_panel3.widget.blendingBG = tokens.INVERSE_SUBTRACTIVE
    lg_panel3.widget.blendingFG = tokens.ALPHA
    lg_panel3.widget.normal_color = vec4(0.75,0.75,0.75,1)
    lg_panel3.widget.font_color = vec4(1,1,1,1)
    lg_panel3.widget.theme = tokens.highc_box
    self.ezapp.uicontext.debug_event_routing = True
    theme_engine = self.ezapp.uicontext.theme_engine
    styledb = theme_engine.styledb
    style = styledb.getStyle(tokens.highc_box)
    self.style = style
    #lg_panel3.widget.clear = False
    #vpak = lg_panel3.widget.makeChild( uiclass=lev2.ui.VerticalPack,
    #                            args=["VPACK"] )
    #vpak.widget.item_height = 28
    #cbox2 = vpak.widget.makeChild( uiclass = lev2.ui.Checkbox,
    #                               args = ["CHK2",vec3(0.25,0.25,0.30)] )

    #cbox1.layout.left.anchorTo(lg_panel3.layout.left)
    #cbox1.layout.right.anchorTo(lg_panel3.layout.right)
    #cbox1.layout.top.anchorTo(lg_panel3.layout.top)
    #cbox1.layout.bottom.anchorTo(lg_panel3.layout.bottom)
    #cbox1.layout
    #cbox1.widget.setSize(96,24)
    #cbox2.widget.setSize(96,24)
    #cbox1.widget.setPos(8,8)
    #cbox2.setPos(8,8+24+2)
    
  def onGpuUpdate(self,ctx):
    super().onGpuUpdate(ctx)
    #self.style->_border_width = 12 + 8 * abs( math.sin( self.ezapp.timeSeconds() * 2.0 ) )
    bw = 6 + 4 * abs( math.sin( self.absolutetime * 2.0 ) )
    cr = 32 * abs( math.cos( self.absolutetime * 3.5 ) )
    r = 0.5 + 0.5 * math.sin( self.absolutetime * 1.0 )
    g = 0.5 + 0.5 * math.sin( self.absolutetime * 1.3 + 2.0 )
    b = 0.5 + 0.5 * math.sin( self.absolutetime * 1.7 + 4.0 )
    a = 0.5 + 0.5 * math.sin( self.absolutetime * 2.3 + 6.0 )
    
    br = 0.5 + 0.5 * math.sin( self.absolutetime * 0.9 + 1.0 )
    bg = 0.5 + 0.5 * math.sin( self.absolutetime * 1.1 + 3.0 )
    bb = 0.5 + 0.5 * math.sin( self.absolutetime * 1.4 + 5.0 )
    ba = 0.5 + 0.5 * math.sin( self.absolutetime * 1.9 + 7.0 )
    blend = int((0.5 + 0.5 * math.sin( self.absolutetime * 0.7 )) * 4.0)
    self.style.border_width = int(bw)
    self.style.corner_radius = int(cr)   
    self.style.bg_color = vec4(r,g,b,a)   
    self.style.border_color = vec4(br,bg,bb,ba)
    match blend:
      case 0:
        self.style.blend_mode = tokens.ALPHA
      case 1:
        self.style.blend_mode = tokens.ALPHA_ADDITIVE
      case 2:
        self.style.blend_mode = tokens.ALPHA_SUBTRACTIVE
      case 3:
        self.style.blend_mode = tokens.ALPHA_SUBTRACTIVE
      case 4:
        self.style.blend_mode = tokens.ALPHA_MODULATE
        
###############################################################################

app = ComplexMovieApp()
app.ezapp.mainThreadLoop(on_iter=lambda : False)
app.ezapp.shutdown()

if host.IsOsx and not app.freerun:
  time.sleep(1)
  os.system(f"open {args.outputpath}")
