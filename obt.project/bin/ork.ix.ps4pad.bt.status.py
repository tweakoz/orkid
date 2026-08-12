#!/usr/bin/env ork.python
################################################################
# ork.ix.ps4pad.bt.status.py — live PS4 DualShock 4 monitor.
#
#  Shows whether a DS4 is connected (bluetooth + evdev + battery)
#  and loops rendering the live button / stick / trigger state.
#  Reads /dev/input/event* directly (no display server needed).
#  Ctrl-C to exit.
################################################################

import fcntl
import os
import re
import select
import struct
import subprocess
import sys
import time

from rich.console import Console, Group
from rich.live import Live
from rich.panel import Panel
from rich.table import Table
from rich.text import Text

console = Console()

DS4_NAME_RX = re.compile(r"wireless controller|dualshock", re.IGNORECASE)

# struct input_event on 64-bit: timeval(2*long) u16 type u16 code s32 value
EV_FMT = "llHHi"
EV_SIZE = struct.calcsize(EV_FMT)
EV_KEY, EV_ABS = 0x01, 0x03

BTN_LABELS = {
    0x130: "cross", 0x131: "circle", 0x133: "triangle", 0x134: "square",
    0x136: "L1", 0x137: "R1", 0x138: "L2b", 0x139: "R2b",
    0x13a: "share", 0x13b: "options", 0x13c: "PS",
    0x13d: "L3", 0x13e: "R3",
}

ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ = 0, 1, 2, 3, 4, 5
ABS_HAT0X, ABS_HAT0Y = 16, 17


def find_pad():
  """(event_path, name, mac) of the DS4's MAIN input node, or None.
  The pad exposes 3 nodes; skip the Touchpad / Motion Sensors ones."""
  try:
    blocks = open("/proc/bus/input/devices").read().split("\n\n")
  except OSError:
    return None
  for blk in blocks:
    nm = re.search(r'N: Name="([^"]*)"', blk)
    if not nm or not DS4_NAME_RX.search(nm.group(1)):
      continue
    if re.search(r"touchpad|motion", nm.group(1), re.IGNORECASE):
      continue
    ev = re.search(r"H: Handlers=.*?(event\d+)", blk)
    if not ev:
      continue
    uq = re.search(r"U: Uniq=([0-9a-fA-F:]+)", blk)
    return ("/dev/input/" + ev.group(1), nm.group(1),
            uq.group(1) if uq else "")
  return None


def absinfo(fd, code):
  """EVIOCGABS -> (value, min, max)"""
  # _IOR('E', 0x40+code, struct input_absinfo[6 x s32])
  buf = fcntl.ioctl(fd, 0x80184540 + code, bytes(24))
  v, lo, hi, _, _, _ = struct.unpack("6i", buf)
  return v, lo, hi


def battery_pct(mac):
  """DS4 battery via power_supply (hid-sony and hid-playstation names)"""
  if not mac:
    return None
  tag = mac.lower().replace(":", ":")
  base = "/sys/class/power_supply"
  try:
    for d in os.listdir(base):
      if "battery" in d and tag in d.lower():
        return int(open(f"{base}/{d}/capacity").read().strip())
  except OSError:
    pass
  return None


def bt_connected(mac):
  if not mac:
    return None  # unknown (e.g. USB)
  try:
    r = subprocess.run(["bluetoothctl", "info", mac.upper()],
                       capture_output=True, text=True, timeout=5)
    return "Connected: yes" in r.stdout
  except Exception:
    return None


def link_signal(mac):
  """(rssi, lq) of the live classic link, None-able each. Classic RSSI is
  relative to the 'golden receive range': 0 = fine, negative = weak.
  hcitool needs a raw HCI socket (root or cap_net_raw) — degrade to None."""
  if not mac:
    return None, None
  rssi = lq = None
  try:
    r = subprocess.run(["hcitool", "rssi", mac.upper()],
                       capture_output=True, text=True, timeout=5)
    m = re.search(r"RSSI return value: (-?\d+)", r.stdout)
    rssi = int(m.group(1)) if m else None
    r = subprocess.run(["hcitool", "lq", mac.upper()],
                       capture_output=True, text=True, timeout=5)
    m = re.search(r"Link quality: (\d+)", r.stdout)
    lq = int(m.group(1)) if m else None
  except Exception:
    pass
  return rssi, lq


