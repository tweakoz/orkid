////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/ui/widget.h>
#include <ork/lev2/gfx/util/grid.h>

namespace ork::ui {

///////////////////////////////////////////////////////////////////////////////
struct GraphPanel {
  void setRect(int iX, int iY, int iW, int iH, bool snap = false);
  ui::anchor::layout_ptr_t _panelLayout;
  ui::layoutgroup_ptr_t _layoutgroup;
  ui::panel_ptr_t _uipanel;
  ui::surface_ptr_t _uisurface;
};
///////////////////////////////////////////////////////////////////////////////
struct GraphChannel {
  using range_lambda_t               = std::function<fvec2()>;
  using count_lambda_t               = std::function<size_t()>;
  using indexed_point_lambda_t       = std::function<fvec2(size_t)>;
  range_lambda_t _getVerticalRange   = nullptr;
  range_lambda_t _getHorizontalRange = nullptr;
  count_lambda_t _getCount           = nullptr;
  indexed_point_lambda_t _getPoint   = nullptr;
  std::string _name;
  fvec3 _color;
  bool _visible = true;
};
using graphchannel_ptr_t = std::shared_ptr<GraphChannel>;
///////////////////////////////////////////////////////////////////////////////
struct GraphView : public ui::Surface {
  GraphView();
  void DoRePaintSurface(ui::drawevent_constptr_t drwev) override;
  void _doGpuInit(lev2::Context* pt) override;
  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t EV) override;

  graphchannel_ptr_t channel(std::string named);

  std::vector<graphchannel_ptr_t> _channelmap;
  lev2::Grid2d _grid;
  fvec2 _downPos;
  fvec2 _downCenter;
  bool _lockX;
  bool _lockY;
  bool _lockYZOOM;
  bool _dragging;
};
using graphview_ptr_t = std::shared_ptr<GraphView>;

} // namespace ork::ui
