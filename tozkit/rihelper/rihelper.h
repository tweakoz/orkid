#include <stdio.h>
#include <IECore/IECore.h>
#include <IECore/AttributeState.h>
#include <IECoreRI/Renderer.h>
#include <IECoreRI/RIBWriter.h>
#include "IECore/DisplayDriverServer.h"
#include "IECore/ImageDisplayDriver.h"
#include "IECore/TIFFImageWriter.h"
#include "IECore/SimpleTypedData.h"
#include "IECore/VectorTypedData.h"
#include "IECore/PointsPrimitive.h"
#include "IECore/MeshPrimitive.h"
#include "IECore/MeshPrimitiveBuilder.h"
#include <OpenEXR/ImathMatrix.h>
#include <OpenEXR/ImathMatrixAlgo.h>

#include "variant.h"

using namespace IECore;
using namespace IECoreRI;

typedef Imath::Matrix44<float> m44;
typedef Imath::V3f v3f;
typedef Imath::V3i v3i;
typedef Imath::V2f v2f;
typedef Imath::V2i v2i;
typedef Imath::Color3f c3f;
typedef Imath::Color4f c4f;
typedef Imath::Box2i box2i;

typedef variant256 var_t;
typedef std::string str_t;
typedef const char* c_str_t;
typedef std::vector<float> float_v_t;


/////////////////////////////////////////////////////////

inline ImagePrimitive* CopyImagePrimitive( const ImagePrimitive* src )
{
	ImagePrimitive* dst = new ImagePrimitive( src->getDataWindow(), src->getDisplayWindow() );

	std::vector<std::string> channels;
	src->channelNames (channels); 

	for( const auto it : channels )
	{
		printf( "channel<%s>\n", it.c_str() );
		auto src_channeldata = src->getChannel<float>(it.c_str());
		auto dst_channeldata = dst->createChannel<float>(it.c_str());
		dst_channeldata->writable() = src_channeldata->readable();
	}	

	return dst;
}

/////////////////////////////////////////////////////////

struct var_map
{
	var_map(){}

	var_t& operator[](const str_t&key);
	void clear();
	void FillCompoundDataMap(CompoundDataMap&cmap) const;
	CompoundDataMap GetCompoundDataMap() const;
	IECore::CompoundData* GenCompoundData() const;
	void display(IECoreRI::Renderer* r, const char*a,const char*b,const char*c) const;
	void attribute(IECoreRI::Renderer* r, const char*a) const;
	void light(IECoreRI::Renderer* r, const char*typ,const char*nam) const;
	void shader(IECoreRI::Renderer* r, const char*typ,const char*nam) const;
	void camera(IECoreRI::Renderer* r, const char*nam) const;

	void options(IECoreRI::Renderer* r,const char*base_nam) const;

	std::map<str_t,var_t> the_map;

};

/*struct prim_var_map
{
	prim_var_map(){}

	var_t& operator[](const str_t&key);
	const PrimitiveVariableMap& Get
	PrimitiveVariableMap the_map;
};*/

/////////////////////////////////////////////////////////

m44 GenLookAtMatrix( const v3f& eye_pos, const v3f& tgt_pos, const v3f& up_dir );

/////////////////////////////////////////////////////////

V2iData* genv2i( int x, int y );
V3iData* genv3i( int x, int y, int z );

/////////////////////////////////////////////////////////

struct my_renderer
{
	my_renderer( const std::string& base_name, bool bwrite_rib );
	~my_renderer();

	IECoreRI::Renderer* rib() { return mpRenderer; }

	IECoreRI::Renderer* mpRenderer;
	DisplayDriverServer* mpDDServer;
	std::string mTifFileName;
	std::string mRibFileName;

	bool mbWriteRib;

};//

struct ShaderMap
{
	var_t& operator[](const std::string&key){return the_map[key];}
	std::map<std::string,var_t> the_map;

	void Apply( IECoreRI::Renderer* r, const std::string& k ) const;

};

/////////////////////////////////////////////////////////

struct CompoundState
{
	var_t& operator[](const std::string&key){return mSubStateMap[key];}

	std::map<std::string,var_t> mSubStateMap;

	void Apply( IECoreRI::Renderer* r ) const;

};

/////////////////////////////////////////////////////////

struct StateGen
{
	var_t& operator[](const str_t&key);

	var_map     mVars;
};

/////////////////////////////////////////////////////////

struct Shader : public StateGen
{
	Shader(const std::string&tgt="yo",const std::string&nam="xxx");
	void Apply( IECoreRI::Renderer* r ) const;

	std::string mShaderId;
	std::string mShaderTarget;
};

/////////////////////////////////////////////////////////

struct Light : public StateGen
{
	Light(const std::string&typ="yo",const std::string&nam="xxx");
	void Apply( IECoreRI::Renderer* r ) const;

	std::string mLightId;
	std::string mLightType;
};

/////////////////////////////////////////////////////////

struct Attributes : public StateGen
{
	Attributes(const std::string&id="xxx");
	void Apply( IECoreRI::Renderer* r ) const;

	std::string mAttribId;
};

/////////////////////////////////////////////////////////

PointsPrimitive* GenPointsPrim( const std::vector<v3f>& v3vect, float fradius );
PointsPrimitive* GenPointsPrimF( const std::vector<v3f>& v3vect, float fradius );

MeshPrimitive* Points2MetaBalls(	PointsPrimitive* psys,
						 			float smoothing_radius,
						 			const v3i& march_res,
						 			float march_thresh );
