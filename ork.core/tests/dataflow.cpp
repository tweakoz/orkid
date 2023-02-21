////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>

#include <ork/kernel/timer.h>
#include <ork/dataflow/dataflow.h>
#include <ork/application/application.h>
#include <ork/reflect/properties/register.h>

#if 1

////////////////////////////////////////////////////////////

namespace ork { namespace dataflow { namespace test {
////////////////////////////////////////////////////////////
struct ImgBase{};
struct Img32 : public ImgBase{};
struct Img64 : public ImgBase{};
////////////////////////////////////////////////////////////
} // test
////////////////////////////////////////////////////////////

template<> void outplugdata<ork::dataflow::test::ImgBase>::describeX(class_t* clazz){}
template<> void inplugdata<ork::dataflow::test::ImgBase>::describeX(class_t* clazz){}
template<> int MaxFanout<ork::dataflow::test::ImgBase>() { return 0; }

////////////////////////////////////////////////////////////
namespace test {
////////////////////////////////////////////////////////////

typedef ork::dataflow::outplugdata<ImgBase> ImgOutPlug;
typedef ork::dataflow::inplugdata<ImgBase> ImgInPlug;

struct BaseModuleData : public DgModuleData
{
	DeclareAbstractX( BaseModuleData, DgModuleData );
public:
	BaseModuleData() {}
};

void BaseModuleData::describeX(class_t* clazz)
{}


////////////////////////////////////////////////////////////

using float_ptr_t = std::shared_ptr<float>;

struct GlobalModuleData : public BaseModuleData
{
	DeclareConcreteX( GlobalModuleData, BaseModuleData );

public: //

  GlobalModuleData(){
    _outputA = std::make_shared<float>(0.0f);
    _outputB = std::make_shared<float>(0.5f);
    _outputC = std::make_shared<float>(1.0f);
  }

	static std::shared_ptr<GlobalModuleData> createShared(){
		auto gmd = std::make_shared<GlobalModuleData>();
    createOutputPlug<float>(gmd,EPR_UNIFORM,gmd->_outputA,"OutputA");
    createOutputPlug<float>(gmd,EPR_UNIFORM,gmd->_outputB,"OutputB");
    createOutputPlug<float>(gmd,EPR_UNIFORM,gmd->_outputC,"OutputC");
    return gmd;
	}

  float_ptr_t _outputA;
  float_ptr_t _outputB;
  float_ptr_t _outputC;

};

void GlobalModuleData::describeX(class_t* clazz)
{}

struct Buffer{};

Buffer gBuffer;

struct ImgModuleData : public BaseModuleData {

	DeclareConcreteX( ImgModuleData, BaseModuleData );

public: //

	static Img32 g_no_connection;

	/*Buffer& GetWriteBuffer( graphinst_ptr_t ptex )
	{	ImgOutPlug* outplug = 0;
		GetTypedOutput<ImgBase>(0,outplug);
		const ImgBase& base = outplug->GetValue();
		//printf( "MOD<%p> WBI<%d>\n", this, base.miBufferIndex );
		return gBuffer;
		//return ptex.GetBuffer(outplug->GetValue().miBufferIndex);
	}*/

};

Img32 ImgModuleData::g_no_connection;

void ImgModuleData::describeX(class_t* clazz)
{
}

////////////////////////////////////////////////////////////

using img32_ptr_t = std::shared_ptr<Img32>;
using img32_outplug_t = outplugdata<Img32>;
using img32_outplug_ptr_t = std::shared_ptr<img32_outplug_t>;
using img32_inplug_t = inplugdata<Img32>;
using img32_inplug_ptr_t = std::shared_ptr<img32_inplug_t>;


////////////////////////////////////////////////////////////

struct Img32ModuleData : public ImgModuleData
{
	DeclareConcreteX( Img32ModuleData, ImgModuleData );

public: //

	Img32ModuleData()
		: ImgModuleData() {

      _image_out = std::shared_ptr<Img32>();
		
	}

  img32_ptr_t _image_out;

protected:

  static void sharedConstructor(moduledata_ptr_t subclass_instance){
    auto as_im32mod = std::dynamic_pointer_cast<Img32ModuleData>(subclass_instance);
    createOutputPlug<Img32>(subclass_instance,EPR_UNIFORM,as_im32mod->_image_out,"Output");
  }

};

void Img32ModuleData::describeX(class_t* clazz)
{
}

////////////////////////////////////////////////////////////

struct GradientModuleData : public Img32ModuleData
{
	DeclareConcreteX( GradientModuleData, Img32ModuleData );

public: //


