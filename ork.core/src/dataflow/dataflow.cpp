////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 



#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
template class ork::orklut<ork::PoolString,ork::dataflow::module*>;

typedef ork::dataflow::outplug<float>			OrkDataflowOutPlugFloat;
typedef ork::dataflow::inplug<float>			OrkDataflowInpPlugFloat;
typedef ork::dataflow::outplug<ork::CVector3>	OrkDataflowOutPlugFloat3;
typedef ork::dataflow::inplug<ork::CVector3>	OrkDataflowInpPlugFloat3;
typedef ork::dataflow::floatinplugxf<ork::dataflow::floatxf> OrkDataflowFloatInpPlugXf; 
typedef ork::dataflow::vect3inplugxf<ork::dataflow::vect3xf> OrkDataflowFloat3InpPlugXf; 
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::module,"dflow/module");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::dgmodule,"dflow/dgmodule");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::plugroot,"dflow/plugroot");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::inplugbase,"dflow/inplugbase");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::outplugbase,"dflow/outplugbase");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::floatinplug,"dflow/floatinplug");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::vect3inplug,"dflow/vect3inplug");

INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::modscabias,"dflow/ModScaleBias");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::floatxfitembase,"dflow/floatxfitembase");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::floatxfmsbcurve,"dflow/floatxfmsbcurve");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::floatxfmodstep,"dflow/floatxfmodstep");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::floatxf,"dflow/floatxf");
INSTANTIATE_TRANSPARENT_RTTI(ork::dataflow::vect3xf,"dflow/vect3xf");

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowOutPlugFloat,"dflow/outplug<float>");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowInpPlugFloat,"dflow/inplug<float>");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowOutPlugFloat3,"dflow/outplug<vect3>");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowInpPlugFloat3,"dflow/inplug<vect3>");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowFloatInpPlugXf,"dflow/inplugxf<float,floatxf>");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(OrkDataflowFloat3InpPlugXf,"dflow/inplugxf<vect3,vect3xf>");

