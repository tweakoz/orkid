////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#ifndef _ORK_MEM_VRAMMANAGER_H
#define _ORK_MEM_VRAMMANAGER_H

#include <ork/mem/zonemanager.h>
#include <ork/mem/framezone.h>
#include <ork/mem/buddyzone.h>
#include <ork/mem/poolzone.h>

namespace ork {

class VRAMManager
{
	static VRAMManager* gvrammanager;

	//static const int kAllocationOffset = 1024;
	CFrameZone mFrameZone;
	BuddyZone mBuddyZone;
	char mBuddyZoneNodeData[8 << 10];
	PoolZone mBuddyZoneNodeZone;
	TZoneManager<PoolZone> mVRAMZoneManager;
	bool mDynamicMode;

public:
 	/// returned by Allocate if allocation failed.
	static const u32 kOOMF = u32(0xffffffff);

	VRAMManager(void* base, void* end)
		: mFrameZone(base, IZone::Difference(base,end))
		, mBuddyZoneNodeZone(mBuddyZoneNodeData, sizeof(mBuddyZoneNodeData), BuddyZone::BuddyNodeSize() )
		, mVRAMZoneManager(&mBuddyZoneNodeZone)
		//, mAlignment(alignment)
		, mBuddyZone()
		, mDynamicMode(false)
	{
		orkprintf( "VRAMManager(%08x,%08x)\n", base, end );

		gvrammanager = this;
	}

	void* Allocate(u32 size, int ialignment, bool temporary);
	bool Deallocate(void* address);

	void Clear();
	void ClearTemporary();

	bool CheckDynamicMode() { return mDynamicMode; }

	void PushState();
	void PopState();
	size_t GetFrameAvailSize();

	size_t StartDynamicMode();
	void EndDynamicMode();

	void Profile()
	{
		void *base;
		size_t size;
		mFrameZone.GetFreeRegion(base, size);

		orkprintf("%d bytes in FrameZone : %d free\n", u32(mFrameZone.GetAllocationBase()) - u32(mFrameZone.GetBase()), size);

		if(mDynamicMode)
		{
			mBuddyZone.Profile();
		}
	}

	bool ResetOK()
	{
		void *base;
		size_t size;
		mFrameZone.GetFreeRegion(base, size);

		return !mDynamicMode && size == mFrameZone.GetSize();
	}

	static VRAMManager* GetRef() { return gvrammanager; }

	CFrameZone& GetFrameZone() { return mFrameZone; }
};

}

#endif // _ORK_MEM_VRAMMANAGER_H
