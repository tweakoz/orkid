////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/kernel/spawner.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/shadlang.h>
#include "../../src/gfx/shadlang/shadlang_backend_spirv.h"
#include <ork/lev2/ezapp.h>
#include <boost/program_options.hpp>

using namespace ork;
using namespace ork::lev2;
namespace po = ::boost::program_options;

int main(int argc, char** argv, char** envp) {

  auto init_data  = std::make_shared<AppInitData>(argc, argv, envp);
  init_data->_enable_graphics = true;
  init_data->_offscreen = true;
  init_data->_std_asset_catalog = false;
  auto desc       = init_data->commandLineOptions("Orkid Shader Language Frontend");
  auto rval       = desc->add_options() //
      ("help", "produce help message") //
      ("in", po::value<std::string>()->default_value(""), "input shader file path") //
      ("ast", po::value<std::string>()->default_value(""), "output shader AST file path")
      ("dot", po::value<std::string>()->default_value(""), "output shader DOT file path")
      ("enhanced-dot", po::value<std::string>()->default_value(""), "output enhanced DOT file with merged resource visualization")
      ("glfx", po::value<std::string>()->default_value(""), "output shader glfx file path")
      ("spirv", "compile every stage in the unit to SPIR-V and report per-stage word counts");
  auto opts = init_data->parse();

  auto ezapp = lev2appinit(init_data);

  auto input_path = init_data->commandLineOption("in").as<std::string>();
  auto ast_output_path = init_data->commandLineOption("ast").as<std::string>();
  auto dot_output_path = init_data->commandLineOption("dot").as<std::string>();
  auto enhanced_dot_output_path = init_data->commandLineOption("enhanced-dot").as<std::string>();
  auto glfx_output_path = init_data->commandLineOption("glfx").as<std::string>();
  printf( "input_path<%s>\n", input_path.c_str());
  printf( "ast_output_path<%s>\n", ast_output_path.c_str());
  printf( "dot_output_path<%s>\n", dot_output_path.c_str());
  printf( "enhanced_dot_output_path<%s>\n", enhanced_dot_output_path.c_str());
  printf( "glfx_output_path<%s>\n", glfx_output_path.c_str());
  auto slp_cache = std::make_shared<shadlang::ShadLangParserCache>();
  auto tunit      = shadlang::parseFromFile(slp_cache, input_path);
  if(tunit){
    if( ast_output_path.length() ){
        auto ast = shadlang::SHAST::toASTstring(tunit);
        //printf( "AST<%s>\n", ast.c_str());
        bool OK = File::writeString(ast_output_path, ast);
        OrkAssert(OK);
    }
    if( dot_output_path.length() ){
        auto dot = shadlang::toDotFile(tunit);
        bool OK = File::writeString(dot_output_path, dot);
        OrkAssert(OK);
        auto spawner = std::make_shared<Spawner>();
        spawner->mCommandLine = std::string("dot -Tpng -o ") + dot_output_path + ".png " + dot_output_path;
        spawner->spawnSynchronous();

    }
    if( enhanced_dot_output_path.length() ){
        // Create merged resource data for enhanced visualization
        auto merged_resources = shadlang::createMergedResourceData(tunit);
        auto enhanced_dot = shadlang::toEnhancedDotFile(tunit, merged_resources);
        bool OK = File::writeString(enhanced_dot_output_path, enhanced_dot);
        OrkAssert(OK);
        auto spawner = std::make_shared<Spawner>();
        spawner->mCommandLine = std::string("dot -Tpng -o ") + enhanced_dot_output_path + ".png " + enhanced_dot_output_path;
        spawner->spawnSynchronous();
    }
    if( glfx_output_path.length() ){
        auto dot = shadlang::toGLFX1(tunit);
        bool OK = File::writeString(glfx_output_path, dot);
        OrkAssert(OK);

    }
    // SPIR-V: the back half of the front end. Parsing proves the grammar; only an actual
    //  shaderc compile proves the EMITTED GLSL is legal for the stage it claims to be —
    //  which is the whole question for a stage the engine has never emitted before.
    if( init_data->commandLineOption("spirv").empty() == false ){
        int failures = 0;
        auto compiler = std::make_shared<shadlang::spirv::SpirvCompiler>(tunit, true);
        auto emit_all = [&]<typename node_t>(const char* stage_name) {
          for (auto sh : shadlang::SHAST::AstNode::collectNodesOfType<node_t>(tunit)) {
            auto shader = std::dynamic_pointer_cast<shadlang::SHAST::Shader>(sh);
            auto name = shader->template typedValueForKey<std::string>("object_name").value();
            try {
              auto emitted = compiler->emitShader(shader);
              auto binary  = shadlang::spirv::SpirvCompiler::compileGlslToSpirv(
                  emitted._name, emitted._glsl, emitted._kind);
              printf("SPIRV-OK   stage<%s> name<%s> words<%zu>\n", stage_name, name.c_str(), binary.size());
            } catch (const std::exception& e) {
              printf("SPIRV-FAIL stage<%s> name<%s>: %s\n", stage_name, name.c_str(), e.what());
              failures++;
            } catch (...) {
              printf("SPIRV-FAIL stage<%s> name<%s>\n", stage_name, name.c_str());
              failures++;
            }
          }
        };
        emit_all.template operator()<shadlang::SHAST::TaskShader>("task");
        emit_all.template operator()<shadlang::SHAST::MeshShader>("mesh");
        emit_all.template operator()<shadlang::SHAST::VertexShader>("vertex");
        emit_all.template operator()<shadlang::SHAST::GeometryShader>("geometry");
        emit_all.template operator()<shadlang::SHAST::FragmentShader>("fragment");
        emit_all.template operator()<shadlang::SHAST::ComputeShader>("compute");
        if (failures) {
          printf("SPIRV: %d stage(s) FAILED to compile\n", failures);
          return -1;
        }
    }
  }
  return (tunit!=nullptr) ? 0 : -1;
}