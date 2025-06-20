////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <functional>
#include <ork/kernel/concurrent_queue.h>
#include <map>
#include <vector>
#include <string>

namespace ork::lev2::ca {
struct AuContext;
///////////////////////////////////////////////////////////////////////////////
static const int kMAXFRAMESIZE = 1024;
///////////////////////////////////////////////////////////////////////////////
struct Track;
struct Layer;

struct Fragment
{
	Fragment();
	float ComputePower() const;
	void Clear();
	void Sum( const Fragment& from, float level );
	int mNumFrames;
	int mChannel;
	int mFrameAccum;
	float mSampleData[kMAXFRAMESIZE];
	const Track* mTrack;
};

struct Track
{
	Track() : mAmplitude(1.0f), mPan(0.0f) {}

	std::vector<Fragment*> mFragments;
	float mAmplitude;
	float mPan;
};

///////////////////////////////////////////////////////////////////////////////
struct StereoFragment
{
	StereoFragment() ;
	void Init(int numfr);
	void Clear();
	Fragment mMixLeft;
	Fragment mMixRight;
	int mNumUsed;
	int mNumFrames;
};
///////////////////////////////////////////////////////////////////////////////
struct LayerFragment
{
	LayerFragment();
	~LayerFragment();

	void Init(int inumchannels, int inumfr);
	void Copy( const LayerFragment* from );
	void MixDown(StereoFragment*dest) const;
	
	int mNumChannels = 0;
	int mNumFrames = 0;
	int mFrameAccum = 0;
	Fragment* mChannels = nullptr;
	Layer* mLayer = nullptr;

};
///////////////////////////////////////////////////////////////////////////////
struct Layer
{
	Layer();
	~Layer();
	void Init(int defsize);
	const LayerFragment* GetFragment(int idx) const;
	LayerFragment* GetFragment(int idx);
	void SetFragment(int idx,LayerFragment*f);
	void AppendFragment(LayerFragment*f);
	size_t GetNumFragments() const { return mFragments.size(); }
	Layer* Double() const;
	Layer* Clone() const;
	void ClearFragments();
	void ResizeFragments(int count);
	void MakeTexture();
	void Trackify();

	uint32_t mTextureObject = 0;
	bool mEnabled = true;
	bool mSolo = false;
	std::string mName;
	std::vector<Track*> mTracks;
	float mAmplitude = 1.0f;;
	int mTimeShift = 0;

private:
	std::vector<LayerFragment*> mFragments;

};
///////////////////////////////////////////////////////////////////////////////
struct Pattern
{
	Pattern();
	~Pattern();

	int barlength() const;

	Pattern* Double() const;
	Pattern* Clone() const;
	float mTempo;
	int mPatternLength;
	int mNumBars;
	Layer* NewLayer();

	const std::string& GetName() const { return mName; }
	void SetName(std::string n) { mName=n; }
	const Layer* GetLayer(int i) const
	{ 
		if( i >= mLayers.size() ) return nullptr;
		return mLayers[i];
	}
	Layer* GetLayer(int idx) {
		if( idx >= mLayers.size() ) return nullptr;
		return mLayers[idx];
	}
	void AddLayer(Layer*l) { mLayers.push_back(l); }
	void RemoveLastLayer();
	int GetNumLayers() const { return mLayers.size(); }
	
private:
	std::string mName;
	std::vector<Layer*> mLayers;
};
struct PatternPlayback
{
	PatternPlayback(const Pattern* p);
	
	int GetCurrentSegment() const;
	int IncrementSegment();
	void StartPattern(const Pattern* p);
	//
	const Pattern* pat;
	const Pattern* the_pattern;

private:
	int mSegIdx = 0;

};
///////////////////////////////////////////////////////////////////////////////
struct Library
{
	Pattern* NewPattern(int numfrags);
	std::string GenPatternName();
	std::map<std::string,Pattern*> mPatterns;
};
///////////////////////////////////////////////////////////////////////////////
struct Arrangement
{
	Arrangement();
	void Load(const std::string& fname );
	void Save(const std::string& fname );

	Pattern* NewPattern(std::string nam="");
	void ReplacePattern(Pattern*opat,Pattern*npat);
	Pattern* ClonePattern(const Pattern*);
	Pattern* GetCurrentPattern();
	const Pattern* GetCurrentPattern() const;
	void IncrementPattern();
	int GetNumPatterns() const { return mPatternSequence.size(); }
	Pattern* GetPattern( int idx );
	const Pattern* GetPattern( int idx ) const;
	void SetCurrentPattern(int idx);
	void QueNextPattern(int i);
	void TogglePatternLock();
	bool IsPatternLocked() const { return mPatternLock; }
	std::string GenPatternName();
	int GetNextPattern() const { return mPatternNext; }

private:
	std::map<std::string,Pattern*> mPatterns;
	std::vector<Pattern*> mPatternSequence;
	int mPatternNext = -1;
	int mCurPatIdx = -1;
	int mPatSerial = -1;
	bool mPatternLock = false;
};
///////////////////////////////////////////////////////////////////////////////
template <typename T>
struct ObjectPool
{
	ObjectPool(int max);
	~ObjectPool();

	MpMcBoundedQueue<T*,4096> mObjectPool;
	T* AllocObject();
	void ReturnObject(T*);

	ork::atomic<int> mNumObjectsAllocated;
	const int mMaxObjects;
	ork::atomic<int> mNumObjectsProcessed;
	ork::atomic<int> mNumObjectsOut;
	bool mGoingDown = false;

	typedef std::function<void(int)> usage_cb_t;
	usage_cb_t mUsageCb = nullptr;

};
///////////////////////////////////////////////////////////////////////////////
typedef ObjectPool<Fragment> FragmentPool;
typedef ObjectPool<StereoFragment> StereoFragmentPool;
typedef ObjectPool<LayerFragment> LayerFragmentPool;
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ca {
