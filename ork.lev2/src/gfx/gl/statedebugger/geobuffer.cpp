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

  _colortext(NODES, YEL, BLK, "currentVAO<%d:%s>", currentVAO, vaoNameStr.c_str());
  if(currentVAO != 0){
    // validate all bound VBOs are valid
    GLint numAttribs = 0;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &numAttribs);
    _colortext(NODES, YEL, BLK, "numAttribs<%d>", numAttribs);
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
