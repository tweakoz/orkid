################################################################################
# lev2 PyOpenGL helpers
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

from typing import Any
from OpenGL import GL
import numpy as np
import ctypes

################################################################################

class PyShader:
  def __init__(self,vtx_src,frg_src,inp_params):
    vertex_shader = GL.glCreateShader(GL.GL_VERTEX_SHADER)
    GL.glShaderSource(vertex_shader, vtx_src)
    GL.glCompileShader(vertex_shader)
    if not GL.glGetShaderiv(vertex_shader, GL.GL_COMPILE_STATUS):
        raise RuntimeError(GL.glGetShaderInfoLog(vertex_shader).decode())

    # Compile fragment shader
    fragment_shader = GL.glCreateShader(GL.GL_FRAGMENT_SHADER)
    GL.glShaderSource(fragment_shader, frg_src)
    GL.glCompileShader(fragment_shader)
    if not GL.glGetShaderiv(fragment_shader, GL.GL_COMPILE_STATUS):
        raise RuntimeError(GL.glGetShaderInfoLog(fragment_shader).decode())

    # Link shaders into a program
    shader_program = GL.glCreateProgram()
    GL.glAttachShader(shader_program, vertex_shader)
    GL.glAttachShader(shader_program, fragment_shader)
    GL.glLinkProgram(shader_program)
    if not GL.glGetProgramiv(shader_program, GL.GL_LINK_STATUS):
        raise RuntimeError(GL.glGetProgramInfoLog(shader_program))

    # Clean up shaders (they are linked into our program now and no longer necessary)
    GL.glDeleteShader(vertex_shader)
    GL.glDeleteShader(fragment_shader)
    
    self.par_time = GL.glGetUniformLocation(shader_program, "time")
    self.par_color = GL.glGetUniformLocation(shader_program, "ModColor")

    self._shader_program = shader_program
    self._shader_params = dict()
    for item in inp_params:
      self._shader_params[item] = GL.glGetUniformLocation(shader_program, item)
    
  def __getattribute__(self, name: str) -> Any:
    if name == "shader_program":
      return self._shader_program
    elif name == "params":
      return self._shader_params[name]
    else:
      return super().__getattribute__(name)
   
################################################################################
   
class GeometryBuffer:
  def __init__(self, vertices, indices):
    # Quad vertices and texture coordinates

    # Generate and bind VAO
    VAO = GL.glGenVertexArrays(1)
    GL.glBindVertexArray(VAO)

    # Generate and bind VBO
    VBO = GL.glGenBuffers(1)
    GL.glBindBuffer(GL.GL_ARRAY_BUFFER, VBO)
    GL.glBufferData(GL.GL_ARRAY_BUFFER, vertices.nbytes, vertices, GL.GL_STATIC_DRAW)

    # Generate and bind EBO
    EBO = GL.glGenBuffers(1)
    GL.glBindBuffer(GL.GL_ELEMENT_ARRAY_BUFFER, EBO)
    GL.glBufferData(GL.GL_ELEMENT_ARRAY_BUFFER, indices.nbytes, indices, GL.GL_STATIC_DRAW)

    # Specify the layout of the vertex data
    GL.glVertexAttribPointer(0, 3, GL.GL_FLOAT, GL.GL_FALSE, 5 * vertices.itemsize, ctypes.c_void_p(0))
    GL.glEnableVertexAttribArray(0)
    GL.glVertexAttribPointer(1, 2, GL.GL_FLOAT, GL.GL_FALSE, 5 * vertices.itemsize, ctypes.c_void_p(3 * vertices.itemsize))
    GL.glEnableVertexAttribArray(1)

    # Unbind the VBO, VAO
    GL.glBindBuffer(GL.GL_ARRAY_BUFFER, 0)
    GL.glBindVertexArray(0)
    
    self.VAO = VAO
