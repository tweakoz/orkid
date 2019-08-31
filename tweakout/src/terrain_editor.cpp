////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/drawable.h>
#include <ork/math/plane.h>
#include <ork/math/misc_math.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/gfx/camera.h>
//
#include <ork/lev2/gfx/texman.h>
#include <ork/kernel/prop.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/IObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.h>
#include <pkg/ent/editor/editor.h>
#include <ork/dataflow/scheduler.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/heightmap.h>
#include "terrain_editor.h"
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#if 0
INSTANTIATE_TRANSPARENT_RTTI( ork::terrain::heightfield_ed_component, "heightfield_ed_component" );
INSTANTIATE_TRANSPARENT_RTTI( ork::terrain::heightfield_ed_inst, "heightfield_ed_inst" );
INSTANTIATE_TRANSPARENT_RTTI( ork::terrain::HeightFieldEditorArchetype, "HeightFieldArch" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace terrain {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class HeightFieldEditorDrawable : public ent::Drawable
{
	const sheightfield_iface_editor& mhf;

public:
	HeightFieldEditorDrawable(ent::Entity* pent, const sheightfield_iface_editor& hf)
		: ent::Drawable(pent)
		, mhf( hf )
	{
	}

	~HeightFieldEditorDrawable() {} /*virtual*/

private:
	
	void Queue(lev2::Renderer* renderer) const; /*virtual*/
};
///////////////////////////////////////////////////////////////////////////////
void HeightFieldEditorDrawable::Queue(lev2::Renderer* renderer) const /*virtual*/
{
	// this will potentially generate ALOT of renderables
	// depending on heightfield granularity

	/*lev2::CallbackRenderable& renderable = renderer->QueueCallback();
	renderable.SetUserData0((const void*) & mhf );
	renderable.SetTransformNode( & GetDagNode()->GetTransformNode() );
	renderable.SetObject( GetOwner() );
	renderable.SetCallback( sheightfield_iface_editor::Render );*/
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void heightfield_ed_component::Describe()
{
	ork::ent::RegisterFamily<heightfield_ed_component>(ork::AddPooledLiteral("control"));
	/////////////////////////////////////////////////
	/////////////////////////////////////////////////
	reflect::RegisterProperty(	"ErodeEnable",
								& heightfield_ed_component::ErodeEnableGetter,
								&heightfield_ed_component::ErodeEnableSetter );
	reflect::RegisterProperty(	"NumCycles",
								& heightfield_ed_component::NumErosionCyclesGetter,
								&heightfield_ed_component::NumErosionCyclesSetter );
	reflect::RegisterProperty(	"FillBasinsInitial",
								& heightfield_ed_component::FillBasinsInitialGetter,
								&heightfield_ed_component::FillBasinsInitialSetter );
	reflect::RegisterProperty(	"FillBasinsPerCycle",
								& heightfield_ed_component::FillBasinsCycleGetter,
								&heightfield_ed_component::FillBasinsCycleSetter );
	reflect::RegisterProperty(	"SmoothingRate",
								& heightfield_ed_component::SmoothingRateGetter,
								&heightfield_ed_component::SmoothingRateSetter );
	reflect::RegisterProperty(	"ErosionRate",
								& heightfield_ed_component::ErosionRateGetter,
								&heightfield_ed_component::ErosionRateSetter );
	reflect::RegisterProperty(	"SlumpScale",
								& heightfield_ed_component::SlumpScaleGetter,
								&heightfield_ed_component::SlumpScaleSetter );
#if 0
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "ErodeEnable", "editor.range.min", "0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "ErodeEnable", "editor.range.max", "1" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NumCycles", "editor.range.min", "0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NumCycles", "editor.range.max", "1000" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "FillBasinsInitial", "editor.range.min", "0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "FillBasinsInitial", "editor.range.max", "200" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "FillBasinsPerCycle", "editor.range.min", "0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "FillBasinsPerCycle", "editor.range.max", "20" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "ItersPerCycle", "editor.range.min", "1" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "ItersPerCycle", "editor.range.max", "10" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "SmoothingRate", "editor.range.min", "0.1" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "SmoothingRate", "editor.range.max", "100.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "SmoothingRate", "editor.range.log", "true" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "ErosionRate", "editor.range.min", "0.00001" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "ErosionRate", "editor.range.max", "0.2" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "ErosionRate", "editor.range.log", "true" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "SlumpScale", "editor.range.min", "0.1" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "SlumpScale", "editor.range.max", "10.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "SlumpScale", "editor.range.log", "true" );
#endif

	/////////////////////////////////////////////////
	/////////////////////////////////////////////////

	reflect::RegisterProperty(	"Detail",
								& heightfield_ed_component::HeightFieldSizeGetter,
								&heightfield_ed_component::HeightFieldSizeSetter );

	reflect::RegisterProperty(	"NoiseOctaves",
								& heightfield_ed_component::NumOctavesGetter,
								&heightfield_ed_component::NumOctavesSetter );
	
	reflect::RegisterProperty(	"NoiseRepeatBase",
								& heightfield_ed_component::FrqBaseGetter,
								&heightfield_ed_component::FrqBaseSetter );

	reflect::RegisterProperty(	"MapRepeat",
								& heightfield_ed_component::MapFrqGetter,
								&heightfield_ed_component::MapFrqSetter );
	reflect::RegisterProperty(	"MapHeight",
								& heightfield_ed_component::MapAmpGetter,
								&heightfield_ed_component::MapAmpSetter );

	reflect::RegisterProperty(	"NoiseRepeatRamp",
								& heightfield_ed_component::OctaveFrqScaleGetter,
								&heightfield_ed_component::OctaveFrqScaleSetter );

	reflect::RegisterProperty(	"NoiseHeightBase",
								& heightfield_ed_component::AmpBaseGetter,
								&heightfield_ed_component::AmpBaseSetter );

	reflect::RegisterProperty(	"NoiseHeightRamp",
								& heightfield_ed_component::OctaveAmpScaleGetter,
								&heightfield_ed_component::OctaveAmpScaleSetter );

	reflect::RegisterProperty(	"NoiseRotate",
								& heightfield_ed_component::RotateGetter,
								&heightfield_ed_component::RotateSetter );

#if 0
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "Detail", "editor.range.min", "5" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "Detail", "editor.range.max", "11" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseOctaves", "editor.range.min", "1" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseOctaves", "editor.range.max", "11" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseRepeatBase", "editor.range.min", "0.01" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseRepeatBase", "editor.range.max", "10.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseRepeatBase", "editor.range.log", "true" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseRepeatRamp", "editor.range.min", "0.1" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseRepeatRamp", "editor.range.max", "10.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseRepeatRamp", "editor.range.log", "true" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "MapRepeat", "editor.range.min", "0.01" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "MapRepeat", "editor.range.max", "10.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "MapRepeat", "editor.range.log", "true" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "MapHeight", "editor.range.min", "1.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "MapHeight", "editor.range.max", "1000.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "MapHeight", "editor.range.log", "true" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseHeightBase", "editor.range.min", "1.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseHeightBase", "editor.range.max", "1000.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseHeightBase", "editor.range.log", "true" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseHeightRamp", "editor.range.min", "0.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseHeightRamp", "editor.range.max", "2.0" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseRotate", "editor.range.min", "-3.1415" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseRotate", "editor.range.max", "3.1415" );
#endif

	/////////////////////////////////////////////////
	/////////////////////////////////////////////////

	/*reflect::RegisterMapProperty( "GradLo", & heightfield_ed_component::mGradLo );
	reflect::RegisterMapProperty( "GradHi", & heightfield_ed_component::mGradHi );

	reflect::RegisterProperty(	"GradHeightLo",
								& heightfield_ed_component::GradLoGetter,
								& heightfield_ed_component::GradLoSetter );

	reflect::RegisterProperty(	"GradHeightHi",
								& heightfield_ed_component::GradHiGetter,
								& heightfield_ed_component::GradHiSetter );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "GradHeightLo", "editor.range.min", "-300.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "GradHeightLo", "editor.range.max", "300.0" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "GradHeightHi", "editor.range.min", "-300.0" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "GradHeightHi", "editor.range.max", "300.0" );

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "GradLo", "editor.class", "ged.factory.gradient" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "GradHi", "editor.class", "ged.factory.gradient" );

	*/

	/////////////////////////////////////////////////
	/////////////////////////////////////////////////

	reflect::RegisterProperty( "MapTexture", & heightfield_ed_component::GetDepthTexture, & heightfield_ed_component::SetDepthTexture );
	reflect::RegisterProperty( "NoiseTexture", & heightfield_ed_component::GetNoiseTexture, & heightfield_ed_component::SetNoiseTexture );
	reflect::RegisterProperty( "LightEnvTexture", & heightfield_ed_component::GetLightEnvTexture, & heightfield_ed_component::SetLightEnvTexture );
	reflect::RegisterProperty( "ColorTexture", & heightfield_ed_component::GetColorTexture, & heightfield_ed_component::SetColorTexture );
	//ork::reflect::AnnotatePropertyForEditor<ParticleItem>( "Texture", "editor.class", "ged.factory.assetlist" );
	//ork::reflect::AnnotatePropertyForEditor<ParticleItem>( "Texture", "editor.assettype", "lev2tex" );
	//ork::reflect::AnnotatePropertyForEditor<ParticleItem>("Texture", "editor.assetclass", "lev2tex");

#if 0

	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "MapTexture", "editor.class", "ged.factory.assetlist" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "MapTexture", "editor.assettype", "lev2tex" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "MapTexture", "editor.assetclass", "lev2tex");
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseTexture", "editor.class", "ged.factory.assetlist" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseTexture", "editor.assettype", "lev2tex" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "NoiseTexture", "editor.assetclass", "lev2tex");
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "LightEnvTexture", "editor.class", "ged.factory.assetlist" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "LightEnvTexture", "editor.assettype", "lev2tex" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "LightEnvTexture", "editor.assetclass", "lev2tex");
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "ColorTexture", "editor.class", "ged.factory.assetlist" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "ColorTexture", "editor.assettype", "lev2tex" );
	reflect::AnnotatePropertyForEditor<heightfield_ed_component>( "ColorTexture", "editor.assetclass", "lev2tex");
