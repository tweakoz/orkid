#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI to a window, 
#   delegates some rendering code to PyOpenGL and PyImGui
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

#pip3 install imgui_bundle

import traceback, time, json
import threading, concurrent.futures
import subprocess, os
from PIL import Image
from obt import path as obt_path
from string import Template
################################################################################
from imgui_bundle import imgui, hello_imgui, imgui_md
from imgui_bundle import imgui_color_text_edit as ed
import numpy as np
from OpenGL.GL import *
################################################################################
from orkengine.core import CrcStringProxy, lev2_pyexdir, VarMap
from orkengine.core import vec2, vec3, vec4, quat, mtx3, mtx4
from orkengine import lev2
################################################################################
lev2_pyexdir.addToSysPath()
from lev2utils.imgui import ImGuiWrapper, installImguiOnApp
from lev2utils.pyopengl import PyShader, GeometryBuffer
################################################################################

tokens = CrcStringProxy()
TextEditor = ed.TextEditor

################################################################################
# vertex shader source code
################################################################################

vertex_shader_source = """
#version 410 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
"""

################################################################################
# fragment shader source code
################################################################################

fragment_shader_source = """
#version 410 core
 uniform float time;
 uniform float scale;
 uniform float scale_power;     // range (0.5,2.0,0)
 uniform float rot_scale;       // range (0, 10, 0.5)
 uniform float spin_rate;       // range (-1,1,0.1)
 uniform float spin_bias;       // range (-3.1415,3.1415)
 uniform float cycle_rate;      // range (-4,4,0.1)
 uniform float d_final_scale;
 uniform float d_final_bias;
 uniform vec3 ModColor;
 out vec4 FragColor;
 in vec2 TexCoord;
 void main() {
   vec2 uv = TexCoord;
   vec2 uvc = uv-vec2(0.5,0.5);
   float angle = atan(uvc.x,uvc.y);
   float sina = 0.5+sin(spin_bias+time*spin_rate*-1+angle*rot_scale)*0.5;
   float d = length(uvc)*scale*sina;
   d = sign(scale)*pow(abs(d),scale_power);
   d = mod(d+time*cycle_rate,1.0);
   d = d*d_final_scale + d_final_bias;
   FragColor = vec4(ModColor*d, 1.0);
}"""

#####

SSAA_MULT = 4
SSAA_EXT = SSAA_MULT-2
SSAA_SCALE = 1.0/(SSAA_MULT*SSAA_MULT)

fragment_shader_ssaa_resolve = """
#version 410 core
uniform sampler2D ssaaTex;
in vec2 TexCoord;
out vec4 FragColor;
void main() {

    vec2 texelSize = 1.0 / textureSize(ssaaTex, 0);
    vec2 uv = TexCoord;
    vec3 result = vec3(0.0);
    for(int x = -$EXTENT; x <= $EXTENT; ++x)
    {
        for(int y = -$EXTENT; y <= $EXTENT; ++y)
        {
            vec2 offset = vec2(x, y) * texelSize;
            result += texture(ssaaTex, uv + offset).rgb;
        }
    }
    result *= $SCALE;
    FragColor = vec4(result, 1.0);
}
"""
template = Template(fragment_shader_ssaa_resolve)
fragment_shader_ssaa_resolve = template.substitute( EXTENT = SSAA_EXT, SCALE=SSAA_SCALE )


################################################################################
# Geometry data
################################################################################

extent = 1.0
vertices = np.array([
    # positions        # texture coords
    -extent, -extent, 0.0,   0.0, 0.0,
     extent, -extent, 0.0,   1.0, 0.0,
     extent,  extent, 0.0,   1.0, 1.0,
    -extent,  extent, 0.0,   0.0, 1.0
], dtype=np.float32)

# Indices for the quad (two triangles)
indices = np.array([
    0, 1, 2,
    2, 3, 0
], dtype=np.uint32)

################################################################################
# Our hybrid application class
################################################################################

