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

// Forward declarations of helper classes and structures
struct SamplerTypeInfo {
  GLenum textureTarget;
  const char* displayName;
};

// Lookup table for sampler types
const std::unordered_map<GLenum, SamplerTypeInfo> samplerTypeMap = {
  { GL_SAMPLER_2D, { GL_TEXTURE_BINDING_2D, "sampler2D" } },
  { GL_SAMPLER_3D, { GL_TEXTURE_BINDING_3D, "sampler3D" } },
  { GL_SAMPLER_CUBE, { GL_TEXTURE_BINDING_CUBE_MAP, "samplerCube" } },
  { GL_SAMPLER_2D_SHADOW, { GL_TEXTURE_BINDING_2D, "sampler2DShadow" } },
  { GL_SAMPLER_2D_ARRAY, { GL_TEXTURE_BINDING_2D_ARRAY, "sampler2Darray" } },
  { GL_SAMPLER_2D_ARRAY_SHADOW, { GL_TEXTURE_BINDING_2D_ARRAY, "sampler2DArrayShadow" } },
  { GL_SAMPLER_2D_MULTISAMPLE, { GL_TEXTURE_BINDING_2D_MULTISAMPLE, "sampler2DMS" } },
  { GL_SAMPLER_2D_MULTISAMPLE_ARRAY, { GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY, "sampler2DMSArray" } },
  { GL_SAMPLER_CUBE_SHADOW, { GL_TEXTURE_BINDING_CUBE_MAP, "samplerCubeShadow" } },
  { GL_SAMPLER_1D, { GL_TEXTURE_BINDING_1D, "sampler1D" } },
  { GL_SAMPLER_1D_ARRAY, { GL_TEXTURE_BINDING_1D_ARRAY, "sampler1Darray" } },
  { GL_SAMPLER_1D_SHADOW, { GL_TEXTURE_BINDING_1D, "sampler1DShadow" } },
  { GL_SAMPLER_BUFFER, { GL_TEXTURE_BINDING_BUFFER, "samplerBuffer" } },
  { GL_SAMPLER_2D_RECT, { GL_TEXTURE_BINDING_RECTANGLE, "sampler2Drect" } },
  { GL_SAMPLER_2D_RECT_SHADOW, { GL_TEXTURE_BINDING_RECTANGLE, "samp2DrectSHAD" } },
  { GL_INT_SAMPLER_1D, { GL_TEXTURE_BINDING_1D, "intsamp1D" } }
};

// Map of shader type to display name
const std::unordered_map<GLenum, std::string> shaderTypeNames = {
  { GL_VERTEX_SHADER, "VtxShader" },
  { GL_FRAGMENT_SHADER, "FrgShader" },
  { GL_GEOMETRY_SHADER, "GeoShader" },
  { GL_TESS_CONTROL_SHADER, "TesCtrlShader" },
  { GL_TESS_EVALUATION_SHADER, "TesEvalShader" }
};

// Map GL types to color coding
const std::unordered_map<GLenum, irgb> typeColorMap = {
  { GL_FLOAT, WHI },
  { GL_FLOAT_VEC2, irgb{255, 255, 96} },
  { GL_FLOAT_VEC3, irgb{255, 255, 128} },
  { GL_FLOAT_VEC4, irgb{255, 255, 192} },
  { GL_INT, WHI },
  { GL_INT_VEC2, WHI },
  { GL_INT_VEC3, WHI },
  { GL_UNSIGNED_INT, WHI },
  { GL_FLOAT_MAT2, irgb{96, 192, 192} },
  { GL_FLOAT_MAT3, irgb{128, 128, 255} },
  { GL_FLOAT_MAT4, irgb{128, 192, 192} },
  { GL_SAMPLER_2D, irgb{255, 128, 255} },
  { GL_SAMPLER_3D, irgb{255, 128, 255} },
  { GL_SAMPLER_CUBE, irgb{255, 64, 255} },
  { GL_SAMPLER_2D_ARRAY, irgb{255, 192, 255} }
};

