////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

VerilogBackEnd::VerilogBackEnd(){
    fout = fopen("test.v","wt");
}
VerilogBackEnd::~VerilogBackEnd(){
}

void VerilogBackEnd::flush() {
    fclose(fout);
    fout = nullptr;
}

///////////////////////////////////

void VerilogBackEnd:: endline(){
    fputc( '\n', stdout );
    fputc( '\n', fout );
}
void VerilogBackEnd:: startline(){
    for( int j=0; j<_indentlevel*4; j++ ){
        fputc( ' ', stdout );
        fputc( ' ', fout );
    }
}

///////////////////////////////////

void VerilogBackEnd::outputline(const char*fmt, ...){
    constexpr int kmaxlen = 1024;
    char buffer[kmaxlen];
    va_list argp;
	va_start(argp, fmt);
	vsnprintf( & buffer[0], kmaxlen, fmt, argp);
	va_end(argp);

    startline();
        printf( "%s", buffer );
        fprintf( fout, "%s", buffer );
    endline();
}

///////////////////////////////////

void VerilogBackEnd::output(const char*fmt, ...){
    constexpr int kmaxlen = 1024;
    char buffer[kmaxlen];
    va_list argp;
	va_start(argp, fmt);
	vsnprintf( & buffer[0], kmaxlen, fmt, argp);
	va_end(argp);
    printf( "%s", buffer );
    fprintf( fout, "%s", buffer );
}

///////////////////////////////////

void VerilogBackEnd::output(std::string str){
    printf( "%s", str.c_str() );
    fprintf( fout, "%s", str.c_str() );
}

///////////////////////////////////

void VerilogBackEnd::beginModule(Module* m) {
}

///////////////////////////////////

void VerilogBackEnd::endModule(Module* m) {
}

///////////////////////////////////

void VerilogBackEnd::emitSegment(segment_t s) {

    assert(s);

    for( auto expr : s->_exprs )
        emitExpression(expr);

    for( auto c : s->_children ){
        if( c->_noautoemit == false )
            emitSegment(c);
    }

}
///////////////////////////////////

void VerilogBackEnd::emitRef(const Ref& r) {

    ///////////////////////////
    // output variable name
    ///////////////////////////

    auto s = _currentexpr
           ? _currentexpr->_segment
           : nullptr;

    output(r.resolveInstanceName(s));

    ///////////////////////////
    // output addressed memory reference ?
    ///////////////////////////

    if(auto as_reg = r._ref->TryAs<Reg>() ){
        auto reg = as_reg.value();
        if( auto addras = r._address.TryAs<ref_t>() ){
            output("[");
            emitRef(*addras.value());
            output("]");
        }
    }

    ///////////////////////////
    // output sliced reference ?
    ///////////////////////////

    if(r._slice_start!=-1 and r._slice_end!=-1){
        if( r._slice_start == r._slice_end )
            output("[%zu]", r._slice_end );
        else
            output("[%zu:%zu]", r._slice_end, r._slice_start);
    }
}

///////////////////////////////////

void VerilogBackEnd::emitRvalue(const Rvalue& r) {
    if(auto as_e = r._payload.TryAs<expr_t>())
        emitExpression(as_e.value());
    else if(auto as_r = r._payload.TryAs<Ref>())
        emitRef(as_r.value());
    else if(auto as_u = r._payload.TryAs<KUIntC>()){
        size_t l = as_u.value()._size;
        uint64_t val = as_u.value()._value;
        output( "%zu'h%x", l, val );
    }
    else if(auto as_s = r._payload.TryAs<KSIntC>()){
        size_t l = as_s.value()._size-1;
        int64_t val = as_s.value()._value;
        if( val>=0 )
            output( "%zu'sd%ld", l, val );
        else
            output( "-%zu'd%ld", l, -(val) );
    }
    else
        output("UnknownRvalue");
}


///////////////////////////////////

void VerilogBackEnd::emitExpression(expr_t e) {
    _currentexpr = e;
    auto& child = e->_child;
    if( auto as_op_x = child.TryAs<Unknown>() ){
        output("Unknown");
    }
    else if(auto as=child.TryAs<astnode_t>()){
        as.value()->emitVerilog(this);
    }
    else {
        output( "UnknownExpr");
    }
    _currentexpr = nullptr;
}

///////////////////////////////////

void VerilogBackEnd::emitNext(const Ref& r) {
    emitRef(r);
}

///////////////////////////////////

void VerilogBackEnd::visitModule(Module* m){
    Module* topm = nullptr;

    if( _modstack.empty()==false )
        topm = _modstack.top();

    bool pushed = false;
    if( topm!=m ){
        topm = m;
        _modstack.push(topm);
        pushed = true;
    }

    for( auto child : m->_children )
        visitModule(child);

    if( pushed ){
        _modstack.pop();
        _orderedmodules.push_back(topm);
    }

}

///////////////////////////////////

void VerilogBackEnd::visitRootModule(Module& m) {
    visitModule(&m);
    for( auto mm : _orderedmodules ){
        emitModule(mm);

    }
}

///////////////////////////////////

std::string VerilogBackEnd::legalModuleName(Module*m) const {
    auto fixname = m->_name;
    std::replace( fixname.begin(), fixname.end(), '.', '_');
    return fixname;
}

///////////////////////////////////

