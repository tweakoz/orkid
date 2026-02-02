#!/usr/bin/env python3

################################################################################
# DAW-style Mixer View for Singularity Test Harness
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

"""
DAW-style channel strip UI for Singularity synth.

Provides horizontal row-based channel strips with actual widgets:
  - Label (bus name)
  - ComboBox (program selector)
  - FloatSlider (gain)
  - FloatSlider (pan)
  - Button (mute)
  - Button (solo)
  - Label (fx name)

Usage:
  from _daw_mixerview import SingulTestAppNewUI

  class MyApp(SingulTestAppNewUI):
      def onGpuInit(self, ctx):
          super().onGpuInit(ctx)
          # load your soundbank, set up programs, etc.
"""

import sys
from orkengine.core import *
from orkengine.lev2 import *

tokens = CrcStringProxy()

################################################################################
# Key Actions - abstract actions that UI classes implement
################################################################################

class KeyActions:
    """Mixin providing abstract key actions and binding registry."""

    # Default key bindings (old UI compatible)
    DEFAULT_BINDINGS = {
        ord(","): "prev_program",
        ord("."): "next_program",
        ord("-"): "prev_effect",
        ord("="): "next_effect",
        ord("["): "decr_gain",
        ord("]"): "incr_gain",
        ord("Z"): "decr_octave",
        ord("X"): "incr_octave",
        ord(" "): "hold_drones",
        ord("C"): "release_drones",
        ord("!"): "panic",
        ord("N"): "new_sequence",
        ord("M"): "toggle_recording",
        ord("Q"): "quantize_clip",
    }

    # DAW-specific bindings (adds to default)
    DAW_BINDINGS = {
        ord("m"): "toggle_mute",
        ord("s"): "toggle_solo",
    }

    def __init__(self):
        self.key_bindings = dict(self.DEFAULT_BINDINGS)
        # Subclasses can add DAW_BINDINGS in their __init__

    def add_daw_bindings(self):
        """Add DAW-specific key bindings."""
        self.key_bindings.update(self.DAW_BINDINGS)
        # Number keys 0-9 for channel selection
        for i in range(10):
            self.key_bindings[ord(str(i))] = f"select_channel_{i}"

    def dispatch_key_action(self, keycode, shift=False):
        """Dispatch key to action method. Returns True if handled."""
        action_name = self.key_bindings.get(keycode)
        if action_name:
            # Check for channel selection
            if action_name.startswith("select_channel_"):
                idx = int(action_name.split("_")[-1])
                return self.action_select_channel(idx, shift)
            # Look up action method
            method = getattr(self, f"action_{action_name}", None)
            if method:
                return method(shift)
        return False

    # Abstract actions - subclasses implement these
    def action_prev_program(self, shift): pass
    def action_next_program(self, shift): pass
    def action_prev_effect(self, shift): pass
    def action_next_effect(self, shift): pass
    def action_decr_gain(self, shift): pass
    def action_incr_gain(self, shift): pass
    def action_decr_octave(self, shift): pass
    def action_incr_octave(self, shift): pass
    def action_hold_drones(self, shift): pass
    def action_release_drones(self, shift): pass
    def action_panic(self, shift): pass
    def action_new_sequence(self, shift): pass
    def action_toggle_recording(self, shift): pass
    def action_quantize_clip(self, shift): pass
    def action_toggle_mute(self, shift): pass
    def action_toggle_solo(self, shift): pass
    def action_select_channel(self, index, shift): pass


################################################################################
# ChannelStripWidget - HPack containing channel controls
################################################################################

