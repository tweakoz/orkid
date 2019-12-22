////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/opq.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>
#include <ork/lev2/gfx/camera/cameradata.h>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <pkg/ent/LightingSystem.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <orktool/qtui/qtui_tool.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <ork/reflect/enum_serializer.inl>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/filter/gfx/collada/collada.h>
#include <orktool/filter/gfx/meshutil/meshutil_fixedgrid.h>
#include <ork/math/audiomath.h>
#include <ork/kernel/mutex.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/kernel/orkpool.h>
#include <ork/file/chunkfile.h>

#include "SurfaceBaker.h"

#include <ork/lev2/qtui/qtui.hpp>

namespace ork { namespace tool { extern OrkQtApp* gpQtApplication; } }

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

class BakeEvent : public QEvent
{
public:
	static const QEvent::Type gevtype = QEvent::Type(QEvent::User+1);
	BakeEvent() : QEvent( gevtype ), mbSlowConnection(false), mbLocal(false) {}
	std::string			mCommandCompressPyg;
	std::string			mCommandLineCopyThere;
	std::string			mCommandDeCompressPyg;
	std::string			mCommandLineBakeIt;
	std::string			mCommandLineCopyBack;
	bool				mbSlowConnection;
	bool				mbLocal;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class BakeProcessor : public QObject
{

public:

	BakeJobThread*		mJobThread;

	BakeProcessor() : mJobThread(0) {}

private:
	///////////////////////////////////////////////////////////////////////////////
	void WaitForSlowConnectionLock()
	{
		mJobThread->SigAppendString( QString("Aquiring Slow Connection Lock\n") );
		mJobThread->GetBakeContext()->mSlowConnectionLock.LockForWrite();
		mJobThread->SigAppendString( QString("Aquired Slow Connection Lock\n") );
	}
	///////////////////////////////////////////////////////////////////////////////
	void ReleaseSlowConnectionLock()
	{
		mJobThread->GetBakeContext()->mSlowConnectionLock.UnLock();
		mJobThread->SigAppendString( QString("Released Slow Connection Lock\n") );
	}
	///////////////////////////////////////////////////////////////////////////////
	void SmallSleep()
	{
		int irand = std::rand()%500;
		ork::msleep(irand+500);
	}
	///////////////////////////////////////////////////////////////////////////////
	bool BakeLocal(BakeEvent* bev)
	{
		int iret = ProcessCommand( bev->mCommandLineBakeIt );
		mJobThread->DoCleanupJob();
		return (iret==0);
	}
	bool BakeRemote(BakeEvent* bev)
	{	bool bslow = bev->mbSlowConnection;
		int iret = ProcessCommand( bev->mCommandCompressPyg );
		if( iret !=0 ) goto notok;
		SmallSleep();
		////////////////////////////////////////////////
		if( bslow ) WaitForSlowConnectionLock();
		iret = ProcessCommand( bev->mCommandLineCopyThere );
		if( bslow )
		{
			ReleaseSlowConnectionLock();
		}
		SmallSleep();
		if( iret !=0 ) goto notok;
		////////////////////////////////////////////////
		iret = ProcessCommand( bev->mCommandDeCompressPyg );
		if( iret !=0 ) goto notok;
		SmallSleep();
		////////////////////////////////////////////////
		iret = ProcessCommand( bev->mCommandLineBakeIt );
		if( iret !=0 ) goto notok;
		SmallSleep();
		////////////////////////////////////////////////
		if( bslow ) WaitForSlowConnectionLock();
		iret = ProcessCommand( bev->mCommandLineCopyBack );
		if( bslow )
		{
			ReleaseSlowConnectionLock();
		}
		SmallSleep();
		if( iret !=0 ) goto notok;
		////////////////////////////////////////////////
		////////////////////////////////////////////////
		mJobThread->DoCleanupJob();
		return true;
notok:
		mJobThread->DoCleanupJob();
		return false;

	}
	///////////////////////////////////////////////////////////////////////////////
	bool event ( QEvent * e )
	{
		bool rval = false;
		if( e->type() == BakeEvent::gevtype )
		{	BakeEvent* bev = static_cast<BakeEvent*>( e );
			if( bev->mbLocal )	BakeLocal(bev);
			else				BakeRemote(bev);
		}

		return rval;
	}
	///////////////////////////////////////////////////////////////////////////////
	int ProcessCommand( const std::string& cmdstr )
	{	UiFarmNode* fn = mJobThread->GetFarmNode();
		if( fn->mProcessViewer )
		{	auto lamb = [&]()
			{	fn->mProcessViewer->start(cmdstr.c_str());
			};
			mainThreadQueue().enqueueAndWait(Op(lamb));
			return fn->mProcessViewer->wait();
		}
		return -1000;
	}
	///////////////////////////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

BakeJobThread::BakeJobThread(BakeContext*bc,UiFarmNode*fn)
	: bkctx( bc )
	, pfarmnode( fn )
	, pqueuejob( 0 )
	, mProcessor(0)
	, pactivejob(0)
{
	bool bcon = mIdleTimer.connect( & mIdleTimer, SIGNAL(timeout()), this, SLOT(OnTimer()));
	mIdleTimer.setInterval(1000);
	mIdleTimer.setSingleShot(false);
	mIdleTimer.start();
}

///////////////////////////////////////////////////////////////////////////////

void BakeJobThread::OnTimer()
{
	if( pqueuejob )
	{
		if( 0 == pactivejob )
		{
			orkset<FarmJob*>& locked_active_jobs = bkctx->mActiveJobSet.LockForWrite();
			pactivejob = pqueuejob;
			ActivateJob();
			bkctx->mActiveJobSet.UnLock();
		}
	}
	else
	{	// Get me a job
		////////////////////////////////////////////////
		orkset<FarmJob*>& locked_jobs = bkctx->mJobSet.LockForWrite();
		orkset<FarmJob*>& locked_active_jobs = bkctx->mActiveJobSet.LockForWrite();
		////////////////////////////////////////////////
		{	orkset<FarmJob*>::iterator itj = locked_jobs.begin();
			if( itj != locked_jobs.end() )
			{	pqueuejob = *itj;
				locked_jobs.erase( itj );
			}
		}
		////////////////////////////////////////////////
		bkctx->mJobSet.UnLock();
		bkctx->mActiveJobSet.UnLock();
		////////////////////////////////////////////////
		if( pqueuejob )
		{	orkset<FarmJob*>& locked_active_jobs = bkctx->mActiveJobSet.LockForWrite();
			{	orkset<FarmJob*>::iterator itj = locked_active_jobs.begin();
				locked_active_jobs.insert( pqueuejob );
			}
			bkctx->mActiveJobSet.UnLock();
		}
		////////////////////////////////////////////////
	}
}

///////////////////////////////////////////////////////////////////////////////

void BakeJobThread::SigAppendString( QString str )
{
	//Moc.Emit( this, "Sig1(QString)", str );
}

///////////////////////////////////////////////////////////////////////////////

void BakeJobThread::ActivateJob()
{
	const ork::file::Path & LitFilePath = bkctx->mLitPath;

	const ork::file::Path::NameType & WorkingFolder = ork::file::GetStartupDirectory();
	const ork::file::Path WorkingFolderPath = ork::file::Path( WorkingFolder.c_str() ).ToAbsolute();
	const char* puserfolder = getenv( "USERPROFILE" );
	const std::string id_dsafile = CreateFormattedString( "%s\\.ssh\\id_dsa.ppk", puserfolder );
	const PoolString& hostname = pfarmnode->mFarmNode.GetHostName();
	const int isshport = pfarmnode->mFarmNode.GetSshPort();
	file::Path rgmpath = pactivejob->GetDaeName();
	rgmpath.SetExtension("rgm");
	const PoolString& BakeGroupMatch = pactivejob->GetBakeGroupMatch();
	const std::string machinename = hostname.c_str();
	bool blocal = (strcmp("localhost",hostname.c_str())==0)&&(isshport==0);
	const file::Path src_rgmfilename = rgmpath.ToAbsolute();
	file::Path dst_rgmfilename( rgmpath.GetName().c_str() );
	dst_rgmfilename.SetExtension( rgmpath.GetExtension().c_str() );

	ork::file::Path dst_imgpathA = pactivejob->GetDaeName().ToAbsoluteFolder(ork::file::Path::EPATHTYPE_POSIX);
	ork::file::Path dst_imgpathB = pactivejob->GetDaeName().ToAbsoluteFolder(ork::file::Path::EPATHTYPE_POSIX);
	dst_imgpathA.AppendFolder( "../lightmaps/" );
	dst_imgpathB.AppendFolder( "../../lightmaps/" );
	ork::file::Path dst_imagepath =
			FileEnv::DoesDirectoryExist(dst_imgpathA)
		?	dst_imgpathA
		:	dst_imgpathB;

	if( false == FileEnv::DoesDirectoryExist(dst_imagepath) )
	{
		std::string  str = CreateFormattedString("ERROR lightmaps Folder Does Not Exist<%s>\n",dst_imagepath.c_str());
		SigAppendString( QString(str.c_str()) );
		return;
	}

	mCurrentRgmFile = dst_rgmfilename.c_str();

	//////////////////////////////////////////////
	// do the work
	//////////////////////////////////////////////
	std::string pscpcmd = CreateFormattedString( "%sext\\miniork\\ext\\bin\\pscp.exe ", WorkingFolderPath.c_str() );
	std::string plnkcmd = CreateFormattedString( "%sext\\miniork\\ext\\bin\\plink.exe ", WorkingFolderPath.c_str() );
	//////////////////////////////////////////////
	std::string fullcmd_compre = CreateFormattedString(
		"%sext\\miniork\\ext\\bin\\bzip2.exe -k -f %s",
		WorkingFolderPath.c_str(),
		dst_rgmfilename.c_str()
		);
	//////////////////////////////////////////////
	std::string src_bz2filename = std::string(src_rgmfilename.c_str()) + std::string(".bz2");
	std::string dst_bz2filename = std::string(dst_rgmfilename.c_str()) + std::string(".bz2");
	//////////////////////////////////////////////
	std::string copythere_options =
		CreateFormattedString(
			"-v -P %d -i %s %s michael@%s:%s",
			isshport,
			id_dsafile.c_str(),
			src_bz2filename.c_str(),
			machinename.c_str(),
			dst_bz2filename.c_str()
		);
	std::string fullcmd_ct = pscpcmd + copythere_options;
	//////////////////////////////////////////////
	std::string remotedecomp_options =
		CreateFormattedString(
			"-v -ssh -C -P %d -i %s michael@%s bzip2 -d -f %s",
			isshport,
			id_dsafile.c_str(),
			machinename.c_str(),
			dst_bz2filename.c_str()
		);
	std::string fullcmd_decomp = plnkcmd + remotedecomp_options;
	//////////////////////////////////////////////
	std::string remotegelato_options =
		CreateFormattedString(
			"-v -ssh -C -P %d -i %s michael@%s gelato %s",
			isshport,
			id_dsafile.c_str(),
			machinename.c_str(),
			dst_rgmfilename.c_str()
		);
	std::string fullcmd_gel = plnkcmd + remotegelato_options;
	//////////////////////////////////////////////
	std::string copyback_options =
		CreateFormattedString(
			"-v -C -P %d -i %s michael@%s:%s %s",
			isshport,
			id_dsafile.c_str(),
			machinename.c_str(),
			dst_imagepath.c_str(),
			dst_imagepath.c_str()
		);
	std::string fullcmd_cb = pscpcmd + copyback_options;
	//////////////////////////////////////////////
	if( blocal )
	{
		fullcmd_gel =
		CreateFormattedString(
			"ext\\miniork\\ext\\bin\\lmapper.exe -in %s -litin %s -outdir %s",
			src_rgmfilename.c_str(),
			LitFilePath.ToAbsolute().c_str(),
			dst_imagepath.c_str()
		);
	}
	//////////////////////////////////////////////
	if( pfarmnode )
	{
		std::string  str = CreateFormattedString("Starting Job<%s>\n",mCurrentRgmFile.c_str());
		SigAppendString( QString(str.c_str()) );
		str = CreateFormattedString("%s<%d:%s>\n",hostname.c_str(), pfarmnode->miNumJobsProcessed, mCurrentRgmFile.c_str());
		pfarmnode->mProcessViewer->gfxdock->setWindowTitle( QString(str.c_str()) );
	}

	if( mProcessor )
	{
		mProcessor->mJobThread = this;

		BakeEvent* be = new BakeEvent;
		be->mbSlowConnection = pfarmnode->mFarmNode.IsExclusiveConnection();
		be->mCommandCompressPyg = fullcmd_compre;
		be->mCommandLineCopyThere = fullcmd_ct;
		be->mCommandDeCompressPyg = fullcmd_decomp;
		be->mCommandLineBakeIt = fullcmd_gel;
		be->mCommandLineCopyBack = fullcmd_cb;
		be->mbLocal = blocal;

		QCoreApplication::postEvent( mProcessor, be );
	}

}

///////////////////////////////////////////////////////////////////////////////

void BakeJobThread::DoCleanupJob( )
{	//////////////////////////////////////////////
	// clean up the job
	//////////////////////////////////////////////
	bool bjobcleaned = false;
	{	orkset<FarmJob*>& locked_active_jobs = bkctx->mActiveJobSet.LockForWrite();
		////////////////////////////////////////////////
		{	orkset<FarmJob*>::iterator itj = locked_active_jobs.find(pactivejob);
			locked_active_jobs.erase( itj );
			bjobcleaned = true;
		}
		pactivejob = 0;
		pqueuejob = 0;
		////////////////////////////////////////////////
		bkctx->mActiveJobSet.UnLock();
	}
	////////////////////////////////////////////////
	if( pfarmnode )
	{
		pfarmnode->miNumJobsProcessed++;

		if( bjobcleaned )
		{
			std::string  str = CreateFormattedString("Cleaned Job<%s>\n",mCurrentRgmFile.c_str());
			SigAppendString( QString(str.c_str()) );
		}
		else
		{
			std::string  str = CreateFormattedString("ERROR Cleaning Job<%s>\n",mCurrentRgmFile.c_str());
			SigAppendString( QString(str.c_str()) );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void BakeJobThread::run()
{
	BakeProcessor psor;
	mProcessor = & psor;
	this->exec();
	orkprintf( "exiting thread!\n" );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void WriteLightsFile( const MeshUtil::LightContainer& lights, const file::Path& outpath );
void CollectLights( MeshUtil::LightContainer& lc, const SceneData* psd, const std::string& match );

bool PerformBake( BakeOps* pOPS, const BakerSettings* psetting,const FarmNodeGroup*pfarmgroup )
{
	const SceneData* psd = pOPS->mpARCH->GetSceneData();
	MeshUtil::LightContainer lights;
	CollectLights( lights, psd, "" );

	///////////////////////////////////////////
	// write out lights
	///////////////////////////////////////////

	const file::Path& daepath = psetting->GetDaeInput();

	ork::file::Path lit_out_path = daepath;
	lit_out_path.AppendFile("_lit");
	lit_out_path.SetExtension("lit");
	WriteLightsFile( lights, lit_out_path );

	///////////////////////////////////////////

	BakeContext* bkctx = new BakeContext;
	bkctx->mLitPath = lit_out_path ;

	///////////////////////////////////////////
	// enumerate jobs, place into MT-Safe set
	///////////////////////////////////////////
	const FarmJobSet* jobset = psetting->GetCurrentJobSet();
	if(jobset)
	{	//////////////////////////////////////////
		orkset<FarmJob*>& locked_jobs = bkctx->mJobSet.LockForWrite();
		//////////////////////////////////////////
		{	const orklut<PoolString,FarmJob*>& jobs = jobset->GetJobs();
			for( orklut<PoolString,FarmJob*>::const_iterator
				it=jobs.begin();
				it!=jobs.end();
				it++ )
			{
				locked_jobs.insert( new FarmJob(*it->second) );
			}
		}
		//////////////////////////////////////////
		bkctx->mJobSet.UnLock();
		//////////////////////////////////////////
	}
	///////////////////////////////////////////
	// enumerate render farm nodes, place into MT-Safe Pool
	// create a thread for each farm node
	///////////////////////////////////////////
	const orklut<PoolString,FarmNode*>& Nodes = pfarmgroup->GetNodes();
	FarmNodePool& fnpool = bkctx->mLockedFarmNodePool.LockForWrite();
	{	fnpool.mPool.reset_size((int)Nodes.size());
		int idx = 0;
		for( orklut<PoolString,FarmNode*>::const_iterator
				it=Nodes.begin();
				it!=Nodes.end();
				it++ )
		{
			fnpool.mPool.direct_access(idx).mFarmNode = *(it->second);
			fnpool.mPool.direct_access(idx).mProcessViewer =
				new ork::tool::ProcessView( it->first.c_str(), ork::tool::gpQtApplication->mpMainWindow );

			BakeJobThread* bjt = new BakeJobThread( bkctx, & fnpool.mPool.direct_access(idx) );

			bool bcon = bjt->connect(
				bjt,  SIGNAL(SigAppendString(QString)),
				fnpool.mPool.direct_access(idx).mProcessViewer, SLOT(AppendString(QString))
				);

			bjt->start();

			idx++;
		}
	}
	bkctx->mLockedFarmNodePool.UnLock();
	///////////////////////////////////////////
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void CollectLights( MeshUtil::LightContainer& lc, const SceneData* psd, const std::string& match )
{
	const tokenlist match_toklist = CreateTokenList( match.c_str(), " " );
	bool bmatchAll = match_toklist.size()==0;

	const orkmap<PoolString, SceneObject*>& scene_objects = psd->GetSceneObjects();
	for( orkmap<PoolString, SceneObject*>::const_iterator itso=scene_objects.begin(); itso!=scene_objects.end(); itso++ )
	{	const SceneObject* pso = itso->second;
		const EntData* pentd = rtti::autocast(pso);
		if( pentd )
		{	std::string LightName = itso->first.c_str();

			bool bmatchitem = bmatchAll;

			////////////////////////////////////////////////
			for(	tokenlist::const_iterator
					itm=match_toklist.begin();
					itm!=match_toklist.end();
					itm++ )
			{	const std::string& str = (*itm);
				size_t itfind = LightName.find(str);
				if( itfind != std::string::npos )
				{
					bmatchitem = true;
				}
			}
			////////////////////////////////////////////////

			if( bmatchitem )
			{
				fmtx4 MtxWorld;
				pentd->GetDagNode().GetTransformNode().GetMatrix(MtxWorld);
				const LightingComponentData* plightdatacomp = pentd->GetTypedComponent<LightingComponentData>();
				if( plightdatacomp )
				{	if( false == plightdatacomp->IsDynamic() )
					{	const ork::lev2::LightData* plightdata = plightdatacomp->GetLightData();
						const ork::lev2::DirectionalLightData* pdld = rtti::autocast(plightdata);
						const ork::lev2::PointLightData* ppld = rtti::autocast(plightdata);
						const ork::lev2::SpotLightData* psld = rtti::autocast(plightdata);
						const ork::lev2::AmbientLightData* pald = rtti::autocast(plightdata);
						fvec3 LightColor = plightdata->GetColor();
						MeshUtil::Light* pgl = 0;
						if( pdld )
						{	ork::lev2::DirectionalLight dlight( MtxWorld, pdld );
							ork::fvec3 LightDir = dlight.GetDirection();
							MeshUtil::DirLight* pgdl = new MeshUtil::DirLight;
							pgdl->mWorldMatrix = MtxWorld;
							pgdl->mFrom = dlight.GetWorldPosition();
							pgdl->mTo = pgdl->mFrom+(LightDir*1.0f);
							pgdl->mShadowBias = pdld->GetShadowBias();
							pgl = pgdl;
						}
						if( ppld )
						{	ork::lev2::PointLight plight( MtxWorld, ppld );
							ork::fvec3 LightDir = plight.GetDirection();
							MeshUtil::PointLight* pgpl = new MeshUtil::PointLight;
							pgpl->mWorldMatrix = MtxWorld;
							pgpl->mPoint = plight.GetWorldPosition();
							pgpl->mFalloff = ppld->GetFalloff();
							pgpl->mRadius = ppld->GetRadius();
							pgpl->mShadowBias = ppld->GetShadowBias();
							pgl = pgpl;
						}
						if( pgl )
						{
							pgl->mColor = LightColor;
							pgl->mIntensity = 1.0f;
							pgl->mbIsShadowCaster = plightdata->IsShadowCaster();
							pgl->mShadowSamples = plightdata->GetShadowSamples();
							pgl->mShadowBlur = plightdata->GetShadowBlur();
							pgl->mbSpecular = plightdata->GetSpecular();
							pgl->mShadowBias = plightdata->GetShadowBias();
							lc.mLights.AddSorted( itso->first, pgl );
						}
					}
				}
			}
		}
	}
}

void WriteLightsFile( const MeshUtil::LightContainer& lights, const file::Path& outpath )
{
	chunkfile::Writer chunkwriter( "lit" );
	chunkfile::OutputStream* HeaderStream = chunkwriter.AddStream("header");

	int inumlights = (int) lights.mLights.size();
	HeaderStream->AddItem(inumlights);
	for( orklut<PoolString,MeshUtil::Light*>::const_iterator itl=lights.mLights.begin(); itl!=lights.mLights.end(); itl++ )
	{
		const PoolString& name = itl->first;
		const MeshUtil::Light* plight = itl->second;
		int iname = chunkwriter.stringIndex(name.c_str());
		HeaderStream->AddItem(iname);
		HeaderStream->AddItem(plight->mWorldMatrix);
		HeaderStream->AddItem(plight->mColor);
		HeaderStream->AddItem(plight->mIntensity);
		int icastsshadows = int(plight->mbIsShadowCaster);
		HeaderStream->AddItem(icastsshadows);

		const MeshUtil::PointLight* pplight = rtti::autocast(plight);
		if( pplight )
		{
			int itype = chunkwriter.stringIndex("PointLight");
			HeaderStream->AddItem(itype);
			HeaderStream->AddItem(pplight->mPoint);
			HeaderStream->AddItem(pplight->mFalloff);
			HeaderStream->AddItem(pplight->mRadius);
		}
		else
		{
			int itype = chunkwriter.stringIndex("Other");
			HeaderStream->AddItem(itype);
		}
	}
	chunkwriter.WriteToFile( outpath );
}
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
