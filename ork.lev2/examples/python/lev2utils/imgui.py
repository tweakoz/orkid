################################################################################
# lev2 PyImGui helpers
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

from imgui_bundle import imgui, hello_imgui
from imgui_bundle.python_backends.opengl_backend import ProgrammablePipelineRenderer
import glfw, json
from obt import path as obt_path
from orkengine.core import CrcStringProxy
from orkengine import lev2

################################################################################
tokens = CrcStringProxy()
################################################################################

def installImguiOnApp(app):
  assert(hasattr(app,"ezapp"))
  assert(hasattr(app,"onOverlayUiEvent"))
  assert(hasattr(app,"onGpuPostFrame"))
  assert(hasattr(app,"onGpuInit"))
  assert(hasattr(app,"onExit"))
  assert(hasattr(app,"newAppState"))
  assert(hasattr(app,"app_vars"))
  UIOVERLAY = lev2.EzUiEventInterceptor()
  UIOVERLAY.onUiEvent = lambda uievent : app.onOverlayUiEvent(uievent)
  app.ezapp.uicontext.overlayWidget = UIOVERLAY

################################################################################

class ImGuiWrapper:

  ####################################
  def __init__(self, ezapp, appID, docking=True ):
    self.appID = appID
    self.imgui_settings_file = obt_path.temp()/("_imgui_settings_%s.ini" % self.appID)
    self.app_settings_file = obt_path.temp()/("_app_settings_%s.json" % self.appID)
    self.ezapp = ezapp
    self.uicontext = ezapp.uicontext
    self.topwidget = ezapp.topWidget
    ###################################
    # key remap table
    ###################################
    self.GLFW_TO_IMGUI_KEY_MAP = {
      glfw.KEY_DELETE: imgui.Key.delete,
      glfw.KEY_BACKSPACE: imgui.Key.backspace,
      glfw.KEY_ENTER: imgui.Key.enter,
      glfw.KEY_TAB: imgui.Key.tab,
      glfw.KEY_LEFT: imgui.Key.left_arrow,
      glfw.KEY_RIGHT: imgui.Key.right_arrow,
      glfw.KEY_UP: imgui.Key.up_arrow,
      glfw.KEY_DOWN: imgui.Key.down_arrow,
      glfw.KEY_ESCAPE: imgui.Key.escape,
      # numerics
      glfw.KEY_0: imgui.Key._0,
      glfw.KEY_1: imgui.Key._1,
      glfw.KEY_2: imgui.Key._2,
      glfw.KEY_3: imgui.Key._3,
      glfw.KEY_4: imgui.Key._4,
      glfw.KEY_5: imgui.Key._5,
      glfw.KEY_6: imgui.Key._6,
      glfw.KEY_7: imgui.Key._7,
      glfw.KEY_8: imgui.Key._8,
      glfw.KEY_9: imgui.Key._9,
      # alphabetic
      glfw.KEY_A: imgui.Key.a,
      glfw.KEY_B: imgui.Key.b,
      glfw.KEY_C: imgui.Key.c,
      glfw.KEY_D: imgui.Key.d,
      glfw.KEY_E: imgui.Key.e,
      glfw.KEY_F: imgui.Key.f,
      glfw.KEY_G: imgui.Key.g,
      glfw.KEY_H: imgui.Key.h,
      glfw.KEY_I: imgui.Key.i,
      glfw.KEY_J: imgui.Key.j,
      glfw.KEY_K: imgui.Key.k,
      glfw.KEY_L: imgui.Key.l,
      glfw.KEY_M: imgui.Key.m,
      glfw.KEY_N: imgui.Key.n,
      glfw.KEY_O: imgui.Key.o,
      glfw.KEY_P: imgui.Key.p,
      glfw.KEY_Q: imgui.Key.q,
      glfw.KEY_R: imgui.Key.r,
      glfw.KEY_S: imgui.Key.s,
      glfw.KEY_T: imgui.Key.t,
      glfw.KEY_U: imgui.Key.u,
      glfw.KEY_V: imgui.Key.v,
      glfw.KEY_W: imgui.Key.w,
      glfw.KEY_X: imgui.Key.x,
      glfw.KEY_Y: imgui.Key.y,
      glfw.KEY_Z: imgui.Key.z,
      # punctuation
      glfw.KEY_GRAVE_ACCENT: imgui.Key.grave_accent,
      glfw.KEY_MINUS: imgui.Key.minus,
      glfw.KEY_EQUAL: imgui.Key.equal,
      glfw.KEY_LEFT_BRACKET: imgui.Key.left_bracket,
      glfw.KEY_RIGHT_BRACKET: imgui.Key.right_bracket,
      glfw.KEY_BACKSLASH: imgui.Key.backslash,
      glfw.KEY_SEMICOLON: imgui.Key.semicolon,
      glfw.KEY_APOSTROPHE: imgui.Key.apostrophe,
      glfw.KEY_COMMA: imgui.Key.comma,
      glfw.KEY_PERIOD: imgui.Key.period,
      glfw.KEY_SLASH: imgui.Key.slash,
      # Add more key mappings as needed
    }

    # fill in alphanumeric keys for both cases
    #for i in range(ord('A'), ord('Z')+1):
    #  self.GLFW_TO_IMGUI_KEY_MAP[ord('A')] = imgui.Key.a + i - ord('A')
    for i in range(ord('A'), ord('Z') + 1):
      self.GLFW_TO_IMGUI_KEY_MAP[i] = getattr(imgui.Key, chr(i).lower())

    ###################################

    self.GLFW_IGNORE_KEYS = {
      glfw.KEY_LEFT_SHIFT,
      glfw.KEY_RIGHT_SHIFT,
      glfw.KEY_LEFT_CONTROL,
      glfw.KEY_RIGHT_CONTROL,
      glfw.KEY_LEFT_ALT,
      glfw.KEY_RIGHT_ALT,
      glfw.KEY_LEFT_SUPER,
      glfw.KEY_RIGHT_SUPER,
      glfw.KEY_MENU,
    }
    self.docking = docking
  ####################################

  def _remapNativeKeyToImguiKey(self,keycode):
    imkey = None
    if keycode in self.GLFW_TO_IMGUI_KEY_MAP:
      imkey = self.GLFW_TO_IMGUI_KEY_MAP[keycode]
    return imkey

  ####################################

  def _shouldIgnoreKey(self,keycode):
    return keycode in self.GLFW_IGNORE_KEYS

  ####################################

  def onGpuInit(self,ctx, vars):
    imgui.create_context()
    imgui.style_colors_dark()
    imgui.load_ini_settings_from_disk(str(self.imgui_settings_file))
    self.imgui_io = imgui.get_io()
    self.imgui_io.fonts.get_tex_data_as_rgba32()
    self.imgui_renderer = ProgrammablePipelineRenderer()
    self.app_dict = {}
    if self.app_settings_file.exists():
      with open(self.app_settings_file, "r") as f:
        appdict_as_json = f.read()
        self.app_dict = json.loads(appdict_as_json)
    for item in self.app_dict:
      if item in vars.keys():
        value = self.app_dict[item]
        if isinstance(value,list):
          value = tuple(value)
        setattr(vars,item,value)

  ####################################

  def onExit(self,vars):
    imgui.save_ini_settings_to_disk(str(self.imgui_settings_file))
    as_dict = dict()
    for k in vars.keys():
      val = vars[k]
      as_dict[k] = val
    appdict_as_json = json.dumps(as_dict)
    print(self.app_settings_file)
    print(appdict_as_json)
    with open(self.app_settings_file, "w") as f:
      f.write(appdict_as_json)

  ####################################

  def beginFrame(self):
    TOPW = self.ezapp.topWidget    
    scr_w = TOPW.width
    scr_h = TOPW.height
    io = self.imgui_io
    io.display_size = scr_w, scr_h
    self.imgui_io.config_flags |= imgui.ConfigFlags_.docking_enable
    imgui.new_frame()
    ####################
    # set up docking
    ####################
    if self.docking:
      self.imgui_io = imgui.get_io()
      window_flags = imgui.WindowFlags_.menu_bar | imgui.WindowFlags_.no_docking
      dockspace_flags = imgui.DockNodeFlags_.none
   
      imgui.push_style_var(imgui.StyleVar_.window_rounding, 0.0)
      imgui.push_style_var(imgui.StyleVar_.window_border_size, 0.0)

      imgui.push_style_var(imgui.StyleVar_.window_padding, (0.0, 0.0))
      imgui.begin("DockSpace Demo", True, window_flags)
      imgui.pop_style_var()

      # DockSpace
      if self.imgui_io.config_flags & imgui.ConfigFlags_.docking_enable:
          dockspace_id = imgui.get_id("MyDockSpace")
          imgui.dock_space(dockspace_id, (0.0, 0.0), dockspace_flags)

  ####################################

  def endFrame(self):
    ####################
    # end docking
    ####################
    if self.docking:
      imgui.pop_style_var(2)
      imgui.end()
    ####################
    # render
    ####################
    imgui.render()
    imgui.end_frame()
    self.render(imgui.get_draw_data())

  ####################################

  def render(self, draw_data):
    self.imgui_renderer.render(draw_data)

  ####################################

  def onUiEvent(self,uievent):
    handler = self.uicontext.overlayWidget
    ###############################################################
    if uievent.code == tokens.KEY_DOWN.hashed:
      if not self.imgui_io.want_capture_keyboard:
        handler = None
      else:
        keycode = uievent.keycode
        remapped_key = self._remapNativeKeyToImguiKey(keycode)
        if remapped_key!=None:
          self.imgui_io.add_key_event(remapped_key,True)
          self.imgui_io.add_key_event(remapped_key,False)
          key_0 = getattr(imgui.Key,"_0")
          key_9 = getattr(imgui.Key,"_9")
          key_a = getattr(imgui.Key,"a")
          key_z = getattr(imgui.Key,"z")
          character = None
          ###################################
          if remapped_key >= key_a and remapped_key <= key_z:
            character = int(remapped_key) - int(key_a) + ord('a')
            if uievent.shift:
              character += ord('A') - ord('a')
          ###################################
          elif remapped_key >= key_0 and remapped_key <= key_9:
            character = int(remapped_key) - int(key_0) + ord('0')
            if uievent.shift:
              index = character - ord('0')
              character = ord('!@#$%^&*()'[index-1])
          ###################################
          else: # handle punctuation
            if remapped_key == imgui.Key.comma:
              character = ord('<') if uievent.shift else ord(',')
            elif remapped_key == imgui.Key.period:
              character = ord('>') if uievent.shift else ord('.')
            elif remapped_key == imgui.Key.slash:
              character = ord('?') if uievent.shift else ord('/')
            elif remapped_key == imgui.Key.backslash:
              character = ord('|') if uievent.shift else ord('\\')
            elif remapped_key == imgui.Key.grave_accent:
              character = ord('~') if uievent.shift else ord('`')
            elif remapped_key == imgui.Key.minus:
              character = ord('_') if uievent.shift else ord('-')
            elif remapped_key == imgui.Key.equal:
              character = ord('+') if uievent.shift else ord('=')
            elif remapped_key == imgui.Key.left_bracket:
              character = ord('{') if uievent.shift else ord('[')
            elif remapped_key == imgui.Key.right_bracket:
              character = ord('}') if uievent.shift else ord(']')
            elif remapped_key == imgui.Key.semicolon:
              character = ord(':') if uievent.shift else ord(';')
            elif remapped_key == imgui.Key.apostrophe:
              character = ord('"') if uievent.shift else ord('\'')
          ################################### 
          no_metas = (uievent.alt == False) and (uievent.ctrl == False)                       
          if character != None and no_metas:
            self.imgui_io.add_input_character(character)
    ###############################################################
    elif uievent.code == tokens.KEY_UP.hashed:
      keycode = uievent.keycode
      if self.imgui_io.want_capture_keyboard:
        handler = None
    ###############################################################
    elif uievent.code == tokens.PUSH.hashed:
      self.imgui_io.mouse_down[0] = True
      if not self.imgui_io.want_capture_mouse:
        handler = None
      pass
    ###############################################################
    elif uievent.code == tokens.RELEASE.hashed:
      self.imgui_io.mouse_down[0] = False
      if not self.imgui_io.want_capture_mouse:
        handler = None
      pass
    ###############################################################
    elif uievent.code == tokens.BEGIN_DRAG.hashed:
      pass
    ###############################################################
    elif uievent.code == tokens.END_DRAG.hashed:
      pass
    ###############################################################
    elif uievent.code == tokens.DRAG.hashed:
      self.imgui_io.mouse_pos = uievent.x, uievent.y
      if not self.imgui_io.want_capture_mouse:
        handler = None
      pass
    ###############################################################
    elif uievent.code == tokens.MOVE.hashed:
      self.imgui_io.mouse_pos = uievent.x, uievent.y
      if not self.imgui_io.want_capture_mouse:
        handler = None
      pass
    ###############################################################
    result = lev2.ui.HandlerResult()
    result.setHandler(handler)
    return result