class ChannelStripWidget:
    """
    A channel strip as an HPack with child widgets.

    Layout: [Name] [Program ComboBox] [Gain Slider] [Pan Slider] [M] [S] [FX]
    """

    # Class-level colors for selection state
    LABEL_COLOR_NORMAL = vec4(0.2, 0.2, 0.3, 1)
    LABEL_COLOR_SELECTED = vec4(0.4, 0.4, 0.6, 1)

    def __init__(self, mixer_view, hpack, bus, bus_index, source=None):
        self.mixer_view = mixer_view
        self.hpack = hpack
        self.bus = bus
        self.bus_index = bus_index
        self.source = source
        self.is_selected = False

        # Colors (vec4 for TextBox, vec3 for ComboBox/Button)
        label_col = self.LABEL_COLOR_NORMAL
        combo_col = vec3(0.15, 0.15, 0.25)
        btn_col = vec3(0.2, 0.2, 0.2)

        # Configure hpack
        hpack.margin = 2
        hpack.uniform = False
        hpack.fill = True

        # Bus name label (fixed width)
        self.name_label = hpack.makeChild(uiclass=ui.TextBox, args=["name", label_col, bus.name])
        self.name_label.fixed_width = 60

        # Program combo box
        self.prog_combo = hpack.makeChild(uiclass=ui.ComboBox, args=["Prog", combo_col])
        self.prog_combo.fixed_width = 256
        self.prog_combo.onSelectionChanged = self._on_program_changed

        # Gain edit (-60 to +12 dB)
        self.gain_edit = hpack.makeChild(uiclass=ui.F32Edit, args=["gain", "Gain", 0.0, -60.0, 12.0])
        self.gain_edit.fixed_width = 100

        # Pan edit (-1 to +1)
        self.pan_edit = hpack.makeChild(uiclass=ui.F32Edit, args=["pan", "Pan", 0.0, -1.0, 1.0])
        self.pan_edit.fixed_width = 80

        # Mute button
        self.mute_btn = hpack.makeChild(uiclass=ui.Button, args=["M", btn_col])
        self.mute_btn.fixed_width = 30
        self.mute_btn.onPressed = self._on_mute_pressed

        # Solo button
        self.solo_btn = hpack.makeChild(uiclass=ui.Button, args=["S", btn_col])
        self.solo_btn.fixed_width = 30
        self.solo_btn.onPressed = self._on_solo_pressed

        # FX label
        self.fx_label = hpack.makeChild(uiclass=ui.TextBox, args=["fx", label_col, "FX: none"])
        self.fx_label.fixed_width = 120

    def _on_program_changed(self, combo):
        """Called when program combo selection changes."""
        index = combo.selected_index
        name = combo.selectedItem()
        if self.mixer_view.app.soundbank and name:
            prog = self.mixer_view.app.soundbank.programByName(name)
            if prog:
                self.bus.uiprogram = prog
                self.mixer_view.app.prog = prog
                self.mixer_view.app.prog_index = index
                if self.mixer_view.app.pgmview:
                    self.mixer_view.app.pgmview.setProgram(prog)

    def _on_mute_pressed(self, btn):
        """Called when mute button pressed."""
        self.bus.mute = not self.bus.mute

    def _on_solo_pressed(self, btn):
        """Called when solo button pressed."""
        self.bus.solo = not self.bus.solo

    def set_selected(self, selected):
        """Update selection state and highlight name label."""
        self.is_selected = selected
        if selected:
            self.name_label.color = self.LABEL_COLOR_SELECTED
        else:
            self.name_label.color = self.LABEL_COLOR_NORMAL

    def set_programs(self, program_names):
        """Set the list of available programs in the combo box."""
        self.prog_combo.setItems(program_names)

    def refresh(self):
        """Update widget states from bus state."""
        # Sync gain: widget -> bus (user edited) or bus -> widget (external change)
        widget_gain = self.gain_edit.value
        if abs(widget_gain - self.bus.gain) > 0.01:
            # Widget changed, update bus
            self.bus.gain = widget_gain

        # Sync pan: widget -> bus
        widget_pan = self.pan_edit.value
        if abs(widget_pan - self.bus.pan) > 0.01:
            self.bus.pan = widget_pan

        # Update FX label
        fx_name = self.bus.effectName if self.bus.effectName else "none"
        if len(fx_name) > 15:
            fx_name = fx_name[:13] + ".."
        self.fx_label.setText(f"FX: {fx_name}")


