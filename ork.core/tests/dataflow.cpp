#include <ork/pch.h>

#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>

#include <ork/kernel/timer.h>
#include <ork/dataflow/dataflow.h>
#include <ork/application/application.h>
#include <ork/reflect/RegisterProperty.h>

////////////////////////////////////////////////////////////

#define DeclareImg32OutPlug( name )\
Img32 OutDataName(name);\
ImgOutPlug	OutPlugName(name);\
ork::Object* OutAccessor##name() { return & OutPlugName(name); }

#define DeclareImg64OutPlug( name )\
Img64 OutDataName(name);\
ImgOutPlug	OutPlugName(name);\
ork::Object* OutAccessor##name() { return & OutPlugName(name); }

#define DeclareImgInpPlug( name )\
ImgInPlug	InpPlugName(name);\
ork::Object* InpAccessor##name() { return & InpPlugName(name); }

////////////////////////////////////////////////////////////

namespace ork { namespace dataflow { namespace test {
////////////////////////////////////////////////////////////
struct ImgBase{};
struct Img32 : public ImgBase{};
struct Img64 : public ImgBase{};
////////////////////////////////////////////////////////////
} // test
////////////////////////////////////////////////////////////

template<> void outplug<ork::dataflow::test::ImgBase>::Describe(){}
template<> void inplug<ork::dataflow::test::ImgBase>::Describe(){}
template<> int MaxFanout<ork::dataflow::test::ImgBase>() { return 0; }
template<> const ork::dataflow::test::ImgBase& outplug<ork::dataflow::test::ImgBase>::GetInternalData() const
{	OrkAssert(mOutputData!=0);
	return *mOutputData;
}
template<> const ork::dataflow::test::ImgBase& outplug<ork::dataflow::test::ImgBase>::GetValue() const
{
	return GetInternalData();
}

////////////////////////////////////////////////////////////
namespace test {
////////////////////////////////////////////////////////////

typedef ork::dataflow::outplug<ImgBase> ImgOutPlug;
typedef ork::dataflow::inplug<ImgBase> ImgInPlug;

class BaseModule : public dgmodule
{
	RttiDeclareAbstract( BaseModule, dgmodule );
	void Compute( workunit* wu ) override {}
	void CombineWork( const cluster* c ) override {}
public:
	BaseModule() {}
};

void BaseModule::Describe()
{}


////////////////////////////////////////////////////////////

struct GlobalModule : public BaseModule
{
	RttiDeclareConcrete( GlobalModule, BaseModule );

public: //

	DeclareFloatOutPlug( OutputA );
	DeclareFloatOutPlug( OutputB );
	DeclareFloatOutPlug( OutputC );

	void Compute( workunit* wu ) override {}
	void CombineWork( const cluster* c ) override {}

public:

	GlobalModule()
		: ConstructOutPlug( OutputA, EPR_UNIFORM )
		, ConstructOutPlug( OutputB, EPR_UNIFORM )
		, ConstructOutPlug( OutputC, EPR_UNIFORM )
		, mOutDataOutputA(1.0f) 
		, mOutDataOutputB(2.0f) 
		, mOutDataOutputC(3.0f) 
	{

	}

};

void GlobalModule::Describe()
{}

////////////////////////////////////////////////////////////

struct Buffer
{

};
Buffer gBuffer;

struct ImgModule : public BaseModule
{
	RttiDeclareAbstract( ImgModule, BaseModule );
public: //
	static Img32 gNoCon;

	Buffer& GetWriteBuffer( graph_inst& ptex )
	{	ImgOutPlug* outplug = 0;
		GetTypedOutput<ImgBase>(0,outplug);
		const ImgBase& base = outplug->GetValue();
		//printf( "MOD<%p> WBI<%d>\n", this, base.miBufferIndex );
		return gBuffer;
		//return ptex.GetBuffer(outplug->GetValue().miBufferIndex);
	}

};

Img32 ImgModule::gNoCon;

void ImgModule::Describe()
{	
}

////////////////////////////////////////////////////////////

struct Img32Module : public ImgModule
{
	RttiDeclareAbstract( Img32Module, ImgModule );

public: //

