#include "rihelper.h"

#include "IECore/MarchingCubes.h"
#include "IECore/ZhuBridsonImplicitSurfaceFunction.h"

using namespace IECore;
using namespace Imath;

///////////////////////////////////////////////////////////////////////////////

var_t& var_map::operator[](const str_t&key)
{
	return the_map[key];
}

///////////////////////////////////////////////////////////////////////////////

void var_map::clear()
{
	the_map.clear();
}

///////////////////////////////////////////////////////////////////////////////

void var_map::FillCompoundDataMap(CompoundDataMap&cmap) const
{
	for( const auto& item : the_map )
	{
		const str_t& k = item.first;
		const var_t& v = item.second;

		if( v.IsA<str_t>() )
		{
			const str_t& s = v.Get<str_t>();
			cmap[k] = new StringData(s.c_str());
		}
		if( v.IsA<float_v_t>() )
		{
			const float_v_t& s = v.Get<float_v_t>();
			cmap[k] = new FloatVectorData(s);
		}
		if( v.IsA<v3f>() )
		{
			const v3f& s = v.Get<v3f>();
			cmap[k] = new V3fData(s);
		}
		if( v.IsA<float>() )
		{
			const float& s = v.Get<float>();
			cmap[k] = new FloatData(s);
		}
		if( v.IsA<v2i>() )
		{
			const v2i& s = v.Get<v2i>();
			cmap[k] = new V2iData(s);
		}
		if( v.IsA<int>() )
		{
			const int& s = v.Get<int>();
			cmap[k] = new IntData(s);
		}
		if( v.IsA<c3f>() )
		{
			const c3f& s = v.Get<c3f>();
			cmap[k] = new Color3fData(s);
		}

	}
}

///////////////////////////////////////////////////////////////////////////////

CompoundDataMap var_map::GetCompoundDataMap() const
{
	CompoundDataMap cmap;
	FillCompoundDataMap(cmap);
	return cmap;
}

IECore::CompoundData* var_map::GenCompoundData() const
{
	CompoundDataMap cmap;
	FillCompoundDataMap(cmap);
	auto ret = new IECore::CompoundData(cmap);
	return ret;
}

void var_map::display(IECoreRI::Renderer* r, const char*a,const char*b,const char*c) const
{
	r->display(a,b,c,GetCompoundDataMap() );
}

void var_map::attribute(IECoreRI::Renderer* r, const char*a) const
{
	r->setAttribute(a,GenCompoundData());
}
void var_map::light(IECoreRI::Renderer* r, const char*typ,const char*nam) const
{
	r->light(typ,nam,GetCompoundDataMap());
}
void var_map::shader(IECoreRI::Renderer* r, const char*typ,const char*nam) const
{
	r->shader(typ,nam,GetCompoundDataMap());
}

void var_map::camera(IECoreRI::Renderer* r, const char*nam) const
{
	r->camera(nam,GetCompoundDataMap());
}
void var_map::options(IECoreRI::Renderer* r,const char*nam) const
{
	auto cdat = GetCompoundDataMap();
	for( const auto& it : cdat )
	{
		std::string cnam = std::string(nam)+it.first.value();

		r->setOption(cnam.c_str(),it.second);
	}
}

///////////////////////////////////////////////////////////////////////////////

my_renderer::my_renderer( const std::string& base_name, bool bwrite_rib )
	: mpRenderer(nullptr)
	, mpDDServer(nullptr)
	, mbWriteRib(bwrite_rib)
{
	mTifFileName = base_name + ".tif";
	mRibFileName = base_name + ".rib";

	mpRenderer = new IECoreRI::Renderer(bwrite_rib?mRibFileName.c_str():"");

	if( false == mbWriteRib )
	{
		mpDDServer = new DisplayDriverServer(1559);
		usleep(2<<20);
	}

	float_v_t zero_vec;
	zero_vec.push_back(0.0f);
	zero_vec.push_back(0.0f);
	zero_vec.push_back(0.0f);
	zero_vec.push_back(0.0f);

	//////////////////////////////
	//write one image direct to memory
	//////////////////////////////
	
	var_map vmap;
	vmap["quantize"]=zero_vec;

	if( mbWriteRib )
	{
		mpRenderer->display(mTifFileName.c_str(),"tiff","rgba", vmap.GetCompoundDataMap() );
	}
	else
	{
		vmap["driverType"]=str_t("ImageDisplayDriver");
		vmap["handle"]=str_t("myLovelySphere");
		mpRenderer->display("test","ie","rgba", vmap.GetCompoundDataMap() );
	}

	var_map opts_map;
	opts_map["pixelSamples"]=v2i(6,6);
	opts_map["render:standardatmosphere"]=int(0); // enable 3delight atmos shader optimization
	opts_map.options(mpRenderer,"ri:");

	//////////////////////////////
	//write another to disk
	//////////////////////////////

}