#endif
	
	/////////////////////////////////////////////////
	/////////////////////////////////////////////////

	static const char* EdGrpStr = 
		"grp://GENERATION Detail MapTexture MapRepeat MapHeight NoiseTexture NoiseOctaves NoiseRepeatBase NoiseHeightBase NoiseRepeatRamp NoiseHeightRamp NoiseRotate "
		"grp://EROSION ErodeEnable NumCycles ItersPerCycle FillBasinsInitial FillBasinsPerCycle SmoothingRate SlumpScale ErosionRate "
		"grp://LIGHTING LightEnvTexture "
		"grp://COLOR ColorTexture";

	reflect::AnnotateClassForEditor<heightfield_ed_component>( "editor.prop.groups", EdGrpStr );

	reflect::AnnotateClassForEditor<heightfield_ed_component>( "editor.object.ops", ConstString("nmapout lmapout hmapout cmapout") );

}

heightfield_ed_component::heightfield_ed_component()
	: mhfsize( 5 )
	, mhf( mGradientSet, 32, 1000.0f )
	, mhfif( mhf )
	, mGradientSet()
	, mColorTex( 0 )
{
	mGradientSet.mGradientLo = & mGradLo;
	mGradientSet.mGradientHi = & mGradHi;
}
 
bool heightfield_ed_component::PostDeserialize(reflect::IDeserializer &)
{
	return true;
}

