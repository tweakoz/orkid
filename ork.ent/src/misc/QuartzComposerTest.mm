////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if defined(ORK_OSX)
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/event/MeshEvent.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <Foundation/NSString.h>
#include <Foundation/NSGeometry.h>
#include <ApplicationServices/ApplicationServices.h>
///////////////////////////////////////////////////////////////////////////////
#include "QuartzComposerTest.h"
#include <ork/util/crc64.h>
#include <ork/kernel/string/string.h>
#include <ork/util/stl_ext.h>
#include "../../ork/lev2/gfx/gl/gl.h"
///////////////////////////////////////////////////////////////////////////////
#include <Quartz/Quartz.h>
///////////////////////////////////////////////////////////////////////////////
namespace  ork { namespace lev2 {
extern CGLContextObj gOGLdefaultctx;
extern CGLPixelFormatObj gpixfmt;
extern ork::Objc::Object* gNSGLdefaultctx;
extern ork::Objc::Object* gNSGLfirstctx;
extern ork::Objc::Object* gNSPixelFormat;
extern std::vector<ork::Objc::Object*> gWindowContexts;

//extern ork::Objc::Object* gNSGLcurrentctx;
}}

ork::Objc::Object* MainCtx() { return ork::lev2::gWindowContexts[1]; }

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::QuartzComposerArchetype, "QuartzComposerArchetype" );
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::QuartzComposerData, "QuartzComposerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::QuartzComposerInst, "QuartzComposerInst");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
void QuartzComposerArchetype::Describe()
{
}
QuartzComposerArchetype::QuartzComposerArchetype()
{
}

void QuartzComposerArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<EditorPropMapData>();
	composer.Register<ork::ent::QuartzComposerData>();
	//pedpropmapdata->SetProperty( "visual.lighting.reciever.scope", "static" );
}

///////////////////////////////////////////////////////////////////////////////

void QuartzComposerData::Describe()
{
	ork::ent::RegisterFamily<QuartzComposerData>(ork::AddPooledLiteral("control"));

	ork::reflect::RegisterProperty("Composition", &QuartzComposerData::mCompositionPath);
	ork::reflect::annotatePropertyForEditor<QuartzComposerData>("Composition", "editor.class", "ged.factory.filelist");
	ork::reflect::annotatePropertyForEditor<QuartzComposerData>("Composition", "editor.filetype", "qtz");

	ork::reflect::RegisterProperty("StartTime", &QuartzComposerData::mfStartTime);
	ork::reflect::RegisterProperty("EndTime", &QuartzComposerData::mfEndTime);
	ork::reflect::RegisterProperty("FPS", &QuartzComposerData::miFPS);

	reflect::annotatePropertyForEditor<QuartzComposerData>( "StartTime", "editor.range.min", "0.0" );
	reflect::annotatePropertyForEditor<QuartzComposerData>( "StartTime", "editor.range.max", "30.0" );

	reflect::annotatePropertyForEditor<QuartzComposerData>( "EndTime", "editor.range.min", "0.0" );
	reflect::annotatePropertyForEditor<QuartzComposerData>( "EndTime", "editor.range.max", "30.0" );

	reflect::annotatePropertyForEditor<QuartzComposerData>( "FPS", "editor.range.min", "1.0" );
	reflect::annotatePropertyForEditor<QuartzComposerData>( "FPS", "editor.range.max", "60.0" );
}
QuartzComposerData::QuartzComposerData()
	: mfStartTime(0.0f)
	, mfEndTime(1.0f)
	, miFPS(30)
{
}
///////////////////////////////////////////////////////////////////////////////

ComponentInst *QuartzComposerData::createComponent(Entity *pent) const
{
	ComponentInst* pinst = OrkNew QuartzComposerInst( *this, pent );
	return pinst;
}
void QuartzComposerInst::Describe()
{

}

