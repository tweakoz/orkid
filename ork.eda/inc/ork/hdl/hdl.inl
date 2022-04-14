////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <stdio.h>
#include <string>
#include <map>
#include <set>
#include <stack>
#include <vector>
#include <ork/kernel/svariant.h>
#include <ork/kernel/string/string.h>
#include <ork/math/misc_math.h>
#include <atomic>

#if defined( ORK_OSX )
#define _GNU_SOURCE
#endif 
#define BOOST_STACKTRACE_USE_ADDR2LINE
#include <boost/stacktrace.hpp>

namespace ork { namespace hdl {

typedef std::function<void()> vlambda_t;

struct FrontEnd;
struct Module;
struct Expression;
struct Ref;
struct Next;
struct Node;
struct Segment;
struct BinaryOp;
struct IBackEnd;
struct Clock;
struct Edge;
struct Rvalue;
struct SwitchNode;
struct IfNode;

typedef ork::svar128_t modulevar_t;

typedef std::shared_ptr<Module> module_t;
typedef std::shared_ptr<Segment> segment_t;
typedef std::shared_ptr<Expression> expr_t;
typedef std::shared_ptr<Ref> ref_t;
typedef std::shared_ptr<IBackEnd> backend_t;

#define initref(typ, x) x(signal<typ>(#x))
#define initrefn(typ, x, y) x(signal<typ>(y))

///////////////////////////////////////////////////

#define hdl_if(exp, l) If(exp, [=]() mutable l)
#define hdl_else(l) Else([=]() mutable l)
#define hdl_elif(exp, l) Elif(exp, [=]() mutable l)
#define hdl_endif() Endif()

#define hdl_switch(exp, l) Switch(exp, [=]() mutable l)
#define hdl_case(exp, l) Case(exp, [=]() mutable l)
#define hdl_sync(l) on_sync([=]() mutable l)
#define hdl_comb(l) assignments([=]() mutable l)
#define hdl_posedge(c, l) on_posedge(c, [=]() mutable l)

///////////////////////////////////////////////////

template <class Allocator> std::string bt_to_string(const boost::stacktrace::basic_stacktrace<Allocator>& bt) {
  if (bt) {
    return boost::stacktrace::detail::to_string(&bt.as_vector()[0], bt.size());
  } else {
    return std::string();
  }
}

///////////////////////////////////////////////////
// verify left and right bitwidths are identical
///////////////////////////////////////////////////

template <typename L, typename R> void check_bitwidths(L _l, R _r) {
  size_t lw = _l.bitwidth();
  size_t rw = _r.bitwidth();
  if (lw != rw) {
    auto trace = bt_to_string(boost::stacktrace::stacktrace());
    printf("%s\n", trace.c_str());
    printf("Bitwidth MisMatch lhs<%zu> rhs<%zu>\n", lw, rw);
    assert(false);
  }
}

///////////////////////////////////////////////////
// validate identifier for legality
///////////////////////////////////////////////////

void check_identifier(const std::string& inp) {

  static std::set<std::string> keywords = {"input", "output", "wire", "reg", "module", "begin", "end"};

  auto it = keywords.find(inp);

  if (it != keywords.end()) {
    auto trace = bt_to_string(boost::stacktrace::stacktrace());
    printf("%s\n", trace.c_str());
    printf("Invalid token is a keyword<%s>\n", inp.c_str());
    assert(false);
  }
}

///////////////////////////////////////////////////
// Backend Base
//  will eventuall have yosys BE
//  and simulation BE
///////////////////////////////////////////////////

struct IBackEnd {
  virtual ~IBackEnd() {
  }
  virtual void flush() {
  }
  virtual void beginModule(Module* m)     = 0;
  virtual void endModule(Module* m)       = 0;
  virtual void visitRootModule(Module& s) = 0;
};

///////////////////////////////////////////////////
// Verilog synthesizer
///////////////////////////////////////////////////

struct VerilogBackEnd final : public IBackEnd {

  VerilogBackEnd();
  ~VerilogBackEnd();
  void flush() final;

