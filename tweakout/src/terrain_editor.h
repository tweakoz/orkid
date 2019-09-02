////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if 0

#include <orktool/qtui/qtui_tool.h>
#include "qtui_scenevp.h"
#include "qtvp_uievh.h"
#include "terrain_synth.h"
#include <ork/event/Event.h>
#include <ork/lev2/ui/event.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace terrain {
///////////////////////////////////////////////////////////////////////////////

class heightfield_ed_component : public ent::ComponentData
{
	RttiDeclareConcrete( heightfield_ed_component, ent::ComponentData );
	sheightfield_iface_editor	mhfif;
	TerrainSynth				mhf;
	int							mhfsize;
	orkmap<float,fvec4>		mGradLo;
	orkmap<float,fvec4>		mGradHi;

	ent::GradientSet			mGradientSet;

	lev2::Texture*				mColorTex;

	/*virtual*/ void Event( const event::Event& Evc );

	////////////////////////////////////////////

	void ErodeEnableGetter(int &a) const;
	void ErodeEnableSetter(const int &a);

	void NumErosionCyclesGetter(int &a) const;
	void NumErosionCyclesSetter(const int &a);

	void FillBasinsInitialGetter(int &a) const;
	void FillBasinsInitialSetter(const int &a);

	void FillBasinsCycleGetter(int &a) const;
	void FillBasinsCycleSetter(const int &a);

	void ItersPerCycleGetter(int &a) const;
	void ItersPerCycleSetter(const int &a);

	void SmoothingRateGetter(float &a) const;
	void SmoothingRateSetter(const float &a);

	void ErosionRateGetter(float &a) const;
	void ErosionRateSetter(const float &a);

	void SlumpScaleGetter(float &s) const;
	void SlumpScaleSetter(const float &s);

	////////////////////////////////////////////

	void NumOctavesGetter(int &a) const;
	void NumOctavesSetter(const int &a);

	void OctaveAmpScaleGetter(float &a) const;
	void OctaveAmpScaleSetter(const float &a);

	void OctaveFrqScaleGetter(float &a) const;
	void OctaveFrqScaleSetter(const float &a);

	void AmpBaseGetter(float &a) const;
	void AmpBaseSetter(const float &a);

	void FrqBaseGetter(float &a) const;
	void FrqBaseSetter(const float &a);

	void HeightFieldSizeGetter(int &a) const;
	void HeightFieldSizeSetter(const int &a);

	void RotateGetter(float &a) const;
	void RotateSetter(const float &a);

	////////////////////////////////////////////

	void MapFrqGetter(float &a) const;
	void MapFrqSetter(const float &a);
	void MapAmpGetter(float &a) const;
	void MapAmpSetter(const float &a);

	////////////////////////////////////////////

	void GradLoGetter(float &a) const;
	void GradLoSetter(const float &a);
	void GradHiGetter(float &a) const;
	void GradHiSetter(const float &a);

	////////////////////////////////////////////

public:

	void SetColorTex( lev2::Texture* ptex )
	{
		//mColorTex=ptex;
		mhfif.mColorTexture = ptex;
		mhf.GetTargetModule().SetColorMapTexture( ptex );
	}
	lev2::Texture* GetColorTex() const { return mColorTex; }

	void SetNoiseTexture( ork::rtti::ICastable* const & l2tex);
	void GetNoiseTexture( ork::rtti::ICastable* & l2tex) const;
	void SetDepthTexture( ork::rtti::ICastable* const & l2tex);
	void GetDepthTexture( ork::rtti::ICastable* & l2tex) const;
	void SetLightEnvTexture( ork::rtti::ICastable* const & l2tex);
	void GetLightEnvTexture( ork::rtti::ICastable* & l2tex) const;
	void SetColorTexture( ork::rtti::ICastable* const & l2tex);
	void GetColorTexture( ork::rtti::ICastable* & l2tex) const;

	heightfield_ed_component();

	virtual bool PostDeserialize(reflect::IDeserializer &);

	virtual ent::ComponentInst* createComponent(ent::Entity* pent) const;

	sheightfield_iface_editor& GetHfIf() { return mhfif; }
	const sheightfield_iface_editor& GetHfIf() const { return mhfif; }
	const TerrainSynth& GetTerrainSynth() const { return mhf; }
	TerrainSynth& GetTerrainSynth() { return mhf; }
};