QuartzComposerInst::QuartzComposerInst(const QuartzComposerData &data, Entity *pent)
	: ComponentInst( & data, pent )
	, mCD(data)
	, miW(512)
	, miH(512)
	, mFBO(0)
	, mDBO(0)
	, mpQuartzComposerDrawable(0)
	, mfUpdateTime(0.0f)
{
	const file::Path& pth = mCD.GetCompositionPath();

	NSOpenGLContext* poglctx = MainCtx()->mInstance;
	[poglctx makeCurrentContext];

	////////////////////////////////
	// compute set of frametimes
	////////////////////////////////
	int iFPS = mCD.GetFPS();
	float fstart = mCD.GetStartTime();
	float fend = mCD.GetEndTime();
	float fstep = 1.0f/float(iFPS);
	////////////////////////////////
	QCRenderer* pqcren = 0;
	file::Path abspath = pth.ToAbsolute();
	bool bQTZPRESENT = FileEnv::GetRef().DoesFileExist( abspath );
	if( bQTZPRESENT )
	{	printf( "found qtz file<%s> \n", abspath.c_str() );
		NSString* PathToComp = [NSString stringWithUTF8String:abspath.c_str()];
		////////////////////////////////
		QCComposition* pcomp = [QCComposition compositionWithFile:PathToComp];
		////////////////////////////////
		NSSize size;
		size.width = float(miW);
		size.height = float(miH);
		CGColorSpaceRef csref = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
		pqcren = [	[QCRenderer alloc]
					initWithOpenGLContext:MainCtx()->mInstance
					pixelFormat:lev2::gNSPixelFormat->mInstance
					file:PathToComp];

		OrkAssert( pqcren!=nil );

		mQCRenderer = pqcren;
		mQCRenderer.Dump();
	}

	//////////////////////////////////////////
	// gen RTT texture
	//////////////////////////////////////////

	glGenTextures(1, &mTEX);
	glBindTexture(GL_TEXTURE_2D, mTEX);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, miW, miH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	printf( "TEX<%d>\n", int(mTEX) );

	//////////////////////////////////////////
	// gen FBO and DBO
	//////////////////////////////////////////

	glGenFramebuffersEXT( 1, &mFBO );
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mFBO);
	glGenRenderbuffersEXT(1, &mDBO);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, mDBO);

	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, miW, miH );
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, mDBO);

	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, mTEX, 0);

	printf( "FBO<%d>\n", int(mFBO) );
	printf( "DBO<%d>\n", int(mDBO) );

	//////////////////////////////////////////
	// check for FBO completeness
	//////////////////////////////////////////

	GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

	OrkAssert( status == GL_FRAMEBUFFER_COMPLETE_EXT );

	UpdateFBO();

}
QuartzComposerInst::~QuartzComposerInst()
{
	glDeleteTextures( 1, &mTEX );
	glDeleteFramebuffersEXT( 1, &mFBO );
	glDeleteRenderbuffersEXT( 1, &mDBO );

	QCRenderer* prenderer = (QCRenderer*) mQCRenderer.mInstance;
	[prenderer release];
}

void DrawTexQuad(float kmin, float kmax, float depth )
{
	glBegin( GL_TRIANGLES );
		glTexCoord2f( 0.0f, 0.0f );
		glVertex3f( kmin, kmin, depth );
		glTexCoord2f( 1.0f, 0.0f );
		glVertex3f( kmax, kmin, depth );
		glTexCoord2f( 1.0f, 1.0f );
		glVertex3f( kmax, kmax, depth );

		glTexCoord2f( 0.0f, 0.0f );
		glVertex3f( kmin, kmin, depth );
		glTexCoord2f( 1.0f, 1.0f );
		glVertex3f( kmax, kmax, depth );
		glTexCoord2f( 0.0f, 1.0f );
		glVertex3f( kmin, kmax, depth );
	glEnd();
}
void DrawCross(float depth )
{
	float kmin = -0.9f;
	float kmax = +0.9f;

	glBegin( GL_LINES );
		glVertex3f( kmin, kmin, depth );
		glVertex3f( kmax, kmax, depth );

		glVertex3f( kmin, kmax, depth );
		glVertex3f( kmax, kmin, depth );
	glEnd();
}
void PushIdentity()
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
}
void PopIdentity()
{
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void QuartzComposerInst::UpdateFBO()
{
	QCRenderer* prenderer = (QCRenderer*) mQCRenderer.mInstance;
	NSOpenGLContext* poglctx = MainCtx()->mInstance;

	GLenum buffers[] =	{	GL_COLOR_ATTACHMENT0_EXT,
						};

	//printf( "BindFBO<%d> numattachments<%d>\n", int(FboObj->mFBOMaster), inumtargets );

	[poglctx makeCurrentContext];
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, mFBO );
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, mDBO );
	glDrawBuffers( 1, buffers );
	{
		PushIdentity();
		glPushAttrib( GL_ALL_ATTRIB_BITS );
		glViewport( 0,0, 512, 512 );
		glScissor( 0,0, 512, 512 );
		{
			glClearColor( 1.0f,0.0f,0.0f,1.0f);
			glClearDepth( 1.0f );
			glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
			glColor3f( 1.0f,1.0f, 0.0f );
			glDisable(GL_LIGHTING);
			glDisable(GL_ALPHA_TEST);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_BLEND);
			//DrawCross( 0.5f );
			BOOL bOK = [prenderer renderAtTime:double(mfUpdateTime) arguments:nil];
			OrkAssert( bOK );
			//[prenderer description];
			[poglctx flushBuffer];
		}
		PopIdentity();
		glPopAttrib();

	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
	glDrawBuffer( GL_BACK );

}

