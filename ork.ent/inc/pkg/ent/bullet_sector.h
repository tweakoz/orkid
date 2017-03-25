////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/plane.h>
#include <ork/math/cvector3.h>
#include <ork/file/chunkfile.h>
#include <pkg/ent/entity.h>

class btBvhTriangleMeshShape;

namespace ork { namespace lev2 {
class XgmAnimAsset;
} }

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent { namespace bullet {
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

typedef enum
{
	COLLISIONFLAG_WALL = (1<<0),
	COLLISIONFLAG_SLOW = (1<<1),
	COLLISIONFLAG_SLIPPERY = (1<<2),
} CollisionFlags;

typedef enum
{
	SECTORFLAG_START = (1<<0),
	SECTORFLAG_SPLITH = (1<<1),
	SECTORFLAG_SPLITV = (1<<2),
} SectorFlags;

typedef enum
{// left/right are from the perspective of inside the sector
	PORTAL_CORNER_BL,
	PORTAL_CORNER_TL,
	PORTAL_CORNER_TR,
	PORTAL_CORNER_BR,
	NUM_PORTAL_CORNERS
} SectorPortalCorners;

///////////////////////////////////////////////////////////////////////////////

class Track;
struct SectorData;
struct SectorVert;
struct MidlineVert;

struct SectorPortal {
	int mCornerVerts[NUM_PORTAL_CORNERS];
	CPlane mPlane; // points inside sector will be in front of this one
	float mTrackProgress; // 0 = just after starting line, 1 = just before starting line
	int mNeighbor; // sector on other side

	ork::CVector3 GetCenter(orkvector<SectorVert> boundsVerts) const;
	void ContributeBasis(bool reverse, SectorData &data);

	SectorPortal() :
		mPlane(0,0,0,-1),
		mTrackProgress(-1),
		mNeighbor(-1)
	{
		for(int i=0 ; i<NUM_PORTAL_CORNERS ; i++)
			mCornerVerts[i] = -1;
	}
	~SectorPortal()
	{
		mNeighbor = 0;
	}
};

///////////////////////////////////////////////////////////////////////////////

struct Sector
{
	static const int kmaxportals = 3;

	Sector();
	~Sector()
	{
		mFlags = 0;
	}

	bool ContainsPoint(const SectorData &data, const ork::CVector3& position) const;
	void Transform(const CMatrix4& transform);

	float ContributeBasis(SectorData &data);

	void GetRelativePositionOfPoint(const SectorData &data, const ork::CVector3& position, float &x, float &y, float &z) const ;

	float GetProgressForPosition(const SectorData &data, const ork::CVector3& position) const;
	void GetBasis(const SectorData& data, const ork::CVector3& position, CVector3& dirX, CVector3& dirY, CVector3& dirZ) const;
	bool GetMidline(const SectorData& data, const ork::CVector3& position, MidlineVert& midline) const;
	bool CrossedKill(const SectorData& data, const ork::CVector3& oldpos, const ork::CVector3& newpos) const;

	bool IsStart() const { return mFlags & SECTORFLAG_START; }
	void FlagStart() { mFlags |= SECTORFLAG_START; }
	void UnflagStart() { mFlags &= ~SECTORFLAG_START; }
	bool IsSplit() const {return mFlags & (SECTORFLAG_SPLITV|SECTORFLAG_SPLITH);}
	int NumPortals() const {return IsSplit()?3:2;}

	// number of portal slots used is determined by flags
	SectorPortal mPortals[kmaxportals];
	CPlane mSplitPlane;


	float Length(orkvector<SectorVert> mBoundsVerts) const;

/*	CVector3 mDirX;
	CVector3 mDirY;
	CVector3 mDirZ;
	CVector3 mCenter;

	CVector3 mUBasis;
	CVector3 mVBasis;
	CVector3 mUVNormal;
	CVector3 mOrigin;*/

	U32 mFlags;
//	float mTrackPosition;

	int mMidlineTriCount; // number of tris
	int mMidlineTriStart; // index in mMidlineVertTable 

	int mKillTriCount; // number of tris
	int mKillTriStart; // index/3 in mKillTris, index in mKillPlanes
};

///////////////////////////////////////////////////////////////////////////////

struct SectorVert {
	// x basis vector would never be used anyway
	CVector3 mPos, mY, mZ;

	SectorVert() :
		mY(0,0,0),
		mZ(0,0,0)
	{}

	SectorVert(const CVector3 &pos) :
		mPos(pos),
		mY(0,0,0),
		mZ(0,0,0)
	{}

	~SectorVert()
	{
		mPos = ork::CVector3::Black();
	}

};

///////////////////////////////////////////////////////////////////////////////

struct MidlineVert {
	CVector3 mPos;
	float mTubeGrav;
	float mTotalGrav;
	float mVelocityFollow;

	MidlineVert() {}

	MidlineVert(const CVector3 &pos, float tube, float total, float velfollow) :
		mPos(pos),
		mTubeGrav(tube),
		mTotalGrav(total),
		mVelocityFollow(velfollow)
	{}
	~MidlineVert()
	{
		mPos = ork::CVector3::Black();
	}
};

///////////////////////////////////////////////////////////////////////////////

struct SectorData {
	orkvector<Sector> mSectors;
	orkvector<SectorVert> mSectorVerts;
	orkvector<int> mMidlineTris;
	orkvector<MidlineVert> mMidlineVerts;
	orkvector<int> mKillTris;
	orkvector<CPlane> mKillPlanes;
	orkvector<CVector3> mKillVerts;
	float mLength;
	~SectorData()
	{
		mLength = 0.0f;
	}
};

///////////////////////////////////////////////////////////////////////////////

struct TrackSaveData : public SectorData
{
	orkvector<CVector3> mTrackVerts;
	orkvector<int> mTrackIndices;
	orkvector<int> mTrackFlags;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class TriangleIndexVertexBuffer;

class Track
{
public:
	Track() : mTrackShape(NULL), miStartIndex(-1), mTIVB(0) { }

	~Track();

	bool Load(const ork::file::Path& path, const ork::CMatrix4& transform);
	static bool Save(const ork::file::Path& path, const TrackSaveData& data);

	int FindSectorByPoint(const ork::CVector3& point) const;

	btBvhTriangleMeshShape* GetTrackShape() const { return mTrackShape; }

	void GetBasis(unsigned int currSector, const ork::CVector3& pos, CVector3& dirX, CVector3& dirY, CVector3& dirZ) const;
	bool GetMidline(unsigned int currSector, const ork::CVector3& position, MidlineVert& midline) const;
	bool CrossedKill(unsigned int currSector, const ork::CVector3& oldpos, const ork::CVector3& newpos) const;

	int CheckTraversal(unsigned int currSector, const ork::CVector3& pos, bool allowInvalid = false) const;
	float GetProgressForPosition(unsigned int currSector, const ork::CVector3& pos) const	;
	
	CVector3 GetSectorEntry(int sector) const;

	//DEBUG
	int GetNumSectors() const { return mSectorData.mSectors.size(); }
	const Sector& RefSector(int sector) const { return mSectorData.mSectors[sector]; }

	int GetStartIndex() const { return miStartIndex; }

	int GetTriFlags(int tri) const {return mTriangleFlags[tri];}

	float GetLength() const {return mSectorData.mLength;}

private:

	btBvhTriangleMeshShape*		mTrackShape;
	TriangleIndexVertexBuffer*	mTIVB;
	SectorData mSectorData;
	int miStartIndex;

	orkvector<int> mTriangleFlags;
};

///////////////////////////////////////////////////////////////////////////////

class SectorRange : public ork::Object
{
	RttiDeclareConcrete(SectorRange, ork::Object)
public:
	SectorRange(float begin = 0.0f, float end = 0.1f) : mBegin(begin), mEnd(end) {}

	float GetBegin() const { return mBegin; }
	void SetBegin(float begin) { mBegin = begin; }

	float GetEnd() const { return mEnd; }
	void SetEnd(float end) { mEnd = end; }
private:
	float mBegin;
	float mEnd;
};

class TrackData : public ork::ent::ComponentData
{
	RttiDeclareConcrete(TrackData, ork::ent::ComponentData)

public:

	TrackData();
	~TrackData();

	const ork::file::Path& GetSecMeshName() const { return mSecMeshName; }
	float GetTrackScale() const { return mTrackScale; }
	ork::lev2::XgmAnimAsset *GetIntroCameraAnimation() const { return mIntroCameraAnimation; }

	const ork::orklut<ork::PoolString, ork::ent::bullet::SectorRange *> &GetNoRespawnRanges() const { return mNoRespawnRanges; }
	ork::orklut<ork::PoolString, ork::ent::bullet::SectorRange *> &GetNoRespawnRanges() { return mNoRespawnRanges; }

	float GetRespawnProgress(float actual_progress) const;
private:
	
	ork::ent::ComponentInst *CreateComponent(ork::ent::Entity *pent) const final;

	ork::file::Path				mSecMeshName;
	float						mTrackScale;
	ork::lev2::XgmAnimAsset*	mIntroCameraAnimation;

	ork::orklut<ork::PoolString, ork::ent::bullet::SectorRange *> mNoRespawnRanges;
};

class TrackInst : public ork::ent::ComponentInst
{
	RttiDeclareAbstract(TrackInst, ork::ent::ComponentInst)

	const TrackData&			mData;
	Track						mTrack;

public:

	TrackInst(const TrackData &data, ork::ent::Entity *pent);
	~TrackInst();

	const TrackData &GetData() const { return mData; }

	const Track& GetTrack() const { return mTrack; }

protected:
	bool DoLink(ork::ent::SceneInst *inst) final;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// SectorTracker
//  is a component which can track its location on the track. (yeah track is an overloaded term)
//   call Init() to set up it's initial position (at entity startup, or maybe at other times)
//   call TrackPos() to update it's position
//   it will "link" itself to the "TrackInst" component of the entity named "track"
//   currently this component is used by ships and missiles
//   it should be used by anything that needs to track its location on a sector based track.
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class SectorTrackerData : public ork::ent::ComponentData
{
	RttiDeclareConcrete(SectorTrackerData, ork::ent::ComponentData)

public:

	SectorTrackerData();
	~SectorTrackerData();

private:

	ork::ent::ComponentInst *CreateComponent(ork::ent::Entity *pent) const final;

};

///////////////////////////////////////////////////////////////////////////////

class SectorTrackerInst : public ork::ent::ComponentInst
{
	RttiDeclareAbstract(SectorTrackerInst, ork::ent::ComponentInst)

public:

	SectorTrackerInst(const SectorTrackerData &data, ork::ent::Entity *pent);
	~SectorTrackerInst();

	const SectorTrackerData &GetData() const { return mData; }

	////////////////////////////////////////
	bool UpdatePos( const ork::CVector3& newpos, ork::CVector3 &newvel );
	////////////////////////////////////////

	bool IsValid() const { return mCurrSector>=0; }

	int						GetCurrentSector() const { return mCurrSector; }
	const ork::CVector3&	GetBasisX() const { return mBasisX; }
	const ork::CVector3&	GetBasisY() const { return mBasisY; }
	const ork::CVector3&	GetBasisZ() const { return mBasisZ; }
	float					GetGravity() const { return mMidlineVert.mTotalGrav; }

	float					GetTrackProgress() const { return mTrackProgress; }

	void Init( const ork::CVector3& pos );

	const Track *GetTrack() const { return mTrack; }

	bool IsDisallowCheating() const { return mDisallowCheating; }
	void SetDisallowCheating(bool disallow) { mDisallowCheating = disallow; }

	const ork::CVector3 &GetOldPos() const { return mOldpos; }
private:

	bool DoStart(ork::ent::SceneInst *inst, const ork::CMatrix4 &world) final;
	bool DoLink(ork::ent::SceneInst *inst) final;
	bool DoNotify(const ork::event::Event *event) final;

	const SectorTrackerData&		mData;
	const Track*					mTrack;
	int								mCurrSector;
	float							mTrackProgress;
	ork::CVector3					mBasisX;
	ork::CVector3					mBasisY;
	ork::CVector3					mBasisZ;
	ork::CVector3					mOldpos;
	MidlineVert						mMidlineVert;
	bool							mCrossedKill;
	bool							mDisallowCheating;
};

// Class: TrackArchetype
// Use for the "track" entities.
class TrackArchetype : public ork::ent::Archetype
{
	RttiDeclareConcrete(TrackArchetype, ork::ent::Archetype);

private:

	void DoCompose(ork::ent::ArchComposer& composer) final;
	void DoLinkEntity(ork::ent::SceneInst *inst, ork::ent::Entity *pent) const final;
	void DoStartEntity(ork::ent::SceneInst *inst, const ork::CMatrix4 &world, ork::ent::Entity *pent) const final;

};

///////////////////////////////////////////////////////////////////////////////
}}}
///////////////////////////////////////////////////////////////////////////////