void heightfield_ed_component::Event( const event::Event& Evc )
{
	/*if( Evc == "nmapout" )
	{
		QString FileName = QFileDialog::getSaveFileName( 0, "Output Terrain Normals Image", "", "Targa (*.tga)" );
		std::string fname = FileName.toStdString();
		if( fname.length() )
		{
			const hmap_hfield_module& hfm = mhf.GetTargetModule();
			file::Path outpath( fname.c_str() );
			hfm.SaveNormalsToTexture( outpath );
		}
	}
	else if( Evc == "hmapout" )
	{
		QString FileName = QFileDialog::getSaveFileName( 0, "Output Terrain Height Image", "", "DDS (*.dds)" );
		std::string fname = FileName.toStdString();
		if( fname.length() )
		{
			const hmap_hfield_module& hfm = mhf.GetTargetModule();
			file::Path outpath( fname.c_str() );
			hfm.SaveHeightToTexture( outpath );
		}
	}
	else if( Evc == "lmapout" )
	{
		QString FileName = QFileDialog::getSaveFileName( 0, "Output Terrain Light Image", "", "Targa (*.tga)" );
		std::string fname = FileName.toStdString();
		if( fname.length() )
		{
			const hmap_hfield_module& hfm = mhf.GetTargetModule();
			file::Path outpath( fname.c_str() );
			hfm.SaveLightingToTexture( outpath );
		}
	}
	else if( Evc == "cmapout" )
	{
		QString FileName = QFileDialog::getSaveFileName( 0, "Output Terrain Color Image", "", "Targa (*.tga)" );
		std::string fname = FileName.toStdString();
		if( fname.length() )
		{
			const hmap_hfield_module& hfm = mhf.GetTargetModule();
			file::Path outpath( fname.c_str() );
			hfm.SaveColorsToTexture( outpath );
		}
	}*/
}

