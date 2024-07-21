#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a UI to a window, delegating some rendering code to PyOpenGL
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

#pip3 install imgui[glfw] pyopengl

import numpy as np
from orkengine.core import *
from orkengine import lev2
from OpenGL.GL import *
import imgui
from imgui.integrations.opengl import ProgrammablePipelineRenderer

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
out vec4 FragColor;
in vec2 TexCoord;

void main() {
  vec2 uv = TexCoord;
  vec2 uvc = uv-vec2(0.5,0.5);
  float d = length(uvc)*10;
  d = mod(d+time*0.3,1.0);
  FragColor = vec4(uv*d, d, 1.0);
}
"""

################################################################################
# Shader Container class
#################################################################################

class Shaders:
  def __init__(self):
    vertex_shader = glCreateShader(GL_VERTEX_SHADER)
    glShaderSource(vertex_shader, vertex_shader_source)
    glCompileShader(vertex_shader)
    if not glGetShaderiv(vertex_shader, GL_COMPILE_STATUS):
        raise RuntimeError(glGetShaderInfoLog(vertex_shader).decode())

    # Compile fragment shader
    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER)
    glShaderSource(fragment_shader, fragment_shader_source)
    glCompileShader(fragment_shader)
    if not glGetShaderiv(fragment_shader, GL_COMPILE_STATUS):
        raise RuntimeError(glGetShaderInfoLog(fragment_shader).decode())

    # Link shaders into a program
    shader_program = glCreateProgram()
    glAttachShader(shader_program, vertex_shader)
    glAttachShader(shader_program, fragment_shader)
    glLinkProgram(shader_program)
    if not glGetProgramiv(shader_program, GL_LINK_STATUS):
        raise RuntimeError(glGetProgramInfoLog(shader_program))

    # Clean up shaders (they are linked into our program now and no longer necessary)
    glDeleteShader(vertex_shader)
    glDeleteShader(fragment_shader)
    
    self.par_time = glGetUniformLocation(shader_program, "time")

    self.shader_program = shader_program

################################################################################
# Geometry Container class
#################################################################################

class Geometry:
  def __init__(self):
    # Quad vertices and texture coordinates
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

    # Generate and bind VAO
    VAO = glGenVertexArrays(1)
    glBindVertexArray(VAO)

    # Generate and bind VBO
    VBO = glGenBuffers(1)
    glBindBuffer(GL_ARRAY_BUFFER, VBO)
    glBufferData(GL_ARRAY_BUFFER, vertices.nbytes, vertices, GL_STATIC_DRAW)

    # Generate and bind EBO
    EBO = glGenBuffers(1)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO)
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices, GL_STATIC_DRAW)

    # Specify the layout of the vertex data
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * vertices.itemsize, ctypes.c_void_p(0))
    glEnableVertexAttribArray(0)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * vertices.itemsize, ctypes.c_void_p(3 * vertices.itemsize))
    glEnableVertexAttribArray(1)

    # Unbind the VBO, VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0)
    glBindVertexArray(0)
    
    self.VAO = VAO

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

  ##############################################
  # onUpdate - called from update / simulation thread
  ##############################################

  def onUpdate(self,updev):
    self.time = updev.absolutetime*self.rate

  ##############################################
  def onOverlayUiEvent(self,uievent):
    handler = self.uicontext.overlayWidget
    if uievent.code == tokens.KEY_DOWN.hashed:
      #print("KEY_DOWN :", uievent )
      pass
    elif uievent.code == tokens.KEY_UP.hashed:
      #print("KEY_UP :", uievent )
      pass
    elif uievent.code == tokens.PUSH.hashed:
      #print("PUSHED :", uievent )
      self.imgui_io.mouse_down[0] = True
      if not self.imgui_io.want_capture_mouse:
        #print("PUSHED -> native :", uievent )
        handler = None
        #custom_mouse_button_handler(button, action, mods)
      pass
    elif uievent.code == tokens.RELEASE.hashed:
      self.imgui_io.mouse_down[0] = False
      if not self.imgui_io.want_capture_mouse:
        #print("RELEASED :", uievent )
        handler = None
      pass
    elif uievent.code == tokens.BEGIN_DRAG.hashed:
      #print("BEGIN_DRAG :", uievent )
      pass
    elif uievent.code == tokens.END_DRAG.hashed:
      #print("END_DRAG :", uievent )
      pass
    elif uievent.code == tokens.DRAG.hashed:
      self.imgui_io.mouse_pos = uievent.x, uievent.y
      if not self.imgui_io.want_capture_mouse:
        #print("DRAG :", uievent )
        handler = None
      pass
    elif uievent.code == tokens.MOVE.hashed:
      self.imgui_io.mouse_pos = uievent.x, uievent.y
      if not self.imgui_io.want_capture_mouse:
        #print("MOVE :", uievent )
        handler = None
      pass
            
    result = lev2.ui.HandlerResult()
    result.setHandler(handler)
    return result

  ##############################################
  # onGpuInit - called once at startup (in rendering thread)
  #  this is where pyopengl initialization should be done
  ##############################################

  def onGpuInit(self,ctx):
    self.shaders = Shaders()
    self.geometry = Geometry()
    imgui.create_context()
    self.imgui_io = imgui.get_io()
    self.imgui_io.fonts.get_tex_data_as_rgba32()
    self.imgui_renderer = ProgrammablePipelineRenderer()

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

    TOPW = self.ezapp.topWidget    
    scr_w = TOPW.width
    scr_h = TOPW.height
    W = self.test_widget
    wx = W.x
    wy = scr_h-W.y2-1 # raw-opengl has origin at bottom left
    ww = W.width
    wh = W.height    
    
    #self.imgui_io.display_pos = wx, wy
    self.imgui_io.display_size = scr_w, scr_h
    imgui.new_frame()
    imgui.begin("Orkid/PyImGui/PyOpenGL integration example", True)

    clicked, self.rate = imgui.slider_float(
        label="TimeRate",
        value=self.rate,
        min_value=-10.0,
        max_value=10.0,
    )
    imgui.end()

    imgui.render()
    imgui.end_frame()
    self.imgui_renderer.render(imgui.get_draw_data())

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
