#include "vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/shadlang_nodes.h>
#include <shaderc/shaderc.hpp>

#if defined( __APPLE__ )
//#include <MoltenVK/mvk_vulkan.h>
#endif

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

#if 0
VkPipelineCache pipelineCache;
VkPipelineCacheCreateInfo cacheInfo = {};
cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
// if you have a previously saved cache:
cacheInfo.initialDataSize = savedCacheSize;  
cacheInfo.pInitialData = pSavedCacheData;
vkCreatePipelineCache(device, &cacheInfo, NULL, &pipelineCache);

size_t cacheSize;
vkGetPipelineCacheData(device, pipelineCache, &cacheSize, NULL);
void* pData = malloc(cacheSize);
vkGetPipelineCacheData(device, pipelineCache, &cacheSize, pData);
#endif

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

        struct shader_proc_context {
          transunit_ptr_t _transu;
          shader_ptr_t _shader;
          miscgroupnode_ptr_t _group;
          void appendText(const char* formatstring, ...) {
            char formatbuffer[512];
            va_list args;
            va_start(args, formatstring);
            vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
            va_end(args);
            _group->appendChild<InsertLine>(formatbuffer);
          }

        };

        /////////////////////////////////////////////////////////////////////////////

        auto process_inh_uset = [&](shader_proc_context& SPC, astnode_ptr_t semainhnode){
            auto INHID = semainhnode->typedValueForKey<std::string>("inherit_id").value();
            auto USET = SPC._transu->find<UniformSet>(INHID);
            auto decls = AstNode::collectNodesOfType<DataDeclaration>(USET);
            SPC.appendText("layout(set=0, binding=0) uniform %s {", INHID.c_str() );
            //SPC.appendText("layout(set=0) uniform %s {", INHID.c_str() );
            for( auto d : decls ){
              auto tid = d->childAs<TypedIdentifier>(0);
              OrkAssert(tid);
              dumpAstNode(tid);
              auto dt = tid->typedValueForKey<std::string>("data_type").value();
              auto id = tid->typedValueForKey<std::string>("identifier_name").value();
              printf( "  dt<%s> id<%s>\n", dt.c_str(), id.c_str() );
              SPC.appendText("%s %s;", dt.c_str(), id.c_str());
            }
            SPC.appendText("};");
        };

        /////////////////////////////////////////////////////////////////////////////

        auto process_ios = [&](shader_proc_context& SPC, astnode_ptr_t interface_node){
            auto input_groups = AstNode::collectNodesOfType<InterfaceInputs>(interface_node);
            auto output_groups = AstNode::collectNodesOfType<InterfaceOutputs>(interface_node);
            printf( "  num_input_groups<%zu>\n", input_groups.size() );
            printf( "  num_output_groups<%zu>\n", output_groups.size() );
            OrkAssert(input_groups.size() == 1);
            OrkAssert(output_groups.size() == 1);
            auto input_group = input_groups[0];
            auto output_group = output_groups[0];
            //
            auto inputs = AstNode::collectNodesOfType<InterfaceInput>(input_group);
            auto outputs = AstNode::collectNodesOfType<InterfaceOutput>(output_group);
            printf( "  num_inputs<%zu>\n", inputs.size() );
            printf( "  num_outputs<%zu>\n", outputs.size() );
            //
            size_t input_index = 0;
            for( auto input : inputs ){
              auto tid = input->childAs<TypedIdentifier>(0);
              OrkAssert(tid);
              dumpAstNode(tid);
              auto dt = tid->typedValueForKey<std::string>("data_type").value();
              auto id = tid->typedValueForKey<std::string>("identifier_name").value();
              SPC.appendText("layout(location=%zu) in %s %s;", input_index, dt.c_str(), id.c_str());
              input_index++;
            }
            //
            size_t output_index = 0;
            for( auto output : outputs ){
              auto tid = output->childAs<TypedIdentifier>(0);
              OrkAssert(tid);
              dumpAstNode(tid);
              auto dt = tid->typedValueForKey<std::string>("data_type").value();
              auto id = tid->typedValueForKey<std::string>("identifier_name").value();
              SPC.appendText("layout(location=%zu) out %s %s;", output_index, dt.c_str(), id.c_str());
              output_index++;
            }

        };

        /////////////////////////////////////////////////////////////////////////////

        auto process_vtx_inhlist = [&](shader_proc_context& SPC){

            //
            auto inh_vifs = AstNode::collectNodesOfType<SemaInheritVertexInterface>(SPC._shader);
            printf( "inh_vifs<%zu>\n", inh_vifs.size() );
            OrkAssert(inh_vifs.size() == 1);
            auto INHVIF = inh_vifs[0];
            auto INHID = INHVIF->typedValueForKey<std::string>("inherit_id").value();
            printf( "  inh_vif<%s> INHID<%s>\n", INHVIF->_name.c_str(), INHID.c_str() );
            auto VIF = SPC._transu->find<VertexInterface>(INHID);
            OrkAssert(VIF);
            process_ios(SPC,VIF);
            //dumpAstNode(VIF);
            //
            auto inh_semausets = AstNode::collectNodesOfType<SemaInheritUniformSet>(VIF);
            for( auto inh_uset : inh_semausets ){
              process_inh_uset(SPC,inh_uset);
            }

        };

        /////////////////////////////////////////////////////////////////////////////

        auto compile_shader = [](shader_proc_context& SPC, shaderc_shader_kind shader_type){

            ///////////////////////////////////////////////////////
            // final prep for shaderc
            ///////////////////////////////////////////////////////

            auto grpnode = SPC._group;

            // version line
            grpnode->insertChildAt<InsertLine>(0, "#version 450");
            grpnode->appendChild<InsertLine>("void main()");
            // shader body (should be a compount statement, with {} included...)
            grpnode->appendChildrenFrom(SPC._shader);

            ///////////////////////////////////////////////////////
            //emit
            ///////////////////////////////////////////////////////

            auto as_glsl = shadlang::toGLFX1(grpnode);
            auto obj_name = SPC._shader->typedValueForKey<std::string>("object_name").value();
            printf( "// shader<%s>:\n%s\n", obj_name.c_str(), as_glsl.c_str() );

            ///////////////////////////////////////////////////////
            // compile with shaderc
            ///////////////////////////////////////////////////////

            shaderc::Compiler compiler;
            shaderc::CompileOptions options;
            shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv( //
                as_glsl.c_str(),   // glsl source (string)
                as_glsl.length(),  // glsl source length
                shader_type,       // shader type
                "main",            // entry point name
                options);

            if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
              std::cerr << result.GetErrorMessage();
              OrkAssert(false);
            }                

            std::vector<uint32_t> spirv_binary(result.cbegin(), result.cend());

            #if 0 //defined( __APPLE__ )
            mvk::SPIRVToMSLConverter converter;
            mvk::SPIRVToMSLConversionConfiguration config;
            // You can customize the `config` if needed.
            mvk::MSLConversionResult mslResult;
            bool success = converter.convert(spirv_binary.data(), spirv_binary.size(), config, mslResult);
            if (success) {
                std::string mslCode = mslResult.mslCode;  // This contains the generated MSL.
                OrkAssert(false);
            }
            #endif

        };

        /////////////////////////////////////////////////////////////////////////////
        // compile all shaders from translation unit
        /////////////////////////////////////////////////////////////////////////////

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

        shader_proc_context SPC;
        SPC._transu = sh->_trans_unit;

        // vertex shaders
        for( auto vshader : vtx_shaders ){
          SPC._shader = vshader;
          SPC._group = std::make_shared<MiscGroupNode>();
          process_vtx_inhlist(SPC);
          compile_shader(SPC,shaderc_glsl_vertex_shader);
        }

        // fragment shaders
        for( auto fshader : frg_shaders ){
          SPC._shader = fshader;
          SPC._group = std::make_shared<MiscGroupNode>();
          compile_shader(SPC,shaderc_glsl_fragment_shader);
        }

        // compute shaders
        for( auto cshader : cu_shaders ){
          SPC._shader = cshader;
          SPC._group = std::make_shared<MiscGroupNode>();
          compile_shader(SPC,shaderc_glsl_compute_shader);
        }

        /////////////////////////////////////////////////////////////////////////////

        OrkAssert(false);

        /////////////////////////////////////////////////////////////////////////////

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