	GradientModuleData()
		: Img32ModuleData() {

      _image_input_A = std::shared_ptr<Img32>();
      _image_input_B = std::shared_ptr<Img32>();

	}

  static std::shared_ptr<GradientModuleData> createShared(){
    auto gmd = std::make_shared<GradientModuleData>();
    Img32ModuleData::sharedConstructor(gmd);
    createInputPlug<Img32>(gmd,EPR_UNIFORM,gmd->_image_input_A,"InputA");
    createInputPlug<Img32>(gmd,EPR_UNIFORM,gmd->_image_input_B,"InputB");
    return gmd;
  }

  img32_ptr_t _image_input_A;
  img32_ptr_t _image_input_B;

};

void GradientModuleData::describeX(class_t* clazz)
{

}

////////////////////////////////////////////////////////////
#if 0

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

}
#endif

////////////////////////////////////////////////////////////

class TestGraphData : public GraphData {
public:

	DeclareConcreteX(TestGraphData, GraphData);

	bool canConnect( inplugdata_constptr_t pin, outplugdata_constptr_t pout ) const final {
		return true;
	}

};

void TestGraphData::describeX(class_t* clazz){

}

TEST(dflow_a)
{
	auto gdata = std::make_shared<TestGraphData>();
	auto gl = GlobalModuleData::createShared();
	auto grA = GradientModuleData::createShared();
  auto grB = GradientModuleData::createShared();

	GraphData::addModule(gdata,"a",gl);
	GraphData::addModule(gdata,"grA",grA);
  GraphData::addModule(gdata,"grB",grB);

  auto gl_out = gl->outputNamed("OutputA");
  auto gra_out = grA->outputNamed("Output");
  auto grb_inp_a = grB->inputNamed("InputA");

  OrkAssert(gl_out);
  OrkAssert(gra_out);
  OrkAssert(grb_inp_a);

  gdata->safeConnect(grb_inp_a,gra_out);

  auto dgctx = std::make_shared<dgcontext>();
  auto dgsorter = std::make_shared<DgSorter>(gdata.get(),dgctx);

  auto topo = dgsorter->generateTopology(dgctx);
  OrkAssert(topo);

  dgsorter->dumpInputs(gl);
  dgsorter->dumpOutputs(gl);

  dgsorter->dumpInputs(grA);
  dgsorter->dumpOutputs(grA);

  dgsorter->dumpInputs(grB);
  dgsorter->dumpOutputs(grB);

}


////////////////////////////////////////////////////////////

}}} // namespace ork::dataflow::test

template<>
int ork::dataflow::MaxFanout<ork::dataflow::test::Img32>(){
  return 0;
}

template<>
void ork::dataflow::outplugdata<ork::dataflow::test::Img32>::describeX(class_t* clazz){

}

template<>
void ork::dataflow::inplugdata<ork::dataflow::test::Img32>::describeX(class_t* clazz){
  
}


ImplementTemplateReflectionX(ork::dataflow::outplugdata<ork::dataflow::test::Img32>,"dflowtest/outplugimg32");
ImplementTemplateReflectionX(ork::dataflow::inplugdata<ork::dataflow::test::Img32>,"dflowtest/inplugimg32");

ImplementReflectionX(ork::dataflow::test::BaseModuleData,"dflowtest/BaseModule");
ImplementReflectionX(ork::dataflow::test::GlobalModuleData,"dflowtest/GlobalModule");
ImplementReflectionX(ork::dataflow::test::TestGraphData,"dflowtest/TestGraphData");
ImplementReflectionX(ork::dataflow::test::ImgModuleData,"dflowtest/ImgModule");
ImplementReflectionX(ork::dataflow::test::Img32ModuleData,"dflowtest/Img32Module");
ImplementReflectionX(ork::dataflow::test::GradientModuleData,"dflowtest/GradientModule");

/*INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::Op1Module,"dflowtest/Op1Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::Op2Module,"dflowtest/Op2Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::Op3Module,"dflowtest/Op3Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::test::Img64Module,"dflowtest/Img64Module");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ork::dataflow::outplug<ork::dataflow::test::ImgBase>,"dflowtest/OutImgPlug");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ork::dataflow::inplug<ork::dataflow::test::ImgBase>,"dflowtest/InImgPlug");
*/
#endif
