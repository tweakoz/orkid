////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.hpp>
#include <pkg/ent/bullet.h>
#include <pkg/ent/bullet_sector.h>
#include <pkg/ent/RacingLineData.h>
#include <btBulletDynamicsCommon.h>

#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/entity.hpp>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/kernel/orklut.hpp>

#define WRITE_DEBUG_TXT 0

#if WRITE_DEBUG_TXT && defined(WIN32)
#define SECTOR_DEBUG_OUT(format, ...) {if (dbgOut) fprintf(dbgOut, (format), ##__VA_ARGS__);}
#else
#define SECTOR_DEBUG_OUT(...) ((void)0)
#endif

#define SECTOR_DEBUG_SPAM	(0)

static const char* sVersionChunkName		= "sec_version";
static const char* sSectorChunkName			= "sectors";
static const char* sBoundsIndicesChunkName	= "bounds_index";
static const char* sBoundsVertsChunkName	= "bounds_verts";
static const char* sBoundsBVHChunkName		= "bounds_bvh";
static const char* sTrackIndicesChunkName	= "track_index";
static const char* sTrackVertsChunkName		= "track_verts";
static const char* sTrackFlagsChunkName		= "track_flags";
static const char* sTrackBVHChunkName		= "track_bvh";

//NOTE: Any time you change the format enough that it wouldn't be able to be read by an older version
//of this code, bump this number up by 1.
static const int sSecVersion				= 13  ;

namespace ork {
namespace wii {
void* MEM2Alloc( int isize );
}}

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::bullet::SectorRange, "SectorRange");

