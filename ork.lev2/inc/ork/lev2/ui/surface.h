#pragma once

#include <ork/lev2/ui/widget.h>

namespace ork { namespace ui {

////////////////////////////////////////////////////////////////////
// surface : optionally image backed group
//  can redraw without repainting when clean!
////////////////////////////////////////////////////////////////////

struct Surface : public Group
{
public:

	Surface( const std::string & name, int x, int y, int w, int h, CColor3 color, F32 depth );

	void SurfaceRender(lev2::RenderContextFrameData&fd, const std::function<void()>& l);
	void Clear();

	CColor3 &GetClearColorRef( void ) { return mcClearColor; }
	F32 GetClearDepth( void ) { return mfClearDepth; }

	void PushFrameTechnique( lev2::FrameTechniqueBase* ftek );
	void PopFrameTechnique();
	lev2::FrameTechniqueBase* GetFrameTechnique() const;

	void BeginSurface(lev2::FrameRenderer&frenderer);
	void EndSurface(lev2::FrameRenderer&frenderer);

	void GetPixel( int ix, int iy, lev2::GetPixelContext& ctx );

	void RePaintSurface(DrawEvent& drwev);

	void MarkSurfaceDirty() { mNeedsSurfaceRepaint=true; SetDirty(); }

protected:

	void RenderCached();
	void OnResize( void ) override;
	virtual void DoSurfaceResize() {}

	orkstack<lev2::FrameTechniqueBase*>	mpActiveFrameTek;
	bool								mbClear;
	CColor3								mcClearColor;
	F32									mfClearDepth;
	lev2::RtGroup*						mRtGroup;
	bool 								mNeedsSurfaceRepaint;
	lev2::PickBufferBase*				mpPickBuffer;

	void DoDraw(DrawEvent& drwev);
	virtual void DoRePaintSurface(DrawEvent& drwev) {}

};

}} // namespace ork { namespace ui {