  void startline();
  void endline();
  void output(const char* fmt, ...);
  void output(std::string str);
  void outputline(const char* fmt, ...);
  void beginModule(Module* m) final;
  void endModule(Module* m) final;
  void emitSegment(segment_t s);
  void emitRef(const Ref& r);
  void emitRvalue(const Rvalue& r);
  void emitExpression(expr_t e);
  void emitNext(const Ref& r);
  void visitModule(Module* m);
  void visitRootModule(Module& m) final;
  void emitModule(Module* m);

  void indent() {
    _indentlevel++;
  }
  void unindent() {
    _indentlevel--;
  }

  std::string legalModuleName(Module* m) const;

  FILE* fout;
  expr_t _currentexpr = nullptr;
  std::stack<Module*> _modstack;
  std::vector<Module*> _orderedmodules;
  int _indentlevel = 0;
  int _column      = 0;
};

///////////////////////////////////////////////////

struct Unknown {
  std::string dump() const {
    return ork::FormatString("Unknown");
  }
};

///////////////////////////////////////////////////
// konstant signed integer
///////////////////////////////////////////////////

struct KSIntC {
  KSIntC(size_t size, int64_t value)
      : _size(size)
      , _value(value) {
  }
  size_t bitwidth() const {
    printf("ksintc bw<%zu>\n", _size);
    return _size;
  }
  size_t _size;
  int64_t _value;
};

///////////////////////////////////////////////////
// konstant unsigned integer
///////////////////////////////////////////////////

struct KUIntC {
  KUIntC(size_t size, uint64_t value)
      : _size(size)
      , _value(value) {
  }
  size_t bitwidth() const {
    printf("kuintc bw<%zu>\n", _size);
    return _size;
  }
  size_t _size;
  uint64_t _value;
};

///////////////////////////////////////////////////
// module input (wire)
///////////////////////////////////////////////////

struct Input {
  Input(std::string key, size_t size)
      : _key(key)
      , _size(size) {
  }
  size_t bitwidth() const {
    return _size;
  }
  std::string _key;
  size_t _size;
  bool _signed = false;
};

///////////////////////////////////////////////////
// module output (wire or register)
///////////////////////////////////////////////////

struct Output {
  Output(std::string key, size_t size, bool reg = false)
      : _key(key)
      , _size(size)
      , _register(reg) {
  }
  size_t bitwidth() const {
    return _size;
  }
  std::string _key;
  size_t _size;
  bool _register;
  bool _signed = false;
};

///////////////////////////////////////////////////
// module internal wire
///////////////////////////////////////////////////

struct Wire {
  Wire(std::string key, size_t size)
      : _key(key)
      , _size(size) {
  }
  size_t bitwidth() const {
    return _size;
  }
  std::string _key;
  size_t _size;
  bool _signed = false;
};

///////////////////////////////////////////////////
// module internal register (FlipFlops)
///////////////////////////////////////////////////

struct Reg {
  Reg(std::string key, size_t size)
      : _key(key)
      , _size(size) {
  }
  size_t bitwidth() const {
    return _size;
  }
  std::string _key;
  size_t _size;
  size_t _depth = 0;
  bool _signed  = false;
};

///////////////////////////////////////////////////
// RValue in the c++ sense
//  can appear on the right side of an
//   equals sign or assignment
///////////////////////////////////////////////////

struct Rvalue {

  Rvalue(expr_t r);
  Rvalue(Ref r);
  Rvalue(KUIntC u);
  Rvalue(KSIntC u);
  size_t bitwidth() const;
  ork::svar160_t _payload;
};

///////////////////////////////////////////////////
// Ref :
//  a Reference wrapper to a piece of state,
//   be it memory, wires, inputs, outputs,regs, etc...
//  references can be sliced, indexed, etc..
///////////////////////////////////////////////////

struct Ref {

  Ref(Module* m, const std::string& key, modulevar_t* r = nullptr);

  std::string resolveInstanceName(Segment* s) const;

  void next(Rvalue rhs);
  void next(int rhs);