template class ork::orklut<ork::PoolString, ork::ent::bullet::SectorRange *>;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent { namespace bullet {
///////////////////////////////////////////////////////////////////////////////

struct CollisionLoadAllocator
{
	void* alloc(const char* chunkName, int len)
	{
		void* pmem = 0;
#if defined(WII)
		pmem = ork::wii::MEM2Alloc(len);
#else
		pmem = new unsigned char[len];
#endif

		return pmem;
	}
	void done(const char* chunkName, void* data)
	{
#if defined(WII)
#else
		char* pchdata = (char*) data;
		delete[] pchdata;
#endif
	}
};

typedef ork::chunkfile::Reader<CollisionLoadAllocator> CollisonReader;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct TriangleIndexVertexBuffer
{
public:

	///////////////////////////////////////////////////////////////////////////////

	TriangleIndexVertexBuffer(ork::chunkfile::InputStream* indexStream, ork::chunkfile::InputStream* vertexStream)
		: mLoaded(false)
		, mNumIndices(0)
		, mNumVerts(0)
		, mNumTris(0)
		, mVerts(0)
		, mFlags(0)
	{
		if (!indexStream || !vertexStream)
			return;

		//Read the number of objects from the streams
		indexStream->GetItem(mNumIndices);
		vertexStream->GetItem(mNumVerts);

		//Create storage for the indices and verts
		mIndices = new int[mNumIndices];
		mVerts = new fvec3[mNumVerts];

		if (!mVerts || !mIndices)
			return;

		//Read the mIndices from the stream
		for (int i = 0; i < mNumIndices; ++i)
			indexStream->GetItem(mIndices[i]);

		//Read the mVerts from the stream
		for (int i = 0; i < mNumVerts; ++i)
			vertexStream->GetItem(mVerts[i]);

		mLoaded = true;
	}

	///////////////////////////////////////////////////////////////////////////////

	TriangleIndexVertexBuffer(const orkvector<int>& indices, const orkvector<fvec3>& verts)
		: mLoaded(false)
		, mNumIndices(0)
		, mNumVerts(0)
		, mNumTris(0)
		, mVerts(0)
		, mIndices(0)
		, mFlags(0)
	{
		mNumVerts = verts.size();
		mNumIndices = indices.size();
		mNumTris = mNumIndices/3;

		mVerts = new fvec3[mNumVerts];
		mIndices = new int[mNumIndices];
		mFlags = new int[mNumTris];

		if (!mVerts || !mIndices)
			return;

		for (int i = 0; i < int(indices.size()); ++i)
			mIndices[i] = indices[i];

		for (int i = 0; i < int(verts.size()); ++i)
			mVerts[i] = verts[i];

		mLoaded = true;
	}

	///////////////////////////////////////////////////////////////////////////////

	~TriangleIndexVertexBuffer()
	{
		if( mVerts ) delete[] mVerts;
		if( mIndices ) delete[] mIndices;
		if( mFlags ) delete[] mFlags;
	}

	///////////////////////////////////////////////////////////////////////////////

	bool Save(ork::chunkfile::OutputStream* indexStream, ork::chunkfile::OutputStream* vertexStream) const
	{
		if (!indexStream || !vertexStream)
			return false;

		//Write the mIndices to a stream in the file
		indexStream->AddItem(mNumIndices);
		for (int i = 0; i < mNumIndices; ++i)
			indexStream->AddItem(mIndices[i]);

		//Write the mVerts to a stream in the file
		vertexStream->AddItem(mNumVerts);
		for (int i = 0; i < mNumVerts; ++i)
			vertexStream->AddItem(mVerts[i]);

		return true;
	}

	///////////////////////////////////////////////////////////////////////////////

	btTriangleIndexVertexArray* ToBullet() const
	{
		//Create a mesh description object
		btIndexedMesh mesh;
		mesh.m_numTriangles = mNumIndices / 3;;
		mesh.m_triangleIndexBase = (const unsigned char*)mIndices;
		mesh.m_triangleIndexStride = 3 * sizeof(int);
		mesh.m_indexType = PHY_INTEGER;
		mesh.m_numVertices = mNumVerts;
		mesh.m_vertexBase = (const unsigned char*)mVerts;
		mesh.m_vertexStride = sizeof(fvec3);

		//Create a new bullet triangle index/vertex array and add the mesh to it
		btTriangleIndexVertexArray* triangles = new btTriangleIndexVertexArray;
		triangles->addIndexedMesh(mesh);

		return triangles;
	}

	///////////////////////////////////////////////////////////////////////////////

	bool Loaded() const { return mLoaded; }

	///////////////////////////////////////////////////////////////////////////////

	void Transform(const ork::fmtx4& transform)
	{
		for (int vertIdx = 0;  vertIdx < mNumVerts; ++vertIdx)
			mVerts[vertIdx] = mVerts[vertIdx].Transform(transform).xyz();
	}

	///////////////////////////////////////////////////////////////////////////////

private:
	bool mLoaded;
	fvec3* mVerts;
	int* mIndices;
	int* mFlags;
	int mNumVerts;
	int mNumIndices;
	int mNumTris;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static bool SaveMeshOptimizations(btTriangleIndexVertexArray* triArray, ork::chunkfile::OutputStream* bvhStream)
{
	if (!triArray || !bvhStream)
		return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////////

static btBvhTriangleMeshShape* LoadMeshOptimizations(btTriangleIndexVertexArray* triArray, ork::chunkfile::InputStream* bvhStream)
{
	if (!triArray || !bvhStream)
		return NULL;

	//Calculate an aabb and create an optimized triangle mesh shape
	btVector3 aabbMin, aabbMax;
	triArray->calculateAabbBruteForce(aabbMin, aabbMax);
	btBvhTriangleMeshShape* trimeshShape = new btBvhTriangleMeshShape(triArray
		, /*useQuantizedAabbCompression=*/true, aabbMin, aabbMax);

	return trimeshShape;
}

///////////////////////////////////////////////////////////////////////////////

static bool SaveSectors(chunkfile::OutputStream* sectorStream, const SectorData &saveData, FILE* dbgOut)
{
	if (!sectorStream)
		return false;

	sectorStream->AddItem((int)saveData.mSectorVerts.size());
	SECTOR_DEBUG_OUT("NumSectorVerts<%d>\n", saveData.mSectorVerts.size());
	//Write the sectors to the file
	for (int vertIdx = 0; vertIdx < (int)saveData.mSectorVerts.size(); vertIdx++)
	{
		sectorStream->AddItem(saveData.mSectorVerts[vertIdx].mPos);

		if (dbgOut)
		{
			SECTOR_DEBUG_OUT("SectorVert %d : %.1f, %.1f, %.1f\n",
				vertIdx,
				saveData.mSectorVerts[vertIdx].mPos.GetX(),
				saveData.mSectorVerts[vertIdx].mPos.GetY(),
				saveData.mSectorVerts[vertIdx].mPos.GetZ());
		}
	}

	int numSectors = saveData.mSectors.size();
	sectorStream->AddItem(numSectors);
	SECTOR_DEBUG_OUT("NumSectors<%d>\n", numSectors);

	//Write the sectors to the file
	for (int sectIdx = 0; sectIdx < numSectors; sectIdx++)
	{
		const Sector& sector = saveData.mSectors[sectIdx];

		for (int portalIdx = 0; portalIdx < Sector::kmaxportals; ++portalIdx)
		{
			const SectorPortal& portal = sector.mPortals[portalIdx];

			for(int i=0 ; i<NUM_PORTAL_CORNERS ; i++)
				sectorStream->AddItem(portal.mCornerVerts[i]);
			sectorStream->AddItem(portal.mPlane);
			sectorStream->AddItem(portal.mTrackProgress);
			sectorStream->AddItem(portal.mNeighbor);
		}

		sectorStream->AddItem(sector.mSplitPlane);
		sectorStream->AddItem((int)sector.mFlags);
		sectorStream->AddItem((int)sector.mMidlineTriStart);
		sectorStream->AddItem((int)sector.mMidlineTriCount);
		sectorStream->AddItem((int)sector.mKillTriStart);
		sectorStream->AddItem((int)sector.mKillTriCount);

		if (dbgOut)
		{
			SECTOR_DEBUG_OUT("Sector %d (%s)%s:  midline %d-%d\n",
				sectIdx, (sector.mFlags&SECTORFLAG_SPLITH)?"hsplit":(sector.mFlags&SECTORFLAG_SPLITV)?"vsplit":"straight",
				sector.IsStart() ? " (Start sector)" : "",
				sector.mMidlineTriStart, sector.mMidlineTriStart+sector.mMidlineTriCount*3);
			for(int portalIdx = 0 ; portalIdx < sector.NumPortals() ; portalIdx++) {
			const SectorPortal& portal = sector.mPortals[portalIdx];
				SECTOR_DEBUG_OUT("  Portal %d: @%.3f [%d,%d,%d,%d] -> %d\n",
					portalIdx,
					portal.mTrackProgress,
					portal.mCornerVerts[0], portal.mCornerVerts[1], portal.mCornerVerts[2], portal.mCornerVerts[3],
					portal.mNeighbor
					);
				SECTOR_DEBUG_OUT("    Plane<%f,%f,%f><%f>\n", portal.mPlane.GetNormal().GetX(), portal.mPlane.GetNormal().GetY(), portal.mPlane.GetNormal().GetZ(), portal.mPlane.GetD());
			}
		}
	}


	sectorStream->AddItem((int)saveData.mMidlineTris.size());
	SECTOR_DEBUG_OUT("NumMidlineTris<%d>\n", saveData.mMidlineTris.size());
	//Write the sectors to the file
	for (int vertIdx = 0; vertIdx < (int)saveData.mMidlineTris.size(); vertIdx++)
	{
		sectorStream->AddItem(saveData.mMidlineTris[vertIdx]);
		SECTOR_DEBUG_OUT("  [%d] = %d\n", vertIdx, saveData.mMidlineTris[vertIdx]);
	}

	sectorStream->AddItem((int)saveData.mMidlineVerts.size());
	SECTOR_DEBUG_OUT("NumMidlineVerts<%d>\n", saveData.mMidlineVerts.size());
	//Write the sectors to the file
	for (int vertIdx = 0; vertIdx < (int)saveData.mMidlineVerts.size(); vertIdx++)
	{
		sectorStream->AddItem(saveData.mMidlineVerts[vertIdx].mPos);
		sectorStream->AddItem(saveData.mMidlineVerts[vertIdx].mTubeGrav);
		sectorStream->AddItem(saveData.mMidlineVerts[vertIdx].mTotalGrav);
		sectorStream->AddItem(saveData.mMidlineVerts[vertIdx].mVelocityFollow);
		SECTOR_DEBUG_OUT("  [%d] = <%f,%f,%f> tube:%f grav:%f, velfol:%f\n",
			vertIdx,
			saveData.mMidlineVerts[vertIdx].mPos.GetX(),
			saveData.mMidlineVerts[vertIdx].mPos.GetY(),
			saveData.mMidlineVerts[vertIdx].mPos.GetZ(),
			saveData.mMidlineVerts[vertIdx].mTubeGrav,
			saveData.mMidlineVerts[vertIdx].mTotalGrav,
			saveData.mMidlineVerts[vertIdx].mVelocityFollow);
	}

	sectorStream->AddItem((int)saveData.mKillVerts.size());
	SECTOR_DEBUG_OUT("NumKillVerts<%d>\n", saveData.mKillVerts.size());
	for (int vertIdx = 0; vertIdx < (int)saveData.mKillVerts.size(); vertIdx++)
	{
		sectorStream->AddItem(saveData.mKillVerts[vertIdx]);
		SECTOR_DEBUG_OUT("  [%d] = <%f,%f,%f> \n",
			vertIdx,
			saveData.mKillVerts[vertIdx].GetX(),
			saveData.mKillVerts[vertIdx].GetY(),
			saveData.mKillVerts[vertIdx].GetZ());
	}

	sectorStream->AddItem((int)saveData.mKillTris.size()/3);
	SECTOR_DEBUG_OUT("NumKillTris<%d>\n", saveData.mKillTris.size()/3);
	//Write the sectors to the file
	for (int triIdx = 0; triIdx < (int)saveData.mKillTris.size()/3; triIdx++)
	{
		sectorStream->AddItem(saveData.mKillTris[triIdx*3+0]);
		sectorStream->AddItem(saveData.mKillTris[triIdx*3+1]);
		sectorStream->AddItem(saveData.mKillTris[triIdx*3+2]);
		SECTOR_DEBUG_OUT("  [%d] = %d : %d : %d\n", triIdx,
			saveData.mKillTris[triIdx*3+0],
			saveData.mKillTris[triIdx*3+1],
			saveData.mKillTris[triIdx*3+2]);
	}
	sectorStream->AddItem((int)'done');

	return true;
}

///////////////////////////////////////////////////////////////////////////////

static bool LoadSectors(chunkfile::InputStream* sectorStream, SectorData &saveData)
{
	if (!sectorStream)
	{
		orkprintf("Bad input stream pointer!\n");
		return false;
	}

	int numVerts = 0;

	sectorStream->GetItem(numVerts);
	saveData.mSectorVerts.resize(numVerts);

	for (int vertIdx = 0; vertIdx < numVerts; vertIdx++)
	{
		sectorStream->GetItem(saveData.mSectorVerts[vertIdx].mPos);
	}

	int numSectors = 0;

	sectorStream->GetItem(numSectors);

	saveData.mSectors.resize(numSectors);

	orkprintf("Loading Sectors: %d sectors\n", numSectors);
	float lensum = 0;
	for (int sectorIdx = 0; sectorIdx < numSectors; ++sectorIdx)
	{
		Sector& sector = saveData.mSectors[sectorIdx];

		for (int portalIdx = 0; portalIdx < Sector::kmaxportals; ++portalIdx)
		{
			SectorPortal& portal = sector.mPortals[portalIdx];

			for(int i=0 ; i<NUM_PORTAL_CORNERS ; i++)
				sectorStream->GetItem(portal.mCornerVerts[i]);
			sectorStream->GetItem(portal.mPlane);
			sectorStream->GetItem(portal.mTrackProgress);
			sectorStream->GetItem(portal.mNeighbor);
		}

		sectorStream->GetItem(sector.mSplitPlane);
		sectorStream->GetItem(sector.mFlags);
		sectorStream->GetItem(sector.mMidlineTriStart);
		sectorStream->GetItem(sector.mMidlineTriCount);
		sectorStream->GetItem(sector.mKillTriStart);
		sectorStream->GetItem(sector.mKillTriCount);

		// recalculate basis vectors
		lensum += sector.ContributeBasis(saveData);
	}
	saveData.mLength = lensum / numSectors;

	// normalize basis vectors
	for (int vertIdx = 0; vertIdx < (int)saveData.mSectorVerts.size() ; vertIdx++)
	{
		saveData.mSectorVerts[vertIdx].mY.Normalize();
		saveData.mSectorVerts[vertIdx].mZ.Normalize();
	}

	sectorStream->GetItem(numVerts);
	saveData.mMidlineTris.resize(numVerts);
	for (int vertIdx = 0; vertIdx < (int)saveData.mMidlineTris.size(); vertIdx++)
	{
		sectorStream->GetItem(saveData.mMidlineTris[vertIdx]);
	}

	sectorStream->GetItem(numVerts);
	saveData.mMidlineVerts.resize(numVerts);
	for (int vertIdx = 0; vertIdx < (int)saveData.mMidlineVerts.size(); vertIdx++)
	{
		sectorStream->GetItem(saveData.mMidlineVerts[vertIdx].mPos);
		sectorStream->GetItem(saveData.mMidlineVerts[vertIdx].mTubeGrav);
		sectorStream->GetItem(saveData.mMidlineVerts[vertIdx].mTotalGrav);
		sectorStream->GetItem(saveData.mMidlineVerts[vertIdx].mVelocityFollow);
	}


	sectorStream->GetItem(numVerts);
	saveData.mKillVerts.resize(numVerts);
	for (int vertIdx = 0; vertIdx < (int)saveData.mKillVerts.size(); vertIdx++)
	{
		sectorStream->GetItem(saveData.mKillVerts[vertIdx]);
	}

	sectorStream->GetItem(numVerts);
	saveData.mKillTris.resize(numVerts*3);
	saveData.mKillPlanes.resize(numVerts);
	for (int triIdx = 0; triIdx < numVerts; triIdx++)
	{
		sectorStream->GetItem(saveData.mKillTris[triIdx*3+0]);
		sectorStream->GetItem(saveData.mKillTris[triIdx*3+1]);
		sectorStream->GetItem(saveData.mKillTris[triIdx*3+2]);
		saveData.mKillPlanes[triIdx].CalcPlaneFromTriangle(
			saveData.mKillVerts[saveData.mKillTris[triIdx*3+0]],
			saveData.mKillVerts[saveData.mKillTris[triIdx*3+1]],
			saveData.mKillVerts[saveData.mKillTris[triIdx*3+2]]);
	}

	sectorStream->GetItem(numVerts);
	OrkAssert(numVerts == 'done');

	return true;
}

float Sector::ContributeBasis(SectorData &data) {
	bool reverse = mPortals[1].mTrackProgress < mPortals[0].mTrackProgress;
	float progress = 0;

	mPortals[0].ContributeBasis(reverse, data);
	mPortals[1].ContributeBasis(!reverse, data);
	progress += mPortals[1].mTrackProgress - mPortals[0].mTrackProgress;
	if (IsSplit()) {
		mPortals[2].ContributeBasis(!reverse, data);
		progress += mPortals[2].mTrackProgress - mPortals[0].mTrackProgress;
		progress *= 0.5f;
	}

	if (progress < 0) progress = -progress;

	return Length(data.mSectorVerts)/progress;
}

void SectorPortal::ContributeBasis(bool reverse, SectorData &data) {
	ork::fvec3 y, z;
	z = mPlane.GetNormal();
	y  = data.mSectorVerts[mCornerVerts[PORTAL_CORNER_TL]].mPos - data.mSectorVerts[mCornerVerts[PORTAL_CORNER_BL]].mPos;
	y += data.mSectorVerts[mCornerVerts[PORTAL_CORNER_TR]].mPos - data.mSectorVerts[mCornerVerts[PORTAL_CORNER_BR]].mPos;
	y.Normalize();
	if (reverse) {
		z = -z;
	}
	for(int i=0 ; i<NUM_PORTAL_CORNERS ; i++) {
		data.mSectorVerts[mCornerVerts[PORTAL_CORNER_TL]].mY += y;
		data.mSectorVerts[mCornerVerts[PORTAL_CORNER_TL]].mZ += z;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Sector::Sector()
	: mFlags(0L)
	, mKillTriCount(0)
	, mKillTriStart(0)
	, mMidlineTriStart(0)
	, mMidlineTriCount(0)
{
}

///////////////////////////////////////////////////////////////////////////////

bool Sector::ContainsPoint(const SectorData &data, const ork::fvec3& position) const
{
	for(int portal = 0 ; portal < NumPortals() ; portal++) {
		if ((mPortals[portal].mPlane.GetPointDistance(position)<-EPSILON))
			return false;
	}
	float x, y, z;
	GetRelativePositionOfPoint(data, position, x, y, z);
	if (x < 0 || x > 1 || y < 0 || y > 1) return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////////

void Sector::GetRelativePositionOfPoint(const SectorData &data, const ork::fvec3& position, float &x, float &y, float &z) const {
	fvec3 bl, tl, tr, br;
	int portal1 = 1;
	float xmin = 0, xmax = 1, ymin = 0, ymax = 1;

	switch(mFlags & (SECTORFLAG_SPLITH|SECTORFLAG_SPLITV)) {
	case SECTORFLAG_SPLITH:
		if (mSplitPlane.IsPointBehind(position)) {
			portal1 = 1;
			xmax = 0.5f;
			bl = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BR]].mPos;
			tl = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TR]].mPos;
			tr.Lerp(
				data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TR]].mPos,
				 data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TL]].mPos,
				 0.5f);
			br.Lerp(
				data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BR]].mPos,
				 data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BL]].mPos,
				 0.5f);
		} else {
			portal1 = 2;
			xmin = 0.5f;
			bl.Lerp(
				data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BR]].mPos,
				 data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BL]].mPos,
				 0.5f);
			tl.Lerp(
				data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TR]].mPos,
				 data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TL]].mPos,
				 0.5f);
			tr = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TL]].mPos;
			br = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BL]].mPos;
		}
		break;
	case SECTORFLAG_SPLITV:
		if (mSplitPlane.IsPointBehind(position)) {
			portal1 = 1;
			ymax = 0.5f;
			bl = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BR]].mPos;
			tl.Lerp(
				data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BR]].mPos,
				 data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TR]].mPos,
				 0.5f);
			tr.Lerp(
				data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BL]].mPos,
				 data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TL]].mPos,
				 0.5f);
			br = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BL]].mPos;
		} else {
			portal1 = 2;
			ymin = 0.5f;
			bl.Lerp(
				data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BR]].mPos,
				 data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TR]].mPos,
				 0.5f);
			tl = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TR]].mPos;
			tr = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TL]].mPos;
			br.Lerp(
				data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BL]].mPos,
				 data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TL]].mPos,
				 0.5f);
		}
		break;
	default:
		portal1 = 1;
		bl = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BR]].mPos;
		tl = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TR]].mPos;
		tr = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TL]].mPos;
		br = data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BL]].mPos;
		break;
	}

	{
		float d0 = mPortals[0].mPlane.GetPointDistance(position);
		float d1 = mPortals[portal1].mPlane.GetPointDistance(position);
		z = d0/(d0+d1);
	}

	bl.Lerp(bl, data.mSectorVerts[mPortals[portal1].mCornerVerts[PORTAL_CORNER_BL]].mPos, z);
	tl.Lerp(tl, data.mSectorVerts[mPortals[portal1].mCornerVerts[PORTAL_CORNER_TL]].mPos, z);
	tr.Lerp(tr, data.mSectorVerts[mPortals[portal1].mCornerVerts[PORTAL_CORNER_TR]].mPos, z);
	br.Lerp(br, data.mSectorVerts[mPortals[portal1].mCornerVerts[PORTAL_CORNER_BR]].mPos, z);

	fvec3 dz = ((tr-tl)+(br-bl)).Cross((tl-bl)+(tr-br));

	{
		fvec3 n1 = dz.Cross(br-bl).Normal();
		fvec3 n2 = dz.Cross(tl-tr).Normal();

		float d1 = n1.Dot(position)-n1.Dot(bl);
		float d2 = n2.Dot(position)-n2.Dot(tl);

		y = ymin + (ymax-ymin)*d1/(d1+d2);
	}

	{
		fvec3 n1 = dz.Cross(tl-bl).Normal();
		fvec3 n2 = dz.Cross(br-tr).Normal();

		float d1 = n1.Dot(position)-n1.Dot(bl);
		float d2 = n2.Dot(position)-n2.Dot(br);

		x = xmin + (xmax-xmin)*d1/(d1+d2);
	}
}