// Structure to hold data about a GL uniform
struct UniformData {
  GLenum type;
  GLint size;
  GLint location;
  GLint blockIndex;
  GLuint uniformIndex;
  std::string name;
  bool isArray;
};

///////////////////////////////////////////////////////////////////////////////
// Helper functions for uniform data extraction
///////////////////////////////////////////////////////////////////////////////

// Read buffer data for UBO uniform
template <typename T>
bool readUboData(GLuint currentProgram, 
                GLuint uniformIndex, 
                GLint blockIndex, 
                GLint offset,
                T* outData,
                size_t elementCount = 1,
                GLint arrayStride = 0) {
  if (blockIndex == GL_INVALID_INDEX) {
    return false;
  }
  
  // Get which buffer is bound to this block
  GLint boundBuffer = 0;
  glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, blockIndex, &boundBuffer);
  GL_ERRORCHECK();
  
  if (boundBuffer == 0) {
    return false;
  }
  
  // Bind the buffer to read from it
  glBindBuffer(GL_UNIFORM_BUFFER, boundBuffer);
  GL_ERRORCHECK();
  
  // Get the buffer size
  GLint bufferSize = 0;
  glGetBufferParameteriv(GL_UNIFORM_BUFFER, GL_BUFFER_SIZE, &bufferSize);
  GL_ERRORCHECK();
  
  // Verify the offset is within buffer bounds
  if (offset < 0 || offset >= bufferSize) {
    return false;
  }
  
  // Map the buffer
  void* data = glMapBufferRange(GL_UNIFORM_BUFFER, 0, bufferSize, GL_MAP_READ_BIT);
  GL_ERRORCHECK();
  
  if (!data) {
    return false;
  }
  
  const size_t dataSize = sizeof(T);
  
  if (elementCount == 1) {
    // Single element case
    if (offset + dataSize <= bufferSize) {
      memcpy(outData, (char*)data + offset, dataSize);
    } else {
      glUnmapBuffer(GL_UNIFORM_BUFFER);
      glBindBuffer(GL_UNIFORM_BUFFER, 0);
      return false;
    }
  } else {
    // Array case
    for (size_t i = 0; i < elementCount; i++) {
      GLint elemOffset = offset + i * arrayStride;
      if (elemOffset + dataSize <= bufferSize) {
        memcpy(&outData[i], (char*)data + elemOffset, dataSize);
      } else {
        // We've reached the end of valid data
        glUnmapBuffer(GL_UNIFORM_BUFFER);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        return i > 0; // Return true if we read at least one element
      }
    }
  }
  
  glUnmapBuffer(GL_UNIFORM_BUFFER);
  GL_ERRORCHECK();
  
  // Unbind the buffer
  glBindBuffer(GL_UNIFORM_BUFFER, 0);
  
  return true;
}

// Get array stride for UBO uniform
GLint getUboArrayStride(GLuint program, GLuint uniformIndex) {
  GLint arrayStride = 0;
  glGetActiveUniformsiv(program, 1, &uniformIndex, GL_UNIFORM_ARRAY_STRIDE, &arrayStride);
  GL_ERRORCHECK();
  return arrayStride;
}

// Get offset in UBO for uniform
GLint getUboOffset(GLuint program, GLuint uniformIndex) {
  GLint offset = 0;
  glGetActiveUniformsiv(program, 1, &uniformIndex, GL_UNIFORM_OFFSET, &offset);
  GL_ERRORCHECK();
  return offset;
}

