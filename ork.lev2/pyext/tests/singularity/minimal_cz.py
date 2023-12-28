#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI with four views to the same scenegraph to a window
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, math, random, numpy, obt.path, time, bisect
import plotly.graph_objects as go
from collections import defaultdict
import re
from orkengine.core import *
from orkengine.lev2 import *
################################################################################
sys.path.append((thisdir()/"..").normalized.as_string) # add parent dir to path
from _boilerplate import *
from singularity._harness import SingulTestApp, find_index
################################################################################

class Cz1App(SingulTestApp):

  def __init__(self):
    super().__init__()
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    self.syn_data_base = singularity.baseDataPath()/"casioCZ"
    self.czdata = singularity.CzSynthData()
    #self.czdata.loadBank("5th", self.syn_data_base/"ACIDPEDAL1.syx")
    #self.czdata.loadBank("fx", self.syn_data_base/"fx.bnk")
    #self.czdata.loadBank("dxbank", self.syn_data_base/"dxbank.bnk")
    #self.czdata.loadBank("jxbank", self.syn_data_base/"jxbank.bnk")
    self.czdata.loadBank("bankA", self.syn_data_base/"factoryA.bnk")
    self.czdata.loadBank("bankB", self.syn_data_base/"factoryB.bnk")
    self.czdata.loadBank("bank0", self.syn_data_base/"cz1_1.bnk")
    self.czdata.loadBank("bank1", self.syn_data_base/"cz1_2.bnk")
    self.czdata.loadBank("bank2", self.syn_data_base/"cz1_3.bnk")
    self.czdata.loadBank("bank3", self.syn_data_base/"cz1_4.bnk")
    self.soundbank = self.czdata.bankData
    self.czprogs = self.soundbank.programsByName
    self.sorted_progs = sorted(self.czprogs.keys())

    print("czprogs<%s>" % self.czprogs)
    self.mylist = [
      "Bells and Chimes",
      "ORCHESTRA",
      "Sizzle Cymbal"
    ]
    # find index of "Bells and Chimes" in sorted_progs
    self.prog_index = find_index(self.sorted_progs, "Casio Toms")
    print("prog_index<%d>" % self.prog_index)

  ##############################################

  def genMods(self):
    timebase = self.time
    modrate = math.sin(self.time)*5
    #########
    def modulatePan():
      return math.sin((self.time-timebase)*modrate)*2
    #########
    def sub(name,value):
      channel = name #"%g.%s" % (timebase,name)
      if channel not in self.charts:
        self.charts[channel] = dict()
        self.chart_events[channel] = dict()
      TIME = self.time-timebase
      if type(value) == vec4:
        self.charts[channel][TIME] = value.x
      elif type(value) == str:
        if time not in self.chart_events[channel].keys():
          self.chart_events[channel][TIME] = []
          self.chart_events[channel][TIME] += [value]
          #print("TIME<%g> event<%s>" % (TIME,value))
    #########
    mods = singularity.KeyOnModifiers()
    mods.generators = {
    "LCZX0.STEREOPAN2": modulatePan
    }
    mods.subscribers = {
    "LCZX0.DCAENV0": sub,
    #"LCZX0.DCAENV1": sub
    }
    return mods

  ##############################################

  def showCharts(self):
    fig = go.Figure()
    color_map = {
    "LCZX0.DCAENV0": "red",
    "LCZX0.DCAENV1": "blue",
    "LCZX0.STEREOPAN2": "green"
    }
    default_color = 'gray'
    for name, the_dict in self.charts.items():
      sorted_data = sorted(the_dict.items())
      times, values = zip(*sorted_data)
      line_color = color_map.get(name, default_color)
      fig.add_trace(go.Scatter(x=times, 
                               y=values, 
                               mode='lines', 
                               name=name, 
                               line=dict(color=line_color)))
    ypos = 0
    for name, the_dict in self.chart_events.items():
      # Add an annotation at each event timestamp
      for item in the_dict.items():
        timestamp, event_names = item
        line_color = color_map.get(name, default_color)
        for en in event_names:
          fig.add_annotation(
            x=timestamp, y=ypos, # Position of the annotation
            text=en,  # Event name
            showarrow=False,
            yshift=10,
            xanchor='center',
            font=dict(color=line_color)
          )
          fig.add_shape(
                type="line",
                x0=timestamp, y0=0, x1=timestamp, y1=1, # Line coordinates
                line=dict(color="yellow", width=1)
          )
          ypos += 0.03
    ypos += 0.07
    fig.update_layout({
        'plot_bgcolor': 'rgba(0, 0, 0, 0)',  # Transparent background within the plot area
        'paper_bgcolor': 'rgb(10, 10, 10)',  # Background color outside the plot area
        'font': {
            'color': 'white'  # Text color
        },
        'xaxis': {
            'gridcolor': 'rgb(40, 40, 40)'  # Grid line color for X-axis
        },
        'yaxis': {
            'gridcolor': 'rgb(40, 40, 40)'  # Grid line color for Y-axis
        }
    })
    fig.show()
    self.charts = dict()
    self.chart_events = dict()
    pass

  ##############################################

###############################################################################

app = Cz1App()
app.ezapp.mainThreadLoop()
