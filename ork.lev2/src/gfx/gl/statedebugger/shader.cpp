////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "statedebug.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

ftxui::component_ptr_t code_viewer(std::vector<std::string> lines) {
  using namespace ftxui;
  class CodeViewerImpl : public ComponentBase {
  private:
    float _scroll_x  = 0.1;
    float _scroll_y  = 0.1;
    size_t _numlines = 0;

  public:
    CodeViewerImpl(std::vector<std::string> lines) {
      _numlines    = lines.size();
      auto content = Renderer([=] {
        std::vector<node_ptr_t> nodes;
        for (const auto& line : lines) {
          nodes.push_back(SyntaxHighlightLine(line));
        }
        return vbox(nodes);
      });

      auto scrollable_content = Renderer(content, [&, content] {
        return content->Render() | focusPositionRelative(_scroll_x, _scroll_y) | frame | size(HEIGHT, EQUAL, 32);
      });

      SliderOption<float> option_y;
      option_y.value          = &_scroll_y;
      option_y.min            = 0.f;
      option_y.max            = 1.f;
      option_y.increment      = 0.1f;
      option_y.direction      = Direction::Down;
      option_y.color_active   = Color::Yellow;
      option_y.color_inactive = Color::YellowLight;
      auto scrollbar_y        = Slider(option_y);

      Add(Container::Horizontal({
              scrollable_content,
              scrollbar_y,
          }) |
          flex);
    }

    bool OnEvent(Event event) override {
      float step = 1.0f / float(_numlines);
      if (event.is_mouse()) {
        if (event.mouse().button == Mouse::WheelUp) {
          _scroll_y = std::max(0.0f, _scroll_y - step);
          return true;
        } else if (event.mouse().button == Mouse::WheelDown) {
          _scroll_y = std::min(1.0f, _scroll_y + step);
          return true;
        }
      }
      return false;
    }
  };
  return Make<CodeViewerImpl>(lines);
}

/////////////////////////////////////////////////////////////////////////

Keywords::Keywords() {
  _keywords.insert("uniform");
  _keywords.insert("varying");
  _keywords.insert("in");
  _keywords.insert("out");
  _keywords.insert("inout");
  _keywords.insert("return");
  _keywords.insert("if");
  _keywords.insert("else");
  _keywords.insert("main");
  _keywords.insert("for");
  _keywords.insert("while");
  _keywords.insert("struct");
  _builtintypes.insert("int");
  _builtintypes.insert("float");
  _builtintypes.insert("void");
  _builtintypes.insert("vec2");
  _builtintypes.insert("vec3");
  _builtintypes.insert("vec4");
  _builtintypes.insert("mat2");
  _builtintypes.insert("mat3");
  _builtintypes.insert("mat4");
  _builtintypes.insert("sampler2D");
  _builtintypes.insert("sampler3D");
  _builtintypes.insert("samplerCube");
  _builtintypes.insert("sampler2DShadow");
  _builtintypes.insert("sampler2DArray");
  _builtintypes.insert("usampler2D");
  _builtintypes.insert("usampler3D");

  _builtinmethods.insert("texture");
  _builtinmethods.insert("textureLod");
  _builtinmethods.insert("textureGrad");
  _builtinmethods.insert("textureSize");
  _builtinmethods.insert("textureProj");
  _builtinmethods.insert("textureProjOffset");
  _builtinmethods.insert("textureProjLod");
  _builtinmethods.insert("textureProjGrad");
  _builtinmethods.insert("textureProjGradOffset");
  _builtinmethods.insert("textureLodOffset");
  _builtinmethods.insert("textureGradOffset");
  _builtinmethods.insert("textureProjLodOffset");
  _builtinmethods.insert("textureProjGradOffset");
  _builtinmethods.insert("normalize");
  _builtinmethods.insert("length");
  _builtinmethods.insert("dot");
  _builtinmethods.insert("cross");
  _builtinmethods.insert("reflect");
  _builtinmethods.insert("refract");
  _builtinmethods.insert("faceforward");
  _builtinmethods.insert("mix");
  _builtinmethods.insert("step");
  _builtinmethods.insert("smoothstep");
  _builtinmethods.insert("pow");
  _builtinmethods.insert("exp");
  _builtinmethods.insert("log");
  _builtinmethods.insert("exp2");
  _builtinmethods.insert("log2");
  _builtinmethods.insert("sqrt");
  _builtinmethods.insert("inversesqrt");
  _builtinmethods.insert("abs");
  _builtinmethods.insert("sign");
}

