################################################################################
# lev2 PyImGui helpers
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import imgui, glfw, json
from obt import path as obt_path
from orkengine.core import CrcStringProxy
from orkengine import lev2
from imgui.integrations.opengl import ProgrammablePipelineRenderer

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
  def __init__(self, ezapp, appID ):
    self.appID = appID
    self.imgui_settings_file = obt_path.temp()/("_imgui_settings_%s.ini" % self.appID)
    self.app_settings_file = obt_path.temp()/("_app_settings_%s.json" % self.appID)
    self.ezapp = ezapp
    self.uicontext = ezapp.uicontext
    self.topwidget = ezapp.topWidget
    
    self.GLFW_TO_IMGUI_KEY_MAP = {
      glfw.KEY_DELETE: imgui.KEY_DELETE,
      glfw.KEY_BACKSPACE: imgui.KEY_BACKSPACE,
      glfw.KEY_ENTER: imgui.KEY_ENTER,
      glfw.KEY_TAB: imgui.KEY_TAB,
      glfw.KEY_LEFT: imgui.KEY_LEFT_ARROW,
      glfw.KEY_RIGHT: imgui.KEY_RIGHT_ARROW,
      glfw.KEY_UP: imgui.KEY_UP_ARROW,
      glfw.KEY_DOWN: imgui.KEY_DOWN_ARROW,
      glfw.KEY_ESCAPE: imgui.KEY_ESCAPE,
      # Add more key mappings as needed
    }
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
      glfw.KEY_ESCAPE,
    }
    # fill in alphanumeric keys for both cases
    #for i in range(ord('A'), ord('Z')+1):
    #  self.GLFW_TO_IMGUI_KEY_MAP[ord('A')] = imgui.KEY_A + i - ord('A')
      
  ####################################

  def _remapNativeKeyToImguiKey(self,keycode):
    imkey = keycode
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
    self._map_keys()
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
    imgui.new_frame()

  ####################################

  def endFrame(self):
    imgui.render()
    imgui.end_frame()
    self.render(imgui.get_draw_data())

  ####################################

  def render(self, draw_data):
    self.imgui_renderer.render(draw_data)

  ####################################

  def _map_keys(self):
      key_map = self.imgui_io.key_map

      key_map[imgui.KEY_TAB] = glfw.KEY_TAB
      key_map[imgui.KEY_LEFT_ARROW] = glfw.KEY_LEFT
      key_map[imgui.KEY_RIGHT_ARROW] = glfw.KEY_RIGHT
      key_map[imgui.KEY_UP_ARROW] = glfw.KEY_UP
      key_map[imgui.KEY_DOWN_ARROW] = glfw.KEY_DOWN
      key_map[imgui.KEY_PAGE_UP] = glfw.KEY_PAGE_UP
      key_map[imgui.KEY_PAGE_DOWN] = glfw.KEY_PAGE_DOWN
      key_map[imgui.KEY_HOME] = glfw.KEY_HOME
      key_map[imgui.KEY_END] = glfw.KEY_END
      key_map[imgui.KEY_INSERT] = glfw.KEY_INSERT
      key_map[imgui.KEY_DELETE] = glfw.KEY_DELETE
      key_map[imgui.KEY_BACKSPACE] = glfw.KEY_BACKSPACE
      key_map[imgui.KEY_SPACE] = glfw.KEY_SPACE
      key_map[imgui.KEY_ENTER] = glfw.KEY_ENTER
      key_map[imgui.KEY_ESCAPE] = glfw.KEY_ESCAPE
      key_map[imgui.KEY_PAD_ENTER] = glfw.KEY_KP_ENTER
      key_map[imgui.KEY_A] = glfw.KEY_A
      key_map[imgui.KEY_C] = glfw.KEY_C
      key_map[imgui.KEY_V] = glfw.KEY_V
      key_map[imgui.KEY_X] = glfw.KEY_X
      key_map[imgui.KEY_Y] = glfw.KEY_Y
      key_map[imgui.KEY_Z] = glfw.KEY_Z

  ####################################

  def onUiEvent(self,uievent):
    handler = self.uicontext.overlayWidget
    if uievent.code == tokens.KEY_DOWN.hashed:
      keycode = uievent.keycode
      remapped_key = self._remapNativeKeyToImguiKey(keycode)
      if self.imgui_io.want_capture_keyboard:
        print( "IMGUI KEY_DOWN", remapped_key, uievent )
        self.imgui_io.keys_down[remapped_key] = True
        should_ignore = self._shouldIgnoreKey(keycode)
        if should_ignore:
          handler = None
        else:
          was_remapped = remapped_key != keycode
          if was_remapped:
            if remapped_key == imgui.KEY_BACKSPACE:
              # clear 1 character
              print(dir(self.imgui_io))
              self.imgui_io.clear_input_characters()
          else:
            if 0 < keycode < 0x10000:
              if uievent.shift:
                # check alphabetic keys
                if 0x41 <= keycode <= 0x5A:
                  keycode = ord(chr(keycode).upper())
                # check numeric keys
                if 0x30 <= keycode <= 0x39:
                  # set to punctuation
                  keycode = ord('!@#$%^&*()'[keycode-0x31])
                # check brackets
                if keycode == 0x5B:
                  keycode = ord('{')
                if keycode == 0x5D:
                  keycode = ord('}')
                # check semicolon
                if keycode == 0x3B:
                  keycode = ord(':')
                # check quotes
                if keycode == 0x27:
                  keycode = ord('"')
                # check comma
                if keycode == 44:
                  keycode = ord('<')
                # check period
                if keycode == 46:
                  keycode = ord('>')
                # check slash
                if keycode == 47:
                  keycode = ord('?')
                if keycode == 92:
                  keycode = ord('|')
                if keycode == 45:
                  keycode = ord('_')
                if keycode == 61:
                  keycode = ord('+')
                if keycode == 96:
                  keycode = ord('~')
                  
              else:
                keycode = ord(chr(keycode).lower())
              self.imgui_io.add_input_character(keycode)

      else:
        handler = None
    elif uievent.code == tokens.KEY_UP.hashed:
      remapped_key = self._remapNativeKeyToImguiKey(uievent.keycode)
      if self.imgui_io.want_capture_keyboard:
        print( "IMGUI KEY_UP", uievent )
        self.imgui_io.keys_down[remapped_key] = False
        handler = None
    elif uievent.code == tokens.PUSH.hashed:
      #print("PUSHED :", uievent )
      self.imgui_io.mouse_down[0] = True
      if not self.imgui_io.want_capture_mouse:
        #print("PUSHED -> native :", uievent )
        handler = None
        #custom_mouse_button_handler(button, action, mods)
      pass
    elif uievent.code == tokens.RELEASE.hashed:
      self.imgui_io.mouse_down[0] = False
      if not self.imgui_io.want_capture_mouse:
        #print("RELEASED :", uievent )
        handler = None
      pass
    elif uievent.code == tokens.BEGIN_DRAG.hashed:
      #print("BEGIN_DRAG :", uievent )
      pass
    elif uievent.code == tokens.END_DRAG.hashed:
      #print("END_DRAG :", uievent )
      pass
    elif uievent.code == tokens.DRAG.hashed:
      self.imgui_io.mouse_pos = uievent.x, uievent.y
      if not self.imgui_io.want_capture_mouse:
        #print("DRAG :", uievent )
        handler = None
      pass
    elif uievent.code == tokens.MOVE.hashed:
      self.imgui_io.mouse_pos = uievent.x, uievent.y
      if not self.imgui_io.want_capture_mouse:
        #print("MOVE :", uievent )
        handler = None
      pass
            
    result = lev2.ui.HandlerResult()
    result.setHandler(handler)
    return result
