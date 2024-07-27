################################################################################
# lev2 PyOpenGL helpers
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

from typing import Any
from OpenGL import GL
from orkengine.core import vec2, vec3, vec4
import numpy as np
import ctypes

################################################################################

class PyShader:
  def __init__(self,vtx_src,frg_src):
    self._bound_params = dict()
    self._shader_params = dict()
    self._param_ranges = dict()
    self._param_types = dict()
    
    ###########################################
    # split frg_src into lines
    ###########################################

    frg_lines = frg_src.splitlines()
    
    ###########################################
    # on lines that begin with "uniform"
    # check for a comment of the form "// range (min,max,step)"
    # and if it exists, add the range to _param_ranges dict
    ###########################################
    
    for item in frg_lines:
      if item.startswith("uniform"):
        if "// range (" in item:
          # get uniform name
          uni_name = item.split(" ")[2]
          # strip ;
          uni_name = uni_name.split(";")[0]
          # find "// range (" and strip it and all before
          range_str = item.split("// range (")[1]
          # find ")" and strip it and all after
          range_str = range_str.split(")")[0]
          # split on comma
          range_vals = range_str.split(",")
          # convert to float
          min_val = float(range_vals[0])
          max_val = float(range_vals[1])
          stp_val = 0.0
          if len(range_vals)==3:
            stp_val = float(range_vals[2])
          # add to dict
          # use app_var name
          app_var_name = "sh_" + uni_name
          print(f"uni_name, app_var_name (min,max,stp): {uni_name} {app_var_name} ({min_val},{max_val},{stp_val})")
          self._param_ranges[uni_name] = (min_val,max_val,stp_val)

    
    ###########################################

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
    
    # enumerate uniforms via GL_ACTIVE_UNIFORMS
    num_uniforms = GL.glGetProgramiv(shader_program, GL.GL_ACTIVE_UNIFORMS)
    print(f"Num Uniforms: {num_uniforms}")
    for i in range(num_uniforms):
      name, size, type = GL.glGetActiveUniform(shader_program, i)
      decoded_name = name.decode()
      # get type string
      type_str = ""
      if type == GL.GL_FLOAT:
        type_str = "float"
      elif type == GL.GL_FLOAT_VEC2:
        type_str = "vec2"
      elif type == GL.GL_FLOAT_VEC3:
        type_str = "vec3"
      elif type == GL.GL_FLOAT_VEC4:
        type_str = "vec4"
      elif type == GL.GL_INT:
        type_str = "int"
      else: 
        assert(False)
      print(f"Uniform: {i} {name} {size} {type_str}")
      var_name = "sh_" + decoded_name
      uni_loc = GL.glGetUniformLocation(shader_program, name)
      self._shader_params[decoded_name] = uni_loc

      self._param_types[var_name] = type_str
      if not decoded_name=="time": # skip time
        self._bound_params[var_name] = uni_loc
    
    self.par_time = GL.glGetUniformLocation(shader_program, "time")
    #self.par_color = GL.glGetUniformLocation(shader_program, "ModColor")

    self._shader_program = shader_program
    #for item in inp_params:
    #  self._shader_params[item] = GL.glGetUniformLocation(shader_program, item)
    
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