///////////////////////////////////////////////////////////////////////////////

ork::fvec3 SectorPortal::GetCenter(orkvector<SectorVert> boundsVerts) const {
	ork::fvec3 out(0,0,0);
	for(int i=0 ; i<NUM_PORTAL_CORNERS ; i++)
		out += boundsVerts[mCornerVerts[i]].mPos;
	return out * (1.0f/NUM_PORTAL_CORNERS);
}

///////////////////////////////////////////////////////////////////////////////

float Sector::Length(orkvector<SectorVert> mBoundsVerts) const {
	if (IsSplit())
		return ((mPortals[0].GetCenter(mBoundsVerts) - mPortals[1].GetCenter(mBoundsVerts)).Mag() +
				(mPortals[0].GetCenter(mBoundsVerts) - mPortals[2].GetCenter(mBoundsVerts)).Mag())/2;
	else
		return (mPortals[0].GetCenter(mBoundsVerts) - mPortals[1].GetCenter(mBoundsVerts)).Mag();
}

///////////////////////////////////////////////////////////////////////////////

void Sector::Transform(const fmtx4& transform)
{
	// TODO
/*	for (int i = 0; i < NUM_FACES; ++i)
	{
		mPlanes[i].SimpleTransform(transform);
		mPlaneCenters[i] = mPlaneCenters[i].Transform(transform).xyz();
	}

	mDirX = mDirX.Transform3x3(transform).Normal();
	mDirY = mDirY.Transform3x3(transform).Normal();
	mDirZ = mDirZ.Transform3x3(transform).Normal();
	mCenter = mCenter.Transform(transform).xyz();

	mUBasis += mOrigin;
	mVBasis += mOrigin;

	mUBasis = mUBasis.Transform(transform).xyz();
	mVBasis = mVBasis.Transform(transform).xyz();
	mOrigin = mOrigin.Transform(transform).xyz();

	mUBasis -= mOrigin;
	mVBasis -= mOrigin;
	mUVNormal = mUBasis.Cross(mVBasis).Normal();*/
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

bool Track::Save(const ork::file::Path& path, const TrackSaveData& data)
{
	EndianContext* endianContext = NULL;
	bool bwii = (0!=strstr(path.c_str(),"wii"));
	bool bxb360 = (0!=strstr(path.c_str(),"xb360"));

	if( bxb360 || bwii )
	{
		endianContext = new EndianContext;
		endianContext->mendian = ork::EENDIAN_BIG;
	}

	chunkfile::Writer writer("sec");
#if WRITE_DEBUG_TXT
	file::Path dbgpath = path;
	dbgpath.SetExtension("txt");
	FILE* dbgOut = fopen(dbgpath.ToAbsolute().c_str(), "wt");
#else
	FILE* dbgOut = 0;
#endif
	SECTOR_DEBUG_OUT("SEC Debug Output for <%s>\n", path.c_str());

	//Write sectors as well as track and bounds collision data to streams
	TriangleIndexVertexBuffer trackTris(data.mTrackIndices, data.mTrackVerts);

	if (!trackTris.Loaded())
	{
		orkerrorlog("ERROR: <%s> Failed to convert triangle meshes\n", path.c_str());
		return false;
	}

	if (!trackTris.Save(writer.AddStream(sTrackIndicesChunkName), writer.AddStream(sTrackVertsChunkName)))
	{
		orkerrorlog("ERROR: <%s> Failed to save triangle meshes\n", path.c_str());
		return false;
	}

	{
		ork::chunkfile::OutputStream *flagStream = writer.AddStream(sTrackFlagsChunkName);

		//Write the mVerts to a stream in the file
		flagStream->AddItem((int)(data.mTrackFlags.size()));
		for (int i = 0; i < (int) data.mTrackFlags.size(); ++i)
			flagStream->AddItem(data.mTrackFlags[i]);
	}

	if (!SaveMeshOptimizations(trackTris.ToBullet(), writer.AddStream(sTrackBVHChunkName)))
	{
		orkerrorlog("ERROR: <%s> Failed to save triangle mesh optimizations\n", path.c_str());
		return false;
	}

	if (!SaveSectors(writer.AddStream(sSectorChunkName), data, dbgOut))
	{
		orkerrorlog("ERROR: <%s> Failed to save sectors\n", path.c_str());
		return false;
	}

	chunkfile::OutputStream* versionStream = writer.AddStream(sVersionChunkName);
	versionStream->AddItem(sSecVersion);

	//Write the streams to the file
	file::Path fullPath = path.ToAbsolute();
	fullPath.SetExtension("sec");
	writer.WriteToFile(fullPath);

	if (dbgOut)
		fclose(dbgOut);

	if (endianContext)
		delete endianContext;

	return true;
}

Track::~Track()
{
	if( mTrackShape )
	{
		delete mTrackShape;
	}
	if( mTIVB )
	{
		delete mTIVB;
	}
	miStartIndex = -1;
}

///////////////////////////////////////////////////////////////////////////////

bool Track::Load(const ork::file::Path& path, const ork::fmtx4& transform)
{
	CollisonReader reader(path, "sec");

	if (!reader.IsOk())
	{
		orkerrorlog("ERROR: <%s> Failed to open sector definition file\n", path.c_str() );
		return false;
	}

	chunkfile::InputStream* versionStream = reader.GetStream(sVersionChunkName);
	if (!versionStream)
	{
		orkerrorlog("ERROR: <%s> Couldn't determine sec file version\n", path.c_str());
		return false;
	}

	int version;
	versionStream->GetItem(version);
	if (version < sSecVersion)
	{
		orkerrorlog("ERROR: <%s> Sec file on disk is version %d, current tool version is %d. Reconvert your sec files!\n", path.c_str(), version, sSecVersion);
		return false;
	}
	else if (version > sSecVersion)
	{
		orkerrorlog("ERROR: <%s> Sec file version is %d, current is %d. Use a newer tool/game!\n", path.c_str(), version, sSecVersion);
		return false;
	}

	TrackSaveData savedat;

	if (!LoadSectors(reader.GetStream(sSectorChunkName), mSectorData))
	{
		orkerrorlog("ERROR: <%s> Failed to load sectors\n", path.c_str());
		return false;
	}

	for (orkvector<Sector>::iterator sectorIt = mSectorData.mSectors.begin(); sectorIt != mSectorData.mSectors.end(); ++sectorIt)
	{
		sectorIt->Transform(transform);

		if(sectorIt->IsStart())
			miStartIndex = int(sectorIt - mSectorData.mSectors.begin());
	}

	mTIVB = new TriangleIndexVertexBuffer(reader.GetStream(sTrackIndicesChunkName), reader.GetStream(sTrackVertsChunkName));

	mTIVB->Transform(transform);

	if (!mTIVB->Loaded())
	{
		orkerrorlog("ERROR: <%s> Failed to load triangle meshes\n", path.c_str());
		return false;
	}

	mTrackShape = LoadMeshOptimizations(mTIVB->ToBullet(), reader.GetStream(sTrackBVHChunkName));

	if (!mTrackShape)
	{
		orkerrorlog("ERROR: <%s> Failed to load triangle mesh optimizations\n", path.c_str());
		return false;
	}

	{
		ork::chunkfile::InputStream *flagStream = reader.GetStream(sTrackFlagsChunkName);

		int numTris;
		flagStream->GetItem(numTris);
		mTriangleFlags.resize(numTris);
		for (int i = 0; i < numTris; ++i)
			flagStream->GetItem(mTriangleFlags[i]);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////

int Track::FindSectorByPoint(const ork::fvec3& point) const
{
	for (int sectorIdx = 0; sectorIdx < int(mSectorData.mSectors.size()); sectorIdx++)
		if (mSectorData.mSectors[sectorIdx].ContainsPoint(mSectorData, point))
			return sectorIdx;

	return -1;
}

///////////////////////////////////////////////////////////////////////////////

int Track::CheckTraversal(unsigned int currSector, const ork::fvec3& pos, bool allowInvalid) const
{
	// OPTIMIZE?
	if (currSector == -1) return -1;
	if (mSectorData.mSectors[currSector].ContainsPoint(mSectorData, pos)) return currSector;
	for(int i=0 ; i<mSectorData.mSectors[currSector].NumPortals() ; i++) {
		int other = mSectorData.mSectors[currSector].mPortals[i].mNeighbor;
		if (mSectorData.mSectors[other].ContainsPoint(mSectorData, pos)) return other;
	}
	return -1;
}

///////////////////////////////////////////////////////////////////////////////

float Track::GetProgressForPosition(unsigned int currSector, const ork::fvec3& pos) const
{
	const Sector& sector = mSectorData.mSectors[currSector];
	return sector.GetProgressForPosition(mSectorData, pos);
}

///////////////////////////////////////////////////////////////////////////////

float Sector::GetProgressForPosition(const SectorData& data, const ork::fvec3& position) const
{
	int farportal;
	float x, y, z;
	GetRelativePositionOfPoint(data, position, x, y, z);
	if (IsSplit()) {
		farportal = (mSplitPlane.IsPointBehind(position))?1:2;
	} else {
		farportal = 1;
	}
	return mPortals[0].mTrackProgress + z* (mPortals[farportal].mTrackProgress - mPortals[0].mTrackProgress);
}

///////////////////////////////////////////////////////////////////////////////

void Sector::GetBasis(const SectorData& data, const ork::fvec3& position, fvec3& dirX, fvec3& dirY, fvec3& dirZ) const
{
	float x, y, z;
	GetRelativePositionOfPoint(data, position, x, y, z);

	ork::fvec3 a, b;

	a.Lerp(data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BL]].mY,
		data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BR]].mY, 1-x);
	b.Lerp(data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TL]].mY,
		data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TR]].mY, 1-x);
	dirY.Lerp(a, b, y);
	a.Lerp(data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BL]].mZ,
		data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_BR]].mZ, 1-x);
	b.Lerp(data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TL]].mZ,
		data.mSectorVerts[mPortals[0].mCornerVerts[PORTAL_CORNER_TR]].mZ, 1-x);
	dirZ.Lerp(a, b, y);

	int farportal = 1;
	if (IsSplit()) {
		if (mSplitPlane.IsPointBehind(position)) {
			if (mFlags & SECTORFLAG_SPLITH)
				x = x*2;
			else
				y = y*2;
		} else {
			farportal = 2;
			if (mFlags & SECTORFLAG_SPLITH)
				x = x*2-1;
			else
				y = y*2-1;
		}
	}

	a.Lerp(data.mSectorVerts[mPortals[farportal].mCornerVerts[PORTAL_CORNER_BL]].mY,
		data.mSectorVerts[mPortals[farportal].mCornerVerts[PORTAL_CORNER_BR]].mY, x);
	b.Lerp(data.mSectorVerts[mPortals[farportal].mCornerVerts[PORTAL_CORNER_TL]].mY,
		data.mSectorVerts[mPortals[farportal].mCornerVerts[PORTAL_CORNER_TR]].mY, x);
	a.Lerp(a, b, y);
	dirY.Lerp(dirY, a, z);
	a.Lerp(data.mSectorVerts[mPortals[farportal].mCornerVerts[PORTAL_CORNER_BL]].mZ,
		data.mSectorVerts[mPortals[farportal].mCornerVerts[PORTAL_CORNER_BR]].mZ, x);
	b.Lerp(data.mSectorVerts[mPortals[farportal].mCornerVerts[PORTAL_CORNER_TL]].mZ,
		data.mSectorVerts[mPortals[farportal].mCornerVerts[PORTAL_CORNER_TR]].mZ, x);
	a.Lerp(a, b, y);
	dirZ.Lerp(dirZ, a, z);


	dirY.Normalize();
	// enforce orthogonality
	dirZ -= dirY*(dirY.Dot(dirZ));
	dirZ.Normalize();
	dirX = dirY.Cross(dirZ); // don't need to normalize - Y and Z are orthonormal
}

