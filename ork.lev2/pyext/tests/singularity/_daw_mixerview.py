#!/usr/bin/env python3

################################################################################
# DAW-style Mixer View for Singularity Test Harness
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

"""
DAW-style channel strip UI for Singularity synth.

Provides horizontal row-based channel strips (2 rows per channel) with:
  - Program selector
  - Insert effect slot
  - Gain/Pan controls
  - Mute/Solo buttons

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
# ChannelStripRow - 2-row widget for one bus
################################################################################

class ChannelStripRow:
    """
    A channel strip displayed as 2 horizontal rows.

    Row 1: [BusName] [Program: dropdown] [Insert: dropdown] [Gain] [Pan]
    Row 2: [M] [S] [FX: effect_name]
    """

    def __init__(self, mixer_view, bus, bus_index, source=None):
        self.mixer_view = mixer_view
        self.bus = bus
        self.bus_index = bus_index
        self.source = source
        self.is_selected = False

    def get_display_text(self):
        """Generate display text for this channel strip."""
        bus = self.bus
        name = bus.name

        # Get current program name
        prog_name = "---"
        if bus.uiprogram:
            prog_name = getattr(bus.uiprogram, 'name', '???')
            if len(prog_name) > 16:
                prog_name = prog_name[:14] + ".."

        # Get effect name
        fx_name = bus.effectName if bus.effectName else "none"
        if len(fx_name) > 20:
            fx_name = fx_name[:18] + ".."

        # Get gain/pan
        gain_db = bus.gain
        pan = bus.pan

        # Mute/Solo state
        mute_str = "[M]" if bus.mute else "[ ]"
        solo_str = "[S]" if bus.solo else "[ ]"

        # Selection indicator
        sel = ">" if self.is_selected else " "

        # Format as 2 lines
        line1 = f"{sel}{name:6} | Prg:{prog_name:16} | Ins:[none] | G:{gain_db:+.0f}dB P:{pan:+.1f}"
        line2 = f"       | {mute_str} {solo_str} | FX: {fx_name}"

        return line1 + "\n" + line2

    def set_selected(self, selected):
        self.is_selected = selected


################################################################################
# DAWMixerView - container of channel strips
################################################################################

class DAWMixerView:
    """
    Container holding multiple ChannelStripRows.
    Displays as vertical list of 2-row channel strips.
    """

    def __init__(self, app, text_widget):
        self.app = app
        self.text_widget = text_widget
        self.channel_strips = []
        self.selected_index = 0

    def add_channel(self, bus, source=None):
        """Add a channel strip for a bus."""
        idx = len(self.channel_strips)
        strip = ChannelStripRow(self, bus, idx, source)
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

    def refresh(self):
        """Update the text display with current channel state."""
        lines = []
        lines.append("=" * 70)
        lines.append(" DAW Mixer View")
        lines.append("  Keys: 0-9=select channel, m=mute, s=solo, ,/.=prev/next program")
        lines.append("        -/==prev/next effect, [/]=gain, AWSED..=play notes")
        lines.append("=" * 70)

        for strip in self.channel_strips:
            lines.append("-" * 70)
            lines.append(strip.get_display_text())

        lines.append("-" * 70)

        self.text_widget.setText("\n".join(lines))


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

        # Create grid: [mixer_text | profiler]
        #              [program    | oscope  ]
        #              [spectrum   |         ]
        rccounts = [3, 2]
        self.griditems = lg_group.makeRowsColumns(
            rccounts=rccounts,
            margin=4,
            uiclass=ui.TextBox,
            args=["label", vec4(0.1, 0.1, 0.3, 1)],
        )

        # Set up mixer text widget (item 0)
        self.mixer_text = self.griditems[0].widget
        self.mixer_text.halign = tokens.LEFT
        self.mixer_text.valign = tokens.TOP

        # Key handling
        self.ezapp.uicontext.debug_event_routing = True
        self.mixer_text.onKeyDown(lambda x: self._onKeyEvent(self.mixer_text, x))
        self.mixer_text.onKeyUp(lambda x: self._onKeyEvent(self.mixer_text, x))

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

        # Create mixer view
        self.mixer_view = DAWMixerView(self, self.mixer_text)

        # Add main bus
        self.mixer_view.add_channel(self.mainbus, self.mainbus_source)

        # Add aux buses
        for i in range(self.numaux):
            self.mixer_view.add_channel(self.auxbusses[i], self.auxbus_sources[i])

        # Select main bus
        self.mixer_view.select_channel(0)

        # Create profiler view (item 1)
        item = lg_group.makeChild(uiclass=singularity.ProfilerView, args=["YO"])
        self.profview = lg_group.getUserVar("profilerviews.YO")
        lg_group.replaceChild(self.griditems[1].layout, item)
        item.widget.ignoreEvents = True

        # Create program view (item 2)
        item = lg_group.makeChild(uiclass=singularity.ProgramView, args=["PROGRAM"])
        self.pgmview = lg_group.getUserVar("programviews.PROGRAM")
        lg_group.replaceChild(self.griditems[2].layout, item)
        item.widget.ignoreEvents = True

        # Create oscilloscope (item 3)
        item = lg_group.makeChild(uiclass=singularity.Oscilloscope, args=["MAINBUS"])
        self.oscope = lg_group.getUserVar("oscilloscopes.MAINBUS")
        self.oscope_sink = self.oscope.sink
        lg_group.replaceChild(self.griditems[3].layout, item)

        # Create spectrum analyzer (item 4)
        item = lg_group.makeChild(uiclass=singularity.SpectrumAnalyzer, args=["MAINBUS"])
        self.spectra = lg_group.getUserVar("analyzers.MAINBUS")
        self.spectra_sink = self.spectra.sink
        lg_group.replaceChild(self.griditems[4].layout, item)
        item.widget.ignoreEvents = True

        # Connect scope sources
        self.mainbus_source.connect(self.oscope_sink)
        self.mainbus_source.connect(self.spectra_sink)

        # Initial refresh
        self.mixer_view.refresh()

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
        strip = self.mixer_view.select_channel(index)
        if strip:
            if shift:
                self.synth.soloLayer = index - 1 if index > 0 else -1
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
