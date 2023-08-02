////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shadlang.h>

namespace ork::lev2::shadlang {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct DotThemeItem{
  std::string _fillcolor;
  std::string _fontcolor;
  int _type = 0;
  std::function<bool(SHAST::astnode_ptr_t)> _filter;
};

using dotthemeitem_ptr_t = std::shared_ptr<DotThemeItem>;

struct DotBackend {
  ////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////
  DotBackend(){
    addThemeItem<SHAST::TranslationUnit>("#000000", "yellow");
    addThemeItem<SHAST::PipelineInterface>("#1f1f3f", "yellow");
    addThemeItem<SHAST::Shader>("#1f3f1f", "yellow");
    addThemeItem<SHAST::StateBlock>("#3f1f1f", "yellow");
    addThemeItem<SHAST::StateBlockItem>("#5f1f1f", "white");
    addThemeItem<SHAST::UniformSet>("#3f3f1f", "yellow");
    addThemeItem<SHAST::UniformBlk>("#3f1f3f", "yellow");
    addThemeItem<SHAST::FunctionDef>("#1f3f3f", "yellow");
    addThemeItem<SHAST::Technique>("#3f3f3f", "yellow");
    addThemeItem<SHAST::Extension>("#3f4f5f", "white");
    addThemeItem<SHAST::Dependency>("#5f7f7f", "white");
    addThemeItem<SHAST::ObjectName>("#8f4f4f", "white", 1);
    addThemeItem<SHAST::DataDeclarations>("#5f3f5f", "white");
    addThemeItem<SHAST::DataDeclaration>("#7f3f7f", "white", 1);
    //
    addThemeItem<SHAST::StatementList>("#5f5faf", "white", 1);
    addThemeItem<SHAST::AssignmentStatement>("#cfcfff", "black", 1);
    addThemeItem<SHAST::AssignmentStatementVarRef>("#bfbfef", "black", 1);
    addThemeItem<SHAST::MemberRef>("#1fcfcf", "black", 1);
    addThemeItem<SHAST::ArrayRef>("#1fdfdf", "black", 1);
    addThemeItem<SHAST::Expression>("#ffdfff", "black", 1);
    addThemeItem<SHAST::Sum>("#eebfee", "black", 1);
    addThemeItem<SHAST::Product>("#eebfee", "black", 1);
    addThemeItem<SHAST::Primary>("#eebfee", "black", 1);
    addThemeItem<SHAST::Term>("#eebfee", "black", 1);
    addThemeItem<SHAST::Literal>("#df7fdf", "black", 1);
    addThemeItem<SHAST::RValueConstructor>("#ef8fef", "black", 1);
    addThemeItem<SHAST::FunctionInvokation>("#ffdfcf", "black", 1);
    addThemeItem<SHAST::FunctionInvokationArguments>("#efcfbf", "black", 1);
    addThemeItem<SHAST::FunctionInvokationArgument>("#dfbfaf", "black", 1);
    // default
    addThemeItem<SHAST::LanguageElement>("#cfcfcf", "black", 1);
    addThemeItem<SHAST::AstNode>("#cfcfcf", "black");
  }
  ////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////
  void _visit(SHAST::astnode_ptr_t node, std::string& outstr) {

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
  std::string generate(SHAST::translationunit_ptr_t top) {
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
    item->_filter = [](SHAST::astnode_ptr_t node) -> bool {
      return std::dynamic_pointer_cast<T>(node) != nullptr;
    };
    _theme_items.push_back(item);
  }
  ////////////////////////////////////////////////////////////////
  std::stack<SHAST::astnode_ptr_t> _node_stack;
  std::vector<dotthemeitem_ptr_t> _theme_items;
};
using dotbackend_ptr_t = std::shared_ptr<DotBackend>;

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string toDotFile(SHAST::translationunit_ptr_t top) {
  auto backend = std::make_shared<DotBackend>();
  std::string dotstr = backend->generate(top);
  return dotstr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