ent::ComponentInst* heightfield_ed_component::CreateComponent(ent::Entity* pent) const
{
	return OrkNew heightfield_ed_inst(*this,pent);
}
///////////////////////////////////////////////////////////////////////////////
void heightfield_ed_component::SetNoiseTexture( ork::rtti::ICastable* const & l2tex)
{	
	//lev2::Texture *ptex = 
	//	l2tex ? rtti::safe_downcast<lev2::Texture*>( l2tex ) : 0;

	//mhf.GetPerlinModule().SetNoiseTexture( ptex );

}
void heightfield_ed_component::GetNoiseTexture( ork::rtti::ICastable* & l2tex) const
{
	//l2tex = mhf.GetPerlinModule().GetNoiseTexture();
}
///////////////////////////////////////////////////////////////////////////////
void heightfield_ed_component::SetDepthTexture( ork::rtti::ICastable* const & l2tex)
{	
	/*lev2::Texture *ptex = 
		l2tex ? rtti::safe_downcast<lev2::Texture*>( l2tex ) : 0;

	mhf.GetPerlinModule().SetDepthTexture( ptex );*/

}
void heightfield_ed_component::GetDepthTexture( ork::rtti::ICastable* & l2tex) const
{
	//l2tex = mhf.GetPerlinModule().GetDepthTexture();
}
///////////////////////////////////////////////////////////////////////////////
void heightfield_ed_component::SetLightEnvTexture( ork::rtti::ICastable* const & l2tex)
{	
	/*lev2::Texture *ptex = 
		l2tex ? rtti::safe_downcast<lev2::Texture*>( l2tex ) : 0;

	mhf.GetTargetModule().SetLightEnvTexture( ptex );*/

}
void heightfield_ed_component::GetLightEnvTexture( ork::rtti::ICastable* & l2tex) const
{
	//l2tex = mhf.GetTargetModule().GetLightEnvTexture();
}
///////////////////////////////////////////////////////////////////////////////
void heightfield_ed_component::SetColorTexture( ork::rtti::ICastable* const & l2tex)
{	
	/*lev2::Texture *ptex = 
		l2tex ? rtti::safe_downcast<lev2::Texture*>( l2tex ) : 0;

	mColorTex = ptex;
	mhfif.mColorTexture = ptex;*/
}
void heightfield_ed_component::GetColorTexture( ork::rtti::ICastable* & l2tex) const
{
	//l2tex = mColorTex;
}
///////////////////////////////////////////////////////////////////////////////
// EROSION PARAMS
///////////////////////////////////////////////////////////////////////////////
void heightfield_ed_component::ErodeEnableGetter( int& a ) const
{	a = mhf.GetErodeModule().GetEnable();
}
void heightfield_ed_component::ErodeEnableSetter( const int& a )
{	mhf.GetErodeModule().SetEnable(a);
}
void heightfield_ed_component::NumErosionCyclesGetter(int &a) const
{	a = mhf.GetErodeModule().GetNumErosionCycles();
}
void heightfield_ed_component::NumErosionCyclesSetter(const int &a)
{	mhf.GetErodeModule().SetNumErosionCycles( a );
}
void heightfield_ed_component::FillBasinsInitialGetter(int &a) const
{	a = mhf.GetErodeModule().GetFillBasinsInitial();
}
void heightfield_ed_component::FillBasinsInitialSetter(const int &a)
{	mhf.GetErodeModule().SetFillBasinsInitial( a );
}
void heightfield_ed_component::FillBasinsCycleGetter(int &a) const
{	a = mhf.GetErodeModule().GetFillBasinsCycle();
}
void heightfield_ed_component::FillBasinsCycleSetter(const int &a)
{	mhf.GetErodeModule().SetFillBasinsCycle( a );
}
void heightfield_ed_component::ItersPerCycleGetter(int &a) const
{	a = mhf.GetErodeModule().GetItersPerCycle();
}
void heightfield_ed_component::ItersPerCycleSetter(const int &a)
{	mhf.GetErodeModule().SetItersPerCycle( a );
}
void heightfield_ed_component::SmoothingRateGetter(float &a) const
{	a = mhf.GetErodeModule().GetSmoothingRate();
}
void heightfield_ed_component::SmoothingRateSetter(const float &a)
{	mhf.GetErodeModule().SetSmoothingRate( a );
}
void heightfield_ed_component::ErosionRateGetter(float &a) const
{	a = mhf.GetErodeModule().GetErosionRate();
}
void heightfield_ed_component::ErosionRateSetter(const float &a)
{	mhf.GetErodeModule().SetErosionRate( a );
}
void heightfield_ed_component::SlumpScaleGetter(float &a) const
{	a = mhf.GetErodeModule().GetSlumpScale();
}
void heightfield_ed_component::SlumpScaleSetter(const float &a)
{	mhf.GetErodeModule().SetSlumpScale( a );
}
///////////////////////////////////////////////////////////////////////////////
// PERLIN PARAMS
///////////////////////////////////////////////////////////////////////////////
void heightfield_ed_component::RotateGetter(float &a) const
{	a = mhf.GetPerlinModule().GetRotate();
}
void heightfield_ed_component::RotateSetter(const float &a)
{	mhf.GetPerlinModule().SetRotate( a );
}
void heightfield_ed_component::NumOctavesGetter( int& a ) const
{	a = mhf.GetPerlinModule().GetNumOctaves();
}
void heightfield_ed_component::NumOctavesSetter( const int& a )
{	mhf.GetPerlinModule().SetNumOctaves(a);
}
void heightfield_ed_component::OctaveAmpScaleGetter(float &a) const
{	a = mhf.GetPerlinModule().GetOctaveAmpScale();
}
void heightfield_ed_component::OctaveAmpScaleSetter(const float &a)
{	mhf.GetPerlinModule().SetOctaveAmpScale( a );
}
void heightfield_ed_component::OctaveFrqScaleGetter(float &a) const
{	a = mhf.GetPerlinModule().GetOctaveFrqScale();
}
void heightfield_ed_component::OctaveFrqScaleSetter(const float &a)
{	mhf.GetPerlinModule().SetOctaveFrqScale( a );
}
void heightfield_ed_component::AmpBaseGetter(float &a) const
{	a = mhf.GetPerlinModule().GetAmpBase();
}
void heightfield_ed_component::AmpBaseSetter(const float &a)
{	mhf.GetPerlinModule().SetAmpBase( a );
}
void heightfield_ed_component::MapFrqGetter(float &a) const
{	a = mhf.GetPerlinModule().GetMapFrq();
}
void heightfield_ed_component::MapAmpSetter(const float &a)
{	mhf.GetPerlinModule().SetMapAmp( a );
}
void heightfield_ed_component::MapAmpGetter(float &a) const
{	a = mhf.GetPerlinModule().GetMapAmp();
}
void heightfield_ed_component::MapFrqSetter(const float &a)
{	mhf.GetPerlinModule().SetMapFrq( a );
}
void heightfield_ed_component::FrqBaseGetter(float &a) const
{	
	float fa = mhf.GetPerlinModule().GetFrqBase();
	a = fa;
}
void heightfield_ed_component::FrqBaseSetter(const float &a)
{
	float ws = mhf.GetPerlinModule().HeightMapData().GetWorldSize();
	mhf.GetPerlinModule().SetFrqBase( a );
}
void heightfield_ed_component::HeightFieldSizeGetter(int &a) const
{
	a = mhfsize;

}
void heightfield_ed_component::HeightFieldSizeSetter(const int &a)
{	mhfsize = a;
	mhf.SetSize( 1<<a );
}
void heightfield_ed_component::GradLoGetter(float &a) const
{
	a = mGradientSet.mHeightLo;
}
void heightfield_ed_component::GradLoSetter(const float &a)
{
	mGradientSet.mHeightLo = a;
	mhf.GetTargetModule().HeightOutPlug().SetDirty(true);
}
void heightfield_ed_component::GradHiGetter(float &a) const
{
	a = mGradientSet.mHeightHi;
}
void heightfield_ed_component::GradHiSetter(const float &a)
{
	mGradientSet.mHeightHi = a;
	mhf.GetTargetModule().HeightOutPlug().SetDirty(true);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void HeightFieldEditorArchetype::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
HeightFieldEditorArchetype::HeightFieldEditorArchetype()
{
}
///////////////////////////////////////////////////////////////////////////////
void HeightFieldEditorArchetype::DoLinkEntity(ork::ent::SceneInst* inst, ork::ent::Entity *pent) const // virtual
{
}
void HeightFieldEditorArchetype::DoStartEntity( ent::SceneInst* psi, const ork::CMatrix4& mtx, ent::Entity* pent ) const
{
}
///////////////////////////////////////////////////////////////////////////////
void HeightFieldEditorArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<heightfield_ed_component>();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void heightfield_ed_inst::Describe()
{
}

void heightfield_ed_inst::DoUpdate(ent::SceneInst* sinst)
{

}

heightfield_ed_inst::heightfield_ed_inst( const heightfield_ed_component& hec,ent::Entity *pent  )
	: ork::ent::ComponentInst( &hec, pent )
	, mHEC( hec )
{
	const ent::EntData& edata = pent->GetEntData();

	/*HeightFieldEditorDrawable* pdrw = OrkNew HeightFieldEditorDrawable(pent,hec.GetHfIf());
	pent->AddDrawable( AddPooledLiteral("Default"),pdrw );
	pdrw->SetOwner(  & pent->GetEntData() );	*/
	assert(false);

}
}}
#endif