################################################################################
# DAWMixerView - VPack container of channel strips
################################################################################

class DAWMixerView:
    """
    Container holding multiple ChannelStripWidgets in a VPack.
    """

    def __init__(self, app, vpack):
        self.app = app
        self.vpack = vpack
        self.channel_strips = []
        self.selected_index = 0

        # Configure vpack
        vpack.margin = 4
        vpack.item_height = 32

    def add_channel(self, bus, source=None):
        """Add a channel strip for a bus."""
        idx = len(self.channel_strips)

        # Create HPack for this channel
        hpack = self.vpack.makeChild(uiclass=ui.HorizontalPack, args=[f"ch_{bus.name}"])

        # Create the strip widget
        strip = ChannelStripWidget(self, hpack, bus, idx, source)
        self.channel_strips.append(strip)
        return strip

    def select_channel(self, index):
        """Select a channel by index."""
        if 0 <= index < len(self.channel_strips):
            # Deselect previous
            if 0 <= self.selected_index < len(self.channel_strips):
                self.channel_strips[self.selected_index].set_selected(False)
            # Select new
            self.selected_index = index
            self.channel_strips[index].set_selected(True)
            return self.channel_strips[index]
        return None

    def get_selected_strip(self):
        """Get currently selected channel strip."""
        if 0 <= self.selected_index < len(self.channel_strips):
            return self.channel_strips[self.selected_index]
        return None

    def set_programs(self, program_names):
        """Set the program list for all channels."""
        for strip in self.channel_strips:
            strip.set_programs(program_names)

    def refresh(self):
        """Update all channel strip displays."""
        for strip in self.channel_strips:
            strip.refresh()


################################################################################
# SingulTestAppNewUI - base class using new DAW UI
################################################################################

