////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
namespace ork { namespace hdl {
struct TestBench : public Module {
    TestBench(std::string name)
        : Module(name,nullptr)
        , initref(regbool,sysclock){
        setupTestBench();
    }
    //////////////////////////////////////////////////////
    void generate() final {
        struct TestBenchNode final : public AstNode {
            TestBenchNode(){}
            size_t bitwidth() const final { return 0; }
            void emitVerilog(VerilogBackEnd* be) const final {
                be->outputline("// begin testbench node");
                be->outputline("/* verilator lint_off STMTDLY */");
                be->outputline("initial begin");
                be->outputline(" #0 $dumpfile(\"test.vcd\");");
                be->outputline(" #0 $dumpvars;");
                be->outputline(" #1000 $finish;");
                be->outputline("end");
                be->outputline("always begin");
                be->outputline(" #5 sysclock = !sysclock;");
                be->outputline("end");
                be->outputline("/* verilator lint_on STMTDLY */");
                be->outputline("// end testbench node");
            }
        };
        auto s = pushNewSegment();
        auto e = s->createExpression(true);
        auto op = e->MakeAstNode<TestBenchNode>();
        popSegment();
    }
    //////////////////////////////////////////////////////
    Ref sysclock;
};
}} // namespace ork { namespace hdl {