///////////////////////////////////////////////////////////////////////////////

bool Sector::GetMidline(const SectorData& data, const ork::fvec3& position, MidlineVert& midline) const
{
	if (mMidlineTriCount == 0)
		return false;

	float closestDistSquared = 1.0e20f;
	float rawb=0, rawc=0;
	int closestTri = 0;
	for(int i=0 ; i<mMidlineTriCount ; i++) {
		const MidlineVert &va = data.mMidlineVerts[data.mMidlineTris[mMidlineTriStart+i*3+0]];
		const MidlineVert &vb = data.mMidlineVerts[data.mMidlineTris[mMidlineTriStart+i*3+1]];
		const MidlineVert &vc = data.mMidlineVerts[data.mMidlineTris[mMidlineTriStart+i*3+2]];

		ork::fvec3 v0 = vb.mPos-va.mPos;
		ork::fvec3 v1 = vc.mPos-va.mPos;
		ork::fvec3 vp = position-va.mPos;

		float dot00 = v0.Dot(v0);
		float dot01 = v0.Dot(v1);
		float dot0p = v0.Dot(vp);
		float dot11 = v1.Dot(v1);
		float dot1p = v1.Dot(vp);

		float denom = 1/(dot00*dot11+dot01*dot01);

		float db = (dot00*dot1p-dot01*dot0p )*denom;
		float dc = (dot11*dot0p-dot01*dot1p )*denom;

		if (db + dc > 1) {
			ork::fvec3 v = vc.mPos-vb.mPos;
			float x = v.Dot(position-vb.mPos)/v.Dot(v);
			if (x < 0) x = 0;
			if (x > 1) x = 1;
			db = 1-x;
			dc = x;
		} else if (db < 0) {
			float x = v0.Dot(vp)/v0.Dot(v0);
			if (x < 0) x = 0;
			if (x > 1) x = 1;
			db = x;
			dc = 0;
		} else if (dc < 0) {
			float x = v1.Dot(vp)/v1.Dot(v1);
			if (x < 0) x = 0;
			if (x > 1) x = 1;
			db = 0;
			dc = x;
		}

		float da = 1-(db+dc);

		ork::fvec3 v = da*va.mPos + db*vb.mPos + dc*vc.mPos;

		float dist = (v-position).MagSquared();
		if (dist < closestDistSquared) {
			closestDistSquared = dist;
			rawb = db;
			rawc = dc;
			midline.mPos = v;
			midline.mTotalGrav = da*va.mTotalGrav + db*vb.mTotalGrav + dc*vc.mTotalGrav;
			midline.mTubeGrav = da*va.mTubeGrav+ db*vb.mTubeGrav+ dc*vc.mTubeGrav;
			midline.mVelocityFollow = 1;//da*va.mVelocityFollow+ db*vb.mVelocityFollow+ dc*vc.mVelocityFollow;
			closestTri = i;
		}
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool Sector::CrossedKill(const SectorData& data, const ork::fvec3& oldpos, const ork::fvec3& newpos) const
{
	for(int tri=0 ; tri<mKillTriCount ; tri++) {
		const fplane3 &plane = data.mKillPlanes[mKillTriStart+tri];
		if (plane.GetPointDistance(oldpos)*plane.GetPointDistance(newpos) > 0) break;
		fvec3 intersect;
		float dis;
		plane.Intersect(LineSegment3(oldpos, newpos), dis, intersect);
		ork::fvec3 base = data.mKillVerts[data.mKillTris[(mKillTriStart+tri)*3+0]];
		ork::fvec3 v0 = data.mKillVerts[data.mKillTris[(mKillTriStart+tri)*3+1]]-base;
		ork::fvec3 v1 = data.mKillVerts[data.mKillTris[(mKillTriStart+tri)*3+2]]-base;
		ork::fvec3 vp = intersect-base;

		float dot00 = v0.Dot(v0);
		float dot01 = v0.Dot(v1);
		float dot0p = v0.Dot(vp);
		float dot11 = v1.Dot(v1);
		float dot1p = v1.Dot(vp);

		float denom = 1/(dot00*dot11+dot01*dot01);

		float db = (dot00*dot1p-dot01*dot0p )*denom;
		float dc = (dot11*dot0p-dot01*dot1p )*denom;

		if (db < 0) continue;
		if (dc < 0) continue;
		if (db+dc > 1) continue;
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

fvec3 Track::GetSectorEntry(int currSector) const {
	const ork::ent::bullet::Sector& sector = mSectorData.mSectors[currSector];
	int portal = 0;
	if (sector.mPortals[1].mTrackProgress < sector.mPortals[0].mTrackProgress)
		portal = 1;
	return sector.mPortals[portal].GetCenter(mSectorData.mSectorVerts);
}

void Track::GetBasis(unsigned int currSector, const ork::fvec3& pos, fvec3& dirX, fvec3& dirY, fvec3& dirZ) const
{
	const ork::ent::bullet::Sector& sector = mSectorData.mSectors[currSector];

	sector.GetBasis(mSectorData, pos, dirX, dirY, dirZ);
}

bool Track::GetMidline(unsigned int currSector, const ork::fvec3& pos, MidlineVert &midline) const
{
	const ork::ent::bullet::Sector& sector = mSectorData.mSectors[currSector];

	return sector.GetMidline(mSectorData, pos, midline);
}

bool Track::CrossedKill(unsigned int currSector, const ork::fvec3& oldpos, const ork::fvec3& newpos) const
{
	const ork::ent::bullet::Sector& sector = mSectorData.mSectors[currSector];

	return sector.CrossedKill(mSectorData, oldpos, newpos);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SectorRange::Describe()
{
	ork::reflect::RegisterProperty("Begin", &SectorRange::mBegin);
	ork::reflect::AnnotatePropertyForEditor<SectorRange>("Begin", "editor.range.min", "0.0f");
	ork::reflect::AnnotatePropertyForEditor<SectorRange>("Begin", "editor.range.max", "1.0f");

	ork::reflect::RegisterProperty("End", &SectorRange::mEnd);
	ork::reflect::AnnotatePropertyForEditor<SectorRange>("End", "editor.range.min", "0.0f");
	ork::reflect::AnnotatePropertyForEditor<SectorRange>("End", "editor.range.max", "1.0f");
}

///////////////////////////////////////////////////////////////////////////////

void TrackData::Describe()
{
	ork::reflect::RegisterProperty("SecMesh", &TrackData::mSecMeshName);
	ork::reflect::AnnotatePropertyForEditor<TrackData>("SecMesh", "editor.class", "ged.factory.filelist");
	ork::reflect::AnnotatePropertyForEditor<TrackData>("SecMesh", "editor.filetype", "sec");

	ork::reflect::RegisterProperty("TrackScale", &TrackData::mTrackScale);
	ork::reflect::AnnotatePropertyForEditor<TrackData>("TrackScale", "editor.range.min", "0.01f");
	ork::reflect::AnnotatePropertyForEditor<TrackData>("TrackScale", "editor.range.max", "10.0f");

	ork::reflect::RegisterProperty("IntroCameraAnimation", &TrackData::mIntroCameraAnimation);
	ork::reflect::AnnotatePropertyForEditor<TrackData>("IntroCameraAnimation", "editor.assettype", "xganim");
	ork::reflect::AnnotatePropertyForEditor<TrackData>("IntroCameraAnimation", "editor.assetclass", "xganim");

	ork::reflect::RegisterMapProperty("NoRespawnRanges", &TrackData::mNoRespawnRanges);
	ork::reflect::AnnotatePropertyForEditor<TrackData>("NoRespawnRanges", "editor.factorylistbase", "SectorRange");
}

TrackData::TrackData()
	: mTrackScale( 1.0f )
	, mIntroCameraAnimation(NULL)
{
}

TrackData::~TrackData()
{
}

float TrackData::GetRespawnProgress(float actual_progress) const
{
	float respawn_progress = actual_progress;

	for(ork::orklut<ork::PoolString, ork::ent::bullet::SectorRange *>::const_iterator it = mNoRespawnRanges.begin();
			it != mNoRespawnRanges.end(); it++)
	{
		float begin = it->second->GetBegin();
		float end = it->second->GetEnd();
		if(end > begin && actual_progress >= begin && actual_progress < end)
			respawn_progress = end;
	}

	return respawn_progress;
}

ork::ent::ComponentInst *TrackData::createComponent(ork::ent::Entity *pent) const
{
	return OrkNew TrackInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////

void TrackInst::Describe() {}

TrackInst::TrackInst(const TrackData &data, ork::ent::Entity *pent)
	: ork::ent::ComponentInst(&data, pent)
	, mData(data)
{
	AllocationLabel("TrackInst::TrackInst");
	//Get the transform from the entity
	ork::fmtx4 trans = GetEntity()->GetDagNode().GetTransformNode().GetTransform().GetMatrix();

	//Get the scale from the Data
	ork::fmtx4 scale = fmtx4::Identity;
	float fscale = mData.GetTrackScale();
	scale.SetScale( fscale );
	trans = scale * trans;

	//Load the track data, baking the transform into it
	mTrack.Load(GetData().GetSecMeshName(), trans);
}

TrackInst::~TrackInst()
{
}

bool TrackInst::DoLink(ork::ent::Simulation* psi)
{
	auto* bulletsys = psi->findSystem<ork::ent::BulletSystem>();
	if( 0 == bulletsys ) return false;

	if(btDynamicsWorld* world = bulletsys->GetDynamicsWorld())
	{
		if(ork::ent::bullet::TrackInst* trackInst = GetEntity()->GetTypedComponent<ork::ent::bullet::TrackInst>())
		{
			// Add the track data to the bullet world
			if(trackInst->GetTrack().GetTrackShape())
			{
				btScalar mass(0.0);
				btVector3 localInertia(0, 0, 0);

				btTransform btTrans = !ork::fmtx4::Identity;

				btDefaultMotionState* motionState = new btDefaultMotionState(btTrans);
				btRigidBody::btRigidBodyConstructionInfo bodyInfo(mass, motionState, trackInst->GetTrack().GetTrackShape(), localInertia);
				btRigidBody* body = new btRigidBody(bodyInfo);
				body->setRestitution(1.0f);
				body->setFriction(1.0f);
				body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
				body->setUserPointer(GetEntity());

				world->addRigidBody(body);
			}
		}
		world->setGravity(btVector3(0, 0, 0));
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SectorTrackerData::Describe()
{
}

SectorTrackerData::SectorTrackerData()
{
}
SectorTrackerData::~SectorTrackerData()
{
}
ork::ent::ComponentInst *SectorTrackerData::createComponent(ork::ent::Entity *pent) const
{
	return OrkNew SectorTrackerInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////

void SectorTrackerInst::Describe() {}

SectorTrackerInst::SectorTrackerInst(const SectorTrackerData &data, ork::ent::Entity *pent)
	: ork::ent::ComponentInst(&data, pent)
	, mData(data)
	, mTrack( 0 )
	, mCurrSector(-1)
	, mTrackProgress(0.0f)
//////////////////////////////////////////////////////
//	these need to be initialized so that ShortestRotationArc doesnt choke
	, mBasisX(1.0f,0.0f,0.0f)
	, mBasisY(0.0f,1.0f,0.0f)
	, mBasisZ(0.0f,0.0f,1.0f)
	, mDisallowCheating(false)
//////////////////////////////////////////////////////
{
}

SectorTrackerInst::~SectorTrackerInst()
{
}

bool SectorTrackerInst::DoLink(ork::ent::Simulation *inst)
{
	ork::ent::Entity* ptrackent = inst->FindEntityLoose(ork::AddPooledLiteral("track"));

	if( 0 != ptrackent )
	{
		TrackInst* tinst = ptrackent->GetTypedComponent<TrackInst>();

		mTrack = & tinst->GetTrack();
	}
	return (mTrack!=0);
}

bool SectorTrackerInst::DoStart(ork::ent::Simulation *inst, const ork::fmtx4 &world)
{
	return true;
}

bool SectorTrackerInst::DoNotify(const ork::event::Event *event)
{
	return true;
}

bool SectorTrackerInst::UpdatePos(const ork::fvec3 &newpos, ork::fvec3 &newvel)
{
	OrkAssert(mTrack);

	//OrkAssert(mCurrSector > -1);
	if(mCurrSector < 0)
		return false;

	int sector = mTrack->CheckTraversal(mCurrSector, newpos, /*allowInvalid =*/ true);
	if(sector < 0)
	{
		sector = mTrack->FindSectorByPoint(newpos);
		if(sector < 0)
			return false;
	}

	static const float CHEATING = 0.075f;
	float progress = mTrack->GetProgressForPosition(sector, newpos);
	if(mDisallowCheating)
	{
		if(fabs(progress - mTrackProgress) > CHEATING
				&& fabs(progress - mTrackProgress - 1.0f) > CHEATING
				&& fabs(progress - mTrackProgress + 1.0f) > CHEATING)
		{
			orkprintf("INFO: No cheating!\n");
			return false;
		}
	}

	mCurrSector = sector;
	mTrackProgress = progress;

	ork::fvec3 oldBasisY = mBasisY;

	mTrack->GetBasis(mCurrSector, newpos, mBasisX, mBasisY, mBasisZ);

	if (mTrack->GetMidline(mCurrSector, newpos, mMidlineVert)) {
		// strict not-equal is correct, this is just an optimization for the no-midline case.
		ork::fvec3 tubeY = (mMidlineVert.mPos-newpos).Normal();
		if (mMidlineVert.mTubeGrav > 0)
			mBasisY.Lerp(mBasisY, -tubeY, mMidlineVert.mTubeGrav);
		else
			mBasisY.Lerp(mBasisY, tubeY, -mMidlineVert.mTubeGrav);
		mBasisY.Normalize();

		// enforce orthogonality
		mBasisZ -= mBasisY*(mBasisY.Dot(mBasisZ));
		mBasisZ.Normalize();
		mBasisX = mBasisY.Cross(mBasisZ); // don't need to normalize - Y and Z are orthonormal
	} else {
		mMidlineVert.mPos = newpos;
		mMidlineVert.mTubeGrav = 0;
		mMidlineVert.mTotalGrav = 1;
		mMidlineVert.mVelocityFollow = 0;
	}

	if(mMidlineVert.mVelocityFollow > 0)
	{
		ork::fquat rot;
		rot.ShortestRotationArc(oldBasisY, mBasisY);
		rot.Lerp(fquat(), rot, mMidlineVert.mVelocityFollow);
		newvel = newvel.Transform(rot.ToMatrix3());
	}

	if(mTrack->CrossedKill(mCurrSector, newpos, mOldpos))
	{
		mCrossedKill = true;
	}
	mOldpos = newpos;

	return !mCrossedKill;
}

void SectorTrackerInst::Init(const ork::fvec3 &pos)
{
	OrkAssert(mTrack);

	mCurrSector = mTrack->FindSectorByPoint(pos);
	if(mCurrSector > -1)
		mTrackProgress = mTrack->GetProgressForPosition(mCurrSector, pos);
	mCrossedKill = false;
	mOldpos = pos;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void TrackArchetype::Describe()
{
	ork::ArrayString<64> arrstr;
	ork::MutableString mutstr(arrstr);
	mutstr.format("/arch/track");
	GetClassStatic()->SetPreferredName(arrstr);
}

void TrackArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<TrackData>();
	composer.Register<RacingLineData>();
	composer.Register<ork::ent::ModelComponentData>();
}

void TrackArchetype::DoLinkEntity(ork::ent::Simulation *inst, ork::ent::Entity *pent) const
{

}

void TrackArchetype::DoStartEntity(ork::ent::Simulation *inst, const ork::fmtx4 &world, ork::ent::Entity *pent) const
{
}

}}}
///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::bullet::SectorTrackerData, "SectorTrackerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::bullet::SectorTrackerInst, "SectorTrackerInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::bullet::TrackData, "TrackData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::bullet::TrackInst, "TrackInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::bullet::TrackArchetype, "TrackArchetype");
template class ork::chunkfile::Reader<ork::ent::bullet::CollisionLoadAllocator>;
