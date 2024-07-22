#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI to a window, 
#   delegates some rendering code to PyOpenGL and PyImGui
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

#pip3 install imgui[glfw] pyopengl

################################################################################
import imgui
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
uniform vec3 ModColor;
out vec4 FragColor;
in vec2 TexCoord;

void main() {
  vec2 uv = TexCoord;
  vec2 uvc = uv-vec2(0.5,0.5);
  float d = length(uvc)*10;
  d = mod(d+time*0.3,1.0);
  FragColor = vec4(ModColor*d, 1.0);
}
"""

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
    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    # create and bind overlay event interceptor
    #  so we can route events to imgui
    installImguiOnApp(self)

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 8
    griditems = lg_group.makeGrid( width = 2,
                                   height = 2,
                                   margin = 1,
                                   uiclass = lev2.ui.LambdaBox,
                                   args = ["box",vec4(1,0,1,1)] )

    print(griditems)

    # set up event handlers for the grid items
    griditems[0].widget.onPressed(lambda: print("GRIDITEM0 PUSHED"))
    griditems[1].widget.onPressed(lambda: print("GRIDITEM1 PUSHED"))
    griditems[2].widget.onPressed(lambda: print("GRIDITEM2 PUSHED"))
    griditems[3].widget.onPressed(lambda: print("GRIDITEM3 PUSHED"))
    
    self.griditems = griditems
    
    W = self.griditems[0].widget
    W.enableDraw = False # disable default drawing for widget 0, as it will be drawn by PyOpenGL
    self.test_widget = W

    # animation properties
    self.time = 0.0
    

  ##############################################
  # appstate support
  #  we put all appstate that we want to save/restore in a VarMap
  #  then we can save/restore the VarMap to/from a json file
  #  the save will be done when the app exits (onExit)
  #  the restore will be done when the app starts (onGpuInit)
  ##############################################

  def newAppState(self):
    self.app_vars = VarMap()
    self.app_vars.rate = 0.1
    self.app_vars.color = 1., .0, .5
    self.app_vars.text = "Hello, world!"

  ##############################################
  # onUpdate - called from update / simulation thread
  ##############################################

  def onUpdate(self,updev):
    self.time = updev.absolutetime*self.app_vars.rate

  ##############################################
  # onGpuInit - called once at startup (in rendering thread)
  #  this is where pyopengl initialization should be done
  ##############################################

  def onGpuInit(self,ctx):

    ##################################
    # define geometry and shaders
    ##################################

    self.geometry = GeometryBuffer(vertices, indices )
    self.shaders = PyShader( vertex_shader_source, fragment_shader_source, ["time","ModColor"] )

    ##################################
    # setup imgui
    ##################################

    self.imgui_handler = ImGuiWrapper(self.ezapp, "ork_pyext_test_pyopengl")
    self.imgui_handler.onGpuInit(ctx,self.app_vars)

  ##############################################

  def onExit(self):
    self.imgui_handler.onExit(self.app_vars)

  ##############################################
  # invoked by the UI overlay widget
  #  this is where imgui events are processed
  #  if the event is not handled by imgui, it is passed to the app
  ##############################################

  def onOverlayUiEvent(self,uievent):
    return self.imgui_handler.onUiEvent(uievent)

  ##############################################
  # onGpuPostFrame - called after ALL orkid rendering is done
  #  this is where pyopengl/imgui rendering should be done
  ##############################################

  def onGpuPostFrame(self,ctx):
    self._renderPanel(ctx)
    self._renderImGui(ctx)

  ##############################################
  # _renderPanel - render the panel using PyOpenGL
  ##############################################

  def _renderPanel(self,ctx):

    #################################
    # clear the panel before drawing
    #################################

    self._clearPanel(self.test_widget)

    #################################
    # bind shader and uniforms
    #################################
        
    glUseProgram(self.shaders.shader_program)
    glUniform1f(self.shaders.par_time,self.time)
    glUniform3f(self.shaders.par_color,*(self.app_vars.color))

    #################################
    # bind VAO and draw
    #################################

    glBindVertexArray(self.geometry.VAO)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, None)

    #################################
    # cleanup
    #################################

    glBindVertexArray(0)

  ##############################################
  # _clearPanel - clear the panel before drawing (via PyOpenGL)
  ##############################################

  def _clearPanel(self, widget):

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

    # render it

    glDrawBuffers([GL_BACK_LEFT])
    glBindFramebuffer(GL_FRAMEBUFFER, 0)
    glScissor(wx,wy,ww,wh)
    glViewport(wx,wy,ww,wh)
    glEnable(GL_SCISSOR_TEST)
    glClearColor(0.0, 0.1, 0.1, 1.0)
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)
    glDepthMask(GL_FALSE)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

  ##############################################
  # _renderImGui - render the imgui UI
  ##############################################

  def _renderImGui(self,ctx):

    #################################
    # begin imgui rendering
    #################################

    self.imgui_handler.beginFrame()
    
    #################################

    imgui.begin("Orkid/PyImGui/PyOpenGL integration example", True)

    clicked, self.app_vars.rate = imgui.slider_float(
        label="TimeRate",
        value=self.app_vars.rate,
        min_value=-10.0,
        max_value=10.0,
    )
    
    changed, self.app_vars.color = imgui.color_edit3("Color", *self.app_vars.color )
    changed, self.app_vars.text = imgui.input_text("Text", self.app_vars.text, 256)
    
    imgui.end()
    
    imgui.begin("Imgui Panel2", True)
    imgui.end()

    #################################
    # end imgui rendering
    #################################

    self.imgui_handler.endFrame()

###############################################################################

the_app = UiTestApp()
the_app.ezapp.mainThreadLoop()
the_app.onExit()