class PadState:
  def __init__(self, fd):
    self.buttons = {name: False for name in BTN_LABELS.values()}
    self.axes = {}     # raw values by ABS code
    self.info = {}     # ABS code -> (min, max)
    for code in (ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ,
                 ABS_HAT0X, ABS_HAT0Y):
      try:
        v, lo, hi = absinfo(fd, code)
        self.axes[code] = v
        self.info[code] = (lo, hi)
      except OSError:
        pass
    # DS4 axis layout differs by driver: hid-sony puts the RIGHT STICK on
    # Z/RZ and the triggers on RX/RY; hid-playstation is the reverse.
    # Disambiguate from the resting position captured above: a trigger
    # rests at min, a stick rests mid-range.
    rx_rest, (rx_lo, rx_hi) = self.axes.get(ABS_RX, 0), self.info.get(ABS_RX, (0, 255))
    rx_mid = abs(rx_rest - (rx_lo + rx_hi) / 2) < (rx_hi - rx_lo) / 4
    if rx_mid:   # hid-playstation layout
      self.rx_code, self.ry_code = ABS_RX, ABS_RY
      self.l2_code, self.r2_code = ABS_Z, ABS_RZ
    else:        # hid-sony layout
      self.rx_code, self.ry_code = ABS_Z, ABS_RZ
      self.l2_code, self.r2_code = ABS_RX, ABS_RY

  def feed(self, etype, code, value):
    if etype == EV_KEY and code in BTN_LABELS:
      self.buttons[BTN_LABELS[code]] = bool(value)
    elif etype == EV_ABS:
      self.axes[code] = value

  def stick(self, code):
    lo, hi = self.info.get(code, (0, 255))
    mid, span = (lo + hi) / 2, (hi - lo) / 2 or 1
    return (self.axes.get(code, mid) - mid) / span

  def trigger(self, code):
    lo, hi = self.info.get(code, (0, 255))
    return (self.axes.get(code, lo) - lo) / ((hi - lo) or 1)

  def hat(self, code):
    return self.axes.get(code, 0)


