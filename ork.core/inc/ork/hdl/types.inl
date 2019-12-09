////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2019, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace hdl {

///////////////////////////////////////////////////

inline KUIntC kubool(bool val){
    return KUIntC(1,int(val));
}
inline KUIntC ku8(uint8_t val){
    return KUIntC(8,val);
}
inline KUIntC ku16(uint16_t val){
    return KUIntC(16,val);
}
inline KUIntC ku32(uint32_t val){
    return KUIntC(32,val);
}
inline KSIntC ks32(int32_t val){
    return KSIntC(32,val);
}
inline KUIntC ku64(uint32_t val){
    return KUIntC(64,val);
}

///////////////////////////////////////////////////

struct inpbool {
    Input produce(std::string key){
        return Input(key,1);
    }
};

template <size_t S> struct inpuint {
    Input produce(std::string key){
        return Input(key,S);
    }
};

///////////////////////////////////////////////////

struct wirebool {
    Wire produce(std::string key){
        return Wire(key,1);
    }
};
template <size_t S>
struct wireuint {
    Wire produce(std::string key){
        return Wire(key,S);
    }
};

///////////////////////////////////////////////////

struct outbool {
    Output produce(std::string key){
        return Output(key,1);
    }
};
struct outu8 {
    Output produce(std::string key){
        return Output(key,8);
    }
};
struct outu16 {
    Output produce(std::string key){
        return Output(key,16);
    }
};
struct outu32 {
    Output produce(std::string key){
        return Output(key,32);
    }
};
struct outu64 {
    Output produce(std::string key){
        return Output(key,64);
    }
};

///////////////////////////////////////////////////

struct outregbool {
    Output produce(std::string key){
        return Output(key,1,true);
    }
};

template <size_t S>
struct outreguint {
    Output produce(std::string key){
        return Output(key,S,true);
    }
};

///////////////////////////////////////////////////

struct regbool {
    Reg produce(std::string key){
        return Reg(key,1);
    }
};
template <size_t S>
struct reguint {
    Reg produce(std::string key){
        return Reg(key,S);
    }
};

template <size_t S>
struct regsint {
    Reg produce(std::string key){
        auto r = Reg(key,S);
        r._signed = true;
        return r;
    }
};

///////////////////////////////////////////////////

template <size_t width, size_t depth>
struct blockmem {
    Reg produce(std::string key){
        auto r = Reg(key,width);
        r._depth = depth;
    }
};

///////////////////////////////////////////////////

typedef wireuint<32> u32wire;
typedef wireuint<64> u64wire;

typedef inpuint<32> u32inp;
typedef inpuint<64> u64inp;

typedef outu32 u32out;
typedef outu64 u64out;

typedef reguint<32> u32reg;
typedef reguint<64> u64reg;
typedef regsint<32> s32reg;
typedef regsint<64> s64reg;

typedef outreguint<32> u32outreg;
typedef outreguint<64> u64outreg;

///////////////////////////////////////////////////


}} // namespace ork { namespace hdl {