	DeclareImg32OutPlug( ImgOut );

	Img32Module()
		: ConstructOutTypPlug( ImgOut,dataflow::EPR_UNIFORM, typeid(Img32) )
		, ImgModule()
	{

	}

	dataflow::outplugbase* GetOutput(int idx) override { return & mPlugOutImgOut; }

};

void Img32Module::Describe()
{	
	RegisterObjOutPlug( Img32Module, ImgOut );
}

////////////////////////////////////////////////////////////

struct Img64Module : public ImgModule
{
	RttiDeclareAbstract( Img64Module, ImgModule );

public: //

	DeclareImg64OutPlug( ImgOut );

	Img64Module()
		: ConstructOutTypPlug( ImgOut,dataflow::EPR_UNIFORM, typeid(Img64) )
		, ImgModule()
	{

	}

	dataflow::outplugbase* GetOutput(int idx) override { return & mPlugOutImgOut; }

};

void Img64Module::Describe()
{	
	RegisterObjOutPlug( Img64Module, ImgOut );
}


////////////////////////////////////////////////////////////

struct GradientModule : public Img32Module
{
	RttiDeclareConcrete( GradientModule, Img32Module );

public: //

	//DeclareImg32OutPlug( Output );

	void Compute( workunit* wu ) override
	{
		//auto inpa = mPlugInpInputA.GetValue();
		//auto inpb = mPlugInpInputB.GetValue();
		//mOutDataOutputA = inpa*inpb;

	}

	/*inplugbase* GetInput(int idx) override
	{	ork::dataflow::inplugbase* rval = nullptr;
		switch( idx )
		{	case 0:	rval = & mPlugInpInputA; break; 
			case 1:	rval = & mPlugInpInputB; break; 
		}
		return rval;
	}*/

	void CombineWork( const cluster* c ) override {}

public:

	GradientModule()
		: Img32Module()
		//: ConstructOutPlug( OutputA,dataflow::EPR_UNIFORM )
	{

	}

};

void GradientModule::Describe()
{

}

////////////////////////////////////////////////////////////

struct Op1Module : public Img32Module
{
	RttiDeclareConcrete( Op1Module, Img32Module );

public: //

	DeclareImgInpPlug( Input );
	DeclareFloatXfPlug( FloatInp );

	void Compute( workunit* wu ) override
	{
		auto inpa = mPlugInpInput.GetValue();
	}

	inplugbase* GetInput(int idx) override
	{	ork::dataflow::inplugbase* rval = nullptr;
		switch( idx )
		{	case 0:	rval = & mPlugInpInput; break; 
			case 1:	rval = & mPlugInpFloatInp; break; 
		}
		return rval;
	}

	void CombineWork( const cluster* c ) override {}

	Op1Module()
		: ConstructInpPlug( Input,dataflow::EPR_UNIFORM,gNoCon )
		, ConstructInpPlug( FloatInp,dataflow::EPR_UNIFORM,mfFloatInp )
	{

	}

};

void Op1Module::Describe()
{
	RegisterObjInpPlug( Op1Module, Input );
	RegisterFloatXfPlug( Op1Module, FloatInp, -16.0f, 16.0f, ged::OutPlugChoiceDelegate );
}

////////////////////////////////////////////////////////////

struct Op2Module : public Img32Module
{
	RttiDeclareConcrete( Op2Module, Img32Module );

public: //

	DeclareImgInpPlug( InputA );
	DeclareImgInpPlug( InputB );

	void Compute( workunit* wu ) override
	{
		auto inpa = mPlugInpInputA.GetValue();
		auto inpb = mPlugInpInputB.GetValue();
		//mOutDataOutputA = inpa*inpb;

	}

	inplugbase* GetInput(int idx) override
	{	ork::dataflow::inplugbase* rval = nullptr;
		switch( idx )
		{	case 0:	rval = & mPlugInpInputA; break; 
			case 1:	rval = & mPlugInpInputB; break; 
		}
		return rval;
	}

	void CombineWork( const cluster* c ) override {}