std::string formatFloat(float value) {
  // Use a buffer large enough for any float representation
  char buffer[64];
  
  // First check if it's actually an integer value
  if (std::abs(value - std::round(value)) < 1e-7f) {
      // It's an integer - format as integer
      snprintf(buffer, sizeof(buffer), "%.0f", value);
      return buffer;
  }
  
  // Handle reasonable ranges with fixed-point notation
  if (std::abs(value) < 1e6f && std::abs(value) > 1e-5f || value == 0.0f) {
      // Try with different precisions, starting with 6 significant digits
      snprintf(buffer, sizeof(buffer), "%.6f", value);
      
      // Trim trailing zeros and decimal point if needed
      char* end = buffer + strlen(buffer) - 1;
      while (end > buffer && *end == '0') {
          *end-- = '\0';
      }
      if (end > buffer && *end == '.') {
          *end = '\0';
      }
      
      // If the result is still too long, try a shorter format
      if (strlen(buffer) > 8) {
          snprintf(buffer, sizeof(buffer), "%.3f", value);
          
          // Trim trailing zeros and decimal point again
          end = buffer + strlen(buffer) - 1;
          while (end > buffer && *end == '0') {
              *end-- = '\0';
          }
          if (end > buffer && *end == '.') {
              *end = '\0';
          }
      }
  } else {
      // For very large or small numbers, use scientific notation with 4 significant digits
      snprintf(buffer, sizeof(buffer), "%.4e", value);
  }
  
  return buffer;
}

// Format vector data with proper truncation
template <typename T, size_t N>
std::string formatVectorArray(const std::vector<T>& values, size_t size, size_t strTruncLen, bool* truncated) {
  std::string result = FormatString("vec%df[", N);
  
  for (size_t i = 0; i < size; i++) {
    const auto& v = values[i];
    
    // Format based on vector dimension
    if constexpr (N == 2) {
      result += FormatString("(%s,%s) ", formatFloat(v.x).c_str(), formatFloat(v.y).c_str());
    } else if constexpr (N == 3) {
      result += FormatString("(%s,%s,%s) ", formatFloat(v.x).c_str(), formatFloat(v.y).c_str(), formatFloat(v.z).c_str());
    } else if constexpr (N == 4) {
      result += FormatString("(%s,%s,%s,%s) ", formatFloat(v.x).c_str(), formatFloat(v.y).c_str(), formatFloat(v.z).c_str(), formatFloat(v.w).c_str());
    }
    
    if (result.length() > strTruncLen) {
      *truncated = true;
      break;
    }
  }
  
  result += *truncated ? " ...]" : "]";
  return result;
}

// Format single vector value
template <typename T>
std::string formatVector(const T& v) {
  if constexpr (std::is_same_v<T, fvec2>) {
    return FormatString("vec2(%s,%s)", formatFloat(v.x).c_str(), formatFloat(v.y).c_str());
  } else if constexpr (std::is_same_v<T, fvec3>) {
    return FormatString("vec3(%s,%s,%s)", formatFloat(v.x).c_str(), formatFloat(v.y).c_str(), formatFloat(v.z).c_str());
  } else if constexpr (std::is_same_v<T, fvec4>) {
    return FormatString("vec4(%s,%s,%s,%s)", formatFloat(v.x).c_str(), formatFloat(v.y).c_str(), formatFloat(v.z).c_str(), formatFloat(v.w).c_str());
  /*
  } else if constexpr (std::is_same_v<T, ivec2>) {
    return FormatString("int2(%d,%d)", v.x, v.y);
  } else if constexpr (std::is_same_v<T, ivec3>) {
    return FormatString("int3(%d,%d,%d)", v.x, v.y, v.z);
  } else if constexpr (std::is_same_v<T, ivec4>) {
    return FormatString("int4(%d,%d,%d,%d)", v.x, v.y, v.z, v.w);
  */
  }
  
  // Fallback for unknown type
  return "unknown_vector_type";
}

