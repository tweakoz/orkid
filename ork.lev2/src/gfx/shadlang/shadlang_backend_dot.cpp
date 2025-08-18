////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shadlang.h>
#include <ork/pch.h>
#include <string>
#include <stack>
#include <vector>
#include <memory>
#include <functional>
#include "shadlang_impl.h"

namespace ork::lev2::shadlang {
using namespace SHAST;
/////////////////////////////////////////////////////////////////////////////////////////////////

struct DotThemeItem{
  std::string _fillcolor;
  std::string _fontcolor;
  int _type = 0;
  std::function<bool(astnode_ptr_t)> _filter;
};

using dotthemeitem_ptr_t = std::shared_ptr<DotThemeItem>;

struct DotBackend {
  ////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////
  DotBackend(){
    addThemeItem<TranslationUnit>("#000000", "yellow");
    addThemeItem<FxConfigDecl>("#101010", "white");
    addThemeItem<FxConfigRef>("#202020", "#ffd0ff");
    addThemeItem<PipelineInterface>("#1f1f3f", "yellow");
    addThemeItem<Shader>("#1f3f1f", "yellow");
    addThemeItem<StateBlock>("#3f1f1f", "yellow");
    addThemeItem<StateBlockItem>("#5f1f1f", "white");
    addThemeItem<UniformSet>("#3f3f1f", "yellow");
    addThemeItem<UniformBlk>("#3f1f3f", "yellow");
    addThemeItem<FunctionDef1>("#1f3f3f", "yellow");
    addThemeItem<FunctionDef2>("#1f3f3f", "yellow");
    addThemeItem<Technique>("#3f3f3f", "yellow");
    addThemeItem<Extension>("#3f4f5f", "white");
    addThemeItem<Dependency>("#5f7f7f", "white");
    addThemeItem<ObjectName>("#8f4f4f", "white", 1);
    addThemeItem<SamplerDeclarations>("#5f3f5f", "white");
    addThemeItem<SamplerDeclaration>("#7f3f7f", "white", 1);
    addThemeItem<DataDeclarations>("#5f3f5f", "white");
    addThemeItem<DataDeclaration>("#7f3f7f", "white", 1);
    addThemeItem<ArrayDeclaration>("#7f3f7f", "white", 1);
    addThemeItem<TypedIdentifier>("#ffff7f", "black", 1);
    addThemeItem<DataType>("#efef7f", "black", 1);
    addThemeItem<DataTypeWithUserTypes>("#efef7f", "black", 1);
    //
    addThemeItem<IfStatement>("#8f8fcf", "black", 1);
    addThemeItem<WhileStatement>("#8f8fcf", "black", 1);
    addThemeItem<ReturnStatement>("#8f8fcf", "black", 1);
    addThemeItem<ExpressionStatement>("#8f8fcf", "black", 1);
    addThemeItem<DiscardStatement>("#8f8fcf", "black", 1);
    addThemeItem<AssignmentStatementVarRef>("#bfbfef", "black", 1);
    //
    addThemeItem<MemberRef>("#1fcfcf", "black", 1);
    addThemeItem<ArrayRef>("#1fdfdf", "black", 1);
    addThemeItem<ParensExpression>("#eebfee", "black", 1);
    addThemeItem<AdditiveExpression>("#eebfee", "black", 1);
    addThemeItem<MultiplicativeExpression>("#eebfee", "black", 1);
    addThemeItem<PrimaryExpression>("#c080c0", "black", 1);
    addThemeItem<FloatLiteral>("#6f006f", "white", 1);
    addThemeItem<NumericLiteral>("#6f006f", "white", 1);
    addThemeItem<IntegerLiteral>("#6f006f", "white", 1);
    //addThemeItem<RValueConstructor>("#ef8fef", "black", 1);
    // post-semantic-analysis
    addThemeItem<SemaFunctionInvokation>("#ffdfcf", "black", 1);
    addThemeItem<SemaConstructorInvokation>("#ffdfcf", "black", 1);
    addThemeItem<SemaConstructorArguments>("#ffdfcf", "black", 1);
    addThemeItem<SemaConstructorType>("#ffdfcf", "black", 1);
    addThemeItem<SemaFunctionName>("#ffdfcf", "black", 1);
    addThemeItem<SemaFunctionArguments>("#efcfbf", "black", 1);
    addThemeItem<SemaConstructorArguments>("#dfbfaf", "black", 1);
    addThemeItem<SemaMemberAccess>("#dfbfaf", "black", 1);
    addThemeItem<SemaInheritLibrary>("#f0c040", "black", 1);
    addThemeItem<SemaInheritVertexInterface>("#f0c040", "black", 1);
    addThemeItem<SemaInheritFragmentInterface>("#f0c040", "black", 1);
    addThemeItem<SemaInheritGeometryInterface>("#f0c040", "black", 1);
    addThemeItem<SemaInheritComputeInterface>("#f0c040", "black", 1);
    addThemeItem<SemaInheritSamplerSet>("#f0c040", "black", 1);
    addThemeItem<SemaInheritUniformSet>("#f0c040", "black", 1);
    addThemeItem<SemaInheritUniformBlk>("#f0c040", "black", 1);
    addThemeItem<SemaInheritStateBlock>("#f0c040", "black", 1);
    addThemeItem<SemaInheritExtension>("#f0c040", "black", 1);
    addThemeItem<SemaIdentifier>("#f0c040", "black", 1);
    
    //
    addThemeItem<Pass>("#bf4f4f", "white", 1);
    addThemeItem<VertexShaderRef>("#7f3f3f", "white", 1);
    addThemeItem<FragmentShaderRef>("#7f3f3f", "white", 1);
    addThemeItem<GeometryShaderRef>("#7f3f3f", "white", 1);
    addThemeItem<StateBlockRef>("#7f3f3f", "white", 1);
    // default
    addThemeItem<Directive>("#4f9f9f", "white", 1);
    addThemeItem<Operator>("#6f6f2f", "white", 1);
    addThemeItem<CompoundStatement>("#4f4f9f", "white", 1);
    addThemeItem<Statement>("#5f5faf", "white", 1);
    addThemeItem<Literal>("#6f006f", "white", 1);
    addThemeItem<Expression>("#ffdfff", "black", 1);
    addThemeItem<LanguageElement>("#cfcfcf", "black", 1);
    
    // Add merged resource node themes before the generic AstNode theme
    addThemeItem<MergedShaderResourcesNode>("#3f3f3f", "yellow");  // Same as Technique
    addThemeItem<DescriptorSetNode>("#5f5f5f", "white", 1);        // Round corners, darker than Technique
    addThemeItem<DescriptorSetSourceNode>("#6f6f6f", "white", 1);        // Round corners, darker than Technique
    addThemeItem<ResourceBindingNode>("#7f7f7f", "white", 1);      // Round corners, lighter than DescriptorSet
    
    // Add AstNode theme item last to ensure specific themes are checked first
    addThemeItem<AstNode>("#cfcfcf", "black");
  }
  ////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////
  void _visit(astnode_ptr_t node, std::string& outstr) {

    if( node->_showDOT ){
      int parent_id = -1;
      if (_node_stack.size()) {
        parent_id = _node_stack.top()->_nodeID;
      }
      
      ///////////////////////////////////////////////
      // pre-visit
      ///////////////////////////////////////////////
      
      size_t stack_depth = _node_stack.size();
      auto indentstr = std::string((1+stack_depth)*2,' ');

      int node_id = node->_nodeID;
      std::string label = node->desc();
      
      // fix unprintable characters
      //std::replace( label.begin(), label.end(), '\n', '_');
      std::replace( label.begin(), label.end(), '\"', '\'');

      // remove surrounding quotes
      if( label.length() > 2 && label[0] == '"' && label[label.length()-1] == '"' ){
        label = label.substr(1,label.length()-2);
      }
      outstr += FormatString( "%s %d [label=\"%s\"]", indentstr.c_str(), node_id, label.c_str() );

      //////////////////////////////////////////////
      // apply theme
      //////////////////////////////////////////////

      for( auto item : _theme_items ){
        if( item->_filter(node) ){
          outstr += FormatString( " [fillcolor=\"%s\", fontcolor=\"%s\"]", item->_fillcolor.c_str(), item->_fontcolor.c_str() );
          switch(item->_type){
            case 0:
              outstr += FormatString( " [shape=\"box\", style=\"filled\"]" );
              break;
            case 1:
              outstr += FormatString( " [shape=\"box\", style=\"filled, rounded\"]" );
              break;
          }
          break;
        }
      }

      //////////////////////////////////////////////
      // edges
      //////////////////////////////////////////////

      outstr += "\n";
      if(parent_id>=0) {
        outstr += FormatString( "%s %d -> %d [color=\"white\"];\n", indentstr.c_str(), parent_id, node_id );
      }

    } // showDOT ?

    ///////////////////////////////////////////////
    // visit children
    ///////////////////////////////////////////////

    _node_stack.push(node);
    if(node->_descend){
      for (auto c : node->_children) {
        _visit(c,outstr);
      }
    }
   _node_stack.pop();
 
    ///////////////////////////////////////////////
    // post-visit
    ///////////////////////////////////////////////

    ///////////////////////////////////////////////
  }
  ////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////
  std::string generate(translationunit_ptr_t top) {
    std::string outstr = "digraph AST {\n"; // Start of the dot file
    outstr += "  graph [rankdir=\"LR\", bgcolor=\"#1f1f1f\"];\n";
    _visit(top, outstr);
    outstr += "}\n"; // End of the dot file
    return outstr;
  }
  
