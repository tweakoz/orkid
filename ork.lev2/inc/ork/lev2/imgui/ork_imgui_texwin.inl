#pragma once 

#include <ork/lev2/imgui/imgui.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
#include <ork/lev2/imgui/imgui_impl_opengl3.h>
#include <ork/lev2/imgui/ImGuizmo.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

struct ImGuiTexturedWindow {

  using on_win_size_op_t = std::function<void(int w, int h)>;

  ////////////////////////////////////////////////////////

  ImGuiTexturedWindow(std::string name, bool flipy = false, bool flipx = false)
    : _name(name)
    , _flipy(flipy) 
    , _flipx(flipx) {
    _onsizeop = [](int, int) {};
  }

  ////////////////////////////////////////////////////////

  ~ImGuiTexturedWindow() {
  }

  ////////////////////////////////////////////////////////

  ui::event_ptr_t tryEvent(ui::event_constptr_t src_ev) {

    int based_x       = src_ev->miX - _vpx;
    int based_y       = src_ev->miY - _vpy;
    bool vp3d_range_h = (based_x > 0) and (based_x < _vpw);
    bool vp3d_range_v = (based_y > 0) and (based_y < _vph);

    if (vp3d_range_h and vp3d_range_v) {
      auto hacked_event                = std::make_shared<ui::Event>();
      *hacked_event                    = *src_ev;
      const ui::EventCooked& srcfiltev = src_ev->mFilteredEvent;
      ui::EventCooked& dstfiltev       = hacked_event->mFilteredEvent;
      dstfiltev.miX                    = based_x;
      dstfiltev.miY                    = based_y;
      hacked_event->_vpdim.x           = _vpw;
      hacked_event->_vpdim.y           = _vph;
      return hacked_event;
    }
    return nullptr;
  }

  ////////////////////////////////////////////////////////

  void render(texture_ptr_t texture) {
    if (texture) {
      ImGuiIO& io    = ImGui::GetIO();
      ImVec2 origpos = *io.MouseClickedPos;
      ImGui::Begin(_name.c_str());
      auto win       = ImGui::GetCurrentWindow();
      auto dlist     = win->DrawList;
      auto ori       = win->DC.CursorPos;
      auto wpos      = ImGui::GetWindowPos();
      auto wsiz      = ImGui::GetWindowSize();
      bool in_window = origpos.x >= wpos.x and origpos.x < wpos.x + wsiz.x;
      in_window &= origpos.y >= wpos.y and origpos.y < wpos.y + wsiz.y;

      _vpx = wpos.x;
      _vpy = wpos.y;
      _vpw = wsiz.x;
      _vph = wsiz.y;

      _onsizeop(wsiz.x, wsiz.y);

      auto pa = ori;
      auto pb = ori + ImVec2(wsiz.x, 0);
      auto pc = ori + ImVec2(wsiz.x, wsiz.y);
      auto pd = ori + ImVec2(0, wsiz.y);
      ////////////////////////////////////////////////
      if (auto as_texid = texture->_vars->typedValueForKey<GLuint>("gltexobj")) {
        dlist->AddImageQuad(
          (void*)uint64_t(as_texid.value()), // GL texture handle
          pb,
          pa,
          pd,
          pc,
          ImVec2(_flipx ? 0 : 1, _flipy ? 1 : 0), // uva,
          ImVec2(_flipx ? 1 : 0, _flipy ? 1 : 0), // uvb,
          ImVec2(_flipx ? 1 : 0, _flipy ? 0 : 1), // uvc,
          ImVec2(_flipx ? 0 : 1, _flipy ? 0 : 1), // uvd,
          0xffffffff);
      }
      ImGui::End();
    }
  }
  ////////////////////////////////////////////////////////

  std::string _name;
  int _vpx = 0;
  int _vpy = 0;
  int _vpw = 1;
  int _vph = 1;
  bool _flipy;
  bool _flipx;
  on_win_size_op_t _onsizeop;
};


} // ork::lev2::imgui