// Format matrix value
template <typename MtxType>
std::string formatMatrix(const MtxType& mtx) {
  /*
  if constexpr (std::is_same_v<MtxType, fmtx2>) {
    return FormatString("mat2(%g,%g,%g,%g)",
                         mtx.column(0).x, mtx.column(0).y,
                         mtx.column(1).x, mtx.column(1).y);
  } else 
  */
  if constexpr (std::is_same_v<MtxType, fmtx3>) {
    return FormatString("mat3(%s,%s,%s,%s,%s,%s,%s,%s,%s)",
                         formatFloat(mtx.column(0).x).c_str(), formatFloat(mtx.column(0).y).c_str(), formatFloat(mtx.column(0).z).c_str(),
                         formatFloat(mtx.column(1).x).c_str(), formatFloat(mtx.column(1).y).c_str(), formatFloat(mtx.column(1).z).c_str(),
                         formatFloat(mtx.column(2).x).c_str(), formatFloat(mtx.column(2).y).c_str(), formatFloat(mtx.column(2).z).c_str());
  } else if constexpr (std::is_same_v<MtxType, fmtx4>) {
    return FormatString("mat4(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)",
                         formatFloat(mtx.column(0).x).c_str(), formatFloat(mtx.column(0).y).c_str(), formatFloat(mtx.column(0).z).c_str(), formatFloat(mtx.column(0).w).c_str(),
                         formatFloat(mtx.column(1).x).c_str(), formatFloat(mtx.column(1).y).c_str(), formatFloat(mtx.column(1).z).c_str(), formatFloat(mtx.column(1).w).c_str(),
                         formatFloat(mtx.column(2).x).c_str(), formatFloat(mtx.column(2).y).c_str(), formatFloat(mtx.column(2).z).c_str(), formatFloat(mtx.column(2).w).c_str(),
                         formatFloat(mtx.column(3).x).c_str(), formatFloat(mtx.column(3).y).c_str(), formatFloat(mtx.column(3).z).c_str(), formatFloat(mtx.column(3).w).c_str());
  }
  
  // Fallback for unknown type
  return "unknown_matrix_type";
}

// Helper to get sampler information
std::string getSamplerInfo(GLuint program, GLint location, GLenum samplerType) {
  auto it = samplerTypeMap.find(samplerType);
  if (it == samplerTypeMap.end()) {
    return FormatString("unknown_sampler_type(%d)", samplerType);
  }
  
  const auto& info = it->second;
  int tex_unit = 0;
  int current_active = -1;
  
  glGetIntegerv(GL_ACTIVE_TEXTURE, &current_active);
  current_active -= GL_TEXTURE0;
  
  glGetUniformiv(program, location, &tex_unit);
  GL_ERRORCHECK();
  
  if (location == -1) {
    return FormatString("%s(invalid_location)", info.displayName);
  }
  
  // Switch to the texture unit the sampler uses
  glActiveTexture(GL_TEXTURE0 + tex_unit);
  GL_ERRORCHECK();
  
  // Get the texture bound to this unit
  GLint currentTexture = 0;
  glGetIntegerv(info.textureTarget, &currentTexture);
  GL_ERRORCHECK();
  
  // Get dimensions (if applicable)
  GLint width = 0, height = 0, depth = 0;
  
  // For cube maps, use the first face
  GLenum queryTarget = (info.textureTarget == GL_TEXTURE_BINDING_CUBE_MAP) ? 
                      GL_TEXTURE_CUBE_MAP_POSITIVE_X : 
                      (info.textureTarget == GL_TEXTURE_BINDING_2D_ARRAY) ? 
                      GL_TEXTURE_2D_ARRAY : 
                      (info.textureTarget == GL_TEXTURE_BINDING_3D) ? 
                      GL_TEXTURE_3D : 
                      GL_TEXTURE_2D;
  
  glGetTexLevelParameteriv(queryTarget, 0, GL_TEXTURE_WIDTH, &width);
  GL_ERRORCHECK();
  
  glGetTexLevelParameteriv(queryTarget, 0, GL_TEXTURE_HEIGHT, &height);
  GL_ERRORCHECK();
  
  // Only query depth for 3D or array textures
  if (info.textureTarget == GL_TEXTURE_BINDING_2D_ARRAY || info.textureTarget == GL_TEXTURE_BINDING_3D) {
    glGetTexLevelParameteriv(queryTarget, 0, GL_TEXTURE_DEPTH, &depth);
    GL_ERRORCHECK();
  }
  
  // Restore previous texture unit
  glActiveTexture(GL_TEXTURE0 + current_active);
  GL_ERRORCHECK();
  
  // Format string based on whether we have depth dimension
  std::string result;
  if (depth > 0) {
    result = FormatString(
      "%s(unit: %d dim<%dx%dx%d> tobj: %d)", 
      info.displayName, tex_unit, width, height, depth, currentTexture);
  } else {
    result = FormatString(
      "%s(unit: %d dim<%dx%d> tobj: %d)", 
      info.displayName, tex_unit, width, height, currentTexture);
  }
  
  return result;
}

