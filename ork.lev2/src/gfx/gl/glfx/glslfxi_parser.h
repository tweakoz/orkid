#pragma once

#include "glslfxi_scanner.h"

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct GlSlFxParser {

  GlSlFxParser(const AssetPath &pth, Scanner &s);
  bool IsTokenOneOfTheBlockTypes(const Token &tok);
  Config *ParseFxConfig();
  UniformBlock *parseUniformBlock();
  UniformSet *parseUniformSet();
  StreamInterface *ParseFxInterface(GLenum iftype);
  StateBlock *ParseFxStateBlock();
  LibBlock *ParseLibraryBlock();
  int ParseFxShaderCommon(Shader *pshader);
  ShaderVtx *ParseFxVertexShader();
  ShaderTsC *ParseFxTessCtrlShader();
  ShaderTsE *ParseFxTessEvalShader();
  ShaderGeo *ParseFxGeometryShader();
  ShaderFrg *ParseFxFragmentShader();
  #if defined(ENABLE_NVMESH_SHADERS)
  ShaderNvTask *ParseFxNvTaskShader();
  ShaderNvMesh *ParseFxNvMeshShader();
  #endif
  Technique *ParseFxTechnique();
  int ParseFxPass(int istart, Technique *ptek);
  void DumpAllTokens();
  Container *Parse(const std::string &fxname);

  int itokidx = 0;
  Scanner &scanner;
  const AssetPath mPath;
  Container *mpContainer = nullptr;

  static const std::map<std::string, int> gattrsorter;

};
/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
