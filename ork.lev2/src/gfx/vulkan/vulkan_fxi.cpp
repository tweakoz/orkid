#include "vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/shadlang_nodes.h>
#include <shaderc/shaderc.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

VkFxInterface::VkFxInterface(vkcontext_rawptr_t ctx)
    : _contextVK(ctx) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::_doBeginFrame() {
}

///////////////////////////////////////////////////////////////////////////////

int VkFxInterface::BeginBlock(fxtechnique_constptr_t tek, const RenderContextInstData& data) {
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

bool VkFxInterface::BindPass(int ipass) {
  return false;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::EndPass() {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::EndBlock() {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::CommitParams(void) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::reset() {
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderTechnique* VkFxInterface::technique(FxShader* hfx, const std::string& name) {
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParam* VkFxInterface::parameter(FxShader* hfx, const std::string& name) {
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParamBlock* VkFxInterface::parameterBlock(FxShader* hfx, const std::string& name) {
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamBool(const FxShaderParam* hpar, const bool bval) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamInt(const FxShaderParam* hpar, const int ival) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamFloat(const FxShaderParam* hpar, float fA) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamU32(const FxShaderParam* hpar, uint32_t uval) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamCTex(const FxShaderParam* hpar, const Texture* pTex) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::BindParamU64(const FxShaderParam* hpar, uint64_t uval) {
}

///////////////////////////////////////////////////////////////////////////////

bool VkFxInterface::LoadFxShader(const AssetPath& input_path, FxShader* pshader) {

  auto it = _GVI->_shared_fxshaders.find(input_path);
  vkfxsobj_ptr_t sh;
  if( it == _GVI->_shared_fxshaders.end() ) {
    ////////////////////////////////////////////
    // first check precompiled shader cache
    ////////////////////////////////////////////
    auto str_read = ork::File::readAsString(input_path);
    OrkAssert(str_read != nullptr);
    auto basehasher = DataBlock::createHasher();
    basehasher->accumulateString("vkfxshader-1.0");
    basehasher->accumulateString(str_read->_data);
    uint64_t hashkey   = basehasher->result();
    auto vkfx_datablock = DataBlockCache::findDataBlock(hashkey);
    ////////////////////////////////////////////
    // shader binary already cached
    ////////////////////////////////////////////
    if( vkfx_datablock ){
        OrkAssert(false);
    }
    ////////////////////////////////////////////
    // shader binary not cached, compile and cache
    ////////////////////////////////////////////
    else {

        using namespace shadlang::SHAST;

        sh = std::make_shared<VkFxShaderObject>();
        sh->_trans_unit = shadlang::parseFromString(str_read->_data);
        _GVI->_shared_fxshaders[input_path] = sh;
        auto vtx_shaders = AstNode::collectNodesOfType<VertexShader>(sh->_trans_unit);
        auto frg_shaders = AstNode::collectNodesOfType<FragmentShader>(sh->_trans_unit);
        auto cu_shaders = AstNode::collectNodesOfType<ComputeShader>(sh->_trans_unit);
        
        size_t num_vtx_shaders = vtx_shaders.size();
        size_t num_frg_shaders = frg_shaders.size();
        size_t num_cu_shaders = cu_shaders.size();

        printf( "num_vtx_shaders<%zu>\n", num_vtx_shaders );
        printf( "num_frg_shaders<%zu>\n", num_frg_shaders );
        printf( "num_cu_shaders<%zu>\n", num_cu_shaders );

        for( auto v : vtx_shaders ){

            ///////////////////////////////////////////////////////
            // final prep for shaderc
            ///////////////////////////////////////////////////////

            auto grpnode = std::make_shared<MiscGroupNode>();
            // version line
            grpnode->addChild<InsertLine>("#version 140");
            grpnode->addChild<InsertLine>("void main()");
            // shader body (should be a compount statement, with {} included...)
            grpnode->appendChildrenFrom(v);

            ///////////////////////////////////////////////////////
            //emit
            ///////////////////////////////////////////////////////

            auto as_glsl = shadlang::toGLFX1(grpnode);
            printf( "vtx_shader:\n%s\n", as_glsl.c_str() );

            ///////////////////////////////////////////////////////

            shaderc::Compiler compiler;
            shaderc::CompileOptions options;
            shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv( //
                as_glsl.c_str(),             // glsl source (string)
                as_glsl.length(),            // glsl source length
                shaderc_glsl_vertex_shader,  // shader type
                "main",                      // entry point name
                options);

            if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
              std::cerr << result.GetErrorMessage();
              OrkAssert(false);
            }                
        }
        OrkAssert(false);

        // MoltenVKShaderConverter
        // -gi <glsl input>
        // -so <spirv out>
        // -si <spirv input>
        // -mv <metal version> [2, 2.1, or 2.1.0]
        // -mp <metal platform> [macos, ios, or ios-simulator]
        // -mo <metal output>
        // -t <shader type> [v, f, c]
    }

  }
  else{ // shader already loaded...
    sh = it->second;
  }
  OrkAssert(sh != nullptr);
  pshader->_internalHandle.set<vkfxsobj_ptr_t>(sh);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

FxShader* VkFxInterface::shaderFromShaderText(const std::string& name, const std::string& shadertext) {
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// ubo
///////////////////////////////////////////////////////////////////////////////

FxShaderParamBuffer* VkFxInterface::createParamBuffer(size_t length) {
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

parambuffermappingptr_t VkFxInterface::mapParamBuffer(FxShaderParamBuffer* b, size_t base, size_t length) {
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::unmapParamBuffer(FxShaderParamBufferMapping* mapping) {
}

///////////////////////////////////////////////////////////////////////////////

void VkFxInterface::bindParamBlockBuffer(const FxShaderParamBlock* block, FxShaderParamBuffer* buffer) {
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
///////////////////////////////////////////////////////////////////////////////
const FxComputeShader* VkFxInterface::computeShader(FxShader* hfx, const std::string& name) {
  OrkAssert(false);
  return nullptr;
}
const FxShaderStorageBlock* VkFxInterface::storageBlock(FxShader* hfx, const std::string& name) {
  OrkAssert(false);
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
#endif
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