// Generic function to get uniform value as string
std::string getUniformValueString(
    const UniformData& uniformData,
    GLuint program,
    size_t strTruncLen) {
    
  bool truncated = false;
  bool isUbo = (uniformData.location == -1);
  
  // Handle basic scalar types first
  if (uniformData.type == GL_FLOAT) {
    if (isUbo) {
      GLint offset = getUboOffset(program, uniformData.uniformIndex);
      float value = 0.0f;
      if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, &value)) {
        return FormatString("float(%g)", value);
      }
      return "float(UBO_read_error)";
    } else {
      float value = 0.0f;
      glGetUniformfv(program, uniformData.location, &value);
      GL_ERRORCHECK();
      return FormatString("float(%g)", value);
    }
  }
  else if (uniformData.type == GL_INT) {
    if (isUbo) {
      GLint offset = getUboOffset(program, uniformData.uniformIndex);
      int value = 0;
      if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, &value)) {
        return FormatString("int(%d)", value);
      }
      return "int(UBO_read_error)";
    } else {
      int value = 0;
      glGetUniformiv(program, uniformData.location, &value);
      GL_ERRORCHECK();
      return FormatString("int(%d)", value);
    }
  }
  else if (uniformData.type == GL_UNSIGNED_INT) {
    if (isUbo) {
      GLint offset = getUboOffset(program, uniformData.uniformIndex);
      GLuint value = 0;
      if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, &value)) {
        return FormatString("uint32_t(%u)", value);
      }
      return "uint32_t(UBO_read_error)";
    } else {
      GLuint value = 0;
      glGetUniformuiv(program, uniformData.location, &value);
      GL_ERRORCHECK();
      return FormatString("uint32_t(%u)", value);
    }
  }
  
  // Handle vector types
  else if (uniformData.type == GL_FLOAT_VEC2) {
    if (uniformData.isArray) {
      if (isUbo) {
        GLint offset = getUboOffset(program, uniformData.uniformIndex);
        GLint arrayStride = getUboArrayStride(program, uniformData.uniformIndex);
        std::vector<fvec2> values(uniformData.size);
        
        if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, values.data(), uniformData.size, arrayStride)) {
          return formatVectorArray<fvec2, 2>(values, uniformData.size, strTruncLen, &truncated);
        }
        return "vec2f[UBO_read_error]";
      } else {
        // Handle regular array of vec2 (non-UBO)
        std::vector<fvec2> values(uniformData.size);
        glGetUniformfv(program, uniformData.location, (float*)values.data());
        GL_ERRORCHECK();
        return formatVectorArray<fvec2, 2>(values, uniformData.size, strTruncLen, &truncated);
      }
    } else {
      // Single vec2
      if (isUbo) {
        GLint offset = getUboOffset(program, uniformData.uniformIndex);
        fvec2 value;
        if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, &value)) {
          return formatVector(value);
        }
        return "vec2(UBO_read_error)";
      } else {
        fvec2 value;
        glGetUniformfv(program, uniformData.location, value.asArray());
        GL_ERRORCHECK();
        return formatVector(value);
      }
    }
  }
  
  else if (uniformData.type == GL_FLOAT_VEC3) {
    if (uniformData.isArray) {
      if (isUbo) {
        GLint offset = getUboOffset(program, uniformData.uniformIndex);
        GLint arrayStride = getUboArrayStride(program, uniformData.uniformIndex);
        std::vector<fvec3> values(uniformData.size);
        
        if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, values.data(), uniformData.size, arrayStride)) {
          return formatVectorArray<fvec3, 3>(values, uniformData.size, strTruncLen, &truncated);
        }
        return "vec3f[UBO_read_error]";
      } else {
        // Handle regular array of vec3 (non-UBO)
        std::vector<fvec3> values(uniformData.size);
        glGetUniformfv(program, uniformData.location, (float*)values.data());
        GL_ERRORCHECK();
        return formatVectorArray<fvec3, 3>(values, uniformData.size, strTruncLen, &truncated);
      }
    } else {
      // Single vec3
      if (isUbo) {
        GLint offset = getUboOffset(program, uniformData.uniformIndex);
        fvec3 value;
        if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, &value)) {
          return formatVector(value);
        }
        return "vec3(UBO_read_error)";
      } else {
        fvec3 value;
        glGetUniformfv(program, uniformData.location, value.asArray());
        GL_ERRORCHECK();
        return formatVector(value);
      }
    }
  }
  
  else if (uniformData.type == GL_FLOAT_VEC4) {
    if (uniformData.isArray) {
      if (isUbo) {
        GLint offset = getUboOffset(program, uniformData.uniformIndex);
        GLint arrayStride = getUboArrayStride(program, uniformData.uniformIndex);
        std::vector<fvec4> values(uniformData.size);
        
        if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, values.data(), uniformData.size, arrayStride)) {
          return formatVectorArray<fvec4, 4>(values, uniformData.size, strTruncLen, &truncated);
        }
        return "vec4f[UBO_read_error]";
      } else {
        // Handle regular array of vec4 (non-UBO)
        std::vector<fvec4> values(uniformData.size);
        glGetUniformfv(program, uniformData.location, (float*)values.data());
        GL_ERRORCHECK();
        return formatVectorArray<fvec4, 4>(values, uniformData.size, strTruncLen, &truncated);
      }
    } else {
      // Single vec4
      if (isUbo) {
        GLint offset = getUboOffset(program, uniformData.uniformIndex);
        fvec4 value;
        if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, &value)) {
          return formatVector(value);
        }
        return "vec4(UBO_read_error)";
      } else {
        fvec4 value;
        glGetUniformfv(program, uniformData.location, value.asArray());
        GL_ERRORCHECK();
        return formatVector(value);
      }
    }
  }
  
  // Handle integer vector types
  /*
  else if (uniformData.type == GL_INT_VEC2 || uniformData.type == GL_INT_VEC3) {
    if (isUbo) {
      // UBO handling for int vectors
      GLint offset = getUboOffset(program, uniformData.uniformIndex);
      
      if (uniformData.type == GL_INT_VEC2) {
        ivec2 value;
        if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, &value)) {
          return FormatString("int2(%d,%d)", value.x, value.y);
        }
      } else { // GL_INT_VEC3
        ivec3 value;
        if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, &value)) {
          return FormatString("int3(%d,%d,%d)", value.x, value.y, value.z);
        }
      }
      return "int_vector(UBO_read_error)";
    } else {
      // Non-UBO handling
      if (uniformData.type == GL_INT_VEC2) {
        int value[2];
        glGetUniformiv(program, uniformData.location, value);
        GL_ERRORCHECK();
        return FormatString("int2(%d,%d)", value[0], value[1]);
      } else { // GL_INT_VEC3
        int value[3];
        glGetUniformiv(program, uniformData.location, value);
        GL_ERRORCHECK();
        return FormatString("int3(%d,%d,%d)", value[0], value[1], value[2]);
      }
    }
  }
  */
  
  // Handle matrix types
  else if ( //uniformData.type == GL_FLOAT_MAT2 || 
          uniformData.type == GL_FLOAT_MAT3 || 
          uniformData.type == GL_FLOAT_MAT4) {
    if (isUbo) {
      // UBO matrix handling
      GLint offset = getUboOffset(program, uniformData.uniformIndex);
      
      /*
      if (uniformData.type == GL_FLOAT_MAT2) {
        fmtx2 mtx;
        if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, &mtx)) {
          return formatMatrix(mtx);
        }
      } 
      else
      */
      if (uniformData.type == GL_FLOAT_MAT3) {
        fmtx3 mtx;
        if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, &mtx)) {
          return formatMatrix(mtx);
        }
      }
      else if (uniformData.type == GL_FLOAT_MAT4) {
        fmtx4 mtx;
        if (readUboData(program, uniformData.uniformIndex, uniformData.blockIndex, offset, &mtx)) {
          return formatMatrix(mtx);
        }
      }
      return "matrix(UBO_read_error)";
    } else {
      // Regular matrix uniform
      /*
      if (uniformData.type == GL_FLOAT_MAT2) {
        fmtx2 mtx;
        glGetUniformfv(program, uniformData.location, mtx.asArray());
        GL_ERRORCHECK();
        return formatMatrix(mtx);
      }
      else
      */ 
      if (uniformData.type == GL_FLOAT_MAT3) {
        fmtx3 mtx;
        glGetUniformfv(program, uniformData.location, mtx.asArray());
        GL_ERRORCHECK();
        return formatMatrix(mtx);
      }
      else if (uniformData.type == GL_FLOAT_MAT4) {
        fmtx4 mtx;
        glGetUniformfv(program, uniformData.location, mtx.asArray());
        GL_ERRORCHECK();
        return formatMatrix(mtx);
      }
    }
  }
  
  // Handle sampler types
  else if (samplerTypeMap.find(uniformData.type) != samplerTypeMap.end()) {
    // Samplers can't be in UBOs, so we don't need separate UBO handling
    return getSamplerInfo(program, uniformData.location, uniformData.type);
  }
  
  // Unknown type
  return FormatString("unknown_type(%d)", uniformData.type);
}