class UiTestApp(object):

  def __init__(self):
    super().__init__()

    self.newAppState() # define application state (for save/restore)

    # create ezapp
    self.ezapp = lev2.OrkEzApp.create(self,fullscreen=False)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    # create and bind overlay event interceptor
    #  so we can route events to imgui
    installImguiOnApp(self)

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 5
    griditems = lg_group.makeGrid( width = 2,
                                   height = 1,
                                   margin = 1,
                                   uiclass = lev2.ui.LambdaBox,
                                   args = ["box",vec4(1,0,1,1)] )

    print(griditems)

    # set up event handlers for the grid items
    griditems[0].widget.onPressed(lambda: print("GRIDITEM0 PUSHED"))
    griditems[1].widget.onPressed(lambda: print("GRIDITEM1 PUSHED"))
    
    self.griditems = griditems
    
    IMGW = self.griditems[0].widget
    IMGW.enableDraw = False # disable default drawing for widget 0, as it will be drawn by PyOpenGL
    self.imgui_widget = IMGW
    
    OLGW = self.griditems[1].widget
    OLGW.enableDraw = True # disable default drawing for widget 0, as it will be drawn by PyOpenGL
    self.opengl_widget = OLGW

    self.status_text = "OK"

    # animation properties
    self.time = 0.0
    self.fps_accum = 0.0
    self.fps_time_base = time.time()
    self.ups_accum = 0.0
    self.ups_time_base = time.time()
    self.FPS = 0.0
    self.UPS = 0.0
    
    self.current_preset = "none"
    self.item_current_idx = 0
    self._movie_frames = []
    self.writing_movie = False


  ##############################################
  # appstate support
  #  we put all appstate that we want to save/restore in a VarMap
  #  then we can save/restore the VarMap to/from a json file
  #  the save will be done when the app exits (onExit)
  #  the restore will be done when the app starts (onGpuInit)
  ##############################################

  def newAppState(self):
    self.app_vars = VarMap()
    self.app_vars.preset_name = "NewPreset"
    self.presets = {
      "preset1": {"frg_shader_src": "\n#version 410 core\nuniform float time;\nuniform float scale;\nuniform float scale_power; // range (0.5,2.0,0)\nuniform float rot_scale; // range (0, 10, 0.5)\nuniform float spin_rate; // range (-1,1,0.1)\nuniform float spin_bias; // range (-3.1415,3.1415)\nuniform float cycle_rate; // range (-4,4,0.1)\nuniform float d_final_scale;\nuniform float d_final_bias;\nuniform vec3 ModColor;\nout vec4 FragColor;\nin vec2 TexCoord;\n\nvoid main() {\n  vec2 uv = TexCoord;\n  vec2 uvc = uv-vec2(0.5,0.5);\n  float angle = atan(uvc.x,uvc.y);\n  float sina = 0.5+sin(spin_bias+time*spin_rate*-1+angle*rot_scale)*0.5;\n  float d = length(uvc)*scale*sina;\n  d = sign(scale)*pow(abs(d),scale_power);\n  d = mod(d+time*cycle_rate,1.0);\n  d = d*d_final_scale + d_final_bias;\n  FragColor = vec4(ModColor*d, 1.0);\n}\n", "presets": "", "sh_ModColor": "vec3(0.567358,0.185217,0.803922)", "sh_cycle_rate": 0.4000000059604645, "sh_d_final_bias": -0.3009999990463257, "sh_d_final_scale": 6.984000205993652, "sh_rot_scale": 2.5, "sh_scale": 4.079999923706055, "sh_scale_power": 2.0, "sh_spin_bias": 1.5779999494552612, "sh_spin_rate": 0.0, "text": "Hello, world!"},
      "preset2": {"frg_shader_src": "\n#version 410 core\nuniform float time;\nuniform float scale;\nuniform float scale_power; // range (0.5,2.0,0)\nuniform float rot_scale; // range (0, 10, 0.5)\nuniform float spin_rate; // range (-1,1,0.1)\nuniform float spin_bias; // range (-3.1415,3.1415)\nuniform float cycle_rate; // range (-4,4,0.1)\nuniform float d_final_scale;\nuniform float d_final_bias;\nuniform vec3 ModColor;\nout vec4 FragColor;\nin vec2 TexCoord;\n\nvoid main() {\n  vec2 uv = TexCoord;\n  vec2 uvc = uv-vec2(0.5,0.5);\n  float angle = atan(uvc.x,uvc.y);\n  float sina = 0.5+sin(spin_bias+time*spin_rate*-1+angle*rot_scale)*0.5;\n  float d = length(uvc)*scale*sina;\n  d = sign(scale)*pow(abs(d),scale_power);\n  d = mod(d+time*cycle_rate,1.0);\n  d = d*d_final_scale + d_final_bias;\n  FragColor = vec4(ModColor*d, 1.0);\n}\n", "preset_name": "preset2", "presets": "", "sh_ModColor": "vec3(0.567358,0.085217,0.303922)", "sh_cycle_rate": 0.4000000059604645, "sh_d_final_bias": -0.3009999990463257, "sh_d_final_scale": 6.984000205993652, "sh_rot_scale": 3.0, "sh_rot_scale2": "", "sh_scale": 6.517000198364258, "sh_scale_power": 0.5, "sh_spin_bias": 1.5779999494552612, "sh_spin_rate": 1.0, "text": "Hello, world!"},
      "preset3": {"frg_shader_src": "\n#version 410 core\nuniform float time;\nuniform float scale;\nuniform float scale_power; // range (0.5,2.0,0)\nuniform float rot_scale; // range (0, 10, 0.5)\nuniform float spin_rate; // range (-1,1,0.1)\nuniform float spin_bias; // range (-3.1415,3.1415)\nuniform float cycle_rate; // range (-4,4,0.1)\nuniform float d_final_scale;\nuniform float d_final_bias;\nuniform vec3 ModColor;\nout vec4 FragColor;\nin vec2 TexCoord;\n\nvoid main() {\n  vec2 uv = TexCoord;\n  vec2 uvc = uv-vec2(0.5,0.5);\n  float angle = atan(uvc.x,uvc.y);\n  float sina = 0.5+sin(spin_bias+time*spin_rate*-1+angle*rot_scale)*0.5;\n  float d = length(uvc)*scale*sina;\n  d = sign(scale)*pow(abs(d),scale_power);\n  d = mod(d+time*cycle_rate,1.0);\n  d = d*d_final_scale + d_final_bias;\n  FragColor = vec4(ModColor*d, 1.0);\n}\n", "preset_name": "Preset3", "presets": "", "sh_ModColor": "vec3(1.000000,0.117647,0.117647)", "sh_cycle_rate": 0.4000000059604645, "sh_d_final_bias": -0.3009999990463257, "sh_d_final_scale": 6.984000205993652, "sh_rot_scale": 5.0, "sh_rot_scale2": "", "sh_scale": 4.790999889373779, "sh_scale_power": 2.0, "sh_spin_bias": 1.6100000143051147, "sh_spin_rate": 0.0, "text": "Hello, world!"},
      "preset4": {"frg_shader_src": "\n#version 410 core\nuniform float time;\nuniform float scale;\nuniform float scale_power; // range (0.5,2.0,0)\nuniform float rot_scale; // range (0, 10, 0.5)\nuniform float spin_rate; // range (-10,10,0.1)\nuniform float spin_bias; // range (-3.1415,3.1415)\nuniform float cycle_rate; // range (-4,4,0.1)\nuniform float d_final_scale;\nuniform float d_final_bias;\nuniform vec3 ModColor;\nout vec4 FragColor;\nin vec2 TexCoord;\n\nvoid main() {\n  vec2 uv = TexCoord;\n  vec2 uvc = uv-vec2(0.5,0.5);\n  float angle = atan(uvc.x,uvc.y);\n  float sina = 0.5+sin(spin_bias+time*spin_rate*-1+angle*rot_scale)*0.5;\n  float d = length(uvc)*scale*sina;\n  d = sign(scale)*pow(abs(d),scale_power);\n  d = mod(d+time*cycle_rate,1.0);\n  d = d*d_final_scale + d_final_bias;\n  FragColor = vec4(ModColor*d, 1.0);\n}\n", "preset_name": "Preset4", "presets": "", "sh_ModColor": "vec3(0.000000,0.151961,0.008939)", "sh_cycle_rate": 0.4000000059604645, "sh_d_final_bias": 5.5329999923706055, "sh_d_final_scale": -10.0, "sh_rot_scale": 5.0, "sh_rot_scale2": "", "sh_scale": 2.556999921798706, "sh_scale_power": 2.0, "sh_spin_bias": -1.5149999856948853, "sh_spin_rate": 0.0, "text": "Hello, world!"},
      "preset5": {"frg_shader_src": "\n#version 410 core\nuniform float time;\nuniform float scale;\nuniform float scale_power; // range (0.5,2.0,0)\nuniform float rot_scale; // range (0, 10, 0.5)\nuniform float spin_rate; // range (-10,10,0.1)\nuniform float spin_bias; // range (-3.1415,3.1415)\nuniform float cycle_rate; // range (-4,4,0.1)\nuniform float d_final_scale;\nuniform float d_final_bias;\nuniform vec3 ModColor;\nout vec4 FragColor;\nin vec2 TexCoord;\n\nvoid main() {\n  vec2 uv = TexCoord;\n  vec2 uvc = uv-vec2(0.5,0.5);\n  float angle = atan(uvc.x,uvc.y);\n  float sina = 0.5+sin(spin_bias+time*spin_rate*-1+angle*rot_scale)*0.5;\n  float d = length(uvc)*scale*sina;\n  d = sign(scale)*pow(abs(d),scale_power);\n  d = mod(d+time*cycle_rate,1.0);\n  d = d*d_final_scale + d_final_bias;\n  FragColor = vec4(ModColor*d, 1.0);\n}\n", "preset_name": "Preset5", "presets": "", "sh_ModColor": "vec3(0.106618,0.426471,0.106618)", "sh_cycle_rate": 0.4000000059604645, "sh_d_final_bias": -3.2690000534057617, "sh_d_final_scale": 7.836999893188477, "sh_rot_scale": 3.0, "sh_rot_scale2": "", "sh_scale": 2.26200008392334, "sh_scale_power": 1.9190000295639038, "sh_spin_bias": 1.5779999494552612, "sh_spin_rate": 1.0, "text": "Hello, world!"},
      "preset6": {"frg_shader_src": "\n#version 410 core\nuniform float time;\nuniform float scale;\nuniform float scale_power; // range (0.5,2.0,0)\nuniform float rot_scale; // range (0, 10, 0.5)\nuniform float rot_scale2; // range (0,100,0,01)\nuniform float spin_rate; // range (-10,10,0.1)\nuniform float spin_bias; // range (-3.1415,3.1415)\nuniform float cycle_rate; // range (-4,4,0.1)\nuniform float d_final_scale;\nuniform float d_final_bias;\nuniform vec3 ModColor;\nout vec4 FragColor;\nin vec2 TexCoord;\n\nvoid main() {\n  vec2 uv = TexCoord;\n  vec2 uvc = uv-vec2(0.5,0.5);\n  float angle = atan(uvc.x,uvc.y);\n  float sina = 0.5+sin(spin_bias+time*spin_rate*-1+angle*rot_scale)*0.5;\n  sina = sina * (0.5+sin(spin_bias+time*spin_rate*-1+angle*rot_scale2)*0.5);\n  float d = length(uvc)*scale*sina;\n  d = sign(scale)*pow(abs(d),scale_power);\n  d = mod(d+time*cycle_rate,1.0);\n  d = d*d_final_scale + d_final_bias;\n  FragColor = vec4(ModColor*d, 1.0);\n}\n", "preset_name": "Preset6", "presets": "", "sh_ModColor": "vec3(0.106618,0.228915,0.426471)", "sh_cycle_rate": 0.4000000059604645, "sh_d_final_bias": -3.2690000534057617, "sh_d_final_scale": 7.836999893188477, "sh_rot_scale": 3.0, "sh_rot_scale2": 36.395999908447266, "sh_scale": -2.763000011444092, "sh_scale_power": 0.7350000143051147, "sh_spin_bias": 1.5779999494552612, "sh_spin_rate": 0.0, "text": "Hello, world!"},
      "preset7": {"frg_shader_src": "\n#version 410 core\nuniform float time;\nuniform float scale;\nuniform float scale_power; // range (0.5,2.0,0)\nuniform float rot_scale; // range (0, 10, 0.5)\nuniform float spin_rate; // range (-1,1,0.1)\nuniform float spin_bias; // range (-3.1415,3.1415)\nuniform float cycle_rate; // range (-4,4,0.1)\nuniform float d_final_scale;\nuniform float d_final_bias;\nuniform vec3 ModColor;\nout vec4 FragColor;\nin vec2 TexCoord;\n\nvoid main() {\n  vec2 uv = TexCoord;\n  vec2 uvc = uv-vec2(0.5,0.5);\n  float angle = atan(uvc.x,uvc.y);\n  float sina = 0.5+sin(spin_bias+time*spin_rate*-1+angle*rot_scale)*0.5;\n  float d = length(uvc)*scale*sina;\n  d = sign(scale)*pow(abs(d),scale_power);\n  d = mod(d+time*cycle_rate,1.0);\n  d = d*d_final_scale + d_final_bias;\n  FragColor = vec4(ModColor*d, 1.0);\n}\n", "preset_name": "preset7", "presets": "", "sh_ModColor": "vec3(0.705882,0.296764,0.010381)", "sh_cycle_rate": 1.600000023841858, "sh_d_final_bias": 7.005000114440918, "sh_d_final_scale": -9.10200023651123, "sh_rot_scale": 10.0, "sh_rot_scale2": "", "sh_scale": 1.0160000324249268, "sh_scale_power": 0.5, "sh_spin_bias": -1.3919999599456787, "sh_spin_rate": 0.0, "text": "Hello, world!"},
    }
    self.current_preset = "none"
    self.assignPreset("preset1")

  ##############################################
  # assign preset to app state
  ##############################################

  def assignPreset(self,preset):
    if preset!=self.current_preset:
      the_preset = self.presets[preset]
      self.current_preset = preset
      as_json = json.dumps(the_preset)
      if hasattr(self,"text_editor"):
        self.text_editor.set_text(the_preset["frg_shader_src"])
        self.imgui_handler.deserializeAppState(self.app_vars,as_json)
        self.recompileShader()
    
  ##############################################
  # onUpdate - called from update / simulation thread
  ##############################################

  def onUpdate(self,updev):
    if not self.writing_movie:
      self.time = updev.absolutetime
    self.ups_accum += 1.0
    now = time.time()
    delta = now-self.ups_time_base
    if delta>1.0:
      self.UPS = self.ups_accum/delta
      self.ups_accum = 0.0
      self.ups_time_base = time.time()

  ##############################################
  # onGpuInit - called once at startup (in rendering thread)
  #  this is where pyopengl initialization should be done
  ##############################################

  def onGpuInit(self,ctx):

    self.ssaa_resolve_shader = PyShader(vertex_shader_source, fragment_shader_ssaa_resolve)

    ##################################
    # setup imgui
    ##################################

    self.imgui_handler = ImGuiWrapper( self, 
                                       "ork_pyext_test_pyopengl", 
                                       docking=True, 
                                       lock_to_panel=self.imgui_widget )
    self.imgui_handler.onGpuInit(ctx,self.app_vars)
    self.text_editor = TextEditor()
    if self.app_vars.frg_shader_src != None:
      self.text_editor.set_text(self.app_vars.frg_shader_src)
    self.text_editor.set_language_definition(TextEditor.LanguageDefinition.hlsl())
    self.text_editor.set_handle_keyboard_inputs(True)
   

    if self.imgui_handler.apppresets_file.exists():
      with open(self.imgui_handler.apppresets_file,"r") as f:
        all_presets_json = f.read()
        self.presets = json.loads(all_presets_json)
    
    ##################################
    # define geometry and shaders
    ##################################

    self.geometry = GeometryBuffer(vertices, indices )
    self.recompileShader()
    
    # create SSAA framebuffer
    self.ssaa_fbo = glGenFramebuffers(1)
    glBindFramebuffer(GL_FRAMEBUFFER, self.ssaa_fbo)
    self.ssaa_color = glGenTextures(1)
    glBindTexture(GL_TEXTURE_2D, self.ssaa_color)
    glBindFramebuffer(GL_FRAMEBUFFER, 0)
    self.ssaa_size = vec2(0,0)
    self.ssaa_mult = SSAA_MULT
    
  ##############################################

  def onExit(self):
    self.imgui_handler.onExit(self.app_vars)

  ##############################################
  # invoked by the UI overlay widget
  #  this is where imgui events are processed
  #  if the event is not handled by imgui, it is passed to the app
  ##############################################

  def onOverlayUiEvent(self,uievent):
    handled = False
    if uievent.code == tokens.KEY_DOWN.hashed:
      keycode = uievent.keycode
      is_ctrl = uievent.ctrl 
      if is_ctrl:
        if keycode == ord("R"):
          self.newAppState()
          self.recompileShader()
          self.text_editor.set_text(fragment_shader_source)
        elif keycode == ord("M"):
          self.writing_movie = not self.writing_movie
    if not handled:
      return self.imgui_handler.onUiEvent(uievent)
       
  ##############################################

  def recompileShader(self):
    try:
      frg_src = self.text_editor.get_text()
      sh = PyShader(vertex_shader_source, frg_src)
      self.shaders = sh
      self.app_vars.frg_shader_src = frg_src
      self.status_text = "OK"
      ################################################
      # strip sh_* vars that are not in the new shader
      ################################################
      keys_to_delete = []
      for item in self.app_vars.keys():
        if "sh_" in item:
          label = item[3:]
          if item not in sh._bound_params:
            keys_to_delete.append(item)
      for item in keys_to_delete:
        setattr(self.app_vars,item,"")
      ################################################
      # add sh_* vars that are in the new shader
      #  and not in the app_vars
      ################################################
      for item in sh._bound_params.keys():
        if item not in self.app_vars.keys():
          name = item[3:]
          uni_loc = sh._bound_params[item]
          type_str = sh._param_types[item]
          if type_str=="float":
            setattr(self.app_vars,item,float(1))
          elif type_str=="vec2":
            setattr(self.app_vars,item,vec2(1,1))
          elif type_str=="vec3":
            setattr(self.app_vars,item,vec3(1,1,1))
          elif type_str=="vec4":
            setattr(self.app_vars,item,vec4(1,1,1,1))
          else:
            print("unknown type for shader-var: ",item)
            assert(False)
          print("added shader-var: ",item,type_str)
      ################################################
    except Exception as e:
      print("Error compiling shader:", e)
      self.status_text = "Error: " + str(e)
      callstack = traceback.format_exc()
      print(callstack)        
  ##############################################

  def onMetaKey(self,uievent,im_key):
    handled = True
    if im_key == imgui.Key.s:
      self.recompileShader()
    elif im_key == imgui.Key.c:
      self.text_editor.copy()
    elif im_key == imgui.Key.v:
      self.text_editor.paste()
    elif im_key == imgui.Key.z:
      self.text_editor.undo()
    else:
      handled = False
    return handled

  ##############################################
  # onGpuPostFrame - called after ALL orkid rendering is done
  #  this is where pyopengl/imgui rendering should be done
  ##############################################

  def onGpuPostFrame(self,ctx):
    self._renderPanel(ctx)
    self._renderImGui(ctx)
    imgui.update_platform_windows();
    imgui.render_platform_windows_default();
    self.fps_accum += 1.0
    now = time.time()
    delta = now-self.fps_time_base
    if delta>1.0:
      self.FPS = self.fps_accum/delta
      self.fps_accum = 0.0
      self.fps_time_base = time.time()
    if self.writing_movie:
      self.time += 1.0/60.0
    
  ##############################################
  # _clearPanel - clear the panel before drawing (via PyOpenGL)
  ##############################################

  def _setupPanel(self, widget):

    ############################
    # get dimensions of window
    ############################

    TOPW = self.ezapp.topWidget    
    scr_w = TOPW.width
    scr_h = TOPW.height

    ############################
    # Draw On Top Of test widget
    #  raw-opengl has origin at bottom left
    #  so we need to flip the y coordinate
    ############################

    wx = widget.x
    wy = scr_h-widget.y2-1 
    ww = widget.width
    wh = widget.height    
    ssaa_w = ww*self.ssaa_mult
    ssaa_h = wh*self.ssaa_mult

    # resize ssaa framebuffer
    if self.ssaa_size.x!=ssaa_w or self.ssaa_size.y!=ssaa_h:
      print( "resizing ssaa framebuffer to %d x %d"%(ssaa_w,ssaa_h))
      self.ssaa_size = vec2(ssaa_w,ssaa_h)
      glBindFramebuffer(GL_FRAMEBUFFER, self.ssaa_fbo)
      glBindTexture(GL_TEXTURE_2D, self.ssaa_color)
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ssaa_w, ssaa_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, None)
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE)
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, self.ssaa_color, 0)
      glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0)
      glBindFramebuffer(GL_FRAMEBUFFER, 0)
      # validate framebuffer
      status = glCheckFramebufferStatus(GL_FRAMEBUFFER)
      if status != GL_FRAMEBUFFER_COMPLETE:
        print("framebuffer not complete")
        assert(False)
    

    # clear FBO

    glBindFramebuffer(GL_FRAMEBUFFER, self.ssaa_fbo)
    glViewport(0, 0, ssaa_w, ssaa_h)
    glScissor(0, 0, ssaa_w, ssaa_h)
    glClearColor(1.0, 0.1, 0.1, 1.0)
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)
    glDepthMask(GL_FALSE)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)


  ##############################################
  # _renderPanel - render the panel using PyOpenGL
  ##############################################

  def _renderPanel(self,ctx):

    #################################
    # clear the panel before drawing
    #################################

    self._setupPanel(self.opengl_widget)

    #################################
    # bind shader and uniforms
    #################################
        
    glUseProgram(self.shaders.shader_program)
    glUniform1f(self.shaders.par_time,self.time)

    for item in self.app_vars.keys():
      if "sh_" in item:
        label = item[3:]
        val = self.app_vars[item]
        #print(item,label,val)
        if item in self.shaders._bound_params:
          uni_loc = self.shaders._bound_params[item]
          #print(uni_loc)
          if type(val) == float:
            glUniform1f(uni_loc, val)
          elif type(val) == vec2:
            glUniform2f(uni_loc, val.x, val.y)
          elif type(val) == vec3:
            glUniform3f(uni_loc, val.x, val.y, val.z)
          elif type(val) == vec4:
            glUniform3f(uni_loc, val.x, val.y, val.z, val.w)

    #################################
    # bind VAO and draw
    #################################

    glBindVertexArray(self.geometry.VAO)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, None)
    glBindVertexArray(0)
    glBindFramebuffer(GL_FRAMEBUFFER, 0)

    #################################
    # ssaa resolve from ssaa_fbo to screen (fbo 0)
    # use a fullscreen quad
    #################################

    w = self.opengl_widget.width
    h = self.opengl_widget.height
    x = self.opengl_widget.x
    y = self.opengl_widget.y

    glUseProgram(self.ssaa_resolve_shader.shader_program)
    uni_loc = self.ssaa_resolve_shader._shader_params["ssaaTex"] 
       
    glActiveTexture(GL_TEXTURE0) # select unit 0
    glBindTexture(GL_TEXTURE_2D, self.ssaa_color) # bind the texture to unit 0
    glUniform1i(uni_loc, 0) # set the uniform to the texture unit 0
        
    glBindVertexArray(self.geometry.VAO)
    glViewport(x, y, w, h)
    glScissor(x, y, w, h)    
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, None)
    glBindVertexArray(0)

    #################################
    # write movie
    #################################
    
    if self.writing_movie:
      self._writeMovieFrame()

  ##############################################
  # _writeMovieFrame - write a frame to a movie file
  ##############################################

  def _writeMovieFrame(self):

    w = self.opengl_widget.width
    h = self.opengl_widget.height
    x = self.opengl_widget.x
    y = self.opengl_widget.y

    ########################################
    # capture frame from current viewport
    ########################################

    glPixelStorei(GL_PACK_ALIGNMENT, 1)
    data = glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE)
    # convert to numpy array
    img = np.frombuffer(data, np.uint8)
    img.shape = (h, w, 4)
    # flip image
    img = np.flipud(img)
    index = len(self._movie_frames)
    self._movie_frames.append((index,img))
    NUM_FRAMES = 960
    self.status_text = "capturing movie frame %d of %d"%(len(self._movie_frames),NUM_FRAMES)

    ########################################
    # write movie when we have enough frames
    ########################################

    if len(self._movie_frames)>=NUM_FRAMES:
      self.writing_movie = False
      tmpdir = obt_path.temp()
      def worker(n):
          print(f"Worker {n} is starting.")
          while len(self._movie_frames)>0:
            item = self._movie_frames.pop(0)
            index = item[0]
            img = item[1]
            remaining = len(self._movie_frames)
            self.status_text = "writing images : remaining: %d"%remaining
            imgfile = os.path.join(tmpdir,"frame%04d.png"%index)
            img = Image.fromarray(img)
            img.save(imgfile)
          print(f"Worker {n} is done.")
          return f"Result from worker {n}"

      def do_all():
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            # Submit tasks to the thread pool
            futures = [executor.submit(worker, i) for i in range(10)]            
            # As tasks complete, get the results
            for future in concurrent.futures.as_completed(futures):
                result = future.result()
                print(result)
        outmovie = os.path.join(tmpdir,"movie.mov")
        cmd = "ffmpeg -y -r 60 -i %s/frame%%04d.png -c:v h264 -vf fps=60 -pix_fmt yuv420p %s"%(tmpdir,outmovie)
        self.status_text = "writing movie...."
        subprocess.run(cmd,shell=True)
        self.status_text = "OK"
        subprocess.run("open %s"%outmovie,shell=True)
      threading.Thread(target=do_all).start()

  ##############################################
  # _renderImGui - render the imgui UI
  ##############################################

  def _renderImGui(self,ctx):

    #################################
    # begin imgui rendering
    #################################

    self.imgui_handler.beginFrame()
    io = self.imgui_handler.imgui_io

    imgui.begin("Orkid/PyImGui/PyOpenGL integration example", True)      

    #################################
    # property sheet for app_vars
    #################################

    for item in self.app_vars.keys():
      #print("item<%s>"%item)
      if "sh_" in item:
        uniname = item[3:]
        val = self.app_vars[item]
        minval = -10.0
        maxval = 10.0
        stpval = 0.0
        if uniname in self.shaders._param_ranges:
          minval,maxval,stpval = self.shaders._param_ranges[uniname]
        #print("item<%s> minval<%s> maxval<%s>"%(item,minval,maxval))
                
        if type(val) == float:
          prv_val = self.app_vars[item]
          # quantize input to nearest step value (stpval) 
          # if stpval is not zero
          if stpval>0.0:
            prv_val = round(prv_val/stpval)*stpval
          changed, new_val = imgui.slider_float(
            label=uniname, 
            v=prv_val, 
            v_min=minval, 
            v_max=maxval,
          )
          if changed:
            if stpval>0.0:
              new_val = round(new_val/stpval)*stpval
            setattr(self.app_vars,item,new_val)
        elif type(val) == vec3:        
          inp_val = [val.x,val.y,val.z]
          changed, newval = imgui.color_edit3(
            label=uniname, 
            col=inp_val
          )
          if changed:
            as_v3 = vec3(newval[0],newval[1],newval[2])
            setattr(self.app_vars,item,as_v3)
    #)

    # pack two widgets in a row

    #################################
    # presets combo
    #################################

    lin_dict = list(self.presets.keys())
    combo_preview_value = lin_dict[self.item_current_idx]
    prev_item = self.item_current_idx
    if imgui.begin_combo("PRESET", combo_preview_value, 0):
        was_changed = False
        for n in range(len(self.presets)):
          is_selected = (self.item_current_idx == n)
          key = lin_dict[n]
          was_changed,x = imgui.selectable(key, is_selected)
          if was_changed or x:
            self.item_current_idx = n
        imgui.end_combo()
        if prev_item!=self.item_current_idx:
          preset = lin_dict[self.item_current_idx]
          self.assignPreset(preset)

    #################################
    # presets write button
    #################################

    if imgui.button("SavePreset"):
      self.presets[self.app_vars.preset_name] = self.app_vars
      self.current_preset = self.app_vars.preset_name
      self.item_current_idx = len(self.presets)-1
      as_json = self.imgui_handler.serializeAppState(self.app_vars)
      as_dict = json.loads(as_json)
      self.presets[self.app_vars.preset_name] = as_dict
      all_presets_json = json.dumps(self.presets)
      with open(self.imgui_handler.apppresets_file,"w") as f:
        f.write(all_presets_json)
        
      print("saved preset<%s>"%self.app_vars.preset_name)

    #################################
    # presets name
    #################################

    imgui.same_line()

    changed, self.app_vars.preset_name = imgui.input_text(
      label="PresetName", 
      str=self.app_vars.preset_name,
    )

    #################################
    # FPS
    #################################

    is_shift = io.key_shift
    is_ctrl = io.key_ctrl
    is_alt = io.key_alt
    is_super = io.key_super

    imgui.text("FPS %g"%(self.FPS))
    imgui.text("UPS %g"%(self.UPS))
    imgui.text("is_shift %s is_ctrl %s is_alt %s is_super %s"%(is_shift,is_ctrl,is_alt,is_super))
    
    # draw status color and text
    status_color = imgui.ImVec4(1,0,0,1)
    if self.status_text=="OK":
      status_color = imgui.ImVec4(0,1,0,1)
    imgui.text_colored(status_color, self.status_text)
    
    imgui.end()

    #################################
    # text editor
    #################################

    imgui.begin("FragmentShader", True)
    #imgui.push_font(imgui_md.get_code_font())
    self.text_editor.render("Code")
    #imgui.pop_font()
    imgui.end()
    
    #################################
    # end imgui rendering
    #################################

    self.imgui_handler.endFrame()

###############################################################################

the_app = UiTestApp()
the_app.ezapp.mainThreadLoop()
the_app.onExit()