///////////////////////////////////////////////////////////////////////////////

std::vector<Token> LexLine(const std::string& line) {

  static auto KW = std::make_shared<Keywords>();

  std::vector<Token> tokens;
  size_t pos    = 0;
  size_t length = line.length();

  while (pos < length) {
    char current = line[pos];

    // Handle whitespace
    if (isspace(current)) {
      size_t start = pos;
      while (pos < length && isspace(line[pos]))
        pos++;
      tokens.push_back({Whitespace, line.substr(start, pos - start)});
    }
    // Handle c++comments
    else if (current == '/' && pos + 1 < length && line[pos + 1] == '/') {
      tokens.push_back({Comment, line.substr(pos)});
      break; // Rest of the line is a comment
    }
    // Handle C comments
    else if (current == '/' && pos + 1 < length && line[pos + 1] == '*') {
      size_t start = pos;
      pos += 2;
      while (pos + 1 < length && !(line[pos] == '*' && line[pos + 1] == '/'))
        pos++;
      if (pos + 1 < length)
        pos += 2; // Include closing */
      tokens.push_back({Comment, line.substr(start, pos - start)});
    }
    // Handle preprocessor directives
    else if (current == '#') {
      size_t start = pos;
      while (pos < length && line[pos] != '\n')
        pos++;
      tokens.push_back({Preprocessor, line.substr(start, pos - start)});
    }
    // Handle identifiers and keywords
    else if (isalpha(current) || current == '_') {
      size_t start = pos;
      while (pos < length && (isalnum(line[pos]) || line[pos] == '_'))
        pos++;
      std::string text = line.substr(start, pos - start);
      TokenType type;
      auto it = KW->_keywords.find(text);
      // Simple keyword check
      if (it != KW->_keywords.end()) {
        type = Keyword;
      } else {
        it = KW->_builtinmethods.find(text);
        if (it != KW->_builtinmethods.end()) {
          type = BuiltInMethod;
        } else {
          it = KW->_builtintypes.find(text);
          if (it != KW->_builtintypes.end()) {
            type = BuiltInType;
          } else {
            type = Identifier;
          }
        }
      }
      tokens.push_back({type, text});
    }
    // Handle numbers
    else if (isdigit(current)) {
      size_t start = pos;
      while (pos < length && isdigit(line[pos]))
        pos++;
      tokens.push_back({Number, line.substr(start, pos - start)});
    }
    // Handle strings
    else if (current == '"' || current == '\'') {
      char quote   = current;
      size_t start = pos++;
      while (pos < length && line[pos] != quote)
        pos++;
      if (pos < length)
        pos++; // Include closing quote
      tokens.push_back({String, line.substr(start, pos - start)});
    }
    // Handle operators and punctuation
    else {
      tokens.push_back({Punctuation, std::string(1, current)});
      pos++;
    }
  }

  return tokens;
}

///////////////////////////////////////////////////////////////////////////////

ftxui::Decorator GetTokenDecorator(TokenType type) {
  using namespace ftxui;
  switch (type) {
    case Keyword:
      return bold | color(Color::Yellow);
    case Identifier:
      return color(Color(192, 192, 255));
    case Number:
      return color(Color::Magenta);
    case String:
      return color(Color(255, 192, 64));
    case Comment:
      return dim | color(Color::Green);
    case Operator:
    case Punctuation:
      return color(Color::White);
    case Preprocessor:
      return color(Color::Cyan);
    case BuiltInMethod:
      return color(Color(128, 192, 255));
    case BuiltInType:
      return color(Color(192, 255, 255));
    case Whitespace:
      return nothing;
    default:
      return color(Color::Default);
  }
}

