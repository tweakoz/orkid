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

class KrzApp(SingulTestApp):

  def __init__(self):
    super().__init__()
  
  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    self.syn_data_base = singularity.baseDataPath()/"kurzweil"
    self.krzdata = singularity.KrzSynthData(base_objects=True)
    
    self.soundbank = self.krzdata.bankData
    self.krzprogs = self.soundbank.programsByName
    self.sorted_progs = sorted(self.krzprogs.keys())

    ok_dict = {
    "20's_Trumpet": 0,
    "2000_Odyssey": 6,
    "2nd_Oboe": 0,
    "2nd_Violin": -6,
    "3rd_World_Order": 6,
    "40_Something": 0,
    "5_8ve_Percussion": 0,
    "7th_World_String": 0,
    "AC_Dream": 0,
    "A.Bass+Ride_Pno": 0,
    "AcousGtr+Strings": 0,
    "Acous_12_String": 6,
    "Acoustic_Bass": 6,
    "Alien_Industry": 0,
    "All_In_The_Fader": -21,
    "Ambient_Bells": 0,
    "Analog__Brazz": 0,
    "Anna_Mini": 6,
    "Arco_Dbl_Bass": 0,
    "Attack_Stack": -6,
    "Auto_Percussion": 0,
    "Auto_Wavesession": 0,
    "Bad_Clav": 0,
    "Bamboo_Voices": 6,
    "BaroqueOrchestra": 0,
    "Baroque_Strings": 0,
    "Baroque_Strg_Ens": 0,
    "Batman_Strings": 0,
    "Big_Drum_Corp": 0,
    "Bell_Players": 6,
    "BrazKnuckles": -21,
    "Bright_Piano": 0,
    "Brite_Klav": 6,
    "Cartoon_Perc": 3,
    "Cee_Tuar": -18,
    "Chamber_Section": 0,
    "Chimes": 0,
    "Choral_Sleigh": 6,
    "Chorus_Gtr": -18,
    "Church_Bells": 0,
    "Classical_Gtr": 0,
    "Click": 0,
    "Clock_S+H_Lead": 6,
    "Copland_Sft_Trp": 6,
    "Crowd_Stomper_": 0,
    "CowGogiBell": 0,
    "Default_Program": 0,
    "DigThat_DC_Lead": 0,
    "DigiBass": 0,
    "Dog_Chases_Tail": -15,
    "Doomsday": -3,
    "Dual_Bass": -3,
    "Dual_Tri_Bass": 0,
    "Dubb_Bass": -6,
    "Dyn_Brass_+_Horn": 0,
    "Dyn_Hi_Brass": 0,
    "Dyn_Lo_Brass": 0,
    "Dyno_Synbrass": 0,
    "Dynamic_Harp": 0,
    "Dyno_EP_Lead": 0,
    "Econo_Kit_": 0,
    "EDrum_Kit_2_": 0,
    "Elect_12_String": 12,
    "Ethereal_Echoes": 0,
    "Ethereal_Strings": -6,
    "Ethnoo_Lead": -6,
    "Eye_Saw": 0,
    "Fair_Breath": 0,
    "F_Horn_Con_Sord": 0,
    "Fast_Strings_MW": 0,
    "Fat_Traps_": 0,
    "Finger_Bass": 6,
    "Flatliners": 6,
    "French_Horn_Sec1": 0,
    "French_Horn_Sec2": 0,
    "Fluid_Koto": 0,
    "Fun_Delay_Square": -6,
    "General_MIDI_kit": 0,
    "Glass_Web": 0,
    "Glassy_Eyes": 6,
    "Glistener": 0,
    "Grand_Strings": 0,
    "Gtr_Jazz_Band": -12,
    "Guitar_Mutes_1": 0,
    "Guitar_Mutes_2": 0,
    "Guitar___Flute": 0,
    "Hammeron": 6,
    "Harmon_Section": 0,
    "Harp_Arps": 0,
    "Harp_On_It": -18,
    "Harp_w_8ve_CTL": 0,
    "HelterSkelterGtr": 6,
    "Hi_Res_Sweeper": 0,
    "Hip_Brass": 0,
    "Horn+Flute_w_Str": 0,
    "Hot_Tamali_Kit": 0,
    "House_Bass": 0,
    "Hybrid_Breath": 0,
    "Hybrid_Pan_Flute": 0,
    "Hybrid_Sweep": 6,
    "Islanders": 9,
    "In_The_Well": -12,
    "India_Jam": 0,
    "Jam_Corp": 0,
    "Jazz_Lab_Band": -3,
    "Jazz_Muted_Trp": 6,
    "Jazz_Quartet": 0,
    "Jethro's_Flute": 0,
    "Klakran": 0,
    "Klarinet": -12,
    "Koto_Followers": -12,
    "Kotobira": 0,
    "Kotolin": 0,
    "Low_World_Vox": -15,
    "Lucky_Lead": -18,
    "Lunar_Dance": 0,
    "Mallet_Voice": 0,
    "MarcatoViolin_MW": 0,
    "Marcato_Cello_MW": 0,
    "Marcato_S.Strngs": 0,
    "Mandala": -18,
    "Mbira": 0,
    "Mbira_Stack": 0,
    "Medicine_Man": 0,
    "Mello_Hyb_Brass": 0,
    "Metal_Garden": 0,
    "Miles_Unmuted": 0,
    "Mixed_Choir": 0,
    "Mogue_Bass": 0,
    "Mono_Synth_Bass": 0,
    "My_JayDee": 0,
    "Native_Drum": 0,
    "Oboe+Flute_w_Str": 0,
    "Oh_Bee!!!": -6,
    "Orch_Bassoon": 0,
    "Orch_Clarinet": 0,
    "Orch_EnglishHorn": 0,
    "Orch_Percussion1": 0,
    "Orch_Trumpet": 0,
    "Orch_Viola": 6,
    "Orchestral_Oboe": 0,
    "Orchestral_Pad": 6,
    "Orchestral_Winds": 0,
    "Organarimba": -12,
    "Organellica": -12,
    "Ostinato_Bass": 0,
    "Outside_L_A": 9,
    "ParaKoto": 3,
    "Passion_Source": -15,
    "Pedal_Pipes": -6,
    "Pedal_Steel": -18,
    "Perc_Voices": 18,
    "Perc_Pan_Lead": -15,
    "Piano+SloStrings": 0,
    "Piano___S_String": 0,
    "Pinger": 0,
    "Pipes": 21,
    "Pizzicato_String": 6,
    "PressForThunder!": 0,
    "Press_Evolution": 12,
    "Press_For_Effect": 6,
    #"Preview_Drums": 0,
    "Prs_Koto": 0,
    "Pulsar": 6,
    "Punch_Gate_Kit_": 0,
    "Quillsichord": 0,
    "RainforestCrunch": 78,
    "Ravitar": -3,
    "Real_Drums": 6,
    "Reed_Section": -18,
    "Riding_The_Rails": 0,
    "Ritual_Metals": 0,
    "Rock_Axe": 0,
    "Rock_Axe_mono": 0,
    "Rock_Kit": 9,
    "Sfz_Cres_Brass": -6,
    "Sfz_Strings_MW": 0,
    "Shimmerling": 12,
    "Shadow_Kit": 0,
    "Shudder_Kit_": 0,
    "Sizzl_E_Pno": 0,
    "Skinny_Lead": 6,
    "Slo_Chorus_Gtr": 0,
    "Slo_Solo_Strings": 0,
    "Slo_SynthOrch": 6,
    "Slow_Arco_Bass": -12,
    "Slow_Cello": 3,
    "Slow_Horn": 12,
    "Slow_Viola": 0,
    "Soaring_Brass": -3,
    "Soft_Trumpet": 0,
    "Solo_Bassoon": 0,
    "Solo_Cello": 0,
    "Solo_Clarinet": 0,
    "Solo_EnglishHorn": 0,
    "Solo_Oboe": 6,
    "Solo_Trombone": 0,
    "Solo_Trumpet": 6,
    "Solo_Viola": 0,
    "Solo_Violin": -24,
    "Soprano_Sax": 0,
    "SpaceStation": -36,
    "Stackoid": -36,
    "Steel_Str_Guitar": 0,
    "Stereo_Grand": 0,
    "Stereo_Sweeps": 0,
    "StrataClav": 0,
    "Strataguitar": 0,
    "Straight_Strat": 0,
    "StreetCorner_Sax": 6,
    "Street_Drums": 0,
    "Super_Clav": 0,
    "Sweeper": 6,
    "Sweetar": 18,
    "Sweet_Strings": 0,
    "Syncro_Taps": 18,
    "Talk_Talk": 12,
    "Tangerine": -27,
    "Tenor_Sax": 0,
    "Timpani_Chimes": 0,
    "Timershift": -6,
    "Tine_Elec_Piano": 0,
    "Too_Bad_Bass": 6,
    "TotalCntrl_Orch1": 0,
    "TotalCntrl_Orch2": 0,
    "Touch_Clav": 0,
    "Touch_MiniBass": 0,
    "Touch_StringOrch": 0,
    "Touch_Strings": 0,
    "Toy_Box": 9,
    "Tranquility": -12,
    "Trombone": 30,
    "Trumpet+Bone": 9,
    "Trp+Horns_w_Str": 0,
    "Tuba": 6,
    "Two_Live_Bass": -12,
    "Vectoring": -6,
    "Vibratone": 9,
    "Walking_Bass": -3,
    "Warm_Bass": 6,
    "Waterflute": 9,
    "West_Room_Kit": 6,
    "Wet_Pizz_": 6,
    "Williamsong": 12,
    "Wind_Vox": 6,
    "WonderSynth_Bass": 0,
    "Wood_Bars": 3,
    "Woodwinds_1": 0,
    "Woodwinds_2": -6,
    "Woody_Jam_Rack": 0,
    "World_Rave_Kit": 0,
    "Xylophone": 0
    }
    #ok_dict = {
     # "Ethereal_Echoes": 0,
    #}    
    for item in ok_dict.keys():
      p = self.soundbank.programByName(item)
      p.gainDB = ok_dict[item]
    self.sorted_progs = sorted(ok_dict.keys())
    print("krzprogs<%s>" % ok_dict)    
    ##########################################
    PRG = "Ethereal_Echoes" 
    bus_assignments = {
      "aux1": "Doomsday",
      "aux2": "Slow_Cello",
      "aux3": "Jazz_Quartet",
      "aux4": "Syncro_Taps",
      "aux5": "TotalCntrl_Orch2",
      "aux6": "Touch_Clav",
      "aux7": "WonderSynth_Bass",
      "aux8": "Chamber_Section",
      "aux9": "World_Rave_Kit",
      "main": PRG,
    }
    for item in bus_assignments:
      bus = self.synth.outputBus(item)
      patch_name = bus_assignments[item]
      program = self.soundbank.programByName(patch_name)
      self.setBusProgram(bus, program)
    ##########################################
    self.prog_index = find_index(self.sorted_progs, PRG)
    self.prog = self.soundbank.programByName(PRG)
    self.click_prog = self.soundbank.programByName("Click")
    self.click_noteL = 60
    self.click_noteH = 72
    self.setUiProgram(self.prog)
    self.synth.masterGain = singularity.decibelsToLinear(-36.0)

###############################################################################

app = KrzApp()
app.ezapp.mainThreadLoop()
