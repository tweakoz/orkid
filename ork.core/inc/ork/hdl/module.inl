////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

Module::Module(std::string name, Module* par,args_t args)
    : _name(name)
    , _parent(par) {

    check_identifier(name);

    if( _parent )
        _parent->_children.push_back(this);

    for( auto item : args )
        _instanceargs.push_back(item);
}
void Module::finalize(){

    if( false == _istestbench ){
        sysclock();
    }

    printf( "module<%p,%s>::finalize parent<%p>\n", this, _name.c_str(),_parent);

    for( auto c : _children )
        c->finalize();

}

void Module::setupTestBench(){
    _istestbench = true;
}
///////////////////////////////////////
size_t Module::countIos() const {
    size_t rval = 0;
    for( auto io : _io ){
        auto name = io.first;
        auto var = io.second;
        if( auto as = var.TryAs<Input>() )
            rval++;
        if( auto as = var.TryAs<Output>() )
            rval++;
    }
    return rval;
}
///////////////////////////////////////
void Module::do_generate() {

    _isgenerating = true;

    _rootsegment = pushNewSegment();

    for( auto c : _children )
        c->do_generate();

    auto engine = FrontEnd::get()._engine;
    assert(engine!=nullptr);

    engine->beginModule(this);
    generate();
    engine->endModule(this);

    popSegment();
}
///////////////////////////////////////
Ref Module::operator[](const std::string& key){
    return Ref{this,key,&_io[key]};
}
///////////////////////////////////////
template <typename T>
Ref Module::signal(std::string key){
    check_identifier(key);
    modulevar_t inp;
    auto p = T();
    auto v = p.produce(key);
    inp = v;
    //assert(inp.IsA<typeid(v)>());
    printf("key<%s> bw<%zu>\n", key.c_str(), v.bitwidth() );
    _io[key]=inp;
    _ios_ordered.push_back(inp);
    return Ref{this,key,&_io[key]};
}
///////////////////////////////////////
Ref Module::add_input(std::string key,size_t size){
    check_identifier(key);
    modulevar_t inp = Input(key,size);
    _io[key]=inp;
    _ios_ordered.push_back(inp);
    return Ref{this,key,&_io[key]};
}
///////////////////////////////////////
Ref Module::add_output(std::string key,size_t size){
    check_identifier(key);
    modulevar_t inp = Output(key,size,false);
    _io[key]=inp;
    _ios_ordered.push_back(inp);
    return Ref{this,key,&_io[key]};
}
///////////////////////////////////////
Ref Module::add_regout(std::string key,size_t size){
    check_identifier(key);
    modulevar_t inp = Output(key,size,true);
    _io[key]=inp;
    _ios_ordered.push_back(inp);
    return Ref{this,key,&_io[key]};
}
///////////////////////////////////////
Ref Module::add_wire(std::string key,size_t size){
    check_identifier(key);
    modulevar_t inp = Wire(key,size);
    _io[key]=inp;
    _ios_ordered.push_back(inp);
    return Ref{this,key,&_io[key]};
}
///////////////////////////////////////
Ref Module::add_reg(std::string key,size_t size){
    check_identifier(key);
    modulevar_t inp = Reg(key,size);
    _io[key]=inp;
    _ios_ordered.push_back(inp);
    return Ref{this,key,&_io[key]};
}
///////////////////////////////////////
Ref Module::add_blockmem(std::string key,size_t size,size_t depth){
    check_identifier(key);
    auto inp = Reg(key,size);
    inp._depth = depth;
    _io[key]=inp;
    _ios_ordered.push_back(inp);
    return Ref{this,key,&_io[key]};
}
///////////////////////////////////////
void Module::on_sync(vlambda_t l) {
    auto seg = _segstack.top();
    auto e = seg->createExpression(true);
    auto s = pushNewSegment();
    s->_noautoemit = true;
    e->MakeAstNode<AlwaysBlockNode>(s,sysclock().posedge,l);
    popSegment();
}
///////////////////////////////////////
void Module::on_posedge(Clock clk,vlambda_t l) {
    auto seg = _segstack.top();
    auto e = seg->createExpression(true);
    auto s = pushNewSegment();
    s->_noautoemit = true;
    e->MakeAstNode<AlwaysBlockNode>(s,clk.posedge,l);
    popSegment();
}
///////////////////////////////////////
void Module::assignments(vlambda_t l) {
    auto& ctx = FrontEnd::get();
    auto s = pushNewSegment();
    s->_l = l;
    l();
    popSegment();
}
///////////////////////////////////////
expr_t Module::slice(expr_t inp, size_t start, size_t width) {
    auto e = _segstack.top()->createExpression(false);
    auto op = e->MakeAstNode<OperationSlice>(start,width,inp);
    return e;
}
expr_t Module::r_fill0(expr_t inp,size_t width){
    auto e = _segstack.top()->createExpression(false);
    auto op = e->MakeAstNode<OperationRightExtend>(inp,width);
    return e;
}
expr_t Module::l_fill0(expr_t inp,size_t width){
    auto e = _segstack.top()->createExpression(false);
    auto op = e->MakeAstNode<OperationLeftExtend>(inp,width);
    return e;
}
expr_t Module::wrap(expr_t inp){
    auto e = _segstack.top()->createExpression(false);
    auto op = e->MakeAstNode<WrapNode>(inp);
    return e;
}
///////////////////////////////////////
expr_t Module::select(Rvalue s,Rvalue a,Rvalue b){
    auto e = _segstack.top()->createExpression(false);
    auto op = e->MakeAstNode<SelectAstNode>(s,a,b);
    return e;
}
///////////////////////////////////////
expr_t Module::cat(std::vector<Rvalue> inp){
    auto e = _segstack.top()->createExpression(false);
    auto op = e->MakeAstNode<ConcatAstNode>(inp);
    return e;
}
///////////////////////////////////////
Clock Module::sysclock(){
    auto c = FrontEnd::get().sysclock;
    _clocks[c._name]=c;
    return c;
}
///////////////////////////////////////
segment_t Module::pushNewSegment(){
    int level = _segstack.size();
    auto s = std::make_shared<Segment>();
    s->_module = this;

    if(level>0){
        auto top = _segstack.top();
        top->_children.push_back(s);
    }

    _segstack.push(s);
    FrontEnd::get()._cursegment = s;
    return s;
}
///////////////////////////////////////
void Module::popSegment(){
    _segstack.pop();
}
///////////////////////////////////////
void Module::If(expr_t conditional,vlambda_t l) {
    if(_curifnode!=nullptr){
        auto trace = bt_to_string(boost::stacktrace::stacktrace());
        printf( "%s\n", trace.c_str() );
        printf( "we dont allow nested If's at this time..\n");
        assert(false);
    }

    auto seg = _segstack.top();
    auto e = seg->createExpression(true);

    auto s = pushNewSegment();
    s->_noautoemit = true;
    _curifnode = e->MakeAstNode<IfNode>(s,conditional,l);
    popSegment();
    //s->_l = l;
}
void Module::Elif(expr_t conditional,vlambda_t l) {
    if(_curifnode==nullptr){
        auto trace = bt_to_string(boost::stacktrace::stacktrace());
        printf( "%s\n", trace.c_str() );
        printf( "elif without if\n");
        assert(false);
    }
    auto s = pushNewSegment();
    s->_noautoemit = true;
    _curifnode->onElif(s,conditional,l);
    popSegment();
}
void Module::Else(vlambda_t l) {
    if(_curifnode==nullptr){
        auto trace = bt_to_string(boost::stacktrace::stacktrace());
        printf( "%s\n", trace.c_str() );
        printf( "Else without If.\n");
        assert(false);
    }
    auto s = pushNewSegment();
    s->_noautoemit = true;
    _curifnode->onElse(s,l);
    popSegment();
}
void Module::Endif() {
    _curifnode = nullptr;
}

///////////////////////////////////////
void Module::Switch(expr_t sel,vlambda_t l){
    auto seg = _segstack.top();
    auto e = seg->createExpression(true);
    auto curswitch = e->MakeAstNode<SwitchNode>(sel);
    _switchstack.push(curswitch);
    l();
    _switchstack.pop();
}
///////////////////////////////////////
void Module::Case(int v,vlambda_t l){
    assert(_switchstack.size()!=0);
    auto s = pushNewSegment();
    s->_l = l;
    s->_noautoemit = true;
    auto top = _switchstack.top();
    top->onCase(s,v,l);
    popSegment();
}
///////////////////////////////////////

template <typename T, typename... A>
std::shared_ptr<T> Module::instance(std::string name, args_t conns, A&&... additional_args){

    if(_isgenerating){
        auto trace = bt_to_string(boost::stacktrace::stacktrace());
        printf( "%s\n", trace.c_str() );
        printf( "we dont allow instancing during generation\n");
        assert(false);
    }

    auto new_t = std::make_shared<T>(name, this, conns, std::forward<A>(additional_args)...);
    auto as_m = std::static_pointer_cast<Module>(new_t);
    _submodules.insert(as_m);
    return new_t;
}

}} //namespace ork { namespace hdl {
