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

void _FtxGlDebugger::_validateCurrentGeomBuffers() {

  using namespace ftxui;

  // validate that the current shader program is valid

  node_vect_t NODES;
  // validate that the current IBO state is valid
  GLint currentIBO = 0;
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &currentIBO);
  printf("currentIBO<%d>\n", currentIBO);
  _colortext(NODES, YEL, BLK, "currentIBO<%d>", currentIBO);
  //OrkAssert(currentIBO != 0);
  // validate that the current VAO state is valid
  GLint currentVAO = 0;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &currentVAO);
  // get VAO khr_debug name
  constexpr size_t kMaxNameLength = 256;
  std::vector<GLchar> vaoName(kMaxNameLength);
  glGetObjectLabelEXT(GL_VERTEX_ARRAY_OBJECT_EXT, currentVAO, vaoName.size(), nullptr, &vaoName[0]);
  std::string vaoNameStr(vaoName.begin(), vaoName.end());

  struct GeoBuffer{
      GLuint vbo = 0;
      GLint size           = 0;
      GLenum type          = GL_NONE;
      GLboolean normalized = GL_FALSE;
      GLint stride         = 0;
      GLvoid* offset       = 0;
  };
  std::vector<GeoBuffer> geoBuffers;

  if(true) { //currentVAO != 0){
    // validate all bound VBOs are valid
    GLint numAttribs = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numAttribs);
    _colortext(NODES, YEL, BLK, "numAttribs<%d>", numAttribs);
    _colortext(NODES, WHI, BLU1, "BY VAO");
    for( int i=0; i<numAttribs; i++ ) {
      GLint currentVBO = 0;
      glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &currentVBO);
      // get attrib format
      GLint isEnabled = 0;
      glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &isEnabled);
      if(not isEnabled)
        continue;
      GeoBuffer gb;
      gb.vbo = currentVBO;
      glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_SIZE, &gb.size);
      glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_TYPE, (GLint*)&gb.type);
      glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, (GLint*)&gb.normalized);
      glGetVertexAttribiv(i, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &gb.stride);
      glGetVertexAttribPointerv(i, GL_VERTEX_ATTRIB_ARRAY_POINTER, &gb.offset);
      geoBuffers.push_back(gb);
      auto type_str = _glTypeToString(gb.type);
      _colortext(
          NODES,
          YEL,
          BLK,
          "  attrib<%d> vbo<%d> type<%s:%d> normalized<%d> stride<%d> offset<%u>\n",
          i,
          gb.vbo,
          type_str.c_str(),
          gb.size,
          gb.normalized,
          gb.stride,
          size_t(gb.offset));
    }
    _colortext(NODES, WHI, BLU1, "BY SHADER");
    for ( auto attr_item : _shader_attribs) {
      int i = attr_item.first;
      auto shattrib = attr_item.second;
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
          shattrib->_location,
          shattrib->_name.c_str(),
          currentVBO,
          size,
          type_str.c_str(),
          normalized,
          stride,
          size_t(offset));
    }
    ///////////////////////////////
    // now for each geobuffer, 
    //  dump the first 3 vertices
    ///////////////////////////////

    _colortext(NODES, WHI, BLU1, "GEOMETRY");

    size_t num_verts = 3;

    for( auto gb : geoBuffers ) {
      _colortext(NODES, WHI, BLU2, "VBO<%d>", gb.vbo);
      _colortext(NODES, WHI, BLK, "  size<%d> type<%s> normalized<%d> stride<%d> offset<%u>", gb.size, _glTypeToString(gb.type).c_str(), gb.normalized, gb.stride, size_t(gb.offset));
      _colortext(NODES, WHI, BLK, "  DATA");
      if( gb.vbo != 0 ) {
        glBindBuffer(GL_ARRAY_BUFFER, gb.vbo);
        auto vbo_data = glMapBuffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
        if( vbo_data ) {
          for( size_t i=0; i<num_verts; i++ ) {
            std::string outstr = FormatString("  V%d: [ ", i);
            auto vdata = (const char*)vbo_data + i*gb.stride;
            switch(gb.type){
              case GL_FLOAT: {
                auto fdata = (const float*)vdata;
                for( int j=0; j<gb.size; j++ ) {
                  outstr += FormatString("%f ", fdata[j]);
                }
                break;
              }
              case GL_UNSIGNED_BYTE: {
                auto u8data = (const uint8_t*)vdata;
                for( int j=0; j<gb.size; j++ ) {
                  outstr += FormatString("%d ", u8data[j]);
                }
                break;
              }
              default:
                break;
            }
            outstr += "]";
            _colortext(NODES, WHI, BLK, "%s", outstr.c_str());

          }
          glUnmapBuffer(GL_ARRAY_BUFFER);
        }
      }
    }


    // en
  }
  _node_geometry = vbox({
      text("Geometry State"),
      separator(),
      vbox(std::move(NODES)),
  });
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