class SingulTestAppNewUI(KeyActions):
    """
    Base class for singularity test apps using new DAW-style UI.

    Subclass this and override onGpuInit to load your soundbank.
    """

    def __init__(self, enable_input=False):
        KeyActions.__init__(self)
        self.add_daw_bindings()  # Add M/S/number key bindings

        self.ezapp = OrkEzApp.create(self,
                                     fullscreen=True,
                                     enable_audio_synth=True,
                                     enable_audio=True,
                                     enable_audio_output=True,
                                     enable_audio_input=enable_input,
                                     name="SingularityDAWHarness")
        self.ezapp.setRefreshPolicy(RefreshFastest, 0)
        self.ezapp.topWidget.enableUiDraw()

        lg_group = self.ezapp.topLayoutGroup
        lg_group.margin = 5

        self.held_voices = []
        self.voices = dict()
        self.octave = 5
        self.gain = -24.0
        self.sorted_progs = []
        self.prog_index = 0
        self.prog = None
        self.soundbank = None

        # Note mapping (same as old harness)
        self.base_notes = {
            ord("A"): 0, ord("W"): 1, ord("S"): 2, ord("E"): 3,
            ord("D"): 4, ord("F"): 5, ord("T"): 6, ord("G"): 7,
            ord("Y"): 8, ord("H"): 9, ord("U"): 10, ord("J"): 11,
            ord("K"): 12, ord("O"): 13, ord("L"): 14, ord("P"): 15,
            ord(";"): 16, ord("'"): 17,
        }

        self.mixer_view = None
        self.click_prog = None
        self.click_noteL = 60
        self.click_noteH = 60
        self.pgmview = None

        import signal
        def onCtrlC(signum, frame):
            print("signalling EXIT to ezapp")
            self.ezapp.signalExit()
        signal.signal(signal.SIGINT, onCtrlC)

    ##############################################
    # Synth/Audio Init
    ##############################################

    def onAudioInit(self, synth):
        pass

    def onSynthInit(self, synth):
        self.curseq = None
        self.synth = synth
        assert self.synth is not None
        self.synth.system_tempo = 120.0
        self.sequencer = self.synth.sequencer

        # Create main bus
        self.mainbus = self.synth.outputBus("main")
        self.mainbus_source = self.mainbus.createScopeSource()
        self.synth.setEffect(self.mainbus, "none")

        # Create aux buses
        self.numaux = 9
        self.auxbusses = []
        self.auxbus_sources = []
        for i in range(self.numaux):
            bus = self.synth.createOutputBus(f"aux{i+1}")
            self.auxbusses.append(bus)
            self.auxbus_sources.append(bus.createScopeSource())
            self.synth.setEffect(bus, "none")

        self.program_source = self.mainbus_source
        self.synth.masterGain = singularity.decibelsToLinear(self.gain)

    ##############################################
    # GPU Init - creates UI
    ##############################################

    def onGpuInit(self, ctx):
        self.context = ctx
        lg_group = self.ezapp.topLayoutGroup
        self.rec_trackclips = {}

        # Create grid layout: [text    | mixer   | program ]
        #                     [oscope            | spectrum]
        rccounts = [3, 2]
        # h_splits: row 0 splits at 25% and 75% -> [25% | 50% | 25%]
        #           row 1 splits at 50% (default) -> [50% | 50%]
        h_splits = [[0.25, 0.75], []]
        self.griditems = lg_group.makeRowsColumns(
            rccounts=rccounts,
            margin=4,
            uiclass=ui.Box,
            args=["label", vec4(0.1, 0.1, 0.3, 1)],
            h_splits=h_splits,
        )

        # === TextBox with key handlers in grid cell 0 ===
        text = "NEW UI MODE\n"
        text += "  Keys: AWSEDFTGYHUJKOLP = play notes\n"
        text += "  , . : prev/next program\n"
        text += "  - = : prev/next effect\n"
        text += "  [ ] : adjust gain\n"
        text += "  Z X : octave down/up\n"
        text += "  SPACE : hold drones\n"
        text += "  C : release drones\n"
        text += "  0-9 : select channel\n"
        text += "  m : toggle mute\n"
        text += "  s : toggle solo\n"
        info_item = lg_group.makeChild(uiclass=ui.TextBox, args=["info", vec4(0.1, 0.1, 0.3, 1), text])
        self.info_text = info_item.widget
        lg_group.replaceChild(self.griditems[0].layout, info_item)
        self.info_text.halign = tokens.LEFT
        self.info_text.valign = tokens.TOP

        # Key handling on text widget
        self.ezapp.uicontext.debug_event_routing = True
        self.info_text.onKeyDown(lambda x: self._onKeyEvent(self.info_text, x))
        self.info_text.onKeyUp(lambda x: self._onKeyEvent(self.info_text, x))

        # === Mixer VPack in grid cell 1 (replaces profiler) ===
        mixer_vpack_item = lg_group.makeChild(uiclass=ui.VerticalPack, args=["mixer_vpack"])
        self.mixer_vpack = mixer_vpack_item.widget
        self.mixer_vpack.margin = 2
        self.mixer_vpack.item_height = 36
        lg_group.replaceChild(self.griditems[1].layout, mixer_vpack_item)

        # Create mixer view
        self.mixer_view = DAWMixerView(self, self.mixer_vpack)

        # Add spacer at top (for Mac camera notch)
        spacer = self.mixer_vpack.makeChild(uiclass=ui.Box, args=["spacer", vec4(0, 0, 0, 1)])

        # Add aux buses
        for i in range(self.numaux):
            self.mixer_view.add_channel(self.auxbusses[i], self.auxbus_sources[i])

        # Add main bus at bottom
        self.mixer_view.add_channel(self.mainbus, self.mainbus_source)

        # Select main bus (now at index 9)
        self.mixer_view.select_channel(self.numaux)

        # === Program view in grid cell 2 ===
        item = lg_group.makeChild(uiclass=singularity.ProgramView, args=["PROGRAM"])
        self.pgmview = lg_group.getUserVar("programviews.PROGRAM")
        lg_group.replaceChild(self.griditems[2].layout, item)
        item.widget.ignoreEvents = True

        # === Oscilloscope in grid cell 3 ===
        item = lg_group.makeChild(uiclass=singularity.Oscilloscope, args=["MAINBUS"])
        self.oscope = lg_group.getUserVar("oscilloscopes.MAINBUS")
        self.oscope_sink = self.oscope.sink
        lg_group.replaceChild(self.griditems[3].layout, item)

        # === Spectrum analyzer in grid cell 4 ===
        item = lg_group.makeChild(uiclass=singularity.SpectrumAnalyzer, args=["MAINBUS"])
        self.spectra = lg_group.getUserVar("analyzers.MAINBUS")
        self.spectra_sink = self.spectra.sink
        lg_group.replaceChild(self.griditems[4].layout, item)
        item.widget.ignoreEvents = True

        # Connect scope sources
        self.mainbus_source.connect(self.oscope_sink)
        self.mainbus_source.connect(self.spectra_sink)

    ##############################################
    # Update
    ##############################################

    def onUpdate(self, updinfo):
        self.time = updinfo.absolutetime
        # Refresh mixer display periodically
        if self.mixer_view:
            self.mixer_view.refresh()

    def onGpuUpdate(self, ctx):
        pass

    ##############################################
    # Helper methods
    ##############################################

    def genMods(self):
        return None

    def onNote(self, voice):
        pass

    def setBusProgram(self, bus, prg):
        self.synth.programbus = bus
        self.synth.programbus.uiprogram = prg

    def setUiProgram(self, prg):
        self.synth.programbus.uiprogram = prg
        if self.pgmview:
            self.pgmview.setProgram(prg)

    def _setSource(self, bus, source):
        """Switch to viewing a different bus."""
        self.program_source.disconnect(self.oscope_sink)
        self.program_source.disconnect(self.spectra_sink)
        prev_bus = self.synth.programbus
        self.synth.programbus = bus
        self.program_source = source
        source.connect(self.oscope_sink)
        source.connect(self.spectra_sink)
        if bus.uiprogram is not None:
            self.prog = bus.uiprogram
        else:
            self.prog = prev_bus.uiprogram
        self.setUiProgram(self.prog)

    ##############################################
    # Key Action Implementations
    ##############################################

    def action_select_channel(self, index, shift):
        """Select a channel (0=main, 1-9=aux)."""
        # Remap: key 0 -> main (channel 9), keys 1-9 -> aux (channels 0-8)
        if index == 0:
            channel_index = self.numaux  # main is last
        else:
            channel_index = index - 1  # aux1-9 are channels 0-8
        strip = self.mixer_view.select_channel(channel_index)
        if strip:
            if shift:
                self.synth.soloLayer = channel_index - 1 if channel_index > 0 else -1
            else:
                self._setSource(strip.bus, strip.source)
        return True

    def action_toggle_mute(self, shift):
        """Toggle mute on selected channel."""
        strip = self.mixer_view.get_selected_strip()
        if strip:
            strip.bus.mute = not strip.bus.mute
        return True

    def action_toggle_solo(self, shift):
        """Toggle solo on selected channel."""
        strip = self.mixer_view.get_selected_strip()
        if strip:
            strip.bus.solo = not strip.bus.solo
        return True

    def action_prev_program(self, shift):
        self.prog_index -= 1
        self._update_program()
        return True

    def action_next_program(self, shift):
        self.prog_index += 1
        self._update_program()
        return True

    def _update_program(self):
        if not self.sorted_progs:
            return
        if self.prog_index < 0:
            self.prog_index = len(self.sorted_progs) - 1
        elif self.prog_index >= len(self.sorted_progs):
            self.prog_index = 0
        prgname = self.sorted_progs[self.prog_index]
        self.prog = self.soundbank.programByName(prgname)
        self.synth.programbus.uiprogram = self.prog
        if self.pgmview:
            self.pgmview.setProgram(self.prog)
        # Update the combo box for the selected channel
        strip = self.mixer_view.get_selected_strip()
        if strip:
            strip.prog_combo.selected_index = self.prog_index

    def action_prev_effect(self, shift):
        if shift and self.curseq:
            self.synth.system_tempo -= 1
            self.curseq.timebase.tempo = self.synth.system_tempo
        else:
            self.synth.prevEffect(self.synth.programbus)
        return True

    def action_next_effect(self, shift):
        if shift and self.curseq:
            self.synth.system_tempo += 1
            self.curseq.timebase.tempo = self.synth.system_tempo
        else:
            self.synth.nextEffect(self.synth.programbus)
        return True

    def action_decr_gain(self, shift):
        if shift:
            self.gain -= 6.0
            self.synth.masterGain = singularity.decibelsToLinear(self.gain)
        else:
            self.synth.programbus.gain = self.synth.programbus.gain - 3.0
        return True

    def action_incr_gain(self, shift):
        if shift:
            self.gain += 6.0
            self.synth.masterGain = singularity.decibelsToLinear(self.gain)
        else:
            self.synth.programbus.gain = self.synth.programbus.gain + 3.0
        return True

    def action_decr_octave(self, shift):
        self.octave = max(0, self.octave - 1)
        return True

    def action_incr_octave(self, shift):
        self.octave = min(8, self.octave + 1)
        return True

    def action_hold_drones(self, shift):
        for KC in self.voices:
            voice = self.voices[KC]
            self.held_voices.append(voice)
        self.voices = dict()
        return True

    def action_release_drones(self, shift):
        for v in self.held_voices:
            note = v.note
            self.synth.keyOff(v, note, 0)
        self.held_voices = []
        return True

    def action_panic(self, shift):
        for voice in self.voices:
            self.synth.keyOff(voice)
        self.voices.clear()
        return True

    def action_new_sequence(self, shift):
        # Simplified - subclasses can override
        return True

    def action_toggle_recording(self, shift):
        # Simplified - subclasses can override
        return True

    def action_quantize_clip(self, shift):
        # Simplified - subclasses can override
        return True

    ##############################################
    # Key Event Handler
    ##############################################

    def _onKeyEvent(self, widget, uievent):
        res = ui.HandlerResult()

        if uievent.code == tokens.KEY_REPEAT.hashed or uievent.code == tokens.KEY_DOWN.hashed:
            KC = uievent.keycode
            shift = uievent.shift

            # Try action dispatch first
            if self.dispatch_key_action(KC, shift):
                return res

        # Note playing (KEY_DOWN only)
        if uievent.code == tokens.KEY_DOWN.hashed:
            KC = uievent.keycode
            if KC in self.base_notes:
                if KC not in self.voices:
                    self.prog = self.synth.programbus.uiprogram
                    if self.prog is not None:
                        note = self.base_notes[KC] + (self.octave * 12)
                        mods = self.genMods()
                        voice = self.synth.keyOn(note, 127, self.prog, mods)
                        self.onNote(voice)
                        self.voices[KC] = voice
                    return res

        # Note release (KEY_UP)
        elif uievent.code == tokens.KEY_UP.hashed:
            KC = uievent.keycode
            if KC in self.base_notes:
                note = self.base_notes[KC] + (self.octave * 12)
                if KC in self.voices:
                    voice = self.voices[KC]
                    self.synth.keyOff(voice, note, 0)
                    del self.voices[KC]
                    return res

        return ui.HandlerResult()
