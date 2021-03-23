////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

///////////////////////////////////////////////////

struct BlockMem;
struct BlockMemStorage;

typedef std::shared_ptr<BlockMem> blockmem_t;
typedef std::shared_ptr<BlockMemStorage> blockmemstorage_t;

///////////////////////////////////////////////////

struct BlockMemPort {

    BlockMemPort( std::string name, Module* m, size_t w, size_t d);

    size_t _width = 0;
    size_t _widthpot = 0;
    size_t _depth = 0;
    size_t _addrwidth = 0;
    bool _readonly = true;
    Ref address, datain, dataout, write_enable;
};

///////////////////////////////////////////////////

struct BlockMemImpl : public Module {
    BlockMemImpl(std::string name,
                 Module* parent,
                 args_t args,
                 Ref storage,
                 size_t width,
                 size_t depth );
};

///////////////////////////////////////////////////

struct BlockMemStorage{

    BlockMemStorage( Ref s, size_t w, size_t d);
    void initFromData(const char* data, size_t length);
    void onGenerate(Module* m);
    std::shared_ptr<Ref> _storage;
    const char* _initdata = nullptr;
    size_t _initdatalength = 0;
    size_t _width, _depth;

};

///////////////////////////////////////////////////

BlockMemPort::BlockMemPort(
    std::string name,
    Module* m,
     size_t w,
     size_t d)
    : _width(w)
    , _widthpot(bitsToHold(w-1))
    , _depth(d)
    , _addrwidth(bitsToHold(d-1))
    , address(m->add_input(name+"_address",_addrwidth))
    , datain(m->add_input(name+"_datain",_width))
    , dataout(m->add_regout(name+"_dataout",_width))
    , write_enable(m->add_input(name+"_write_enable",1)){

    printf( "BlockMemPort width<%zu> wPOT<%zu>\n", _width, _widthpot );
    printf( "BlockMemPort depth<%zu> ADDRWIDTH<%zu>\n", _depth, _addrwidth );

    if(w!=8 and w!=16 and w!=32){
        auto trace = bt_to_string(boost::stacktrace::stacktrace());
        printf( "%s\n", trace.c_str() );
        printf( "Unsupported BlockMemPort width\n");
        assert(false);
    }

}

///////////////////////////////////////////////////

BlockMemStorage::BlockMemStorage( Ref s, size_t w, size_t d)
    : _width(w)
    , _depth(d) {

    _storage = std::make_shared<Ref>(s);

}

///////////////////////////////////////////////////

void BlockMemStorage::initFromData(const char* data, size_t length){
    auto copyofdata = malloc(length);
    memcpy(copyofdata,data,length);
    if(length>_depth){
        printf( "BlockMemStorage<%p> initdata length<%zu> truncated to depth<%zu>", this, length, _depth );
        length = _depth;
    }
    _initdata = (const char*) copyofdata;
    _initdatalength = length;
}

///////////////////////////////////////////////////

struct MemInit : public AstNode {
    MemInit(std::string nam, BlockMemStorage* s) : _modulename(nam), _storage(s) {}
    void emitVerilog(VerilogBackEnd* engine) const;
    std::string _modulename;
    BlockMemStorage* _storage;
};

///////////////////////////////////////////////////

void BlockMemStorage::onGenerate(Module* m){
    if( _initdata != nullptr ){
        auto memi = m->makeInitialAstNode<MemInit>(m->_name,this);
    }
}

///////////////////////////////////////////////////

void MemInit::emitVerilog(VerilogBackEnd* engine) const {
    engine->startline();
    auto fname = FormatString("meminitdata_%s.hex",_modulename.c_str());
    FILE* fout = fopen(fname.c_str(),"wt");
    for( size_t i=0; i<_storage->_initdatalength; i++ ){
        char ch = _storage->_initdata[i];
        fprintf(fout,"%02x\n",ch);
    }
    fclose(fout);
    engine->output("$readmemh(\"%s\",",fname.c_str() );
    auto s = _storage->_storage.get();
    engine->emitRef(*s);
    engine->output(");");
    engine->endline();
}

///////////////////////////////////////////////////

struct BlockMem : public Module {
    BlockMem(std::string name,
             Module* parent,
             args_t args,
             size_t width,
             size_t depth );

    void generate() final;

    std::shared_ptr<BlockMemPort> _port;
    blockmemstorage_t _storage;
};

///////////////////////////////////////////////////

BlockMem::BlockMem(std::string name,
         Module* parent,
         args_t args,
         size_t width,
         size_t depth)
    : Module(name,parent,args){

    _port = std::make_shared<BlockMemPort>("porta",this,width,depth);
    Ref s = add_blockmem("storage",width,depth);
    _storage = std::make_shared<BlockMemStorage>(s,width,depth);
}

///////////////////////////////////////////////////

void BlockMem::generate() { // final

    _storage->onGenerate(this);
    auto s = _storage->_storage.get();
    hdl_sync({
        hdl_if(_port->write_enable,{
            (*s)[_port->address].next(_port->datain);
        });
        hdl_else({
            _port->dataout.next((*s)[_port->address]);
        });
    });
}

///////////////////////////////////////////////////

struct DualPortBlockMem : public Module {
    DualPortBlockMem(std::string name,Module* parent,args_t args);

    void setPortA(size_t w, size_t d, args_t args);
    void setPortB(size_t w, size_t d, args_t args);
    void generate() final;

    std::shared_ptr<BlockMemPort> _portA, _portB;
    blockmemstorage_t _storage;
    Module* _parent;
    args_t _args;
};

///////////////////////////////////////////////////

DualPortBlockMem::DualPortBlockMem( std::string name,
                                    Module* parent,
                                    args_t args )
    : Module(name,parent,{})
    , _args(args){

    // portA - 4 io's
    // portB - 4 io's
    assert(_args.size()==0);
}

///////////////////////////////////////////////////

void DualPortBlockMem::setPortA(size_t w, size_t d, args_t args){
    _portA = std::make_shared<BlockMemPort>("porta",this,w,d);
    Ref s = add_blockmem("storage",w,d); // adds Reg
    _storage = std::make_shared<BlockMemStorage>(s,w,d);
    for(auto i : args){
        _instanceargs.push_back(i);
    }
}
void DualPortBlockMem::setPortB(size_t w, size_t d, args_t args){
    _portB = std::make_shared<BlockMemPort>("portb",this,w,d);
    for(auto i : args){
        _instanceargs.push_back(i);
    }
}

///////////////////////////////////////////////////

void DualPortBlockMem::generate() { // final

    _storage->onGenerate(this);
    auto s = _storage->_storage.get();
    hdl_sync({
        hdl_if(_portA->write_enable,{
            (*s)[_portA->address].next(_portA->datain);
        });
        hdl_else({
            _portA->dataout.next((*s)[_portA->address]);
        });
    });
}

///////////////////////////////////////////////////

}} //namespace ork { namespace hdl {