struct QuartzComposerDrawable
{
	Entity *pent;
	QuartzComposerInst*	mpQCI;

	static void doit( lev2::RenderContextInstData& rcid, lev2::Context* targ, const lev2::CallbackRenderable* pren )
	{
		if( targ->FBI()->isPickState() ) return;

		const QuartzComposerDrawable* pyo = pren->GetUserData0().Get<const QuartzComposerDrawable*>();
		QuartzComposerInst* pQCI = pyo->mpQCI;

		ork::Objc::Object& QCR = pQCI->mQCRenderer;

		int iframe = 0;

		QCRenderer* prenderer = (QCRenderer*) QCR.mInstance;

		//printf( "iframe<%d> texobj<%d> qcr<%p>\n", iframe, pQCI->mTEX, prenderer );
		//fflush( stdout );

		////////////////////////////////////////////////////////////////

		OrkAssert( prenderer != 0 );

		//pQCI->UpdateFBO();

		////////////////////////////////////////////////////////////////

		PushIdentity();
		{
			glBindBuffer( GL_ARRAY_BUFFER_ARB, 0 );
			glDisableClientState( GL_VERTEX_ARRAY );
			glDisableClientState( GL_NORMAL_ARRAY );
			glDisableClientState( GL_COLOR_ARRAY );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE1);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE0);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glDisable(GL_LIGHTING);
			glDisable(GL_ALPHA_TEST);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_BLEND);

			glColor3f( 1.0f, 1.0f, 1.0f );
			glClientActiveTextureARB(GL_TEXTURE0);
			glEnable( GL_TEXTURE_2D );
			glBindTexture(GL_TEXTURE_2D, pQCI->mTEX);
			DrawTexQuad( -0.5f, +0.5f, 0.999f );
			glBindTexture(GL_TEXTURE_2D, 0);
			glFlush();

		}
		PopIdentity();
	}
	static void BufferCB(ork::ent::DrawableBufItem&cdb)
	{

	}
};
bool QuartzComposerInst::DoLink(ork::ent::Simulation *psi)
{
	Entity* pent = GetEntity();
	CallbackDrawable* pdrw = new CallbackDrawable(pent);
	pent->addDrawableToDefaultLayer(pdrw);
	pdrw->SetCallback( QuartzComposerDrawable::doit );
	pdrw->SetBufferCallback( QuartzComposerDrawable::BufferCB );
	pdrw->SetOwner(  & pent->GetEntData() );
	pdrw->SetSortKey(0);

	QuartzComposerDrawable* pdrawObj = new QuartzComposerDrawable;
	pdrawObj->pent = pent;
	pdrawObj->mpQCI = this;
	mpQuartzComposerDrawable = pdrawObj;

	anyp ap;
	ap.Set<const QuartzComposerDrawable*>( pdrawObj );
	pdrw->SetData( ap );

	return true;
}
void QuartzComposerInst::DoUpdate( ork::ent::Simulation* psi )
{
	float fdeltaT = psi->GetDeltaTime();
	mfUpdateTime += fdeltaT;

	UpdateFBO();

	miFrame++;
}
bool QuartzComposerInst::DoNotify(const ork::event::Event *event)
{
	return true;
}
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
#endif
