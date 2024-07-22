#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI to a window, 
#   delegates some rendering code to PyOpenGL and PyImGui
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

#pip3 install imgui[glfw] pyopengl

import imgui
import numpy as np
from orkengine.core import *
from orkengine import lev2
lev2_pyexdir.addToSysPath()
from lev2utils.imgui import ImGuiWrapper
from lev2utils.pyopengl import PyShader, GeometryBuffer
from OpenGL.GL import *

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

class UiTestApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.uicontext = self.ezapp.uicontext
    print(self.uicontext)
    UIOVERLAY = lev2.EzUiEventInterceptor()
    UIOVERLAY.onUiEvent = lambda uievent : self.onOverlayUiEvent(uievent)
    self.uicontext.overlayWidget = UIOVERLAY

    self.ezapp.topWidget.enableUiDraw()
    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 8
    griditems = lg_group.makeGrid( width = 2,
                                   height = 2,
                                   margin = 1,
                                   uiclass = lev2.ui.LambdaBox,
                                   args = ["box",vec4(1,0,1,1)] )

    print(griditems)

    griditems[0].widget.onPressed(lambda: print("GRIDITEM0 PUSHED"))
    griditems[1].widget.onPressed(lambda: print("GRIDITEM1 PUSHED"))
    griditems[2].widget.onPressed(lambda: print("GRIDITEM2 PUSHED"))
    griditems[3].widget.onPressed(lambda: print("GRIDITEM3 PUSHED"))
    
    self.griditems = griditems
    print(self.ezapp.mainwin)
    print(self.ezapp.mainwin.appwin)
    print(self.ezapp.topWidget)
    print(self.ezapp.topWidget.name)
    print(lg_group.name)
    print(lg_group)
    print(lg_group.layout)
    print(lg_group.layout.top)
    print(lg_group.layout.bottom)
    print(lg_group.layout.left)
    print(lg_group.layout.right)
    print(lg_group.layout.centerH)
    print(lg_group.layout.centerV)
    print(self.ezapp.uicontext)
    
    W = self.griditems[0].widget
    W.enableDraw = False
    self.test_widget = W
    self.time = 0.0
    self.rate = 0.1
    self.color = 1., .0, .5

    self.text = "Hello, world!"

  ##############################################
  # onUpdate - called from update / simulation thread
  ##############################################

  def onUpdate(self,updev):
    self.time = updev.absolutetime*self.rate

  ##############################################
  # onGpuInit - called once at startup (in rendering thread)
  #  this is where pyopengl initialization should be done
  ##############################################

  def onGpuInit(self,ctx):

    ##################################
    # define geometry and shaders
    ##################################

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

    self.geometry = GeometryBuffer(vertices, indices )
    self.shaders = PyShader( vertex_shader_source, fragment_shader_source, ["time","ModColor"] )

    ##################################
    # setup imgui
    ##################################

    self.imgui_handler = ImGuiWrapper(self.ezapp)
    self.imgui_handler.onGpuInit(ctx)

  ##############################################

  def onOverlayUiEvent(self,uievent):
    return self.imgui_handler.onUiEvent(uievent)

  ##############################################
  # onGpuPostFrame - called after all orkid rendering is done
  #  this is where pyopengl rendering should be done
  ##############################################

  def onGpuPostFrame(self,ctx):

    #################################
    # clear the panel before drawing
    #################################

    self._clearPanel(self.test_widget)

    #################################
    # bind shader and uniforms
    #################################
        
    glUseProgram(self.shaders.shader_program)
    glUniform1f(self.shaders.par_time,self.time)
    glUniform3f(self.shaders.par_color,*self.color)

    #################################
    # bind VAO and draw
    #################################

    glBindVertexArray(self.geometry.VAO)
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, None)

    #################################
    # cleanup
    #################################

    glBindVertexArray(0)

    #################################

    self.imgui_handler.beginFrame()
    
    imgui.begin("Orkid/PyImGui/PyOpenGL integration example", True)

    clicked, self.rate = imgui.slider_float(
        label="TimeRate",
        value=self.rate,
        min_value=-10.0,
        max_value=10.0,
    )
    
    changed, self.color = imgui.color_edit3("Color", *self.color )
    changed, self.text = imgui.input_text("Text", self.text, 256)
    
    imgui.end()
    
    imgui.begin("Win2", True)
    imgui.end()

    self.imgui_handler.endFrame()

  ##############################################
  # _clearPanel - clear the panel before drawing
  ##############################################

  def _clearPanel(self, widget):
    # get dimensions of window

    TOPW = self.ezapp.topWidget    
    scr_w = TOPW.width
    scr_h = TOPW.height

    # Draw On Top Of test widget

    wx = widget.x
    wy = scr_h-widget.y2-1 # raw-opengl has origin at bottom left
    ww = widget.width
    wh = widget.height    

    glDrawBuffers([GL_BACK_LEFT])
    glBindFramebuffer(GL_FRAMEBUFFER, 0)
    glScissor(wx,wy,ww,wh)
    glViewport(wx,wy,ww,wh)
    glEnable(GL_SCISSOR_TEST)
    glClearColor(0.0, 0.1, 0.1, 1.0)
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE)
    glDepthMask(GL_FALSE)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)

###############################################################################

UiTestApp().ezapp.mainThreadLoop()
