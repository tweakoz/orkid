////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/kernel/string/deco.inl>

#include "gl.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct _FtxGlDebugger {

  _FtxGlDebugger();
  void run_loop();

  std::shared_ptr<ftxui::ScreenInteractive> _fxtui_screen;
  ftxui::component_ptr_t _comp_top;
  ftxui::node_ptr_t _node_framebuffer;
  ftxui::node_ptr_t _node_shader;
  ftxui::node_ptr_t _node_geometry;
};

/////////////////////////////////////////////////////////////////////////

struct irgb {
  int r;
  int g;
  int b;
};
static const irgb RED = irgb{255, 0, 0};
static const irgb YEL = irgb{255, 255, 0};
static const irgb BLK = irgb{0, 0, 0};
static const irgb WHI = irgb{255, 255, 255};

void _colortext(ftxui::node_vect_t& NODES, irgb foreground, irgb background, const char* formatstring, ...) {
  using namespace ftxui;
  char out_str[512];
  va_list args;
  va_start(args, formatstring);
  vsnprintf(&out_str[0], sizeof(out_str), formatstring, args);
  va_end(args);
  NODES.push_back(text(out_str) | color(Color::RGB(foreground.r, foreground.g, foreground.b)) |
         bgcolor(Color::RGB(background.r, background.g, background.b)));
}

///////////////////////////////////////////////////////////////////////////////

static ftxui::component_ptr_t Wrap(std::string name, ftxui::component_ptr_t comp_in) {
  using namespace ftxui;
  return Renderer(comp_in, [name, comp_in] {
    return hbox({
               text(name) | size(WIDTH, EQUAL, 8),
               separator(),
               comp_in->Render() | xflex,
           }) |
           xflex;
  });
}

///////////////////////////////////////////////////////////////////////////////

_FtxGlDebugger::_FtxGlDebugger() {
  using namespace ftxui;
}

///////////////////////////////////////////////////////////////////////////////

