#include <utpp/UnitTest++.h>
#include <ork/hdl/hdl.inl>
#include <ork/kernel/spawner.h>
using namespace ork;
using namespace ork::hdl;
extern char** environ;
///////////////////////////////////////////////////
struct IOBundle : public Module {
    IOBundle(std::string name, Module* par,args_t args)
        : Module(name,par,args)
        , initref(inpuint<32>,a)
        , initref(reguint<4>,fsmstate)
        , initref(inpbool,b)
        , initref(outregbool,e)
        , initref(outbool,internal_out)
    {}
    void generate() final {
        enum FsmStates {
            FSM_INIT = 0,
            FSM_STATE1,
            FSM_STATE2,
            FSM_STATE3,
            FSM_END,
        };
        Ref internal = signal<wirebool>("internal");
        hdl_sync({
            hdl_switch(fsmstate,{
                hdl_case(FSM_INIT,{
                    fsmstate.next(FSM_STATE1);
                    e.next(0);
                });
                hdl_case(FSM_STATE1,{
                    fsmstate.next(FSM_STATE2);
                    e.next(1);
                });
                hdl_case(FSM_STATE2,{
                    fsmstate.next(FSM_STATE3);
                    e.next(1);
                });
                hdl_case(FSM_STATE3,{
                    fsmstate.next(FSM_STATE1);
                    e.next(1);
                });
            });
        });
        hdl_comb({
            internal = select(fsmstate==FSM_STATE3,
                       Rvalue(kubool(1)),
                       Rvalue(kubool(0)));
            internal_out = internal;
        });
    }
    Ref a,b,e,internal_out; //,z,e,v;
    Ref fsmstate;
};
///////////////////////////////////////////////////
struct Module1 final : public Module {
    Module1(std::string name, Module* par,args_t args)
        : Module(name,par,args)
        , initref(u32outreg,x)
        , initref(u32outreg,y) {
        auto e = signal<outbool>("o_e");
        auto ob = signal<outbool>("o_ob");
        auto iob = instance<IOBundle>("IOB",{x,y[0],e,ob});
    }
    void generate() final {
        Ref a = signal<u32reg>("a");
        Ref b = signal<u32reg>("b");
        hdl_sync({
            hdl_if(y > x,{
                x.next(x+1);
                y.next(y+2);
                auto sliced = slice(x - y,0,16);
                auto filled = l_fill0(sliced,32);
                a.next(filled);
                b.next(y - x);
            });
            hdl_else({
                //x.next((x - y) / (x + y));
                x.next(x+1);
                y.next(y+4);
                a.next(y != y);
                b.next(x - y);
            });
            hdl_endif();
        });
    }
    Ref x, y;
};
///////////////////////////////////////////////////
struct MUL final : public Module {
    MUL(const std::string& nam,
        Module* par,
        args_t args)
        : Module(nam,par,args)
        , initref(outreguint<64>,product)
        , initrefn(inpuint<32>,input,"input"+nam)
    {}
    void generate() final {
        hdl_sync({
            product.next( input*input );
        });
    }
    Ref input,product;
};
///////////////////////////////////////////////////
struct TOP : public Module {
    constexpr static size_t ROMW = 8;
    constexpr static size_t ROMD = 32;
    constexpr static size_t KROMPOTW = bitsToHold<ROMW>()-1;
    constexpr static size_t KROMPOTD = bitsToHold<ROMD>()-1;
    TOP(Module* tb,args_t args={})
        : Module("Top",tb,args)
        , initref(outregbool,lineout)
        , initref(s32reg,o)
        , initref(s32reg,o2)
        , initref(u32wire,gcdX)
        , initref(u32wire,gcdY)
        , initref(wireuint<ROMW>,romdatain)
        , initref(reguint<ROMW>,romdataout)
        , initref(reguint<KROMPOTD>,romaddr)
        , initref(regbool,romwe)
    {
        auto bram = instance<BlockMem>(
            "BRAM",{
                romaddr,
                romdataout,
                romdatain,
                romwe },
            size_t(ROMW),
            size_t(ROMD));

        const char rominit[ROMD] = "0123456789abcdef0123456789ABCDE";
        bram->_storage->initFromData(rominit,ROMD);

        auto resA = signal<u64wire>("resA");
        auto resB = signal<u64wire>("resB");
        auto m1o_e = signal<wirebool>("iob_e");
        auto m1o_ob = signal<wirebool>("iob_ob");

        auto mula = instance<MUL>("MulA",{o,resA});
        auto mulb = instance<MUL>("MulB",{o2,resB});
        auto mod1 = instance<Module1>("M1",{gcdX,gcdY,m1o_e,m1o_ob});
    }
    void generate() final {
        hdl_posedge(sysclock(), {
            auto sum = (o + ks32(1));
            auto sli = slice(sum,0,32);
            auto wra = wrap(sum);
            o.next(sli<<1);
            romaddr.next(slice(sum,0,KROMPOTD));
        });
        hdl_sync({
            lineout.next(romdatain[0]);
            o2.next(o2-3);
        });
    }
    Ref lineout, o, o2;
    Ref gcdX, gcdY;
    Ref romaddr,romdatain,romdataout,romwe;
};
///////////////////////////////////////////////////
TEST(hdl1){
    /////////////////////////////////////
    auto e = std::make_shared<VerilogBackEnd>();
    e->outputline( "`timescale 1ns / 1ns");
    e->outputline("/* verilator lint_off INITIALDLY */");
    /////////////////////////////////////
    struct TB : public TestBench {
        TB()
          : TestBench("MyTB")
          , initref(wirebool,lineout)
          , _top(this,{lineout}){
        }
        Ref lineout;
        TOP _top;
    };
    TB _dut;
    /////////////////////////////////////
    FrontEnd::get().generate(e,_dut);
    /////////////////////////////////////
    if(1){ // iverilog sim
        ork::Spawner s;
        s.mEnvironment.init_from_envp(::environ);
        s.mWorkingDirectory = getcwd(nullptr,0);
        printf("curwd<%s>\n", s.mWorkingDirectory.c_str() );
        s.mCommandLine = "iverilog";
        s.mCommandLine += " -v -o test.iverilog test.v";
        s.spawn();
        s.collectZombie();
    	int iret = system( "vvp -n test.iverilog");
    }
    /////////////////////////////////////
}