	Op2Module()
		: ConstructInpPlug( InputA,dataflow::EPR_UNIFORM,gNoCon )
		, ConstructInpPlug( InputB,dataflow::EPR_UNIFORM,gNoCon )
	{

	}

};

void Op2Module::Describe()
{

}

////////////////////////////////////////////////////////////

struct Op3Module : public Img32Module
{
	RttiDeclareConcrete( Op3Module, Img32Module );

public: //

	DeclareFloatXfPlug( InputA );
	DeclareFloatXfPlug( InputB );
	DeclareFloatXfPlug( InputC );
	DeclareFloatOutPlug( OutputA );

	void Compute( workunit* wu ) override
	{
		auto inpa = mPlugInpInputA.GetValue();
		auto inpb = mPlugInpInputB.GetValue();
		auto inpc = mPlugInpInputC.GetValue();
		mOutDataOutputA = inpa*inpb+inpc;

	}

	inplugbase* GetInput(int idx) override
	{	ork::dataflow::inplugbase* rval = nullptr;
		switch( idx )
		{	case 0:	rval = & mPlugInpInputA; break; 
			case 1:	rval = & mPlugInpInputB; break; 
			case 2:	rval = & mPlugInpInputC; break; 
		}
		return rval;
	}

	void CombineWork( const cluster* c ) override {}

public:

	Op3Module()
		: ConstructOutPlug( OutputA, EPR_UNIFORM )
		, ConstructInpPlug( InputA,EPR_UNIFORM,mfInputA )
		, ConstructInpPlug( InputB,EPR_UNIFORM,mfInputB )
		, ConstructInpPlug( InputC,EPR_UNIFORM,mfInputC )
		, mOutDataOutputA(1.0f) 
		, mfInputA(2.0f)
		, mfInputB(3.0f)
		, mfInputC(4.0f)
	{

	}

};

void Op3Module::Describe()
{

}

////////////////////////////////////////////////////////////

class TestGraph : public graph_inst
{
	RttiDeclareConcrete( TestGraph, graph_inst );

public:

