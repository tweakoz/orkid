////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "statedebug.h"

namespace ork{
  extern bool _ENABLE_LOGGING;
}
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

static int s_last_selected_page = 0;

void _colortext(ftxui::node_vect_t& NODES, irgb foreground, irgb background, const char* formatstring, ...) {
  using namespace ftxui;
  char out_str[512];
  va_list args;
  va_start(args, formatstring);
  vsnprintf(&out_str[0], sizeof(out_str), formatstring, args);
  va_end(args);
  NODES.push_back(
      text(out_str) | color(Color::RGB(foreground.r, foreground.g, foreground.b)) |
      bgcolor(Color::RGB(background.r, background.g, background.b)));
}
void _colortext_wrap(ftxui::node_vect_t& NODES, irgb foreground, irgb background, const char* formatstring, ...) {
  using namespace ftxui;
  char out_str[512];
  va_list args;
  va_start(args, formatstring);
  vsnprintf(&out_str[0], sizeof(out_str), formatstring, args);
  va_end(args);
  // wrap to 80 columns
  std::string str(out_str);
  while (str.length() > 0) {
    std::string wrapped = str.substr(0, 80);
    NODES.push_back(
        text(wrapped) | color(Color::RGB(foreground.r, foreground.g, foreground.b)) |
        bgcolor(Color::RGB(background.r, background.g, background.b)));
    size_t len = wrapped.length();
    // remove len from beginning of str
    str = str.substr(len, str.length() - len);
  }
}
///////////////////////////////////////////////////////////////////////////////

ftxui::component_ptr_t Wrap(std::string name, ftxui::component_ptr_t comp_in) {
  using namespace ftxui;
  return Renderer(comp_in, [name, comp_in] {
    return vbox({
               text(name) | size(WIDTH, EQUAL, 8),
               separator(),
               comp_in->Render() | xflex,
           }) |
           xflex;
  });
}

///////////////////////////////////////////////////////////////////////////////

_FtxGlDebugger::_FtxGlDebugger(const ContextGL* glctx)
    : _glctx(glctx) {
  using namespace ftxui;
}

///////////////////////////////////////////////////////////////////////////////

