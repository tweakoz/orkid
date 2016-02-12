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

	ent::ComponentInst* DoCreateComponent(ent::Entity* pent) const final;

public:


	PerfAnalyzerControllerData();
	
	bool mbEnable;

};

///////////////////////////////////////////////////////////////////////////////

class PerfAnalyzerControllerInst : public ent::ComponentInst
{
	RttiDeclareAbstract( PerfAnalyzerControllerInst, ent::ComponentInst );

	const PerfAnalyzerControllerData&		mCD;

	void DoUpdate(ent::SceneInst* sinst) final;

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
	void DoCompose(ork::ent::ArchComposer& composer) final;
	void DoLinkEntity(SceneInst* inst, Entity *pent) const final;
	void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const final;
	void DoStopEntity(SceneInst* psi, Entity *pent) const final;

};

} }

#endif // ORK_REFERENCEARCHETYPE_H