	bool CanConnect( const inplugbase* pin, const outplugbase* pout ) const override
	{
		return true;
	}

};

void TestGraph::Describe()
{}

/*graph<0x2314878> RefreshTopology
////////////
toposort k<0> mod<gl>
  mod<gl> out<0> reg<:-1>
  mod<gl> out<1> reg<ptex_float:0>
  mod<gl> out<2> reg<:-1>
toposort k<1> mod<gr>
  mod<gr> out<0> reg<ptex_img32:0>
toposort k<2> mod<xf>
  mod<xf> out<0> reg<ptex_img32:1>
  mod<xf> inp<0> -< module<gr> reg<ptex_img32:0>
  mod<xf> inp<5> -< module<gl> reg<ptex_float:0>
toposort k<3> mod<gr2>
  mod<gr2> out<0> reg<ptex_img32:0>
toposort k<4> mod<k>
  mod<k> out<0> reg<ptex_img64:0>
toposort k<5> mod<bl>
  mod<bl> out<0> reg<ptex_img64:1>
  mod<bl> inp<0> -< module<xf> reg<ptex_img32:1>
  mod<bl> inp<1> -< module<k> reg<ptex_img64:0>
  mod<bl> inp<2> -< module<gr2> reg<ptex_img32:0>
toposort k<6> mod<sh>
  mod<sh> out<0> reg<ptex_img32:0>
  mod<sh> inp<3> -< module<bl> reg<ptex_img64:1>
////////////
*/

////////////////////////////////////////////////////////////

TEST(dflow_1)
{	
	TestGraph tg;
	auto gl = new GlobalModule;
	auto gr = new GradientModule;
	auto gr2 = new GradientModule;
	auto sh = new Op1Module;
	auto xf = new Op1Module;
	auto bl = new Op2Module;
	//auto m2b = new Op2Module;
	//auto m3 = new Op3Module;


	tg.AddChild( "bl", bl );
	tg.AddChild( "gl", gl );
	tg.AddChild( "gr", gr );
	tg.AddChild( "gr2", gr2 );
	tg.AddChild( "sh", sh );
	//tg.AddChild( "m2b", m2b );
	//tg.AddChild( "m3", m3 );
	tg.AddChild( "xf", xf );

	auto imgout_name = AddPooledString("ImgOut");
	auto outa_name = AddPooledString("OutputA");
	auto outb_name = AddPooledString("OutputB");
	auto outc_name = AddPooledString("OutputC");

	auto fli_name = AddPooledString("FloatInp");
	auto inp_name = AddPooledString("Input");
	auto inpa_name = AddPooledString("InputA");
	auto inpb_name = AddPooledString("InputB");
	auto inpc_name = AddPooledString("InputC");

	auto gl_outa = gl->GetOutputNamed(outa_name);
	auto gl_outb = gl->GetOutputNamed(outb_name);
	auto gl_outc = gl->GetOutputNamed(outc_name);

	auto gr_out = gr->GetOutputNamed(imgout_name);
	auto gr2_out = gr2->GetOutputNamed(imgout_name);

	auto bl_inpa = bl->GetInputNamed(inpa_name);
	auto bl_inpb = bl->GetInputNamed(inpb_name);
	auto bl_out = bl->GetOutputNamed(imgout_name);

	auto xf_inp = xf->GetInputNamed(inp_name);
	auto xf_out = xf->GetOutputNamed(imgout_name);
	auto xf_fli = xf->GetInputNamed(fli_name);

	auto sh_inp = sh->GetInputNamed(inp_name);
	auto sh_out = sh->GetOutputNamed(imgout_name);
	//auto m3inpa = m3->GetInputNamed(inpa_name);
	//auto m3inpb = m3->GetInputNamed(inpb_name);
	//auto m3inpc = m3->GetInputNamed(inpc_name);
	//auto m3out = m3->GetOutputNamed(outa_name);

	OrkAssert(xf_inp);
	OrkAssert(xf_out);
	OrkAssert(xf_fli);

	OrkAssert(sh_inp);
	OrkAssert(sh_out);

	OrkAssert(gl_outa);
	OrkAssert(gl_outb);
	OrkAssert(gl_outc);

	OrkAssert(gr_out);
	OrkAssert(gr2_out);

	OrkAssert(bl_inpa);
	OrkAssert(bl_inpb);
	OrkAssert(bl_out);

	//OrkAssert(m3inpa);
	//OrkAssert(m3inpb);
	//OrkAssert(m3inpc);
	//OrkAssert(m3out);

	sh_inp->SafeConnect(tg, xf_out);
	xf_inp->SafeConnect(tg, gr_out);
	xf_fli->SafeConnect(tg, gl_outa);
	bl_inpa->SafeConnect(tg, gr_out);
	bl_inpb->SafeConnect(tg, gr2_out);

	//m3inpa->SafeConnect(tg, goutc);
	//m3inpb->SafeConnect(tg, m2aout);
	//m3inpc->SafeConnect(tg, m2bout);

	OrkAssert( tg.IsComplete() );

	dataflow::dgcontext ctx;
	dataflow::dgregisterblock float_regs("ptex_float", 4);
	dataflow::dgregisterblock img32_regs("ptex_img32", 16);
	dataflow::dgregisterblock img64_regs("ptex_img64", 4);

	ctx.SetRegisters<float>( & float_regs );
	ctx.SetRegisters<Img32>( & img32_regs );
	ctx.SetRegisters<Img64>( & img64_regs );


	tg.RefreshTopology(ctx);

	OrkAssert(false);
}

////////////////////////////////////////////////////////////

}}} // namespace ork::dataflow::test


INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::BaseModule,"dflowtest/BaseModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::GlobalModule,"dflowtest/GlobalModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::Op1Module,"dflowtest/Op1Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::Op2Module,"dflowtest/Op2Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::Op3Module,"dflowtest/Op3Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::GradientModule,"dflowtest/GradientModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::TestGraph,"dflowtest/TestGraph");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::ImgModule,"dflowtest/ImgModule");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::Img32Module,"dflowtest/Img32Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::Img64Module,"dflowtest/Img64Module");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ork::dataflow::outplug<ork::dataflow::test::ImgBase>,"dflowtest/OutImgPlug");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ork::dataflow::inplug<ork::dataflow::test::ImgBase>,"dflowtest/InImgPlug");
