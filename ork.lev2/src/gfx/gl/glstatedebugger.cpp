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
#include <string>
#include <vector>
#include <cctype>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

enum TokenType {
  Keyword,
  Identifier,
  Number,
  String,
  Operator,
  Punctuation,
  Whitespace,
  Comment,
  Preprocessor,
  BuiltInMethod,
  BuiltInType,
  Unknown
};

struct Token {
  TokenType type;
  std::string text;
};

struct Keywords{
  std::unordered_set<std::string> _keywords;
  std::unordered_set<std::string> _builtinmethods;
  std::unordered_set<std::string> _builtintypes;
  Keywords(){
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
};

std::vector<Token> LexLine(const std::string& line) {

  static auto KW = std::make_shared<Keywords>();

  std::vector<Token> tokens;
  size_t pos = 0;
  size_t length = line.length();

  while (pos < length) {
    char current = line[pos];

    // Handle whitespace
    if (isspace(current)) {
      size_t start = pos;
      while (pos < length && isspace(line[pos])) pos++;
      tokens.push_back({Whitespace, line.substr(start, pos - start)});
    }
    // Handle c++comments
    else if (current == '/' && pos + 1 < length && line[pos + 1] == '/') {
      tokens.push_back({Comment, line.substr(pos)});
      break;  // Rest of the line is a comment
    }
    // Handle C comments
    else if (current == '/' && pos + 1 < length && line[pos + 1] == '*') {
      size_t start = pos;
      pos += 2;
      while (pos + 1 < length && !(line[pos] == '*' && line[pos + 1] == '/')) pos++;
      if (pos + 1 < length) pos += 2;  // Include closing */
      tokens.push_back({Comment, line.substr(start, pos - start)});
    }
    // Handle preprocessor directives
    else if (current == '#') {
      size_t start = pos;
      while (pos < length && line[pos] != '\n') pos++;
      tokens.push_back({Preprocessor, line.substr(start, pos - start)});
    }
    // Handle identifiers and keywords
    else if (isalpha(current) || current == '_') {
      size_t start = pos;
      while (pos < length && (isalnum(line[pos]) || line[pos] == '_')) pos++;
      std::string text = line.substr(start, pos - start);
      TokenType type;
      auto it = KW->_keywords.find(text);
      // Simple keyword check
      if (it!=KW->_keywords.end()) {
        type = Keyword;
      } else {
        it = KW->_builtinmethods.find(text);
        if (it!=KW->_builtinmethods.end()) {
          type = BuiltInMethod;
        } else {
          it = KW->_builtintypes.find(text);
          if (it!=KW->_builtintypes.end()) {
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
      while (pos < length && isdigit(line[pos])) pos++;
      tokens.push_back({Number, line.substr(start, pos - start)});
    }
    // Handle strings
    else if (current == '"' || current == '\'') {
      char quote = current;
      size_t start = pos++;
      while (pos < length && line[pos] != quote) pos++;
      if (pos < length) pos++;  // Include closing quote
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

ftxui::Decorator GetTokenDecorator(TokenType type) {
  using namespace ftxui;
  switch (type) {
    case Keyword:
      return bold | color(Color::Yellow);
    case Identifier:
      return color(Color(192,192,255));
    case Number:
      return color(Color::Magenta);
    case String:
      return color(Color(255,192,64));
    case Comment:
      return dim | color(Color::Green);
    case Operator:
    case Punctuation:
      return color(Color::White);
    case Preprocessor:
      return color(Color::Cyan);
    case BuiltInMethod:
      return color(Color(128,192,255));
    case BuiltInType:
      return color(Color(192,255,255));
    case Whitespace:
      return nothing;
    default:
      return color(Color::Default);
  }
}

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

struct ShaderAttrib {
  std::string _name;
  GLint _size  = 0;
  GLenum _type = GL_NONE;
};
using shader_attrib_ptr_t = std::shared_ptr<ShaderAttrib>;

struct _FtxGlDebugger {

  _FtxGlDebugger();
  void run_loop();

  std::shared_ptr<ftxui::ScreenInteractive> _fxtui_screen;
  ftxui::component_ptr_t _comp_top;
  ftxui::node_ptr_t _node_framebuffer;
  ftxui::node_ptr_t _node_shader;
  // ftxui::node_ptr_t _node_shadertext;
  ftxui::node_ptr_t _node_geometry;
  using shader_text_t = std::vector<std::string>;

  std::vector<shader_attrib_ptr_t> _shader_attribs;
  std::map<std::string,shader_text_t> _shader_texts;
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
  NODES.push_back(
      text(out_str) | color(Color::RGB(foreground.r, foreground.g, foreground.b)) |
      bgcolor(Color::RGB(background.r, background.g, background.b)));
}

/////////////////////////////////////////////////////////////////////////

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
    }
  };
  return Make<CodeViewerImpl>(lines);
}

///////////////////////////////////////////////////////////////////////////////

static ftxui::component_ptr_t Wrap(std::string name, ftxui::component_ptr_t comp_in) {
  using namespace ftxui;
  return Renderer(comp_in, [name, comp_in] {
    return vbox({
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

  int num_shader_texts = _shader_texts.size();

  std::vector<std::string> menu_entries = {
      "FrameBufferState",
      "GeometryState",
      "ShaderState",
  };

  auto content_framebuffer = Renderer([&] { return _node_framebuffer | vscroll_indicator | frame; });
  auto content_geometry    = Renderer([&] { return _node_geometry | vscroll_indicator | frame; });

  auto content_shader = Renderer([&] { return _node_shader | vscroll_indicator | frame; });

  std::vector<Component> content_components;
  content_components.push_back(content_framebuffer);
  content_components.push_back(content_geometry);
  content_components.push_back(content_shader);
  for (auto it : _shader_texts) {
    std::string name = it.first;
    const auto& sh_lines = it.second;
    menu_entries.push_back(name);
    auto cview     = code_viewer(sh_lines);
    content_components.push_back(cview);
  }

  int menu_selected      = 0;
  auto content_container = Container::Tab(content_components, &menu_selected);

  auto menu = Menu(&menu_entries, &menu_selected);
  // menu      = Wrap("OrkGLD", menu);

  auto layout = Container::Vertical({menu, content_container});

  _comp_top = Renderer(layout, [&] {
    return vbox({
               menu->Render(),
               separator(),
               content_container->Render(),
           }) |
           xflex | size(WIDTH, GREATER_THAN, 40) | border;
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

void ContextGL::_validateCurrentShaderProgram() const {

  using namespace ftxui;

  auto debugger = _debugger.getShared<_FtxGlDebugger>();

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
  OrkAssert(linkStatus == GL_TRUE);
  OrkAssert(validateStatus == GL_TRUE);
  // validate all bound shader parameters are valid
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
    shattrib->_name = nameBuffer;
    debugger->_shader_attribs.push_back(shattrib);
  }

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
      fg      = WHI;
      bg      = BLK;
      out_str = FormatString("  %d : %s : loc<%d> size<%d> value: %s\n", i, name, location, size, value_str.c_str());
    }
    _colortext(NODES, fg, bg, "%s", out_str.c_str());
  }
  debugger->_node_shader = vbox({
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
    debugger->_shader_texts[shader_type_str] = sh_lines;
  }
  ////////////////////////
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_validateCurrentFramebuffer() const {

  using namespace ftxui;

  GLint currentFBO = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
  GLint status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  node_vect_t NODES;
  bool complete = (status == GL_FRAMEBUFFER_COMPLETE);

  _colortext(NODES, WHI, BLK, "currentFBO<%d> status<%x> complete<%d>\n", currentFBO, status, int(complete));
  _debugger.getShared<_FtxGlDebugger>()->_node_framebuffer = vbox({
      text("Framebuffer State"),
      separator(),
      vbox(std::move(NODES)),
  });
}

/////////////////////////////////////////////////////////////////////////

void ContextGL::_validateCurrentGeomBuffers() const {

  using namespace ftxui;

  auto debugger = _debugger.getShared<_FtxGlDebugger>();

  // validate that the current shader program is valid

  node_vect_t NODES;
  // validate that the current IBO state is valid
  GLint currentIBO = 0;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &currentIBO);
  printf("currentIBO<%d>\n", currentIBO);
  _colortext(NODES, YEL, BLK, "currentIBO<%d>", currentIBO);
  OrkAssert(currentIBO != 0);
  // validate that the current VAO state is valid
  GLint currentVAO = 0;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
  // get VAO khr_debug name
  constexpr size_t kMaxNameLength = 256;
  std::vector<GLchar> vaoName(kMaxNameLength);
  glGetObjectLabelEXT(GL_VERTEX_ARRAY_OBJECT_EXT, currentVAO, vaoName.size(), nullptr, &vaoName[0]);
  std::string vaoNameStr(vaoName.begin(), vaoName.end());

  _colortext(NODES, YEL, BLK, "currentVAO<%d:%s>", currentVAO, vaoNameStr.c_str());
  OrkAssert(currentVAO != 0);
  // validate all bound VBOs are valid
  GLint numAttribs = 0;
  glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numAttribs);
  _colortext(NODES, YEL, BLK, "numAttribs<%d>", numAttribs);
  int j = 0;
  for (int i = 0; i < numAttribs; i++) {
    GLint currentVBO = 0;
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &currentVBO);
    // get attrib format
    GLint isEnabled = 0;
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &isEnabled);
    if (not isEnabled)
      continue;
    GLint size           = 0;
    GLenum type          = GL_NONE;
    GLboolean normalized = GL_FALSE;
    GLint stride         = 0;
    GLvoid* offset       = 0;

    auto shattrib = debugger->_shader_attribs[j];

    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &size);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint*)&type);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, (GLint*)&normalized);
    glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &stride);
    glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &offset);

    auto type_str = _glTypeToString(shattrib->_type);

    _colortext(
        NODES,
        YEL,
        BLK,
        "  attrib<%d:%s> vbo<%d> size<%d> type<%s> normalized<%d> stride<%d> offset<%u>\n",
        j,
        shattrib->_name.c_str(),
        currentVBO,
        size,
        type_str.c_str(),
        normalized,
        stride,
        size_t(offset));

    j++;
  }
  debugger->_node_geometry = vbox({
      text("Geometry State"),
      separator(),
      vbox(std::move(NODES)),
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
