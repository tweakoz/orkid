#include <string>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <memory>
#include <cassert>

using std::string_literals::operator""s;

///////////////////////////////////////////////////////////////////////////////

std::unordered_map<char, std::string> type_map = {
    {'v', "void"},
    {'i', "int"},
    {'f', "float"},
    {'d', "double"},
    {'c', "char"},
    {'b', "bool"},
    {'s', "short"},
    {'l', "long"},
    {'x', "long long"},
    {'z', "size_t"}};

///////////////////////////////////////////////////////////////////////////////

struct Component;
using component_ptr_t = std::shared_ptr<Component>;

struct Component {
  std::string _name;
  component_ptr_t _parent_namespace;
  std::vector<component_ptr_t> _children;
  virtual std::string dump(int index = 0) const = 0;
  void ast_dump(int index = 0) const;
  std::string indent_str(int index) const;
  virtual ~Component() = default;

  bool _is_method    = false;
  bool _is_template  = false;
  bool _is_namespace = false;
  bool _is_const     = false;
  bool _is_pointer   = false;
  bool _is_lvalue_reference = false;
  bool _is_rvalue_reference = false;
  std::string _node_type;
};

struct Arguments : public Component {
  Arguments() {
    _node_type = "Arguments";
  }
  std::string dump(int index = 0) const final;
};
struct Identifier : public Component {
  Identifier() {
    _node_type = "Identifier";
  }
  std::string dump(int index = 0) const final;
};
struct Method : public Component {
  Method() {
    _node_type = "Method";
  }
  std::string dump(int index = 0) const final;
  bool _is_const = false;
};
struct Type : public Component {
  Type() {
    _node_type = "Type";
  }
  std::string dump(int index = 0) const final;
};
struct Namespace : public Component {
  Namespace() {
    _node_type = "Namespace";
  }
  std::string dump(int index = 0) const final;
};
struct TemplateArgs : public Component {
  TemplateArgs() {
    _node_type = "TemplateArgs";
  }
  std::string dump(int index = 0) const final;
};
struct TemplateArg : public Component {
  TemplateArg() {
    _node_type = "TemplateArg";
  }
  std::string dump(int index = 0) const final;
};
struct ScopeZ : public Component {
  ScopeZ() {
    _node_type = "ScopeZ";
  }
  std::string dump(int index = 0) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct Demangler {
  Demangler(const std::string& input)
      : _input(input) {
    // Preload common namespaces like `std`
    auto std_namespace         = std::make_shared<Namespace>();
    std_namespace->_name       = "std";
    _namespaces_by_name["std"] = std_namespace;
    std_namespace->_name       = "__1";
    _namespaces_by_name["__1"] = std_namespace;
  }

  component_ptr_t computeTop();
  component_ptr_t parseNamespaced();
  component_ptr_t parseIdentifier();
  component_ptr_t parseTemplateArgs();
  component_ptr_t parseTemplateArg();
  component_ptr_t parseType();
  component_ptr_t parseComponent();

  char readChar(size_t idx) const;
  bool cursorValid() const;
  std::string _indent() const;

private:
  std::string _input;
  std::unordered_map<std::string, component_ptr_t> _namespaces_by_name;
  int _namespace_counter = 0;
  size_t _index          = 0;
  size_t _stack_index    = 0;
};

///////////////////////////////////////////////////////////////////////////////

std::string Demangler::_indent() const {
  int depth = 2 + (_stack_index * 2);
  return std::string(depth, ' ');
}

///////////////////////////////////////////////////////////////////////////////

std::string Component::indent_str(int index) const {
  return std::string(index, ' ');
}

///////////////////////////////////////////////////////////////////////////////

void Component::ast_dump(int index) const {
  auto indentstr = indent_str(index * 4);
  auto out_str   = _node_type + ": " + _name;
  printf("%s%s ", indentstr.c_str(), out_str.c_str());
  if (_is_const) {
    printf("const");
  }
  if (_is_pointer) {
    printf("*");
  }
  if (_is_lvalue_reference) {
    printf("&");
  }
  if (_is_rvalue_reference) {
    printf("&&");
  }
  printf("\n");
  if (_parent_namespace) {
    _parent_namespace->ast_dump(index + 1);
  }
  for (auto& arg : _children) {
    arg->ast_dump(index + 1);
  }
}

///////////////////////////////////////////////////////////////////////////////

std::string Arguments::dump(int index) const {
  return _name;
}

///////////////////////////////////////////////////////////////////////////////

std::string Identifier::dump(int index) const {
  return _name;
}

///////////////////////////////////////////////////////////////////////////////

std::string Namespace::dump(int index) const {
  std::stringstream ss;
  // ss << "Namespace(";
  if (_parent_namespace) {
    ss << _parent_namespace->dump(index + 1) << "::";
  }
  ss << _name;
  for(auto& child : _children) {
    ss << child->dump(0);
  }
  // ss << ")";
  return ss.str();
}

///////////////////////////////////////////////////////////////////////////////

std::string TemplateArgs::dump(int index) const {
  std::stringstream ss;
  ss << "<";
  for (size_t i = 0; i < _children.size(); i++) {
    ss << _children[i]->dump(0);
    if (i < _children.size() - 1) {
      ss << ",";
    }
  }
  ss << ">";
  return ss.str();
}

///////////////////////////////////////////////////////////////////////////////

std::string TemplateArg::dump(int index) const {
  std::stringstream ss;
  for (size_t i = 0; i < _children.size(); i++) {
    ss << _children[i]->dump(0);
    if (i < _children.size() - 1) {
      ss << ",";
    }
  }
  return ss.str();
}

///////////////////////////////////////////////////////////////////////////////

std::string Type::dump(int index) const {
  std::stringstream ss;
  if (_is_const) {
    ss << "const ";
  }
  if (_parent_namespace) {
    ss << _parent_namespace->dump(index + 1);
  }
  ss << _name;
  if (_is_pointer) {
    ss << "*";
  }
  if (_is_lvalue_reference) {
    ss << "&";
  }
  if (_is_rvalue_reference) {
    ss << "&&";
  }
  return ss.str();
}

///////////////////////////////////////////////////////////////////////////////

std::string Method::dump(int index) const {
  std::stringstream ss;
  // ss << "Method(";
  if (_parent_namespace) {
    ss << _parent_namespace->dump(index + 1)+"::";
  }
  ss << _name;
  ss << "(";
  for (size_t i = 0; i < _children.size(); i++) {
    ss << _children[i]->dump(0);
    if (i < _children.size() - 1) {
      ss << ", ";
    }
  }
  ss << ")";
  if (_is_const) {
    ss << " const";
  }
  // ss << ")";
  return ss.str();
}

///////////////////////////////////////////////////////////////////////////////

char Demangler::readChar(size_t idx) const {
  static size_t prev_char_idx = 0xfffffffff;
  if(idx >= _input.size()) {
    throw std::runtime_error("readChar out of bounds");
  }
  char ch     = _input[idx];
  auto indent = _indent();
  if (prev_char_idx != idx) {
     //printf("%s readChar<%zu:%c>\n", indent.c_str(), idx, ch);
    prev_char_idx = idx;
  }
  return ch;
}

///////////////////////////////////////////////////////////////////////////////

bool Demangler::cursorValid() const {
  return _index < _input.size();
}

///////////////////////////////////////////////////////////////////////////////

component_ptr_t Demangler::parseIdentifier() {
  component_ptr_t rval = std::make_shared<Identifier>();
  auto indent          = _indent();
  _stack_index++;
  char ch = readChar(_index);
   //printf("%s beg parseIdentifier<%c>\n", indent.c_str(), ch);
  size_t len = 0;
  while (cursorValid() && isdigit(readChar(_index))) {
    len = len * 10 + (readChar(_index++) - '0');
  }
  std::string component = _input.substr(_index, len);
  _index += len;
  rval->_name = component;
  _stack_index--;
  //printf("%s end parseIdentifier : <%s>\n", indent.c_str(), component.c_str());
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

component_ptr_t Demangler::parseTemplateArgs() {
  component_ptr_t rval = std::make_shared<TemplateArgs>();
  // parse the template arguments
  auto indent = _indent();
  //printf("%s beg parseTemplateArgs \n", indent.c_str() );
 _stack_index++;
  char ch = readChar(_index);
  assert(ch == 'I');
  _index++;
  while (cursorValid() && readChar(_index) != 'E') {
    auto c = parseTemplateArg();
    rval->_children.push_back(c);
  }
  if (cursorValid() and readChar(_index) == 'E') {
    _index++;
  }
  _stack_index--;
  indent = _indent();
  //printf("%s end parseTemplateArgs \n", indent.c_str() );
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

component_ptr_t Demangler::parseTemplateArg() {
  component_ptr_t rval = std::make_shared<TemplateArg>();
  // parse the template arguments
  auto indent = _indent();
  //printf("%s beg parseTemplateArg \n", indent.c_str() );
  _stack_index++;
  char ch = readChar(_index);
  if(ch=='J'){
      _index++;
      while (cursorValid() && readChar(_index) != 'E') {
        auto c = parseTemplateArg();
        rval->_children.push_back(c);
        //printf("%s parseTemplateArg arg<%s>\n", indent.c_str(), c->dump().c_str());
      }
      assert (readChar(_index) == 'E');
      _index++;
  }
  else{
    auto c = parseType();
    rval->_children.push_back(c);
  }
  _stack_index--;
  indent = _indent();
  //printf("%s end parseTemplateArg \n", indent.c_str() );
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

component_ptr_t Demangler::parseComponent() {
  component_ptr_t rval;
  auto indent = _indent();
  _stack_index++;
  char ch = readChar(_index);
  switch (ch) {
    case 'N': {
      rval = parseNamespaced();
      break;
    }
    case 'I': {
      rval = parseTemplateArgs();
      break;
    }
    default:
      if (isdigit(ch)) {
        rval = parseIdentifier();
      } else {
        throw std::runtime_error("invalid typecode in parseComponent");
      }
      break;
  }
  _stack_index--;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
// Parse nested names and store them in _components and _namespaces_by_name
///////////////////////////////////////////////////////////////////////////////

component_ptr_t Demangler::parseNamespaced() {
  component_ptr_t rval = nullptr;
  auto indent          = _indent();
  //printf("%s beg parseNamespace \n", indent.c_str() );
  _stack_index++;
  char ch                     = readChar(_index);
  indent                      = _indent();
  bool is_namespace_reference = false;
  assert(readChar(_index) == 'N');
  _index++;
  if (readChar(_index) == 'K') { // namespace reference
    _index++;
  }
  if (readChar(_index) == 'O') { // rvalue reference
    _index++;
  }
  std::vector<component_ptr_t> sub_components;
  while (cursorValid() && readChar(_index) != 'E') {
    //////////////////////////////////////////////////////
    // namespace reference
    //////////////////////////////////////////////////////
    if (readChar(_index) == 'S') {
      // parse up to but not including the '-'
      std::string C;
      _index++;
      if (readChar(_index) == 't') { // text namespace reference
        _index++;
        size_t len = 0;
        // read digits
        while (cursorValid() && isdigit(readChar(_index))) {
          len = len * 10 + (readChar(_index++) - '0');
        }
        // read text
        for (size_t i = 0; i < len; i++) {
          C += readChar(_index++);
        }
      } else if (isdigit(readChar(_index))) { // numerical namespace reference
        size_t number = 0;
        // read digits
        while (cursorValid() && isdigit(readChar(_index))) {
          int digit = readChar(_index++) - '0';
          number    = number * 10 + digit;
        }
        C = std::to_string(number + 1);
        _index++;
      } else {
        C = "0";
        _index++;
      }
      auto the_namespace = _namespaces_by_name[C];

      auto c = the_namespace;
      rval   = c;
      if(cursorValid()){
          ch     = readChar(_index);
      }
    }
    //////////////////////////////////////////////////////
    // namespace definition
    //////////////////////////////////////////////////////
    else {
      auto c = parseComponent();
      size_t nargs = c->_children.size();
      //printf("c<%s:%s> args<%zu>\n", c->_node_type.c_str(), c->_name.c_str(), nargs );
      if (auto as_template = std::dynamic_pointer_cast<TemplateArgs>(c)) {
        c->_is_template = true;
        rval->_children.push_back(c);
        //printf("WTF\n");
        c->dump();
      } else if (auto as_identifier = std::dynamic_pointer_cast<Identifier>(c)) {
        auto ns                      = std::make_shared<Namespace>();
        ns->_name                    = c->_name;
        ns->_parent_namespace        = c->_parent_namespace;
        ns->_children               = c->_children;
        ns->_is_namespace            = true;
        c                            = ns;
        auto ns_name                 = std::to_string(_namespace_counter++);
        _namespaces_by_name[ns_name] = c;
        c->_parent_namespace         = rval;
        rval                         = c;
      } else {
        assert(false);
      }
    }
  }
  _stack_index--;
  if(cursorValid()){
      if (readChar(_index) == 'E') {
        _index++;
    }
  }
  indent = _indent();
  //printf("%s end parseNamespaced<%s>\n", indent.c_str(), rval->dump().c_str());
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

component_ptr_t Demangler::parseType() {
  auto indent = _indent();
  //printf("%s beg parseType \n", indent.c_str() );
  _stack_index++;
  char ch = readChar(_index);

  auto rval = std::make_shared<Type>();

  assert(cursorValid());

  bool done = false;
  while (not done) {
    char ch = readChar(_index);
    if (ch == 'P') {
      rval->_is_pointer = true;
      _index++;
    } else if (ch == 'R') {
      rval->_is_lvalue_reference = true;
      _index++;
    } else if (ch == 'O') {
      rval->_is_rvalue_reference = true;
      _index++;
    } else if (ch == 'K') {
      rval->_is_const = true;
      _index++;
    } else if (ch == 'Z') {
      _index++;
    } else if (ch == 'N') {
      auto ns                 = parseNamespaced();
      done                    = true;
      //rval->_name             = ns->_name;
      rval->_parent_namespace = ns;
    } else if (type_map.find(ch) != type_map.end()) {
      rval->_name = type_map[ch];
      _index++;
      done = true;
    } else {
      //printf("%s parseType<%c> not handled\n", indent.c_str(), ch);
        throw std::runtime_error("invalid typecode in parseType");
    }
  }
  _stack_index--;
  //printf("%s end parseType<%s>\n", indent.c_str(), rval->_name.c_str());

  return rval;
}

///////////////////////////////////////////////////////////////////////////////
// Demangler main compute function
///////////////////////////////////////////////////////////////////////////////

component_ptr_t Demangler::computeTop() {

  if (_input.substr(0, 2) == "_Z") {
      _index   = 2;
  }

  auto top = parseComponent();

  while (cursorValid()) {
    auto typ = parseType();
    top->_children.push_back(typ);
    top->_is_method = true;
  }
  if (top->_is_method) {
    auto meth               = std::make_shared<Method>();
    meth->_name             = top->_name;
    meth->_children        = top->_children;
    meth->_parent_namespace = top->_parent_namespace;
    top                     = meth;
  }

  return top;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
int main() {
  // Example list of mangled symbols
  std::vector<std::string> mangled_symbols = {
      //"_Z17OrkAssertFunctionPKcz",
      "_ZN3ork4lev214_FtxGlDebugger8run_loopEv",
      //"_ZNK3ork4lev214_FtxGlDebugger8run_loopEv",
      "_ZNK3ork4lev29ContextGL13stateDebuggerEv",
      "_ZN3ork4lev225GlGeometryBufferInterface16DrawPrimitiveEMLERKNS0_16VertexBufferBaseENS0_13PrimitiveTypeEii",
      "_ZN3ork4lev223GeometryBufferInterface13DrawPrimitiveEPNS0_11GfxMaterialERKNS0_13VtxWriterBaseENS0_13PrimitiveTypeEi",
      "_ZN3ork4lev213GfxPrimitives13RenderQuadAtZEPNS0_11GfxMaterialEPNS0_7ContextEfffffffffb",
      "_ZN3ork2ui7Surface6DoDrawENSt3__110shared_ptrIKNS0_9DrawEventEEE",
      "_ZN3ork2ui6Widget4drawENSt3__110shared_ptrIKNS0_9DrawEventEEE",
      "_ZN3ork2ui5Group12drawChildrenENSt3__110shared_ptrIKNS0_9DrawEventEEE",
      "_ZN3ork2ui11LayoutGroup6DoDrawENSt3__110shared_ptrIKNS0_9DrawEventEEE",
      "_ZN3ork4lev27CtxGLFW11SlotRepaintEv",
      "_ZN3ork4lev27CtxGLFW7runloopEv",
      "_ZN3ork4lev28OrkEzApp14mainThreadLoopEv",
      "_ZNO8pybind116detail15argument_loaderIJNSt3__110shared_ptrIN3ork4lev28OrkEzAppEEENS_6kwargsEEE4callIiNS0_9void_typeERZNS5_",
      "15pyinit_gfx_qtezERNS_7module_EE4$_18EENS2_9enable_ifIXntsr3std7is_voidIT_EE5valueESH_E4typeEOT1_",
      "_ZZN8pybind1112cpp_function10initializeIZN3ork4lev215pyinit_gfx_qtezERNS_7module_EE4$_18iJNSt3__110shared_ptrINS3_"
      "8OrkEzAppEEENS_6kwargsEEJNS_4nameENS_9is_methodENS_7siblingEEEEvOT_PFT0_DpT1_EDpRKT2_ENUlRNS_6detail13function_callEE_8__"
      "invokeESS_",
  };

  for (const auto& symbol : mangled_symbols) {

    printf("////////////////////////////////////////////////////////////////////////////////////////////////////\n");
    std::cout << "Mangled  : " << symbol << std::endl; // << std::endl;

    try{
        Demangler demangler(symbol);
        auto c = demangler.computeTop();
        //c->ast_dump();
        std::cout << "Demangled: " << c->dump() << std::endl;
    }
    catch(const std::exception& e){
        std::cout << "Demangled: " << symbol << std::endl;
        //std::cout << "Error: " << e.what() << std::endl;
    }
    printf("////////////////////////////////////////////////////////////////////////////////////////////////////\n");
  }

  return 0;
}
*/