///////////////////////////////////////////////////////////////////////////////

my_renderer::~my_renderer()
{
	if( false == mbWriteRib )
	{

		auto src = ImageDisplayDriver::removeStoredImage( "myLovelySphere" );
		// for some weird reason we have to copy the imageprimitive to write it out
		ImagePrimitive* cpy = CopyImagePrimitive(src);

		////////////////////////////////

		TIFFImageWriter writer(cpy,mTifFileName.c_str());

		bool bok = writer.canWrite(cpy,mTifFileName.c_str());
		printf( "can_write<%d>\n", int(bok));
		assert(bok);
		writer.write();

		//////////////////////////////

		box2i dims = cpy->getDataWindow();
		v2i dim_min = dims.min;
		v2i dim_max = dims.max;

		//////////////////////////////

		printf( "r<%p> box_min<%d %d> box_max<%d %d>\n", mpRenderer, dim_min.x, dim_min.y, dim_max.x, dim_max.y );
	}

	if( mpDDServer )
		delete mpDDServer;

	delete mpRenderer;
}

///////////////////////////////////////////////////////////////////////////////

V2iData* genv2i( int x, int y )
{
	return new V2iData(v2i(x,y));
}

///////////////////////////////////////////////////////////////////////////////

V3iData* genv3i( int x, int y, int z )
{
	return new V3iData(v3i(x,y,z));
}

///////////////////////////////////////////////////////////////////////////////