INSTANTIATE_TRANSPARENT_RTTI( ork::dataflow::morph_event, "dflow/morph_event" );

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace dataflow {
///////////////////////////////////////////////////////////////////////////////

void morph_event::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

bool gbGRAPHLIVE = false;
void plugroot::Describe(){}
void inplugbase::Describe(){}
void outplugbase::Describe() {}
///////////////////////////////////////////////////////////////////////////////
template<> int MaxFanout<float>() { return 0; }
template<> void inplug<float>::Describe(){}
template<> void outplug<float>::Describe(){}
template class outplug<float>;
///////////////////////////////////////////////////////////////////////////////
template<>
const float& outplug<float>::GetInternalData() const
{	static const float kdefault = 0.0f;
	if( 0 == mOutputData ) return kdefault;
	return *mOutputData;
}
///////////////////////////////////////////////////////////////////////////////
template<> const float& outplug<float>::GetValue() const
{
	return GetInternalData();
}

///////////////////////////////////////////////////////////////////////////////
template<> int MaxFanout<CVector3>() { return 0; }
template<> void inplug<CVector3>::Describe(){}
template<> void outplug<CVector3>::Describe(){}
template class outplug<CVector3>;
///////////////////////////////////////////////////////////////////////////////
template<> const CVector3& outplug<CVector3>::GetInternalData() const
{	static const CVector3 kdefault;
	if( 0 == mOutputData )
	{
		return kdefault;
	}
	return *mOutputData;
}
///////////////////////////////////////////////////////////////////////////////
template<> const CVector3& outplug<CVector3>::GetValue() const
{
	return GetInternalData();
}
///////////////////////////////////////////////////////////////////////////////
void floatinplug::Describe()
{
	ork::reflect::RegisterProperty( "value", & floatinplug::GetValAccessor, & floatinplug::SetValAccessor );
	ork::reflect::AnnotatePropertyForEditor< floatinplug >( "value", "editor.visible", "false" );
}
///////////////////////////////////////////////////////////////////////////////
template <> void floatinplugxf< floatxf >::Describe()
{
	ork::reflect::RegisterProperty( "xf", & floatinplugxf< floatxf >::XfAccessor );
}
///////////////////////////////////////////////////////////////////////////////
void vect3inplug::Describe()
{
	ork::reflect::RegisterProperty( "value", & vect3inplug::GetValAccessor, & vect3inplug::SetValAccessor );
	ork::reflect::AnnotatePropertyForEditor< vect3inplug >( "value", "editor.visible", "false" );
}
///////////////////////////////////////////////////////////////////////////////
template <> void vect3inplugxf< vect3xf >::Describe()
{
	ork::reflect::RegisterProperty( "xf", & vect3inplugxf< vect3xf >::XfAccessor );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void modscabias::Describe()
{
	ork::reflect::RegisterProperty( "mod", & modscabias::mfMod );
	ork::reflect::RegisterProperty( "scale", & modscabias::mfScale );
	ork::reflect::RegisterProperty( "bias", & modscabias::mfBias );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "mod", "editor.range.min", "0.0f" );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "mod", "editor.range.max", "16.0f" );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "scale", "editor.range.min", "-16.0f" );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "scale", "editor.range.max", "16.0f" );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "bias", "editor.range.min", "-16.0f" );
	ork::reflect::AnnotatePropertyForEditor< modscabias >( "bias", "editor.range.max", "16.0f" );
}
void floatxfitembase::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
void floatxfmsbcurve::Describe()
{
	ork::reflect::RegisterProperty( "curve", & floatxfmsbcurve::CurveAccessor );
	ork::reflect::RegisterProperty( "ModScaleBias", & floatxfmsbcurve::ModScaleBiasAccessor );

	ork::reflect::RegisterProperty( "domodscabia", & floatxfmsbcurve::mbDoModScaBia );
	ork::reflect::RegisterProperty( "docurve", & floatxfmsbcurve::mbDoCurve );

	static const char* EdGrpStr =
      "sort:// domodscabia docurve ModScaleBias curve";

	any16 a16;
	a16.Set<const char*>(EdGrpStr);
	reflect::AnnotateClassForEditor<floatxfmsbcurve>( "editor.prop.groups", a16 );

}
///////////////////////////////////////////////////////////////////////////////
float floatxfmsbcurve::transform( float input ) const
{	
	if( mbDoModScaBia || mbDoCurve )
	{
		float fsca = (GetScale()*input)+GetBias();
		float modout = (GetMod()>0.0f) ? std::fmod(fsca,GetMod()) : fsca;
		float biasout = modout;
		input = biasout;
	}
	if( mbDoCurve )
	{
		float clampout = (input<0.0f) ? 0.0f : (input>1.0f) ? 1.0f : input;
		input = mMultiCurve1d.Sample(clampout);
	}
	return input;
}
///////////////////////////////////////////////////////////////////////////////
void floatxfmodstep::Describe()
{
	ork::reflect::RegisterProperty( "Mod", & floatxfmodstep::mMod );
	ork::reflect::RegisterProperty( "Step", & floatxfmodstep::miSteps );
	ork::reflect::RegisterProperty( "OutputBias", & floatxfmodstep::mOutputBias );
	ork::reflect::RegisterProperty( "OutputScale", & floatxfmodstep::mOutputScale );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "Mod", "editor.range.min", "0.01f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "Mod", "editor.range.max", "16.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "Step", "editor.range.min", "1.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "Step", "editor.range.max", "128.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "OutputBias", "editor.range.min", "-16.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "OutputBias", "editor.range.max", "16.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "OutputScale", "editor.range.min", "-1600.0f" );
	ork::reflect::AnnotatePropertyForEditor< floatxfmodstep >( "OutputScale", "editor.range.max", "1600.0f" );

}
///////////////////////////////////////////////////////////////////////////////
float floatxfmodstep::transform( float input ) const
{	
	int isteps = miSteps > 0 ? miSteps : 1;
	
	float fclamped = (input<0.0f) ? 0.0f : (input>1.0f) ? 1.0f : input;
	input = (mMod>0.0f) ? (std::fmod(input,mMod)/mMod) : fclamped;
	float finpsc = input*float(isteps); // 0..1 -> 0..4
	int   iinpsc = int(std::floor(finpsc)); 
	float fout = float(iinpsc)/float(isteps);
	return (fout*mOutputScale)+mOutputBias;
}
///////////////////////////////////////////////////////////////////////////////
floatxf::floatxf()
	: miTest(0)
{
}
///////////////////////////////////////////////////////////////////////////////
floatxf::~floatxf()
{
}
///////////////////////////////////////////////////////////////////////////////
void floatxf::Describe()
{
	//ork::reflect::RegisterProperty( "Test", & floatxf::miTest );

	ork::reflect::RegisterMapProperty( "Transforms", & floatxf::mTransforms );
	ork::reflect::AnnotatePropertyForEditor< floatxf >("Transforms", "editor.factorylistbase", "dflow/floatxfitembase" );
	ork::reflect::AnnotatePropertyForEditor< floatxf >("Transforms", "editor.map.policy.impexp", "true" );

	/*static const char* EdGrpStr =
      "sort:// Test";

	any16 a16;
	a16.Set<const char*>(EdGrpStr);
	reflect::AnnotateClassForEditor<floatxf>( "editor.prop.groups", a16 );
*/
}
///////////////////////////////////////////////////////////////////////////////
float floatxf::transform( float input ) const
{
	if( ! mTransforms.Empty() )
	{
		for( auto itxf : mTransforms )
		{
			floatxfitembase* pitem = rtti::autocast(itxf.second);
			if( pitem )
			{
				input = pitem->transform(input);
			}
		}

	}
	return input;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void vect3xf::Describe()
{

}
///////////////////////////////////////////////////////////////////////////////
CVector3 vect3xf::transform( const CVector3& input ) const
{	
	CVector3 output;
	output.SetX( mTransformX.transform( input.GetX() ) );
	output.SetY( mTransformX.transform( input.GetY() ) );
	output.SetZ( mTransformX.transform( input.GetZ() ) );
	return output;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/*bool inplugbase::IsDirty() const
{
	bool rv = false;
	const outplugbase* pcon = GetConnected();
	if( pcon )
	{
		rv |= pcon->IsDirty();
	}
	return rv;
}*/

plugroot::plugroot( module*pmod, EPlugDir edir, EPlugRate epr, const std::type_info& tid, const char* pname ) 
    : mePlugDir(edir)
    , mModule(pmod)
    , mTypeId(tid)
    , mePlugRate( epr )
    , mPlugName( pname ? ork::AddPooledLiteral( pname ) : ork::AddPooledLiteral( "noname" ) )
{
	printf( "plugroot<%p> pmod<%p> construct name<%s>\n", this, pmod, mPlugName.c_str() );


}
void plugroot::SetDirty( bool bv )
{
	mbDirty=bv;
	DoSetDirty(bv);
	//mModule
}
///////////////////////////////////////////////////////////////////////////////
void morphable::HandleMorphEvent(const morph_event* me)
{
	switch( me->meType )
	{
		case EMET_MORPH:
			Morph1D( me );
			break;
		case EMET_WRITE:
		case EMET_END:
		default:
			break;			
	}

}
///////////////////////////////////////////////////////////////////////////////
inplugbase::inplugbase(	module* pmod, EPlugRate epr,const std::type_info& tid, const char* pname ) 
	: plugroot(pmod,EPD_INPUT,epr,tid, pname)
	, mExternalOutput(0)
	, mpMorphable(0)
{
	if( GetModule() )
		GetModule()->AddInput(this);
}
///////////////////////////////////////////////////////////////////////////////
inplugbase::~inplugbase()
{
	if( GetModule() )
		GetModule()->RemoveInput(this);
	Disconnect();
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::DoSetDirty( bool bd )
{	if( bd )
	{	GetModule()->SetInputDirty( this );
		for( auto& item : mInternalOutputConnections )
		{
			item->SetDirty(bd);
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::ConnectInternal( outplugbase* vt )
{	mInternalOutputConnections.push_back( vt );
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::ConnectExternal( outplugbase* vt )
{	mExternalOutput = vt;
	vt->mExternalInputConnections.push_back(this);
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::SafeConnect( graph_data& gr, outplugbase* vt )
{	//OrkAssert( GetDataTypeId() == vt->GetDataTypeId() );
	bool cc = gr.CanConnect( this, vt );
	OrkAssert( cc );
	ConnectExternal( vt );
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::Disconnect()
{	if( mExternalOutput )
	{	mExternalOutput->Disconnect(this);
	}
	mExternalOutput = 0;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
outplugbase::outplugbase( module*pmod, EPlugRate epr, const std::type_info& tid, const char* pname )
	: plugroot(pmod,EPD_OUTPUT,epr,tid,pname)
	, mpRegister(0)
{
	if( GetModule() )
		GetModule()->AddOutput(this);
}
///////////////////////////////////////////////////////////////////////////////
outplugbase::~outplugbase()
{	
	if( GetModule() )
		GetModule()->RemoveOutput(this);

	while( GetNumExternalOutputConnections() )
	{	inplugbase* pcon = GetExternalOutputConnection(GetNumExternalOutputConnections()-1);
		pcon->Disconnect();
	}
}
///////////////////////////////////////////////////////////////////////////////
void outplugbase::DoSetDirty( bool bd )
{	if( bd )
	{	GetModule()->SetOutputDirty( this );
		for( auto& item : mExternalInputConnections )
		{
			item->SetDirty(bd);
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
void outplugbase::Disconnect( inplugbase* pinplug )
{	auto it= std::find(mExternalInputConnections.begin(),mExternalInputConnections.end(), pinplug );
	if( it!=mExternalInputConnections.end() )
	{
		mExternalInputConnections.erase(it);
	}
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void module::Describe()
{
}
module::module()
	: mpMorphable(nullptr)
	, mNumStaticInputs(0)
	, mNumStaticOutputs(0)
{

}
module::~module()
{
	
}

///////////////////////////////////////////////////////////////////////////////
void module::SetInputDirty( inplugbase* plg )
{	DoSetInputDirty( plg );
}
///////////////////////////////////////////////////////////////////////////////
void module::SetOutputDirty( outplugbase* plg )
{	DoSetOutputDirty( plg );
}
///////////////////////////////////////////////////////////////////////////////
void module::AddDependency( outplugbase& pout, inplugbase& pin )
{	pin.ConnectInternal( & pout );
	//DepPlugSet::value_type v( & pin, & pout );
	//mDependencies.insert( v );
}
void module::AddInput( inplugbase* plg )
{
	auto it = mStaticInputs.find( plg );
	if( it==mStaticInputs.end() )
	{
		mStaticInputs.insert(plg);
		mNumStaticInputs++;
	}
}
void module::AddOutput( outplugbase* plg )
{
	auto it = mStaticOutputs.find( plg );
	if( it==mStaticOutputs.end() )
	{
		mStaticOutputs.insert(plg);
		mNumStaticOutputs++;
	}
}
void module::RemoveInput( inplugbase* plg )
{
	auto it = mStaticInputs.find( plg );
	if( it!=mStaticInputs.end() )
	{
		mStaticInputs.erase(it);
		mNumStaticInputs--;
	}
}
void module::RemoveOutput( outplugbase* plg )
{
	auto it = mStaticOutputs.find( plg );
	if( it!=mStaticOutputs.end() )
	{
		mStaticOutputs.erase(it);
		mNumStaticOutputs--;
	}
}
inplugbase* module::GetStaticInput(int idx) const
{
	int size = mStaticInputs.size();
	auto it = mStaticInputs.begin();
	for( int i=0; i<idx; i++ )
	{
		it++;
	}
	inplugbase* rval = (it!=mStaticInputs.end())
					 ? *it
					 : nullptr;
	return rval;
}
outplugbase* module::GetStaticOutput(int idx) const
{
	int size = mStaticOutputs.size();
	auto it = mStaticOutputs.begin();
	for( int i=0; i<idx; i++ )
	{
		it++;
	}
	outplugbase* rval = (it!=mStaticOutputs.end())
					 ? *it
					 : nullptr;
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
bool module::IsDirty()
{	bool rval = false;
	int inumout = this->GetNumOutputs();
	for( int i=0; i<inumout; i++ )
	{	outplugbase* poutput = GetOutput(i);
		rval |= poutput->IsDirty();
	}
	if( false == rval )
	{	int inumchi=GetNumChildren();
		for( int ic=0; ic<inumchi; ic++ )
		{	module* pchild = GetChild(ic);
			rval |= pchild->IsDirty();
		}
	}
	return rval;
}
inplugbase* GetInput(int idx) 
{
	return 0;
}
outplugbase* GetOutput(int idx)
{
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
inplugbase* module::GetInputNamed( const PoolString& named ) 
{	int inuminp = GetNumInputs();
	for( int ip=0; ip<inuminp; ip++ )
	{	inplugbase* rval = GetInput(ip);
		OrkAssert( rval!=nullptr );
		if( named == rval->GetName() )
		{
			return rval;
		}
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
outplugbase* module::GetOutputNamed( const PoolString& named )
{	int inumout = GetNumOutputs();
	printf("module<%p> numouts<%d>\n", this, inumout );
	for( int ip=0; ip<inumout; ip++ )
	{	outplugbase* rval = GetOutput(ip);
		OrkAssert( rval!=nullptr );
		if( named == rval->GetName() )
		{
			return rval;
		}
	}
	return 0;
}
///////////////////////////////////////////////////////////////////////////////
module* module::GetChildNamed( const ork::PoolString& named ) const
{	int inumchi = GetNumChildren();
	for( int ic=0; ic<inumchi; ic++ )
	{	module* rval = GetChild(ic);
		if( named == rval->GetName() )
		{
			return rval;
		}
	}
	return 0;
}
/*bool module::IsOutputDirty(const ork::dataflow::outplugbase *pplug) const
{
	bool bv = false;
	for( DepPlugSet::const_iterator it=mDependencies.begin(); it!=mDependencies.end(); it++ )
	{
		const DepPlugSet::value_type& v = *it;

		inplugbase* pin = v.first;
		const outplugbase* pout = v.second;

		if( pout == pplug )
		{
			bv |= pin->IsDirty();
		}
	}
	return bv;
}*/
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void dgmodule::Describe()
{
	ork::reflect::RegisterProperty( "mgvpos", & dgmodule::mgvpos );
	ork::reflect::AnnotatePropertyForEditor< dgmodule >("mgvpos", "editor.visible", "false" );
}
///////////////////////////////////////////////////////////////////////////////
dgmodule::dgmodule()
	: mAffinity( dataflow::scheduler::CpuAffinity )
	, mParent(0)
	, mKey()
{
}
///////////////////////////////////////////////////////////////////////////////
void dgmodule::DivideWork( const scheduler& sch,  cluster* clus ) 
{	clus->AddModule( this );
	DoDivideWork( sch, clus );
}
///////////////////////////////////////////////////////////////////////////////
void dgmodule::DoDivideWork( const scheduler& sch,  cluster* clus ) 
{	workunit* wu = new workunit(this,clus,0);
	wu->SetAffinity( GetAffinity() );
	clus->AddWorkUnit(wu);
}
///////////////////////////////////////////////////////////////////////////////
void dgmodule::ReleaseWorkUnit( workunit* wu )
{	OrkAssert( wu->GetModule() == this );
	delete wu;
}
///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