///////////////////////////////////////////////////////////////////////////////

ftxui::Element SyntaxHighlightLine(const std::string& line) {
  using namespace ftxui;
  std::vector<Token> tokens = LexLine(line);
  std::vector<Element> elements;

  for (const auto& token : tokens) {
    Decorator decorator = GetTokenDecorator(token.type);
    elements.push_back(text(token.text) | decorator);
  }

  return hbox(std::move(elements));
}

///////////////////////////////////////////////////////////////////////////////

void _FtxGlDebugger::_validateCurrentShaderProgram() {

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
  _colortext(NODES, WHI, BLK, "currentProgram<%d> linkStatus<%d> validateStatus<%d>\n", currentProgram, linkStatus, validateStatus);
  if (0 == validateStatus) {
    GLint infolen = 0;
    glGetProgramiv(currentProgram, GL_INFO_LOG_LENGTH, &infolen);
    std::vector<char> infolog(infolen);
    glGetProgramInfoLog(currentProgram, infolen, &infolen, infolog.data());
    _colortext_wrap(NODES, RED, BLK, "ProgramInfoLog<%s>\n", infolog.data());
  }
  // OrkAssert(linkStatus == GL_TRUE);
  // OrkAssert(validateStatus == GL_TRUE);
  //  validate all bound shader parameters are valid
  GLint numUniforms = 0;
  glGetProgramiv(currentProgram, GL_ACTIVE_UNIFORMS, &numUniforms);
  _colortext(NODES, WHI, BLK, "numUniforms<%d>\n", numUniforms);

  GLint numActiveAttribs = 0;
  glGetProgramiv(currentProgram, GL_ACTIVE_ATTRIBUTES, &numActiveAttribs);

  for (int i = 0; i < numActiveAttribs; i++) {
    char nameBuffer[256];   // Buffer for attribute name
    GLsizei nameLength = 0; // Actual length of the attribute name

    auto shattrib = std::make_shared<ShaderAttrib>();
    GLint size    = 0; // Size of the attribute
    GLenum type   = 0; // Type of the attribute

    // Retrieve attribute information
    glGetActiveAttrib(currentProgram, i, sizeof(nameBuffer), &nameLength, &shattrib->_size, &shattrib->_type, nameBuffer);
    shattrib->_name                                = nameBuffer;
    shattrib->_location                            = glGetAttribLocation(currentProgram, nameBuffer);
    _shader_attribs[shattrib->_location] = shattrib;
  }

  std::vector<GLuint> uniformIndices(numUniforms);
  std::vector<GLint> uniformBlockIndices(numUniforms);

  for (int i = 0; i < numUniforms; i++) {
    uniformIndices[i] = i;
  }
  glGetActiveUniformsiv(currentProgram, numUniforms, uniformIndices.data(), GL_UNIFORM_BLOCK_INDEX, uniformBlockIndices.data());

  for (int i = 0; i < numUniforms; i++) {
    irgb fg, bg;

    fg = WHI;
    bg = BLK;

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
        fg = irgb{255, 255, 96};
        glGetUniformfv(currentProgram, location, value.asArray());
        value_str = FormatString("vec2(%g %g)", value.x, value.y);
        break;
      }
      case GL_FLOAT_VEC3: {
        fvec3 value;
        fg = irgb{255, 255, 128};
        glGetUniformfv(currentProgram, location, value.asArray());
        value_str = FormatString("vec3(%g %g %g)", value.x, value.y, value.z);
        break;
      }
      case GL_FLOAT_VEC4: {
        fvec4 value;
        fg = irgb{255, 255, 192};
        glGetUniformfv(currentProgram, location, value.asArray());
        value_str = FormatString("vec4(%g %g %g %g)", value.x, value.y, value.z, value.w);
        break;
      }
      case GL_FLOAT_MAT4: {
        fmtx4 mtx;
        fg = irgb{128, 192, 192};
        glGetUniformfv(currentProgram, location, mtx.asArray());
        value_str = FormatString(
            "mat4(%g %g %g %g %g %g %g %g %g %g %g %g %g %g %g %g)",
            mtx.column(0).x,
            mtx.column(0).y,
            mtx.column(0).z,
            mtx.column(0).w,
            mtx.column(1).x,
            mtx.column(1).y,
            mtx.column(1).z,
            mtx.column(1).w,
            mtx.column(2).x,
            mtx.column(2).y,
            mtx.column(2).z,
            mtx.column(2).w,
            mtx.column(3).x,
            mtx.column(3).y,
            mtx.column(3).z,
            mtx.column(3).w);
        break;
      }
      case GL_FLOAT_MAT3: {
        fmtx3 mtx;
        fg = irgb{128, 128, 255};
        glGetUniformfv(currentProgram, location, mtx.asArray());
        value_str = FormatString(
            "mat3(%g %g %g %g %g %g %g %g %g)",
            mtx.column(0).x,
            mtx.column(0).y,
            mtx.column(0).z,
            mtx.column(1).x,
            mtx.column(1).y,
            mtx.column(1).z,
            mtx.column(2).x,
            mtx.column(2).y,
            mtx.column(2).z);
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
        fg                 = irgb{255, 128, 255};
        int tex_unit       = 0;
        int current_active = -1;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &current_active);
        glGetUniformiv(currentProgram, location, &tex_unit);
        glActiveTexture(GL_TEXTURE0 + tex_unit);
        GLint currentTexture = 0;
        GLint width          = 0;
        GLint height         = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTexture);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
        auto it = _glctx->mTxI._texture_set.find(currentTexture);
        std::string texname = "???";
        if (it != _glctx->mTxI._texture_set.end()) {
          auto the_tex = it->second;
          auto glto    = the_tex->_impl.get<gltexobj_ptr_t>();
          if (glto) {
          }
          texname = the_tex->_debugName;
        }
        value_str = FormatString("sampler2D(unit: %d dim<%dx%d> tobj: %d<%s>)", tex_unit, width, height, currentTexture, texname.c_str() );
        glActiveTexture(GL_TEXTURE0 + current_active);
        break;
      }
      case GL_SAMPLER_3D: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampler3D(%d)", value);
        break;
      }
      case GL_SAMPLER_CUBE: {
        fg                 = irgb{255, 64, 255};
        int tex_unit       = 0;
        int current_active = -1;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &current_active);
        glGetUniformiv(currentProgram, location, &tex_unit);
        GLint currentTexture = 0;
        glGetIntegeri_v(GL_TEXTURE_BINDING_CUBE_MAP, tex_unit, &currentTexture);
        glActiveTexture(GL_TEXTURE0 + tex_unit);
        GLint width  = 0;
        GLint height = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_HEIGHT, &height);
        value_str = FormatString("samplerCube(unit: %d tobj: %d dim<%dx%d>)", tex_unit, currentTexture, width, height);
        glActiveTexture(GL_TEXTURE0 + current_active);
        break;
      }
      case GL_SAMPLER_2D_SHADOW: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampler2DShadow(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_ARRAY: {
        fg                 = irgb{255, 192, 255};
        int tex_unit       = 0;
        int current_active = -1;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &current_active);
        glGetUniformiv(currentProgram, location, &tex_unit);
        GLint currentTexture = 0;
        glActiveTexture(GL_TEXTURE0 + tex_unit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &currentTexture);
        GLint width  = 0;
        GLint height = 0;
        GLint depth  = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_HEIGHT, &height);
        glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_DEPTH, &depth);
        std::string texname = "???";
        auto it = _glctx->mTxI._texture_set.find(currentTexture);
        if (it != _glctx->mTxI._texture_set.end()) {
          auto the_tex = it->second;
          auto glto    = the_tex->_impl.get<gltexobj_ptr_t>();
          if (glto) {
          }
          texname = the_tex->_debugName;
        }
        value_str = FormatString("sampler2Darray(unit: %d dim<%dx%dx%d> tobj: %d<%s>)", tex_unit, width, height, depth, currentTexture, texname.c_str() );
        glActiveTexture(GL_TEXTURE0 + current_active);
        break;
      }
      case GL_SAMPLER_2D_ARRAY_SHADOW: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampler2DArrayShadow(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_MULTISAMPLE: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampler2DMS(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_MULTISAMPLE_ARRAY: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampler2DMSArray(%d)", value);
        break;
      }
      case GL_SAMPLER_CUBE_SHADOW: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samplerCubeArrayShadow(%d)", value);
        break;
      }
      case GL_SAMPLER_1D: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampler1D(%d)", value);
        break;
      }
      case GL_SAMPLER_1D_ARRAY: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampler1Darray(%d)", value);
        break;
      }
      case GL_SAMPLER_1D_SHADOW: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampler1DShadow(%d)", value);
        break;
      }
      case GL_SAMPLER_BUFFER: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("samplerBuffer(%d)", value);
        break;
      }
      case GL_SAMPLER_2D_RECT: {
        int value = 0;
        glGetUniformiv(currentProgram, location, &value);
        value_str = FormatString("sampler2Drect(%d)", value);
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
        fg = WHI;
        bg = irgb{96, 0, 0};
      } else {
        fg = WHI;
        bg = irgb{96, 0, 96};
      }
      out_str = FormatString(" %02d BLOCK%02d SIZ%02d : %32s  :  %s\n", i, block_index, size, name, value_str.c_str());
    } else {
      out_str = FormatString(" %02d LOC%02d   SIZ%02d : %32s  :  %s\n", i, location, size, name, value_str.c_str());
    }
    _colortext(NODES, fg, bg, "%s", out_str.c_str());
  }
  _node_shader = vbox({
      text("Shader State"),
      separator(),
      vbox(NODES),
  });

  ////////////////////////
  // get shader text
  ////////////////////////

  GLint shaderCount = 0;
  glGetProgramiv(currentProgram, GL_ATTACHED_SHADERS, &shaderCount);
  std::vector<GLuint> shaders(shaderCount);
  glGetAttachedShaders(currentProgram, shaderCount, nullptr, shaders.data());

  std::vector<GLchar> shaderSource;
  GLint shaderSourceLength = 0;
  std::vector<component_ptr_t> NODES2;
  for (GLuint shader : shaders) {
    glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &shaderSourceLength);
    shaderSource.resize(shaderSourceLength);
    glGetShaderSource(shader, shaderSourceLength, nullptr, shaderSource.data());
    std::string shaderSourceStr(shaderSource.begin(), shaderSource.end());
    auto sh_lines = SplitString(shaderSourceStr, '\n');
    // get type of shader
    GLint shaderType = 0;
    glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);
    std::string shader_type_str;
    switch (shaderType) {
      case GL_VERTEX_SHADER:
        shader_type_str = "VtxShader";
        break;
      case GL_FRAGMENT_SHADER:
        shader_type_str = "FrgShader";
        break;
      case GL_GEOMETRY_SHADER:
        shader_type_str = "GeoShader";
        break;
      case GL_TESS_CONTROL_SHADER:
        shader_type_str = "TesCtrlShader";
        break;
      case GL_TESS_EVALUATION_SHADER:
        shader_type_str = "TesEvalShader";
        break;
      default:
        break;
    }
    _shader_texts[shader_type_str] = sh_lines;
  }
  ////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
