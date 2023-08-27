#include "vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/shadlang_nodes.h>
#include <ork/util/hexdump.inl>
#include <shaderc/shaderc.hpp>

#if defined(__APPLE__)
//#include <MoltenVK/mvk_vulkan.h>
#endif

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
using namespace shadlang::SHAST;
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

struct shader_proc_context {

  vkfxsfile_ptr_t _shaderfile;
  transunit_ptr_t _transu;
  shader_ptr_t _shader;
  miscgroupnode_ptr_t _group;
  vkfxshader_bin_t _spirv_binary;
  std::string _shader_name;
  ////////////////////////////////////////////////////////
  void appendText(const char* formatstring, ...) {
    char formatbuffer[512];
    va_list args;
    va_start(args, formatstring);
    vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
    va_end(args);
    _group->appendChild<InsertLine>(formatbuffer);
  }
  ////////////////////////////////////////////////////////

  void process_uniformsets(astnode_ptr_t par_node) {
    auto inh_semausets              = AstNode::collectNodesOfType<SemaInheritUniformSet>(par_node);
    static size_t descriptor_set_id = 0;
    size_t binding_id               = 0;
    for (auto inh_uset : inh_semausets) {
      auto INHID = inh_uset->typedValueForKey<std::string>("inherit_id").value();
      auto USET  = _transu->find<UniformSet>(INHID);
      auto decls = AstNode::collectNodesOfType<DataDeclaration>(USET);
      appendText(
          "layout(set=%zu, binding=%zu) uniform %s {", //
          descriptor_set_id,                           //
          binding_id,                                  //
          INHID.c_str());
      std::vector<tid_ptr_t> _samplers;
      for (auto d : decls) {
        auto tid = d->childAs<TypedIdentifier>(0);
        OrkAssert(tid);
        dumpAstNode(tid);
        auto dt = tid->typedValueForKey<std::string>("data_type").value();
        auto id = tid->typedValueForKey<std::string>("identifier_name").value();
        if (dt.find("sampler") == 0) {
          _samplers.push_back(tid);
        } else {
          appendText("%s %s;", dt.c_str(), id.c_str());
        }
      }
      appendText("};");
      binding_id++;
      for (auto s : _samplers) {
        auto dt = s->typedValueForKey<std::string>("data_type").value();
        auto id = s->typedValueForKey<std::string>("identifier_name").value();
        appendText(
            "layout(set=%zu, binding=%zu) uniform %s %s;", //
            descriptor_set_id,                             //
            binding_id,                                    //
            dt.c_str(),                                    //
            id.c_str());
        binding_id++;
      }
    }
    descriptor_set_id++;
  }
  ////////////////////////////////////////////////////////
  void process_ios(astnode_ptr_t interface_node) {
    auto input_groups  = AstNode::collectNodesOfType<InterfaceInputs>(interface_node);
    auto output_groups = AstNode::collectNodesOfType<InterfaceOutputs>(interface_node);
    printf("  num_input_groups<%zu>\n", input_groups.size());
    printf("  num_output_groups<%zu>\n", output_groups.size());
    OrkAssert(input_groups.size() == 1);
    OrkAssert(output_groups.size() == 1);
    auto input_group  = input_groups[0];
    auto output_group = output_groups[0];
    //
    auto inputs  = AstNode::collectNodesOfType<InterfaceInput>(input_group);
    auto outputs = AstNode::collectNodesOfType<InterfaceOutput>(output_group);
    printf("  num_inputs<%zu>\n", inputs.size());
    printf("  num_outputs<%zu>\n", outputs.size());
    //
    size_t input_index = 0;
    for (auto input : inputs) {
      auto tid = input->childAs<TypedIdentifier>(0);
      OrkAssert(tid);
      dumpAstNode(tid);
      auto dt = tid->typedValueForKey<std::string>("data_type").value();
      auto id = tid->typedValueForKey<std::string>("identifier_name").value();
      appendText("layout(location=%zu) in %s %s;", input_index, dt.c_str(), id.c_str());
      input_index++;
    }
    //
    size_t output_index = 0;
    for (auto output : outputs) {
      auto tid = output->childAs<TypedIdentifier>(0);
      OrkAssert(tid);
      dumpAstNode(tid);
      auto dt = tid->typedValueForKey<std::string>("data_type").value();
      auto id = tid->typedValueForKey<std::string>("identifier_name").value();
      appendText("layout(location=%zu) out %s %s;", output_index, dt.c_str(), id.c_str());
      output_index++;
    }
  }
  ////////////////////////////////////////////////////////
  void process_extensions() {
    auto inh_exts = AstNode::collectNodesOfType<SemaInheritExtension>(_shader);
    for (auto extension_node : inh_exts) {
      auto ext_name = extension_node->typedValueForKey<std::string>("extension_name").value();
      appendText("#extension %s : enable", ext_name.c_str());
    }
  }
  ////////////////////////////////////////////////////////
  template <typename T, typename U> void process_interface_inheritances() {
    auto inh_vifs = AstNode::collectNodesOfType<T>(_shader);
    printf("inh_vifs<%zu>\n", inh_vifs.size());
    OrkAssert(inh_vifs.size() == 1);
    auto INHVIF = inh_vifs[0];
    auto INHID  = INHVIF->template typedValueForKey<std::string>("inherit_id").value();
    printf("  inh_vif<%s> INHID<%s>\n", INHVIF->_name.c_str(), INHID.c_str());
    auto VIF = _transu->template find<U>(INHID);
    OrkAssert(VIF);
    process_ios(VIF);
    process_uniformsets(VIF);
  }
  ////////////////////////////////////////////////////////
  void compile_shader(shaderc_shader_kind shader_type) {
    ///////////////////////////////////////////////////////
    // final prep for shaderc
    ///////////////////////////////////////////////////////

    // version line
    _group->insertChildAt<InsertLine>(0, "#version 450");
    _group->appendChild<InsertLine>("void main()");
    // shader body (should be a compount statement, with {} included...)
    _group->appendChildrenFrom(_shader);

    ///////////////////////////////////////////////////////
    // emit
    ///////////////////////////////////////////////////////

    auto as_glsl  = shadlang::toGLFX1(_group);
    _shader_name = _shader->typedValueForKey<std::string>("object_name").value();
    printf("// shader<%s>:\n%s\n", _shader_name.c_str(), as_glsl.c_str());
    

    ///////////////////////////////////////////////////////
    // compile with shaderc
    ///////////////////////////////////////////////////////

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv( //
        as_glsl.c_str(),                                              // glsl source (string)
        as_glsl.length(),                                             // glsl source length
        shader_type,                                                  // shader type
        "main",                                                       // entry point name
        options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
      std::cerr << result.GetErrorMessage();
      OrkAssert(false);
    }

    _spirv_binary = vkfxshader_bin_t(result.cbegin(), result.cend());
    hexdumpu32s(_spirv_binary);
#if 0 // defined( __APPLE__ )
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
  }
  ////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

VkFxShaderObject::VkFxShaderObject(vkcontext_rawptr_t ctx, vkfxshader_bin_t bin) //
    : _spirv_binary(bin) {                                                       //

  _shadermoduleinfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  _shadermoduleinfo.codeSize = bin.size() * sizeof(uint32_t);
  _shadermoduleinfo.pCode    = bin.data();
   VkResult result = vkCreateShaderModule( //
      ctx->_vkdevice,      //
      &_shadermoduleinfo,  //
      nullptr,             //
      &_shadermodule);     //
  OrkAssert(result == VK_SUCCESS);
}

///////////////////////////////////////////////////////////////////////////////

bool VkFxInterface::LoadFxShader(const AssetPath& input_path, FxShader* pshader) {

  auto it = _GVI->_shared_fxshaderfiles.find(input_path);
  vkfxsfile_ptr_t vulkan_shaderfile;
  ////////////////////////////////////////////
  // if not yet loaded, load...
  ////////////////////////////////////////////
  if (it == _GVI->_shared_fxshaderfiles.end()) {

    ////////////////////////////////////////////
    // first check precompiled shader cache
    ////////////////////////////////////////////
    auto str_read = ork::File::readAsString(input_path);
    OrkAssert(str_read != nullptr);
    auto basehasher = DataBlock::createHasher();
    basehasher->accumulateString("vkfxshader-1.0");
    basehasher->accumulateString(str_read->_data);
    uint64_t hashkey    = basehasher->result();
    auto vkfx_datablock = DataBlockCache::findDataBlock(hashkey);
    ////////////////////////////////////////////
    // shader binary already cached
    ////////////////////////////////////////////
    if (vkfx_datablock) {
      OrkAssert(false);
    }
    ////////////////////////////////////////////
    // shader binary not cached, compile and cache
    ////////////////////////////////////////////
    else {

      /////////////////////////////////////////////////////////////////////////////
      // compile all shaders from translation unit
      /////////////////////////////////////////////////////////////////////////////

      vulkan_shaderfile                       = std::make_shared<VkFxShaderFile>();
      vulkan_shaderfile->_trans_unit          = shadlang::parseFromString(str_read->_data);
      _GVI->_shared_fxshaderfiles[input_path] = vulkan_shaderfile;
      auto vtx_shaders                        = AstNode::collectNodesOfType<VertexShader>(vulkan_shaderfile->_trans_unit);
      auto frg_shaders                        = AstNode::collectNodesOfType<FragmentShader>(vulkan_shaderfile->_trans_unit);
      auto cu_shaders                         = AstNode::collectNodesOfType<ComputeShader>(vulkan_shaderfile->_trans_unit);

      size_t num_vtx_shaders = vtx_shaders.size();
      size_t num_frg_shaders = frg_shaders.size();
      size_t num_cu_shaders  = cu_shaders.size();

      printf("num_vtx_shaders<%zu>\n", num_vtx_shaders);
      printf("num_frg_shaders<%zu>\n", num_frg_shaders);
      printf("num_cu_shaders<%zu>\n", num_cu_shaders);

      shader_proc_context SPC;
      SPC._shaderfile = vulkan_shaderfile;
      SPC._transu     = vulkan_shaderfile->_trans_unit;
      //////////////////
      // vertex shaders
      //////////////////
      for (auto vshader : vtx_shaders) {
        SPC._shader = vshader;
        SPC._group  = std::make_shared<MiscGroupNode>();
        SPC.process_extensions();
        SPC.process_interface_inheritances<SemaInheritVertexInterface, VertexInterface>();
        SPC.compile_shader(shaderc_glsl_vertex_shader);
        auto vulkan_shobj = std::make_shared<VkFxShaderObject>(_contextVK,SPC._spirv_binary);
        vulkan_shobj->_astnode = vshader;
        vulkan_shaderfile->_shaderobjects[SPC._shader_name] = vulkan_shobj;
      }

      //////////////////
      // fragment shaders
      //////////////////
      for (auto fshader : frg_shaders) {
        SPC._shader = fshader;
        SPC._group  = std::make_shared<MiscGroupNode>();
        SPC.process_extensions();
        SPC.process_interface_inheritances<SemaInheritFragmentInterface, FragmentInterface>();
        SPC.compile_shader(shaderc_glsl_fragment_shader);
        auto vulkan_shobj = std::make_shared<VkFxShaderObject>(_contextVK,SPC._spirv_binary);
        vulkan_shobj->_astnode = fshader;
        vulkan_shaderfile->_shaderobjects[SPC._shader_name] = vulkan_shobj;
      }

      //////////////////
      // compute shaders
      //////////////////
      for (auto cshader : cu_shaders) {
        SPC._shader = cshader;
        SPC._group  = std::make_shared<MiscGroupNode>();
        SPC.process_extensions();
        SPC.compile_shader(shaderc_glsl_compute_shader);
        auto vulkan_shobj = std::make_shared<VkFxShaderObject>(_contextVK,SPC._spirv_binary);
        vulkan_shobj->_astnode = cshader;
        vulkan_shaderfile->_shaderobjects[SPC._shader_name] = vulkan_shobj;
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

  } else { // shader already loaded...
    vulkan_shaderfile = it->second;
  }
  OrkAssert(vulkan_shaderfile != nullptr);
  pshader->_internalHandle.set<vkfxsfile_ptr_t>(vulkan_shaderfile);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

FxShader* VkFxInterface::shaderFromShaderText(const std::string& name, const std::string& shadertext) {
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