void _FtxGlDebugger::run_loop() {
  using namespace ftxui;
  _ENABLE_LOGGING = false;
  
  int num_shader_texts = _shader_texts.size();

  std::vector<std::string> menu_entries = {
      "BackTrace",
      "Renderer",
      "Textures",
      "TextureBindings",
      "FrameBufferState",
      "RasterState",
      "GeometryState",
      "ShaderState",
  };
#if defined(ENABLE_SSBO)
  menu_entries.push_back("StorageState");
#endif

  auto content_backtrace   = Renderer([&] { return _node_backtrace | vscroll_indicator | frame; });
  auto content_renderer    = Renderer([&] { return _node_renderer | vscroll_indicator | frame; });
  auto content_textures    = Renderer([&] { return _node_textures | vscroll_indicator | frame; });
  auto content_texstate    = Renderer([&] { return _node_texturebindingstate | vscroll_indicator | frame; });
  auto content_framebuffer = Renderer([&] { return _node_framebuffer | vscroll_indicator | frame; });
  auto content_raster      = Renderer([&] { return _node_raster | vscroll_indicator | frame; });
  auto content_geometry    = Renderer([&] { return _node_geometry | vscroll_indicator | frame; });
#if defined(ENABLE_SSBO)
  auto content_storage = Renderer([&] { return _node_ssbo | vscroll_indicator | frame; });
#endif

  auto content_shader = Renderer([&] { return _node_shader | vscroll_indicator | frame; });

  std::vector<Component> content_components;
  content_components.push_back(content_backtrace);
  content_components.push_back(content_renderer);
  content_components.push_back(content_textures);
  content_components.push_back(content_texstate);
  content_components.push_back(content_framebuffer);
  content_components.push_back(content_raster);
  content_components.push_back(content_geometry);
  content_components.push_back(content_shader);
#if defined(ENABLE_SSBO)
  content_components.push_back(content_storage);
#endif
  for (auto it : _shader_texts) {
    std::string name     = it.first;
    const auto& sh_lines = it.second;
    menu_entries.push_back(name);
    auto cview = code_viewer(sh_lines);
    content_components.push_back(cview);
  }

  int menu_selected = (s_last_selected_page >= 0 && s_last_selected_page < menu_entries.size()) //
                    ? s_last_selected_page //
                    : 0;
   
  auto content_container = Container::Tab(content_components, &menu_selected);

  auto menu = Menu(&menu_entries, &menu_selected);
  // menu      = Wrap("OrkGLD", menu);

  auto layout = Container::Vertical({menu, content_container});

  _comp_top = Renderer(layout, [&] {
    return vbox({
               menu->Render(),
               separator(),
               content_container->Render(),
           }) |
           xflex | size(WIDTH, GREATER_THAN, 40) | border;
  });

  static bool exit_process = false;
  // Add event handlers for Space and Escape keys
  _comp_top |= CatchEvent([&](Event event) {
      if (event == Event::Character(' ')) {
        if (_fxtui_screen && _fxtui_screen->Active()) {
          s_last_selected_page = menu_selected;
          _fxtui_screen->ExitLoopClosure()();
          return true;
        }
      }
      else if (event == Event::Escape) {
        if (_fxtui_screen && _fxtui_screen->Active()) {
          _fxtui_screen->ExitLoopClosure()();
          exit_process = true;
          return true;
        }
      }
      return false;
    });  
  int w           = Dimension::Full().dimx;
  int h           = Dimension::Full().dimy;
  auto fullscreen = ScreenInteractive::Dimension::Fullscreen;
  _fxtui_screen   = std::make_shared<ScreenInteractive>(w, h, fullscreen, false);

  // on spacebar, exit the loop
  
  _fxtui_screen->Loop(_comp_top);

  if (exit_process) {
    OrkAssert(false);
  }

}

/////////////////////////////////////////////////////////////////////////

void ContextGL::stateDebugger() const {

  auto debugger = _debugger.makeShared<_FtxGlDebugger>(this);

  //printf("GL StateDebugger collecting state....\n");

  /////////////////////////////
  {
    using namespace ftxui;

    std::string callstack = ork::get_backtrace();
    std::vector<std::string> lines;
    size_t pos    = 0;
    size_t length = callstack.length();
    while (pos < length) {
      size_t start = pos;
      while (pos < length && callstack[pos] != '\n')
        pos++;

      std::string line = callstack.substr(start, pos - start);

      int status      = 0;
      char* demangled = abi::__cxa_demangle(line.c_str(), 0, 0, &status);
      if (status == 0) {
        lines.push_back(demangled);
        free(demangled);
      } else {
        lines.push_back(line);
      }
      if (pos < length)
        pos++; // Include newline
    }

    node_vect_t NODES;
    for (const auto& line : lines) {
      _colortext(NODES, WHI, BLK, "%s\n", line.c_str());
    }
    debugger->_node_backtrace = vbox({
        text("BackTrace"),
        separator(),
        vbox(std::move(NODES)),
    });
  }

  /////////////////////////////

  GL_ERRORCHECK();
  debugger->_validateTextures();
  GL_ERRORCHECK();
  debugger->_validateCurrentFramebuffer();
  GL_ERRORCHECK();
  debugger->_validateCurrentGeomBuffers();
  GL_ERRORCHECK();
  debugger->_validateCurrentStorageBuffers();
  GL_ERRORCHECK();
  debugger->_validateRaster();
  GL_ERRORCHECK();
  debugger->_validateCurrentShaderProgram();
  GL_ERRORCHECK();
  debugger->_validateTextureBindingState();
  GL_ERRORCHECK();
  debugger->_validateRenderer();
  GL_ERRORCHECK();

  debugger->run_loop();
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