  operator std::shared_ptr<Expression>();
  expr_t operator>(Rvalue rhs) const;
  expr_t operator<(Rvalue rhs) const;
  expr_t operator-(Rvalue rhs) const;
  expr_t operator-(int rhs) const;
  expr_t operator+(Rvalue rhs) const;
  expr_t operator+(int rhs) const;
  expr_t operator*(Rvalue rhs) const;
  expr_t operator/(Rvalue rhs) const;
  expr_t operator and(Rvalue rhs) const;
  expr_t operator or(Rvalue rhs) const;
  expr_t operator==(Rvalue rhs) const;
  expr_t operator==(int rhs) const;

  expr_t not_equal_to(Rvalue rhs) const;

  //expr_t operator!=(Rvalue rhs) const;
  expr_t operator>>(Rvalue rhs) const;
  expr_t operator<<(Rvalue rhs) const;

  Ref operator[](size_t slicebit) const;
  Ref operator[](Ref address) const;
  Ref& operator=(Rvalue rhs);
  Ref& operator=(Ref rhs);

  size_t slicewidth() const;
  size_t bitwidth() const;
  bool is_signed() const;

  Module* _module         = nullptr;
  modulevar_t* _ref       = nullptr;
  modulevar_t* _connected = nullptr;
  svar16_t _address       = nullptr;
  ssize_t _slice_start    = -1;
  ssize_t _slice_end      = -1;
  std::string _key;
};

///////////////////////////////////////////////////
// AstNode :
//  HDL synthesizer Abstract Syntax Tree
//  AST Node base class
///////////////////////////////////////////////////

struct AstNode {
  virtual ~AstNode() {
  }
  Expression* _expression = nullptr;
  virtual size_t bitwidth() const {
    return 0;
  }
  virtual void emitVerilog(VerilogBackEnd* engine) const = 0;
};

typedef std::shared_ptr<AstNode> astnode_t;

///////////////////////////////////////////////////
// RefAstNode :
//  a Ref wrapped as an AstNode
///////////////////////////////////////////////////

struct RefAstNode : AstNode {
  RefAstNode(const Ref& ref);
  size_t bitwidth() const final;
  void emitVerilog(VerilogBackEnd* engine) const;
  Ref _ref;
};

///////////////////////////////////////////////////
// Expression :
//  container for an ast node
///////////////////////////////////////////////////

struct Expression {
  Expression();
  Expression(Ref& v);
  /*template <typename T, typename... A> T& Make(A&&... args){
      T& rval = _child.Make<T>(std::forward<A>(args)...);
      rval._expression = this;
      return rval;
  };*/
  template <typename T, typename... A> T* MakeAstNode(A&&... args) {
    auto n         = new T(std::forward<A>(args)...);
    n->_expression = this;
    _child         = astnode_t(n);
    ;
    return n;
  };
  size_t bitwidth() const;
  ork::svar512_t _child;
  Segment* _segment = nullptr;
};

///////////////////////////////////////////////////
// ClockEdge
///////////////////////////////////////////////////

struct Edge {
  Edge(const std::string& name)
      : _name(name)
      , _clock(nullptr) {
  }
  std::string _name;
  Clock* _clock;
};

///////////////////////////////////////////////////
// Clock
///////////////////////////////////////////////////

struct Clock {

  Clock(std::string name = "")
      : _name(name)
      , posedge("posedge")
      , negedge("negedge") {
    posedge._clock = this;
    negedge._clock = this;
  }

  std::string _name;
  Edge posedge;
  Edge negedge;
};

///////////////////////////////////////////////////
// FrontEnd
//  top level state for HDL synthesizer
///////////////////////////////////////////////////

struct FrontEnd {

  static FrontEnd& get();
  FrontEnd();

  static segment_t segment() {
    return get()._cursegment;
  };

  void generate(backend_t e, Module& m);

  void _visitSegment(segment_t s);

  static bool dslOK() {
    return get()._dslOK.load() == 1;
  }
  static Module* curmod() {
    Module* rval = nullptr;
    if (false == get()._modulestack.empty())
      rval = get()._modulestack.top();
    return rval;
  }
  static void pushmod(Module* m) {
    get()._modulestack.push(m);
  }
  static void popmod() {
    get()._modulestack.pop();
  }
  static backend_t engine() {
    return get()._engine;
  }
  Clock sysclock;
  std::atomic<int> _dslOK;
  std::stack<Module*> _modulestack;
  backend_t _engine;