void VerilogBackEnd::emitModule(Module*m) {

    _indentlevel = 0;

    ////////////////////////////////
    // fixup name (make sure characters are legal)
    ////////////////////////////////

    auto fixname = legalModuleName(m);

    ////////////////////////////////

    outputline("///////////////////////////////////");
    outputline("// %s", m->_name.c_str() );
    outputline("///////////////////////////////////");
    outputline( "module %s(", fixname.c_str());
    indent();


    ////////////////////////////////
    // clocks are implicit inputs
    ////////////////////////////////

    std::vector<std::string> externals;

    for( auto clk : m->_clocks ){
        auto name = clk.first.c_str();
        externals.push_back(FormatString("input wire %s",name));
    }

    ////////////////////////////////
    // explicit external inputs and outputs
    ////////////////////////////////

    int i=0;
    int count = m->_ios_ordered.size();
    for( auto var : m->_ios_ordered ){
        i++;
        /////////////////////////////////////
        if( auto as = var.TryAs<Input>() ){
            auto& as_val = as.value();
            auto name = as_val._key.c_str();
            bool is_signed = as_val._signed;
            std::string inp = ( as_val._size == 1 )
                            ? FormatString("input wire %s",name)
                            : FormatString("input wire [%zu:0] %s", as_val._size-1, name );
            externals.push_back(inp);
        }
        /////////////////////////////////////
        else if( auto as = var.TryAs<Output>() ){
            auto& as_val = as.value();
            auto name = as_val._key.c_str();
            bool is_signed = as_val._signed;
            auto type = as_val._register
                      ? "reg"
                      : "wire";
            if( as_val._size == 1 )
                externals.push_back(FormatString("output %s %s",type,name));
            else
                externals.push_back(FormatString("output %s [%zu:0] %s", type, as_val._size-1, name ));
        }
    }

    size_t numexts = externals.size();
    for( size_t e=0; e<numexts; e++ ){
        auto ext = externals[e];
        startline();
        output( (e==(numexts-1))
                ? ext
                : ext+"," );
        endline();
    }

    unindent();
    outputline(");");

    ////////////////////////////////
    // internal registers and wires
    ////////////////////////////////

    for( auto var : m->_ios_ordered ){
        if( auto as = var.TryAs<Reg>() ){
            auto& reg = as.value();
            auto name = reg._key.c_str();
            bool is_signed = reg._signed;

            auto type = reg._signed
                      ? "reg signed"
                      : "reg";

            if( reg._depth > 1 ){
                output( "%s [%zu:0] %s [0:%d];\n",
                         type,
                         reg._size-1,
                         name,
                         reg._depth-1 );
            }
            else if( reg._size == 1 ){
                output("%s %s;\n",type, name);
            }
            else {
                output( "%s [%zu:0] %s;\n", type, reg._size-1, name );
            }
        }
        else if( auto as = var.TryAs<Wire>() ){
            auto& as_val = as.value();
            auto name = as_val._key.c_str();
            bool is_signed = as_val._signed;
            if( as_val._size == 1 ){
                output("wire %s;\n",name);
            }
            else {
                output( "wire [%zu:0] %s;\n", as_val._size-1, name );
            }
        }
    }

    ////////////////////////////////
    // instantiate child modules
    ////////////////////////////////

    for( auto c : m->_children ){

        auto fixname = legalModuleName(c);

        //////////////////////////
        // sysclock is implicit,
        //  therefore all instances get it
        //////////////////////////

        startline();

        output ( "%s __%s (sysclock",
                 fixname.c_str(),
                 fixname.c_str() );

        //////////////////////////
        // verify all io's connected
        //////////////////////////

        if(c->_instanceargs.size()!=c->countIos()){
            auto trace = bt_to_string(boost::stacktrace::stacktrace());
            printf( "%s\n", trace.c_str() );
            printf( "Module<%s> Child<%s> Failed to connect all instance ports\n", m->_name.c_str(), c->_name.c_str());
            printf( "args.size<%zu> ios.count<%zu>\n", c->_instanceargs.size(), c->countIos() );
            assert(false);
        }

        //////////////////////////
        // connect
        //////////////////////////

        for( auto a: c->_instanceargs ){
            output( ", ");
            emitRef(a);
        }
        output( ");");
        endline();

        //////////////////////////
    }
    ////////////////////////////////
    // initialize the module
    ////////////////////////////////

    outputline( "initial begin");
    indent();

    for( auto var : m->_ios_ordered ){
        if( auto as = var.TryAs<Reg>() ){
            auto reg = as.value();
            auto name = reg._key.c_str();
            if(reg._depth!=0 ) { //and reg._initdata!=nullptr){
                ////////////////////
                // initialize memory
                ////////////////////
                //$readmemb - the file method
                // todo get working from host RAM
                ////////////////////
            }
            else if(reg._depth==0) // dont init mems with 0
                outputline("%s <= 0;",name,reg._size);
        }
        else if( auto as = var.TryAs<Output>() ){
            auto out = as.value();
            if(out._register){
                auto name = out._key.c_str();
                outputline("%s <= 0;",name,out._size);
            }
        }
    }

    for( auto item : m->_initialAsts )
        item->emitVerilog(this);

    unindent();
    outputline( "end");

    ////////////////////////////////
    // recursive emission of segments
    ////////////////////////////////

    emitSegment(m->_rootsegment);

    ////////////////////////////////
    // done..
    ////////////////////////////////

    unindent();

    outputline( "endmodule" );

}

}} // namespace ork { namespace hdl {
