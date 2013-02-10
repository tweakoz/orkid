////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef	ORK_PERFORMANCE_ANALYZER_H
#define ORK_PERFORMANCE_ANALYZER_H

#include <ork/rtti/RTTI.h>

#include <pkg/ent/entity.h>

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class PerfAnalyzerControllerData : public ent::ComponentData
{
	RttiDeclareConcrete( PerfAnalyzerControllerData, ent::ComponentData );

public:

	virtual ent::ComponentInst* CreateComponent(ent::Entity* pent) const;

	PerfAnalyzerControllerData();
	
	bool mbEnable;

};

///////////////////////////////////////////////////////////////////////////////

class PerfAnalyzerControllerInst : public ent::ComponentInst
{
	RttiDeclareAbstract( PerfAnalyzerControllerInst, ent::ComponentInst );

	const PerfAnalyzerControllerData&		mCD;

	virtual void DoUpdate(ent::SceneInst* sinst);

public:

	const PerfAnalyzerControllerData&	GetCD() const { return mCD; }

	PerfAnalyzerControllerInst( const PerfAnalyzerControllerData& cd, ork::ent::Entity* pent );

	static const int kmaxsamples = 60;
	
	float updbeg[kmaxsamples];
	float updend[kmaxsamples];
	float drwbeg[kmaxsamples];
	float drwend[kmaxsamples];

	int iupdsampleindex;
	int idrwsampleindex;

	float favgupdate;
	float favgdraw;
};

///////////////////////////////////////////////////////////

class PerformanceAnalyzerArchetype : public Archetype
{
	RttiDeclareConcrete( PerformanceAnalyzerArchetype, Archetype );

public:

	PerformanceAnalyzerArchetype();

private:
	/*virtual*/ void DoCompose(ork::ent::ArchComposer& composer);
	/*virtual*/ void DoLinkEntity(SceneInst* inst, Entity *pent) const;
	/*virtual*/ void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const;
	/*virtual*/ void DoStopEntity(SceneInst* psi, Entity *pent) const;

};

} }

#endif // ORK_REFERENCEARCHETYPE_H