  segment_t _cursegment;
};

///////////////////////////////////////////////////
// Code segment -
//  Hierarchical code container
///////////////////////////////////////////////////

struct Segment {
  ///////////////////////////////////////
  Segment();
  ///////////////////////////////////////
  expr_t createExpression(bool add);
  ///////////////////////////////////////
  vlambda_t _l;
  std::vector<expr_t> _exprs;
  ///////////////////////////////////////
  std::vector<segment_t> _children;
  Module* _module  = nullptr;
  bool _noautoemit = false;
};

///////////////////////////////////////////////////
// Module : equivalent to verilog module
///////////////////////////////////////////////////

struct Module {

  typedef std::vector<Ref> args_t;

  Module(std::string name, Module* parent, args_t args = {});
  virtual ~Module() {
  }

  void finalize();
  void setupTestBench();
  size_t countIos() const;
  ///////////////////////////////////////
  void do_generate();
  virtual void generate() {
  }
  ///////////////////////////////////////
  void on_sync(vlambda_t l);
  void on_posedge(Clock clk, vlambda_t l);
  ///////////////////////////////////////
  void If(expr_t c, vlambda_t l);
  void Else(vlambda_t l);
  void Elif(expr_t c, vlambda_t l);
  void Endif();
  void Switch(expr_t c, vlambda_t l);
  void Case(int v, vlambda_t l);
  ///////////////////////////////////////
  void assignments(vlambda_t l);
  ///////////////////////////////////////
  Ref add_input(std::string key, size_t size);
  Ref add_output(std::string key, size_t size);
  Ref add_regout(std::string key, size_t size);
  Ref add_wire(std::string key, size_t size);
  Ref add_reg(std::string key, size_t size);
  Ref add_blockmem(std::string key, size_t size, size_t depth);
  ///////////////////////////////////////
  template <typename T> Ref signal(std::string key);
  ///////////////////////////////////////
  template <typename T, typename... A> std::shared_ptr<T> instance(std::string name, args_t conns, A&&... additional_args);
  ///////////////////////////////////////
  expr_t select(Rvalue s, Rvalue a, Rvalue b);
  expr_t slice(expr_t inp, size_t start, size_t width);
  expr_t r_fill0(expr_t inp, size_t width);
  expr_t l_fill0(expr_t inp, size_t width);
  expr_t wrap(expr_t inp);
  expr_t cat(std::vector<Rvalue> inp);
  ///////////////////////////////////////
  Ref operator[](const std::string& key);
  ///////////////////////////////////////
  Clock sysclock();
  ///////////////////////////////////////
  segment_t pushNewSegment();
  void popSegment();
  ///////////////////////////////////////
  template <typename T, typename... A> T* makeInitialAstNode(A&&... args) {
    auto n = new T(std::forward<A>(args)...);
    _initialAsts.push_back(astnode_t(n));
    return n;
  };
  ///////////////////////////////////////
  std::map<std::string, modulevar_t> _io;
  std::vector<modulevar_t> _ios_ordered;
  std::map<std::string, Clock> _clocks;
  std::vector<Module*> _children;
  std::stack<segment_t> _segstack;
  std::vector<Ref> _instanceargs;
  std::vector<astnode_t> _initialAsts;
  std::set<module_t> _submodules;
  std::string _name;
  Module* _parent = nullptr;
  std::stack<SwitchNode*> _switchstack;
  IfNode* _curifnode = nullptr;
  segment_t _rootsegment;
  bool _istestbench  = false;
  bool _isgenerating = false;
};

}} // namespace ork::hdl

///////////////////////////////////////////////////

#include "frontend.inl"
#include "oper.inl"
#include "node.inl"
#include "ref.inl"

#include "module.inl"
#include "rvalue.inl"
#include "segment.inl"
#include "types.inl"

#include "expression.inl"
#include "mem.inl"

#include "verilogbackend.inl"
#include "testbench.inl"