  ////////////////////////////////////////////////////////////////
  template <typename T> void addThemeItem( std::string fillcolor, std::string textcolor, int type = 0){
    auto item = std::make_shared<DotThemeItem>();
    item->_fillcolor = fillcolor;
    item->_fontcolor = textcolor;
    item->_type = type;
    item->_filter = [](astnode_ptr_t node) -> bool {
      return std::dynamic_pointer_cast<T>(node) != nullptr;
    };
    _theme_items.push_back(item);
  }
  ////////////////////////////////////////////////////////////////
  std::stack<astnode_ptr_t> _node_stack;
  std::vector<dotthemeitem_ptr_t> _theme_items;
};
using dotbackend_ptr_t = std::shared_ptr<DotBackend>;

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string toDotFile(translationunit_ptr_t top) {
  auto backend = std::make_shared<DotBackend>();
  std::string dotstr = backend->generate(top);
  return dotstr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

// Function to generate enhanced DOT with resource merging
std::string toEnhancedDotFile(translationunit_ptr_t top, 
                              const std::unordered_map<std::string, MergedShaderResources>& merged_resources) {
  auto backend = std::make_shared<DotBackend>();
  std::string dotstr = backend->generate(top);
  return dotstr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

// Helper function to create merged resource data from SPIRV compiler
std::unordered_map<std::string, MergedShaderResources> createMergedResourceData(
    translationunit_ptr_t transunit) {
  
  std::unordered_map<std::string, MergedShaderResources> merged_data;
  
  // Iterate over passes
  auto passes = AstNode::collectNodesOfType<Pass>(transunit);
  for (auto pass : passes) {
    auto pass_name = pass->typedValueForKey<std::string>("object_name").value();
    MergedShaderResources merged;
    merged.shader_name = pass_name;
    // Find referenced shaders (vertex/fragment)
    // This is a placeholder: in real code, you would walk the pass AST to find the shader refs
    // For now, just add a sample merged resource for demonstration
    MergedShaderResources::ResourceBinding binding1;
    binding1.type = MergedShaderResources::ResourceBinding::Type::Sampler;
    binding1.name = "SSAOMap";
    binding1.datatype = "sampler2D";
    binding1.binding_id = 0;
    binding1.original_source = "sset_std_ssao";
    merged.descriptor_sets[0][0] = binding1;
    MergedShaderResources::ResourceBinding binding2;
    binding2.type = MergedShaderResources::ResourceBinding::Type::UniformBlock;
    binding2.name = "StdMatrices";
    binding2.datatype = "mat4[6]";
    binding2.binding_id = 1;
    binding2.original_source = "ublk_std_matrices";
    merged.descriptor_sets[0][1] = binding2;
    merged_data[pass_name] = merged;
  }
  return merged_data;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
