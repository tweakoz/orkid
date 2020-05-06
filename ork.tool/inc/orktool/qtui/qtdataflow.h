////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>
#include <ork/lev2/gfx/util/grid.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/pickbuffer.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

namespace ged {
class ObjModel;
}

class GraphVP;

class DataFlowEditor : public ork::AutoConnector {
  RttiDeclareAbstract(DataFlowEditor, ork::AutoConnector);

public:
  GraphVP* mGraphVP;
  std::stack<ork::dataflow::graph_data*> mGraphStack;
  ork::dataflow::dgmodule* mpSelModule;
  ork::dataflow::dgmodule* mpProbeModule;

  DataFlowEditor();
  void Attach(ork::dataflow::graph_data* pgrf);
  void Push(ork::dataflow::graph_data* pgrf);
  void Pop();
  ork::dataflow::graph_data* GetTopGraph();
  size_t StackDepth() const {
    return mGraphStack.size();
  }

  void SelModule(ork::dataflow::dgmodule* pmod) {
    mpSelModule = pmod;
  }
  ork::dataflow::dgmodule* GetSelModule() const {
    return mpSelModule;
  }

  void SetProbeModule(ork::dataflow::dgmodule* pmod) {
    mpProbeModule = pmod;
  }
  ork::dataflow::dgmodule* GetProbeModule() const {
    return mpProbeModule;
  }

  void SlotClear();
  void SlotModelInvalidated();

  // DeclarePublicAutoSlot( RelayModelInvalidated );
  DeclarePublicAutoSlot(ModelInvalidated);
};

struct GraphVP : public ui::Surface {
  GraphVP(DataFlowEditor& dfed, tool::ged::ObjModel& objmdl, const std::string& name);

  void ReCenter();

private:
  static const int kvppickdimw = 512;

  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;
  void DoRePaintSurface(ui::drawevent_ptr_t drwev) override;
  void DoInit(lev2::Context* pt) override;

  DataFlowEditor& GetDataFlowEditor() {
    return mDflowEditor;
  }
  void draw_connections(ork::lev2::Context* pTARG);
  // void GetPixel( int ix, int iy, lev2::PixelFetchContext& ctx );

  lev2::Grid2d mGrid;
  ork::lev2::Texture* mpArrowTex;
  ged::ObjModel& mObjectModel;
  DataFlowEditor& mDflowEditor;
  ork::dataflow::graph_data* GetTopGraph();
  ork::lev2::GfxMaterial3DSolid mGridMaterial;
  ork::lev2::FreestyleMaterial _nodematerial;
};

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::tool
///////////////////////////////////////////////////////////////////////////////