def _track(v, width=21):
  """-1..1 marker track:  ──────────┼────●─────"""
  pos = max(0, min(width - 1, int(round((v + 1) / 2 * (width - 1)))))
  chars = ["─"] * width
  chars[width // 2] = "┼"
  chars[pos] = "●"
  return "".join(chars)


def _bar(v, width=20):
  n = max(0, min(width, int(round(v * width))))
  return "█" * n + "░" * (width - n)


def _chip(label, on, color):
  return f"[reverse bold {color}] {label} [/]" if on \
      else f"[dim]{label}[/]"


def render(state, name, node, mac, batt, bt_ok, sig=(None, None)):
  rssi, lq = sig
  if rssi is None and lq is None:
    sig_part = (("signal:", "dim"), ("n/a", "dim"))
  else:
    # classic-BT RSSI: 0 = inside golden range; below ~-8 gets flaky
    rs = "?" if rssi is None else f"{rssi:+d}"
    rc = "green" if (rssi or 0) >= -4 else \
         "yellow" if (rssi or 0) >= -8 else "red"
    ls = "?" if lq is None else f"{lq}"
    lc = "green" if (lq or 0) >= 200 else \
         "yellow" if (lq or 0) >= 120 else "red"
    sig_part = (("rssi:", "dim"), (rs, rc), (" lq:", "dim"), (ls, lc))
  hdr = Text.assemble(
      (name, "bold"), ("  "),
      (mac or "(usb?)", "yellow"), ("  "),
      ("BT:", "dim"),
      {True: ("connected", "green"), False: ("DISCONNECTED", "red"),
       None: ("n/a", "dim")}[bt_ok], ("  "),
      ("batt:", "dim"),
      (f"{batt}%" if batt is not None else "?",
       "green" if (batt or 0) > 30 else "red"), ("  "),
      *sig_part, ("  "),
      (node, "dim"))

  ax = Table.grid(padding=(0, 2))
  ax.add_column(justify="right", style="bold")
  ax.add_column()
  ax.add_column(justify="right", width=6)
  lx, ly = state.stick(ABS_X), state.stick(ABS_Y)
  rx, ry = state.stick(state.rx_code), state.stick(state.ry_code)
  l2, r2 = state.trigger(state.l2_code), state.trigger(state.r2_code)
  ax.add_row("LX", f"[cyan]{_track(lx)}[/]", f"{lx:+.2f}")
  ax.add_row("LY", f"[cyan]{_track(ly)}[/]", f"{ly:+.2f}")
  ax.add_row("RX", f"[cyan]{_track(rx)}[/]", f"{rx:+.2f}")
  ax.add_row("RY", f"[cyan]{_track(ry)}[/]", f"{ry:+.2f}")
  ax.add_row("L2", f"[magenta]{_bar(l2)}[/]", f"{l2:.2f}")
  ax.add_row("R2", f"[magenta]{_bar(r2)}[/]", f"{r2:.2f}")

  b = state.buttons
  hx, hy = state.hat(ABS_HAT0X), state.hat(ABS_HAT0Y)
  rows = [
      "  ".join((_chip("△", b["triangle"], "green"),
                 _chip("○", b["circle"], "red"),
                 _chip("✕", b["cross"], "bright_blue"),
                 _chip("□", b["square"], "magenta"))),
      "  ".join((_chip("▲", hy < 0, "white"), _chip("▼", hy > 0, "white"),
                 _chip("◀", hx < 0, "white"), _chip("▶", hx > 0, "white"))),
      "  ".join((_chip("L1", b["L1"], "yellow"), _chip("R1", b["R1"], "yellow"),
                 _chip("L2", b["L2b"], "yellow"), _chip("R2", b["R2b"], "yellow"))),
      "  ".join((_chip("SHARE", b["share"], "cyan"),
                 _chip("PS", b["PS"], "bright_white"),
                 _chip("OPTIONS", b["options"], "cyan"),
                 _chip("L3", b["L3"], "cyan"), _chip("R3", b["R3"], "cyan"))),
  ]
  return Panel(Group(hdr, "", ax, "", *rows),
               title="PS4 pad", border_style="blue")


def monitor(path, name, mac):
  try:
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
  except PermissionError:
    console.print(Panel(
        f"no read access to [bold]{path}[/]\n"
        "fix: add yourself to the [bold]input[/] group "
        "(sudo usermod -aG input $USER; re-login)",
        title="[red]permission denied", border_style="red"))
    sys.exit(1)
  try:
    state = PadState(fd)
    batt, bt_ok, sig = battery_pct(mac), bt_connected(mac), link_signal(mac)
    last_slow = time.time()
    with Live(render(state, name, path, mac, batt, bt_ok, sig),
              console=console, refresh_per_second=30) as live:
      while True:
        r, _, _ = select.select([fd], [], [], 0.03)
        if r:
          try:
            data = os.read(fd, EV_SIZE * 64)
          except OSError:      # pad went away mid-read
            return
          for off in range(0, len(data) - EV_SIZE + 1, EV_SIZE):
            _, _, etype, code, value = struct.unpack_from(EV_FMT, data, off)
            state.feed(etype, code, value)
        if time.time() - last_slow > 2.0:   # battery/bt/signal are subprocess-slow
          batt, bt_ok, sig = battery_pct(mac), bt_connected(mac), link_signal(mac)
          last_slow = time.time()
        live.update(render(state, name, path, mac, batt, bt_ok, sig))
  finally:
    os.close(fd)


def main():
  console.print(Panel.fit("[bold]PS4 pad live status[/]  (Ctrl-C quits)",
                          border_style="blue"))
  shown_wait = False
  while True:
    pad = find_pad()
    if pad:
      path, name, mac = pad
      console.print(f"pad online: [bold]{name}[/] [yellow]{mac}[/] [dim]{path}[/]")
      monitor(path, name, mac)          # returns on disconnect
      console.print("[red]pad disconnected[/] — waiting for it to return "
                    "(PS button reconnects)")
      shown_wait = True
      time.sleep(1.0)
      continue
    if not shown_wait:
      console.print(Panel(
          "no DS4 found. tap [bold]PS[/] to reconnect a paired pad,\n"
          "or pair one first:  [bold]ork.ix.ps4pad.bt.pair.py[/]",
          title="waiting", border_style="yellow"))
      shown_wait = True
    time.sleep(2.0)


if __name__ == "__main__":
  try:
    main()
  except KeyboardInterrupt:
    console.print("\nbye")
