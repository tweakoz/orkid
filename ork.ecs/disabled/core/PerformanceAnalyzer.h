////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTI.h>

#include <pkg/ent/entity.h>

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class PerfAnalyzerControllerData : public ent::ComponentData
{
	RttiDeclareConcrete( PerfAnalyzerControllerData, ent::ComponentData );

public:

	ent::ComponentInst* createComponent(ent::Entity* pent) const final;

	PerfAnalyzerControllerData();
	
	bool mbEnable;

};

///////////////////////////////////////////////////////////////////////////////

class PerfAnalyzerControllerInst : public ent::ComponentInst
{
	RttiDeclareAbstract( PerfAnalyzerControllerInst, ent::ComponentInst );

	const PerfAnalyzerControllerData&		mCD;

	void DoUpdate(ent::Simulation* sinst) final;

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
	void DoLinkEntity(Simulation* inst, Entity *pent) const final;
	void DoStartEntity(Simulation* psi, const fmtx4 &world, Entity *pent ) const final;
	void DoStopEntity(Simulation* psi, Entity *pent) const final;

};

} }