void _FtxGlDebugger::run_loop() {
  using namespace ftxui;

  const std::vector<std::string> menu_entries = {
      "FrameBufferState",
      "ShaderState",
      "GeometryState",
  };
  int menu_selected = 0;
  auto menu         = Menu(&menu_entries, &menu_selected);
  menu              = Wrap("OrkGLD", menu);

  auto layout = Container::Vertical({menu});

  _comp_top = Renderer(layout, [&] {
    if (menu_selected == 0) {
      return vbox({
                 menu->Render(),
                 separator(),
                 _node_framebuffer,
             }) |
             xflex | size(WIDTH, GREATER_THAN, 40) | border;
    } else if (menu_selected == 1) {
      return vbox({
                 menu->Render(),
                 separator(),
                 _node_shader,
             }) |
             xflex | size(WIDTH, GREATER_THAN, 40) | border;
    } else if (menu_selected == 2) {
      return vbox({
                 menu->Render(),
                 separator(),
                 _node_geometry,
             }) |
             xflex | size(WIDTH, GREATER_THAN, 40) | border;
    } else {
      OrkAssert(false);
    }
  });

  int w           = Dimension::Full().dimx;
  int h           = Dimension::Full().dimy;
  auto fullscreen = ScreenInteractive::Dimension::Fullscreen;
  _fxtui_screen   = std::make_shared<ScreenInteractive>(w, h, fullscreen, false);

  _fxtui_screen->Loop(_comp_top);

  OrkAssert(false);
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_validateAllStates() const {

  _debugger.makeShared<_FtxGlDebugger>();

  _validateCurrentShaderProgram();
  _validateCurrentFramebuffer();
  _validateCurrentGeomBuffers();

  _debugger.getShared<_FtxGlDebugger>()->run_loop();
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_validateCurrentShaderProgram() const {

  using namespace ftxui;

  // validate that the current shader program is valid

  node_vect_t NODES;

  GLint currentProgram = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
  GLint linkStatus = 0;
  glGetProgramiv(currentProgram, GL_LINK_STATUS, &linkStatus);
  glValidateProgram(currentProgram);
  GLint validateStatus = 0;
  glGetProgramiv(currentProgram, GL_VALIDATE_STATUS, &validateStatus);
  _colortext(NODES,WHI, BLK, "currentProgram<%d> linkStatus<%d> validateStatus<%d>\n", currentProgram, linkStatus, validateStatus);
  OrkAssert(linkStatus == GL_TRUE);
  OrkAssert(validateStatus == GL_TRUE);
  // validate all bound shader parameters are valid
  GLint numUniforms = 0;
  glGetProgramiv(currentProgram, GL_ACTIVE_UNIFORMS, &numUniforms);
  _colortext(NODES,WHI, BLK, "numUniforms<%d>\n", numUniforms);

  std::vector<GLuint> uniformIndices(numUniforms);
  std::vector<GLint> uniformBlockIndices(numUniforms);

  for (int i = 0; i < numUniforms; i++) {
    uniformIndices[i] = i;
  }
  glGetActiveUniformsiv(currentProgram, numUniforms, uniformIndices.data(), GL_UNIFORM_BLOCK_INDEX, uniformBlockIndices.data());

  for (int i = 0; i < numUniforms; i++) {
    irgb fg, bg;

    GLint nameLength = 0;
    GLint size       = 0;
    GLenum type      = GL_NONE;
    GLchar name[256];
    glGetActiveUniform(currentProgram, i, sizeof(name), &nameLength, &size, &type, name);
    // print value
    GLint location = glGetUniformLocation(currentProgram, name);
    std::string value_str;
    switch (type) {
      case GL_FLOAT: {
        float value = 0.0f;
        glGetUniformfv(currentProgram, location, &value);
        value_str = FormatString("float(%g)", value);
        break;
      }
      case GL_FLOAT_VEC2: {
        fvec2 value;
        glGetUniformfv(currentProgram, location, value.asArray());
        value_str = FormatString("vec2(%g %g)", value.x, value.y);
        break;
      }
      case GL_FLOAT_VEC3: {
        fvec3 value;
        glGetUniformfv(currentProgram, location, value.asArray());
        value_str = FormatString("vec3(%g %g %g)", value.x, value.y, value.z);
        break;
      }
      case GL_FLOAT_VEC4: {
        fvec4 value;
        glGetUniformfv(currentProgram, location, value.asArray());
        value_str = FormatString("vec4(%g %g %g %g)", value.x, value.y, value.z, value.w);
        break;
      }
      case GL_FLOAT_MAT4: {
        fvec4 value[4];
        glGetUniformfv(currentProgram, location, value[0].asArray());
        glGetUniformfv(currentProgram, location, value[1].asArray());
        glGetUniformfv(currentProgram, location, value[2].asArray());
        glGetUniformfv(currentProgram, location, value[3].asArray());
        value_str = FormatString(
            "mat4(%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g)",
            value[0].x,
            value[0].y,
            value[0].z,
            value[0].w,
            value[1].x,
            value[1].y,
            value[1].z,
            value[1].w,
            value[2].x,
            value[2].y,
            value[2].z,
            value[2].w,
            value[3].x,
            value[3].y,
            value[3].z,
            value[3].w);
        break;
      }
      case GL_FLOAT_MAT3: {
        fvec3 value[3];
        glGetUniformfv(currentProgram, location, value[0].asArray());
        glGetUniformfv(currentProgram, location, value[1].asArray());
        glGetUniformfv(currentProgram, location, value[2].asArray());
        value_str = FormatString(
            "mat3(%g %g %g %g %g %g %g %g %g)",
            value[0].x,
            value[0].y,
            value[0].z,
            value[1].x,
            value[1].y,
            value[1].z,
            value[2].x,
            value[2].y,
            value[2].z);
        break;
      }
      case GL_INT: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("int(%d)", value);
        break;
      }
      case GL_INT_VEC2: {
        int value[2];
        glGetUniformiv(currentProgram, location, value);
        value_str = FormatString("int2(%d %d)", value[0], value[1]);
        break;
      }
      case GL_INT_VEC3: {
        int value[3];
        glGetUniformiv(currentProgram, location, value);
        value_str = FormatString("int3(%d %d %d)", value[0], value[1], value[2]);
        break;
      }
      case GL_SAMPLER_2D: {
        int unit = 0;
        glGetUniformiv(currentProgram, location, &unit);
        // query texture array dimensions of texture @ location (width, height, depth, nummips)
        // use statless query to get texture array size, or restore state after query

        // store current texture binding
        GLint currentTexture = 0;
        glGetIntegeri_v(GL_TEXTURE_BINDING_2D, unit, &currentTexture);
        // bind texture @ location
        glBindTexture(GL_TEXTURE_2D, currentTexture);
        // query texture array size
        GLint width = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        GLint height = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
        // restore texture binding
        glBindTexture(GL_TEXTURE_2D, currentTexture);

        value_str = FormatString("samp2D(unit: %d tobj: %d dim<%dx%d>)", unit, currentTexture, width, height);
        break;
      }
      case GL_SAMPLER_3D: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp3d(%d)", value);
        break;
      }
      case GL_SAMPLER_CUBE: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampCUBE(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_SHADOW: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2DSHAD(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_ARRAY: {
        int unit = 0;
        glGetUniformiv(currentProgram, location, &unit);
        // value represents the texture unit

        // query texture array dimensions of texture @ unit (width, height, depth, nummips)
        // use statless query to get texture array size, or restore state after query

        // store current texture binding
        GLint currentTexture = 0;
        glGetIntegeri_v(GL_TEXTURE_BINDING_2D_ARRAY, unit, &currentTexture);
        // bind texture @ location
        glActiveTexture(GL_TEXTURE0 + unit);
        // glBindTexture(GL_TEXTURE_2D_ARRAY, value);
        //  query texture array size
        GLint depth = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_DEPTH, &depth);
        // query w,h
        GLint width = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_WIDTH, &width);
        GLint height = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_HEIGHT, &height);
        // restore texture binding
        glBindTexture(GL_TEXTURE_2D_ARRAY, currentTexture);

        value_str = FormatString("samp2Darr(unit: %d tobj: %d dim<%dx%dx%d>)", unit, currentTexture, width, height, depth);
        break;
      }
      case GL_SAMPLER_2D_ARRAY_SHADOW: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2DarrSHAD(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_MULTISAMPLE: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2Dmultisamp(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_MULTISAMPLE_ARRAY: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2Dmultisamparr(%d)", value);
        break;
      }
      case GL_SAMPLER_CUBE_SHADOW: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampCUBESHAD(%d)", value);
        break;
      }
      case GL_SAMPLER_1D: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp1D(%d)", value);
        break;
      }
      case GL_SAMPLER_1D_ARRAY: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp1Darr(%d)", value);
        break;
      }
      case GL_SAMPLER_1D_SHADOW: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp1DSHAD(%d)", value);
        break;
      }
      case GL_SAMPLER_BUFFER: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampbuff(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_RECT: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2Drect(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_RECT_SHADOW: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samp2DrectSHAD(%d)", value);
        break;
      }
      case GL_INT_SAMPLER_1D: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("intsamp1D(%d)", value);
        break;
      }

    } // switch(type){

    std::string out_str;
    if (location == -1) {
      int block_index = uniformBlockIndices[i];
      if (block_index == -1) {
        fg = RED;
        bg = BLK;
      } else {
        fg = YEL;
        bg = BLK;
      }
      out_str = FormatString("  %d : %s : BLOCK<%d> size<%d> value: %s\n", i, name, block_index, size, value_str.c_str());
    } else {
      fg = WHI;
      bg = BLK;
      out_str = FormatString("  %d : %s : loc<%d> size<%d> value: %s\n", i, name, location, size, value_str.c_str());
    }
    _colortext(NODES,fg, bg, "%s", out_str.c_str());
  }
  _debugger.getShared<_FtxGlDebugger>()->_node_shader = vbox({
      text("Shader State"),
      separator(),
      vbox(NODES),
  });
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_validateCurrentFramebuffer() const {

  using namespace ftxui;

  GLint currentFBO = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
  GLint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  node_vect_t NODES;
  bool complete = (status == GL_FRAMEBUFFER_COMPLETE);

  _colortext(NODES,WHI,BLK,"currentFBO<%d> status<%x> complete<%d>\n", currentFBO, status, int(complete));
  _debugger.getShared<_FtxGlDebugger>()->_node_framebuffer = vbox({
      text("Framebuffer State"),
      separator(),
      vbox(std::move(NODES)),
  });

}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_validateCurrentGeomBuffers() const {

  using namespace ftxui;

  // validate that the current shader program is valid

  node_vect_t NODES;
  // validate that the current IBO state is valid
  GLint currentIBO = 0;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &currentIBO);
  printf("currentIBO<%d>\n", currentIBO);
  _colortext(NODES,YEL, BLK, "currentIBO<%d>", currentIBO);
  OrkAssert(currentIBO != 0);
  // validate that the current VAO state is valid
  GLint currentVAO = 0;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
  _colortext(NODES,YEL, BLK, "currentVAO<%d>", currentVAO);
  OrkAssert(currentVAO != 0);
  // validate all bound VBOs are valid
  GLint numAttribs = 0;
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numAttribs);
  _colortext(NODES,YEL, BLK, "numAttribs<%d>", numAttribs);
  for (int i = 0; i < numAttribs; i++) {
    GLint currentVBO = 0;
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &currentVBO);
    // get attrib format
    GLint size           = 0;
    GLenum type          = GL_NONE;
    GLboolean normalized = GL_FALSE;
    GLint stride         = 0;
    GLint offset         = 0;
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint*)&type);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, (GLint*)&normalized);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &offset);
    if (currentVBO == 0 and stride == 0)
      continue;
    _colortext(NODES,
        YEL,
        BLK,
        "  attrib<%d> vbo<%d> size<%d> type<%x> normalized<%d> stride<%d> offset<%d>\n",
        i,
        currentVBO,
        size,
        type,
        normalized,
        stride,
        offset);
  }
  _debugger.getShared<_FtxGlDebugger>()->_node_geometry = vbox({
      text("Geometry State"),
      separator(),
      vbox(std::move(NODES)),
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
