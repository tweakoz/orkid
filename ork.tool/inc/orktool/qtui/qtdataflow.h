////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_TOOL_QTDATAFLOW_H 
#define _ORK_TOOL_QTDATAFLOW_H 
///////////////////////////////////////////////////////////////////////////////
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>
#include <orktool/qtui/gfxbuffer.h>
#include <ork/lev2/gfx/util/grid.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

namespace ged { class ObjModel; }

class GraphVP;

class DataFlowEditor : public ork::Object
{
	RttiDeclareAbstract( DataFlowEditor, ork::Object );

public:

	GraphVP*							mGraphVP;	
	std::stack<ork::dataflow::graph*>	mGraphStack;
	ork::dataflow::dgmodule*			mpSelModule;
	ork::dataflow::dgmodule*			mpProbeModule;

	DataFlowEditor();
	void Attach( ork::dataflow::graph* pgrf );
	void Push( ork::dataflow::graph* pgrf );
	void Pop();
	ork::dataflow::graph* GetTopGraph();
	size_t StackDepth() const { return mGraphStack.size(); }

	void SelModule( ork::dataflow::dgmodule* pmod ) { mpSelModule=pmod; }
	ork::dataflow::dgmodule* GetSelModule() const { return mpSelModule; }

	void SetProbeModule( ork::dataflow::dgmodule* pmod ) { mpProbeModule=pmod; }
	ork::dataflow::dgmodule* GetProbeModule() const { return mpProbeModule; }

	void SlotClear();

};

class GraphVP : public lev2::CUIViewport
{
	static const int kvppickdimw = 512;

	virtual lev2::EUIHandled UIEventHandler( lev2::CUIEvent *pEV );

	lev2::CPickBuffer<GraphVP>*					mpPickBuffer;
	lev2::Grid2d								mGrid;
	ork::lev2::Texture*							mpArrowTex;
	ged::ObjModel&								mObjectModel;
	DataFlowEditor&								mDflowEditor;
	ork::dataflow::graph* GetTopGraph();
	ork::lev2::GfxMaterial3DSolid				mGridMaterial; 

public:
	
	void ReCenter();

	void draw_connections( ork::lev2::GfxTarget* pTARG );

	void DoDraw( /*ork::lev2::GfxTarget* pTARG*/ ); // virtual 

	void GetPixel( int ix, int iy, lev2::GetPixelContext& ctx );
	GraphVP( DataFlowEditor& dfed, tool::ged::ObjModel& objmdl, const std::string & name );

	DataFlowEditor& GetDataFlowEditor() { return mDflowEditor; }
};

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
#endif // _ORK_TOOL_QTCONSOLE_H