class heightfield_ed_inst : public ent::ComponentInst
{
	RttiDeclareAbstract( heightfield_ed_inst, ent::ComponentInst );
	virtual void DoUpdate(ent::SceneInst* sinst);

	const heightfield_ed_component& mHEC;

public:

	heightfield_ed_inst( const heightfield_ed_component& hec, ent::Entity* pent );

	const heightfield_ed_component& GetHEC() const { return mHEC; }
};
struct TerBrushEvent
{
	const lev2::GetPixelContext&	mCtx;
	const fvec3&					mPos;
	const ui::Event*				mUiEv;
	ent::GradientSet				mGradientSet;
	heightfield_ed_component*		mHEC;
	TerrainSynth&					mHF;
	float							mfHFX, mfHFZ;

	TerBrushEvent(	const ui::Event* pev,
					const lev2::GetPixelContext& ctx,
					const fvec3& pos,
					heightfield_ed_component* hec
				);
};

///////////////////////////////////////////////////////////////////////////////

class GpuBrushBuffer : public lev2::GfxBuffer
{
	virtual void DoDraw( const TerBrushEvent& muev ) {}
	void Draw( const TerBrushEvent& muev );

public:

	static const int kx = 0;
	static const int ky = 0;
	static const int kw = 4096;
	static const int kh = 4096;

	GpuBrushBuffer()
		: lev2::GfxBuffer(
			0,
			kx, ky,	kw, kh,
			lev2::EBUFFMT_RGBA32,
			lev2::ETGTTYPE_EXTBUFFER,
			"GpuBrushBuffer" )
	{
		CreateContext();
	}

	virtual void OnEvent( const TerBrushEvent& muev ) { Draw(muev); }

};

///////////////////////////////////////////////////////////////////////////////

class ZBrushTool : public ork::Object
{
	RttiDeclareAbstract(ZBrushTool,ork::Object);

public:

	float									mBrushRadius;
	lev2::Texture*							mEditTexture;

	ZBrushTool();

	bool UIEventHandler( const TerBrushEvent& ev );
	virtual void OnEvent( const TerBrushEvent& ev ) = 0;

};
///////////////////////////////////////////////////////////////////////////////
class RaiseTool : public ZBrushTool
{
	RttiDeclareAbstract(RaiseTool,ZBrushTool);
	void OnEvent( const TerBrushEvent& ev );
};
///////////////////////////////////////////////////////////////////////////////
class LowerTool : public ZBrushTool
{
	RttiDeclareAbstract(LowerTool,ZBrushTool);
	void OnEvent( const TerBrushEvent& ev );
};
///////////////////////////////////////////////////////////////////////////////
class SmoothTool : public ZBrushTool
{
	RttiDeclareAbstract(SmoothTool,ZBrushTool);
	int										mSmoothAmount;
	void OnEvent( const TerBrushEvent& ev );
public:
	SmoothTool();
};
///////////////////////////////////////////////////////////////////////////////
class GradPaintTool : public ZBrushTool
{
	RttiDeclareAbstract(GradPaintTool,ZBrushTool);
	float									mHeightLerp;
	orkmap<float,fvec4>					mGradientLo;
	orkmap<float,fvec4>					mGradientHi;
	void OnEvent( const TerBrushEvent& ev );
public:
	GradPaintTool();
};
///////////////////////////////////////////////////////////////////////////////
class FlowTool : public ZBrushTool
{
	RttiDeclareAbstract(FlowTool,ZBrushTool);
	void OnEvent( const TerBrushEvent& ev );
};
///////////////////////////////////////////////////////////////////////////////
class TexPaint2DTool : public ZBrushTool
{
	RttiDeclareAbstract(TexPaint2DTool,ZBrushTool);
	void OnEvent( const TerBrushEvent& ev );
};
///////////////////////////////////////////////////////////////////////////////
class TexPaint3DTool : public ZBrushTool
{
	float				mRepeat;
	lev2::Texture*		mBrushTexture;
	float				mAlphaBase;
	int					miFilter;

