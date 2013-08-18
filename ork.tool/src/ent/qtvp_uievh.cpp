////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/opq.h>

#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/input/input.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/kernel/timer.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>

#include <orktool/toolcore/dataflow.h>

#include <pkg/ent/editor/qtui_scenevp.h>
#include <pkg/ent/editor/qtvp_uievh.h>
#include <pkg/ent/editor/edmainwin.h>
#include <QtGui/qclipboard.h>
#include <QtCore/QSettings>
#include <pkg/ent/scene.h>
#include <ork/lev2/gfx/camera/cameraman.h>

#include <orktool/qtui/uitoolhandler.hpp>

///////////////////////////////////////////////////////////////////////////////

using namespace ork::lev2;

namespace ork { namespace ent {

SceneEditorVPToolHandler::SceneEditorVPToolHandler( SceneEditorBase& editor )
	: mEditor( editor )
{
}

void OuterPickOp( DeferredPickOperationContext* pickctx );

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::BindToolHandler( SceneEditorVPToolHandler* handler )
{
	if( mpCurrentHandler )
	{
		mpCurrentHandler->Detach(this);
	}
	mpCurrentHandler = handler;
	mpCurrentHandler->Attach(this);
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::BindToolHandler( const std::string& toolname )
{
	OrkAssert( mToolHandlers.find( toolname ) != mToolHandlers.end() );
	BindToolHandler( (*mToolHandlers.find( toolname )).second );
}

///////////////////////////////////////////////////////////////////////////

void SceneEditorVP::RegisterToolHandler( const std::string& toolname, SceneEditorVPToolHandler* handler )
{
	OrkAssert( mToolHandlers.find( toolname ) == mToolHandlers.end() );
	mToolHandlers[ toolname ] = handler;
	handler->SetToolName(toolname);
}

///////////////////////////////////////////////////////////////////////////

ui::HandlerResult SceneEditorVP::DoOnUiEvent( const ui::Event& EV )
{
	ui::HandlerResult ret;
	//OrkAssert( 0 != mpCurrentHandler );

	lev2::CTXBASE * CtxBase = GetTarget() ? GetTarget()->GetCtxBase() : 0;

	ork::ent::SceneInst* psceneinst = mEditor.GetActiveSceneInst();

	////////////////////////////////////////////////////////////////
	// ALT KEY IS ALWAYS CAMERA OPS
	////////////////////////////////////////////////////////////////

	bool isshift = EV.mbSHIFT;
	bool isalt	 = EV.mbALT;
	bool isctrl	 = EV.mbCTRL;
    bool ismeta  = EV.mbMETA;
    bool ismulti = false;
//	isshift = CSystem::IsKeyDepressed(VK_SHIFT);
//	isalt = CSystem::IsKeyDepressed(VK_LMENU);
//	isctrl = CSystem::IsKeyDepressed(VK_CONTROL);

	switch( EV.miEventCode )
	{
		case ui::UIEV_GOT_KEYFOCUS:
		{
			//bool brunning = psceneinst ? psceneinst->GetSceneInstMode()==ork::ent::ESCENEMODE_RUN : false;
			//CtxBase->SetRefreshPolicy( brunning ? lev2::CTXBASE::EREFRESH_FASTEST : lev2::CTXBASE::EREFRESH_WHENDIRTY );
			break;
		}
		case ui::UIEV_LOST_KEYFOCUS:
		{
			//CtxBase->SetRefreshPolicy( lev2::CTXBASE::EREFRESH_WHENDIRTY);
			break;
		}
        case ui::UIEV_MULTITOUCH:
            ismulti=true;
            break;
	}
	if( isalt || ismulti )
	{
		if( mActiveCamera ) mActiveCamera->UIEventHandler( EV );
		ret.SetHandled(this);
		return ret;
	}

	if( 0 == EV.miEventCode ) return ret;

	////////////////////////////////////////////////////////////////

	switch( EV.miEventCode )
	{
		case ui::UIEV_KEYUP:
		{
			break;
		}
		case ui::UIEV_KEY:
		{
			int icode = EV.miKeyCode;

			if( icode == 't' )
			{
				if( isctrl ) mEditor.EditorPlaceEntity();
			}
			else if( icode == 'k' )
			{
				if(isshift)
				{
					// move editor camera to selected locator
					CMatrix4 matrix;
					if(mEditor.EditorGetEntityLocation(matrix) && mActiveCamera )
					{
						mActiveCamera->SetFromWorldSpaceMatrix(matrix);
					}
				}
			}
			else if( icode == 'l' )
			{
				if(isshift)
				{
					// move selected locator to editor camera
					//CMatrix4 matrix = mpActiveCamera->GetVMatrix();
					//matrix.Inverse();
					//mEditor.EditorLocateEntity(matrix);
				}
			}
			else if( icode == 'e' )
			{
				if( isctrl ) mEditor.EditorNewEntity();
			}
			else if( icode == 'r' )
			{
				if( isctrl ) mEditor.EditorReplicateEntity();
			}
			else if( icode == 'q' )
			{
				if( isctrl )
				{
					tool::GetGlobalDataFlowScheduler()->GraphSet().LockForWrite().clear();
					mMainWindow.SlotUpdateAll();
					tool::GetGlobalDataFlowScheduler()->GraphSet().UnLock();
				}
			}
			else if( icode == Qt::Key_Pause )
			{
				/*switch( mEditor.mpSceneInst->GetSceneInstMode() )
				{
					case ent::ESCENEMODE_RUN:
						mEditor.mpSceneInst->SetSceneInstMode( ent::ESCENEMODE_EDIT );
						break;
					default:
						mEditor.mpSceneInst->SetSceneInstMode( ent::ESCENEMODE_RUN );
						break;
				}*/
			}
			else if( icode == 'g' )
			{
				if( isctrl ) mGridMode = (mGridMode+1)%3;
			}
			else if( icode == '~' )
			{
				if( miCullCameraIndex>=0 )
					miCullCameraIndex=-1;
				else
					miCullCameraIndex=miCameraIndex;
				printf( "CULLCAMERAINDEX<%d>\n", miCullCameraIndex );
				ret.SetHandled(this);
			}
			else if( icode == '`' )
			{
				miCameraIndex++;
				printf( "CAMERAINDEX<%d>\n", miCameraIndex );
				ret.SetHandled(this);
			}
			else if( icode == '1' )
			{
				mCompositorSceneIndex++;
				ret.SetHandled(this);
			}
			else if( icode == '2' )
			{
				mCompositorSceneItemIndex++;
				ret.SetHandled(this);
			}
			else if( icode == '!' )
			{
				mCompositorSceneIndex=-1;
				ret.SetHandled(this);
			}
			else if( icode == '@' )
			{
				mCompositorSceneItemIndex=-1;
				ret.SetHandled(this);
			}
			break;
		}
		case ui::UIEV_PUSH:
		{
			printf( "scenevp::uiev_push\n" );
			int ix = EV.miX;
			int iy = EV.miY;

			int ity = 4;
			int itx = 4;
			for( orkmap<std::string,SceneEditorVPToolHandler*>::const_iterator it=mToolHandlers.begin(); it!=mToolHandlers.end(); it++ )
			{
				int iX1 = itx;
				int iY1 = ity;
				int iX2 = itx+32;
				int iY2 = ity+32;

				bool bInWidget = ( (ix >= iX1) && (ix < iX2) && (iy >= iY1) && (iy < iY2) );

				if( bInWidget )
				{
					auto th = (*it).second;

					printf( "bInWidget<%d>, th<%p>\n", int(bInWidget), th );
					BindToolHandler( th );
					ret.SetHandled( th );
				}
				ity += 36;
			}
			break;
		}
	}

	if( mpCurrentHandler )
	{
		if( ! ret.WasHandled() )
		{
			//printf( "CurrentHandler<%s>\n", mpCurrentHandler->GetToolName().c_str() );
			ret = mpCurrentHandler->OnUiEvent( EV );
		}
	}

	return ret;
}

///////////////////////////////////////////////////////////////////////////

static CMatrix4 mtx_spawn;
void SceneEditorVPToolHandler::SetSpawnLoc(const lev2::GetPixelContext& ctx, float fx, float fy )
{
	const CVector4& TestNrmD = ctx.mPickColors[1];
	
	/*const CCameraData& camdat = GetViewport()->GetCamera()->mCameraData;
	CVector3 vdir, vori;
	camdat.ProjectDepthRay( CVector2( fx, fy ), vdir, vori );
	float l = TestNrmD.GetZ();
	float il = 1.0-l;
	
	//float fd = (TestNrmD.GetZ()-camdat.GetNear() / camdat.GetFar());
	float fd = (il*camdat.GetNear())+(l*camdat.GetFar());
	CVector3 SpawnYNormal( TestNrmD.GetX(), TestNrmD.GetY(), TestNrmD.GetW() );
	SpawnYNormal.Normalize();
	CVector3 SpawnCursor = TestNrmD.GetXYZ(); //vori+(vdir*fd);
	GetViewport()->GetCamera()->CamFocus = SpawnCursor;
	orkprintf( "vdir <%f,%f,%f>\n", vdir.GetX(), vdir.GetY(), vdir.GetZ() );
	orkprintf( "spawncursor <%f,%f,%f> fd<%f>\n", SpawnCursor.GetX(), SpawnCursor.GetY(), SpawnCursor.GetZ(), fd );
	ork::CVector3 SpawnXNormal = SpawnYNormal.Cross( vdir.Normal() ).Normal();
	ork::CVector3 SpawnZNormal = SpawnXNormal.Cross( SpawnYNormal ).Normal();
	mtx_spawn.NormalVectorsIn( SpawnXNormal, SpawnYNormal, SpawnZNormal );
	mtx_spawn.SetTranslation( SpawnCursor );
	mEditor.SetSpawnMatrix( mtx_spawn );
	GetViewport()->GetCamera()->CamFocusYNormal = SpawnYNormal;
	*/
}

///////////////////////////////////////////////////////////////////////////

} }