// Get foreground color for a uniform type
irgb getTypeColor(GLenum type) {
  auto it = typeColorMap.find(type);
  if (it != typeColorMap.end()) {
    return it->second;
  }
  
  // Default to white
  return WHI;
}

void _FtxGlDebugger::_validateCurrentShaderProgram() {
  using namespace ftxui;

  // validate current shader program
  node_vect_t NODES;

  // Get current program
  GLint currentProgram = 0;
  GL_ERRORCHECK();
  glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
  GL_ERRORCHECK();
  
  // Validate program
  GLint linkStatus = 0;
  glGetProgramiv(currentProgram, GL_LINK_STATUS, &linkStatus);
  GL_ERRORCHECK();
  
  glValidateProgram(currentProgram);
  GLint validateStatus = 0;
  GL_ERRORCHECK();
  
  glGetProgramiv(currentProgram, GL_VALIDATE_STATUS, &validateStatus);
  _colortext(NODES, WHI, BLK, "currentProgram<%d> linkStatus<%d> validateStatus<%d>\n", currentProgram, linkStatus, validateStatus);
  
  // Check validation status
  if (0 == validateStatus) {
    GLint infolen = 0;
    GL_ERRORCHECK();
    glGetProgramiv(currentProgram, GL_INFO_LOG_LENGTH, &infolen);
    GL_ERRORCHECK();
    std::vector<char> infolog(infolen);
    GL_ERRORCHECK();
    glGetProgramInfoLog(currentProgram, infolen, &infolen, infolog.data());
    GL_ERRORCHECK();
    _colortext_wrap(NODES, RED, BLK, "ProgramInfoLog<%s>\n", infolog.data());
  }

  // Gather information about attributes
  GLint numActiveAttribs = 0;
  GL_ERRORCHECK();
  glGetProgramiv(currentProgram, GL_ACTIVE_ATTRIBUTES, &numActiveAttribs);
  GL_ERRORCHECK();

  for (int i = 0; i < numActiveAttribs; i++) {
    char nameBuffer[256];
    GLsizei nameLength = 0;

    auto shattrib = std::make_shared<ShaderAttrib>();
    
    GL_ERRORCHECK();
    glGetActiveAttrib(currentProgram, i, sizeof(nameBuffer), &nameLength, &shattrib->_size, &shattrib->_type, nameBuffer);
    GL_ERRORCHECK();
    
    shattrib->_name = nameBuffer;
    shattrib->_location = glGetAttribLocation(currentProgram, nameBuffer);
    _shader_attribs[shattrib->_location] = shattrib;
  }

  // Gather uniform information
  GLint numUniforms = 0;
  GL_ERRORCHECK();
  glGetProgramiv(currentProgram, GL_ACTIVE_UNIFORMS, &numUniforms);
  GL_ERRORCHECK();
  _colortext(NODES, WHI, BLK, "numUniforms<%d>\n", numUniforms);

  // Prepare arrays for uniform information
  std::vector<GLuint> uniformIndices(numUniforms);
  std::vector<GLint> uniformBlockIndices(numUniforms);

  for (int i = 0; i < numUniforms; i++) {
    uniformIndices[i] = i;
  }
  
  GL_ERRORCHECK();
  glGetActiveUniformsiv(currentProgram, numUniforms, uniformIndices.data(), GL_UNIFORM_BLOCK_INDEX, uniformBlockIndices.data());
  GL_ERRORCHECK();

  // Process each uniform
  constexpr size_t str_trunc_len = 72;
  for (int i = 0; i < numUniforms; i++) {
    // Create uniform data structure
    UniformData uniformData;
    uniformData.uniformIndex = uniformIndices[i];
    uniformData.blockIndex = uniformBlockIndices[i];
    
    // Get uniform info
    GLint nameLength = 0;
    GLchar gl_name[256];
    
    GL_ERRORCHECK();
    glGetActiveUniform(currentProgram, i, sizeof(gl_name), &nameLength, &uniformData.size, &uniformData.type, gl_name);
    GL_ERRORCHECK();
    
    //printf("gl_name<%s>\n", gl_name);
    
    // Get uniform location
    uniformData.location = glGetUniformLocation(currentProgram, gl_name);
    GL_ERRORCHECK();
    
    uniformData.name = gl_name;
    uniformData.isArray = (uniformData.size > 1);
    
    // Handle array uniforms naming
    if (uniformData.isArray) {
      auto array_spec = FormatString("[%d]", uniformData.size);
      uniformData.name.replace(uniformData.name.find("[0]"), 3, array_spec);
    }
    
    // Get value string using our helper function
    std::string value_str = getUniformValueString(uniformData, currentProgram, str_trunc_len);
    
    // Get colors for display
    irgb fg = getTypeColor(uniformData.type);
    irgb bg = uniformData.isArray ? irgb{32, 32, 32} : irgb{0, 0, 0};
    
    // Format for UBO or regular uniform
    std::string out_str;
    if (uniformData.location == -1) {
      if (uniformData.blockIndex == -1) {
        fg = WHI;
        bg = irgb{96, 0, 0};
      } else {
        fg = WHI;
        bg = irgb{96, 0, 96};
      }
      out_str = FormatString(" %02d BLOCK%02d SIZ%02d : %32s  :  %s\n", 
                             i, uniformData.blockIndex, uniformData.size, 
                             uniformData.name.c_str(), value_str.c_str());
    } else {
      out_str = FormatString(" %02d LOC%02d   SIZ%02d : %32s  :  %s\n", 
                             i, uniformData.location, uniformData.size, 
                             uniformData.name.c_str(), value_str.c_str());
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
  GL_ERRORCHECK();

  std::vector<GLchar> shaderSource;
  GLint shaderSourceLength = 0;
  
  for (GLuint shader : shaders) {
    glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &shaderSourceLength);
    shaderSource.resize(shaderSourceLength);
    glGetShaderSource(shader, shaderSourceLength, nullptr, shaderSource.data());
    std::string shaderSourceStr(shaderSource.begin(), shaderSource.end());
    auto sh_lines = SplitString(shaderSourceStr, '\n');
    
    // Get type of shader
    GLint shaderType = 0;
    glGetShaderiv(shader, GL_SHADER_TYPE, &shaderType);
    GL_ERRORCHECK();
    
    // Get shader type name from our map
    std::string shader_type_str = "UnknownShader";
    auto typeIt = shaderTypeNames.find(shaderType);
    if (typeIt != shaderTypeNames.end()) {
      shader_type_str = typeIt->second;
    }
    
    _shader_texts[shader_type_str] = sh_lines;
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////