	RttiDeclareAbstract(TexPaint3DTool,ZBrushTool);
	void OnEvent( const TerBrushEvent& ev );
	void SetBrushTexture( ork::rtti::ICastable* const & l2tex);
	void GetBrushTexture( ork::rtti::ICastable* & l2tex) const;
public:
	bool GetFilter() const { return (miFilter>0); }
	float GetAlphaBase() const { return mAlphaBase; }
	float GetRepeat() const { return mRepeat; }
	TexPaint3DTool() : mRepeat(1.0f), mBrushTexture(0), miFilter(0), mAlphaBase(0.5f) {}
	lev2::Texture*	GetBrushTexture() const { return mBrushTexture; }
};
///////////////////////////////////////////////////////////////////////////////
class NoisePaint3DTool : public ZBrushTool
{
	float				mRepeat;
	lev2::Texture*		mNoise1Texture;
	lev2::Texture*		mNoise2Texture;
	lev2::Texture*		mColor1Texture;
	lev2::Texture*		mColor2Texture;
	float				mAlphaBase;
	float				mNoiseAmount;
	float				mMinSlope;
	float				mMaxSlope;
	float				mMinAltitude;
	float				mMaxAltitude;
	float				mFilter;

	RttiDeclareAbstract(NoisePaint3DTool,ZBrushTool);
	void OnEvent( const TerBrushEvent& ev );

	void SetNoise1Texture( ork::rtti::ICastable* const & l2tex);
	void GetNoise1Texture( ork::rtti::ICastable* & l2tex) const;
	void SetNoise2Texture( ork::rtti::ICastable* const & l2tex);
	void GetNoise2Texture( ork::rtti::ICastable* & l2tex) const;

	void SetColor1Texture( ork::rtti::ICastable* const & l2tex);
	void GetColor1Texture( ork::rtti::ICastable* & l2tex) const;
	void SetColor2Texture( ork::rtti::ICastable* const & l2tex);
	void GetColor2Texture( ork::rtti::ICastable* & l2tex) const;

public:
	float GetFilter() const { return mFilter; }
	float GetAlphaBase() const { return mAlphaBase; }
	float GetRepeat() const { return mRepeat; }
	NoisePaint3DTool()
		: mRepeat(1.0f)
		, mNoise1Texture(0)
		, mNoise2Texture(0)
		, mColor1Texture(0)
		, mColor2Texture(0)
		, mFilter(0.0f)
		, mAlphaBase(0.5f)
		, mNoiseAmount( 0.0f )
		, mMinSlope(0.0f)
		, mMaxSlope(1.0f)
		, mMinAltitude(0.0f)
		, mMaxAltitude(1.0f)
	{
	}

	float GetMinAltitude() const { return mMinAltitude; }
	float GetMaxAltitude() const { return mMaxAltitude; }
	float GetMinSlope() const { return mMinSlope; }
	float GetMaxSlope() const { return mMaxSlope; }
	float GetNoiseAmount() const { return mNoiseAmount; }
	lev2::Texture*	GetNoise1Texture() const { return mNoise1Texture; }
	lev2::Texture*	GetNoise2Texture() const { return mNoise2Texture; }
	lev2::Texture*	GetColor1Texture() const { return mColor1Texture; }
	lev2::Texture*	GetColor2Texture() const { return mColor2Texture; }
};
///////////////////////////////////////////////////////////////////////////////

class ZBrushHandler : public ent::SceneEditorVPToolHandler
{
	RttiDeclareAbstract(ZBrushHandler,ent::SceneEditorVPToolHandler);

	orkmap<std::string,ZBrushTool*>	mTools;
	ZBrushTool*						mCurrentTool;
	heightfield_ed_component*		mHEC;

	ork::Object* GetCurrentTool();

	/*virtual*/ void DoAttach(ork::ent::SceneEditorVP*);
	/*virtual*/ void DoDetach(ork::ent::SceneEditorVP*);
	/*virtual*/ void DoDrawViewport( lev2::GfxTarget* pTARG );

	/*virtual*/ void Event( const ork::event::Event& Evc );

public:
	virtual bool UIEventHandler( ork::ui::Event *pEV );

	ZBrushHandler(ork::ent::SceneEditorBase& editor);
};

///////////////////////////////////////////////////////////////////////////////

class HeightFieldEditorArchetype : public ent::Archetype
{
	RttiDeclareConcrete( HeightFieldEditorArchetype, ent::Archetype );

	//void DoComposeEntity( ent::Entity *pent ) const;	//virtual
	void DoLinkEntity(ork::ent::SceneInst* inst, ork::ent::Entity *pent) const; // virtual
	void DoCompose(ork::ent::ArchComposer& composer);									//virtual
	void DoStartEntity( ent::SceneInst* psi, const ork::fmtx4& mtx, ent::Entity* pent ) const;	// virtual

public:
	HeightFieldEditorArchetype();
};

///////////////////////////////////////////////////////////////////////////////

}}
#endif