void ShaderMap::Apply( IECoreRI::Renderer* r, const std::string& k ) const
{
	std::map<std::string,var_t>::const_iterator it=the_map.find(k);

	if( it!=the_map.end() )
	{
		printf( "Applying StateBlock<%s>\n", k.c_str() );
		const var_t& v = it->second;

		if( v.IsA<CompoundState>() )
		{
			const CompoundState& cs = v.Get<CompoundState>();
			cs.Apply(r);
		}
		else if( v.IsA<Shader>() )
		{
			const Shader& s = v.Get<Shader>();
			s.Apply(r);
		}
		else if( v.IsA<Light>() )
		{
			const Light& l = v.Get<Light>();
			l.Apply(r);
		}
		else if( v.IsA<Attributes>() )
		{
			const Attributes& as = v.Get<Attributes>();
			as.Apply(r);
		}
		else
		{
			assert(false);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void CompoundState::Apply( IECoreRI::Renderer* r ) const
{
	for( const auto& item : mSubStateMap )
	{
		const std::string& k = item.first;
		const var_t& v = item.second;

		printf( "Applying StateBlockItem<%s>\n", k.c_str() );

		if( v.IsA<CompoundState>() )
		{
			const CompoundState& cs = v.Get<CompoundState>();
			cs.Apply(r);
		}
		else if( v.IsA<Shader>() )
		{
			const Shader& s = v.Get<Shader>();
			s.Apply(r);
		}
		else if( v.IsA<Light>() )
		{
			const Light& l = v.Get<Light>();
			l.Apply(r);
		}
		else if( v.IsA<Attributes>() )
		{
			const Attributes& as = v.Get<Attributes>();
			as.Apply(r);
		}
		else
		{
			assert(false);
		}

	}
}

/////////////////////////////////////////////////////////

var_t& StateGen::operator[](const str_t&key)
{
	return mVars[key];
}

/////////////////////////////////////////////////////////

Shader::Shader(const std::string&tgt,const std::string&nam) 
	: mShaderTarget(tgt)
	, mShaderId(nam)
{}

void Shader::Apply( IECoreRI::Renderer* r ) const
{
	mVars.shader(r,mShaderTarget.c_str(),mShaderId.c_str());
}

/////////////////////////////////////////////////////////

Light::Light(const std::string&typ,const std::string&nam) 
	: mLightType(typ)
	, mLightId(nam)
{}

void Light::Apply( IECoreRI::Renderer* r ) const
{
	r->clear_typehint_map();
	r->set_typehint("from","point");
	r->set_typehint("to","point");

	mVars.light(r,mLightType.c_str(),mLightId.c_str());
	r->clear_typehint_map();
}

/////////////////////////////////////////////////////////

Attributes::Attributes(const std::string&id) 
	: mAttribId(id)
{}

/////////////////////////////////////////////////////////

void Attributes::Apply( IECoreRI::Renderer* r ) const
{
	mVars.attribute(r,mAttribId.c_str());				
}

/////////////////////////////////////////////////////////

m44 GenLookAtMatrix( const v3f& eye_pos, const v3f& tgt_pos, const v3f& up_dir )
{
	v3f tgtmineye = (tgt_pos-eye_pos);
	float eye2tgt_dist = tgtmineye.length();

	v3f eye2tgt_dir = tgtmineye.normalized();
	v3f to_dir(0.0f,0.0f,-1.0f);
	m44 cam_rot = Imath::rotationMatrixWithUpDir(eye2tgt_dir,to_dir,up_dir);

	m44 cam_Itrans;
	cam_Itrans.translate(-eye_pos);

	return (cam_Itrans*cam_rot);
}

/////////////////////////////////////////////////////////

MeshPrimitive* Points2MetaBalls(	PointsPrimitive* psys,
						 			float smoothing_radius,
						 			const v3i& march_res,
						 			float march_thresh )
{
	PrimitiveVariableMap& pmap = psys->variables;

	typedef typename IECore::MarchingCubes<> mc_type;
	//typedef typename mc_type::BoxType box_type;
	typedef typename mc_type::ValueBaseType vb_type;
	typedef typename Imath::Vec3<mc_type::PointBaseType> vect_type;
	typedef Imath::Box<vect_type> box_type;

	typedef std::vector<v3f> PointVector;
	typedef TypedData<PointVector> PointVectorData;

	PointVectorData::ConstPtr p = runTimeCast<PointVectorData>(pmap["p"].data);
	DoubleVectorData::ConstPtr r = runTimeCast<DoubleVectorData>(pmap["width"].data);

	auto iso_fn = new ZhuBridsonImplicitSurfaceFunctionV3ff(
						p,
						r,
						smoothing_radius );

	auto bmin = runTimeCast<V3fData>(pmap["boundMin"].data)->readable();
	auto bmax = runTimeCast<V3fData>(pmap["boundMax"].data)->readable();

	box_type bounding_box( bmin, bmax );

	auto builder = new MeshPrimitiveBuilder;
	auto marcher = new IECore::MarchingCubes<>(iso_fn,builder);
	marcher->march(bounding_box,march_res,march_thresh);
	auto mesh = builder->mesh();

	return mesh;

}

/////////////////////////////////////////////////////////

PointsPrimitive* GenPointsPrim( const std::vector<v3f>& v3vect, float fradius )
{
	typedef PrimitiveVariable pvar_t;
	typedef DoubleVectorData dvd_t;
	typedef V3fVectorData v3vfd_t;
	typedef V3fData v3fd_t;
	typedef Box3fVectorData bx3fd_t;
	v3f bbox_min, bbox_max;

	size_t inump = v3vect.size();
	Box3f ext_box;
	std::vector<double> d_vect;
	d_vect.reserve(inump);
	for( const v3f& item : v3vect )
	{	
		ext_box.extendBy(item);
		d_vect.push_back(double(fradius));
	}
	PointsPrimitive* pret = new PointsPrimitive(inump);
	PrimitiveVariableMap& vmap = pret->variables;
	vmap["P"] = pvar_t( pvar_t::Interpolation::Vertex, new v3vfd_t(v3vect) );
	vmap["width"] = pvar_t( pvar_t::Interpolation::Vertex, new dvd_t(d_vect) );
	vmap["boundMin"] = pvar_t( pvar_t::Interpolation::Constant, new v3fd_t(ext_box.min) );
	vmap["boundMax"] = pvar_t( pvar_t::Interpolation::Constant, new v3fd_t(ext_box.max) );

    return pret;
}

PointsPrimitive* GenPointsPrimF( const std::vector<v3f>& v3vect, float fradius )
{
	typedef PrimitiveVariable pvar_t;
	typedef FloatVectorData dvd_t;
	typedef V3fVectorData v3vfd_t;
	typedef V3fData v3fd_t;
	typedef Box3fVectorData bx3fd_t;
	v3f bbox_min, bbox_max;

	size_t inump = v3vect.size();
	Box3f ext_box;
	std::vector<float> d_vect;
	d_vect.reserve(inump);
	for( const v3f& item : v3vect )
	{	
		ext_box.extendBy(item);
		d_vect.push_back(fradius);
	}
	PointsPrimitive* pret = new PointsPrimitive(inump);
	PrimitiveVariableMap& vmap = pret->variables;
	vmap["P"] = pvar_t( pvar_t::Interpolation::Vertex, new v3vfd_t(v3vect) );
	vmap["width"] = pvar_t( pvar_t::Interpolation::Vertex, new dvd_t(d_vect) );
	vmap["boundMin"] = pvar_t( pvar_t::Interpolation::Constant, new v3fd_t(ext_box.min) );
	vmap["boundMax"] = pvar_t( pvar_t::Interpolation::Constant, new v3fd_t(ext_box.max) );

    return pret;
}

/////////////////////////////////////////////////////////
