#!/usr/bin/env ork.python
################################################################
# ork.ix.ps4pad.bt.pair.py — pair a PS4 DualShock 4 controller
#  over bluetooth (linux side of the pairing handshake).
#
#  Controller side: with the pad OFF, hold SHARE then PS together
#  ~5s until the lightbar double-flashes white (pairing mode).
#  This script does everything else: adapter power, scan,
#  pair / trust / connect.
#
#  --forget : drop any existing DS4 pairing first (use when a pad
#             was previously paired to another host and refuses
#             to reconnect).
################################################################

import argparse
import re
import subprocess
import sys
import time

from rich.console import Console
from rich.panel import Panel

console = Console()

# names a DS4 advertises over BT (hid-sony era pads say just
# "Wireless Controller"; some fw says "DUALSHOCK 4 Wireless Controller")
DS4_NAME_RX = re.compile(r"wireless controller|dualshock", re.IGNORECASE)

SCAN_TIMEOUT_SECS = 60.0


def btctl(*args, timeout=15):
  """one-shot bluetoothctl command -> (rc, combined output)"""
  try:
    r = subprocess.run(
        ["bluetoothctl"] + list(args),
        capture_output=True, text=True, timeout=timeout)
    return r.returncode, (r.stdout + r.stderr)
  except FileNotFoundError:
    console.print("[bold red]bluetoothctl not found[/] — install bluez")
    sys.exit(1)
  except subprocess.TimeoutExpired:
    return 1, "(timeout)"


def known_ds4s():
  """[(mac, name)] of DS4s bluez already knows about"""
  _, out = btctl("devices")
  found = []
  for line in out.splitlines():
    m = re.match(r"Device ((?:[0-9A-F]{2}:){5}[0-9A-F]{2}) (.*)", line)
    if m and DS4_NAME_RX.search(m.group(2)):
      found.append((m.group(1), m.group(2)))
  return found


def is_connected(mac):
  _, out = btctl("info", mac)
  return "Connected: yes" in out


def is_paired(mac):
  _, out = btctl("info", mac)
  return "Paired: yes" in out


def host_bt_status():
  """host-side adapter facts: presence, identity, power, rfkill, flags"""
  st = {"present": False, "mac": "", "name": "", "powered": False,
        "pairable": False, "discoverable": False, "discovering": False,
        "soft_block": False, "hard_block": False, "daemon": True}
  _, out = btctl("show")
  m = re.search(r"Controller ((?:[0-9A-F]{2}:){5}[0-9A-F]{2})", out)
  if m:
    st["present"] = True
    st["mac"] = m.group(1)
    for key, field in (("name", "Name"), ):
      fm = re.search(rf"{field}: (.*)", out)
      st[key] = fm.group(1).strip() if fm else ""
    for key, field in (("powered", "Powered"), ("pairable", "Pairable"),
                       ("discoverable", "Discoverable"),
                       ("discovering", "Discovering")):
      st[key] = f"{field}: yes" in out
    # WHY pairable is off, when it is. Pairable only gates INCOMING pair
    # requests (a remote asking to pair with this host); this script pairs
    # OUTGOING, so 'no' never blocks it. bluez defaults AlwaysPairable=false
    # and only a bluetooth agent (a desktop applet, or bluetoothctl
    # 'pairable on') flips it — nothing does on a headless box.
    if not st["pairable"]:
      tm = re.search(r"PairableTimeout: \S+ \((\d+)\)", out)
      if tm and int(tm.group(1)) > 0:
        st["pairable_why"] = f"was on, timed out after {tm.group(1)}s"
      else:
        try:
          conf = open("/etc/bluetooth/main.conf").read()
        except OSError:
          conf = ""
        if re.search(r"^\s*AlwaysPairable\s*=\s*true", conf, re.MULTILINE):
          st["pairable_why"] = "off despite AlwaysPairable=true (agent?)"
        else:
          st["pairable_why"] = ("never enabled since boot — headless, no bt "
                                "agent (harmless: only gates incoming pairs)")
  elif "not available" in out.lower() or not out.strip():
    # bluetoothctl talks to bluetoothd over dbus — silence usually
    # means the daemon isn't up, not that the adapter is missing
    st["daemon"] = "org.bluez" not in out
  try:
    r = subprocess.run(["rfkill", "list", "bluetooth"],
                       capture_output=True, text=True, timeout=5)
    st["soft_block"] = "Soft blocked: yes" in r.stdout
    st["hard_block"] = "Hard blocked: yes" in r.stdout
  except (FileNotFoundError, subprocess.TimeoutExpired):
    pass
  return st


def _yn(flag, good_when=True):
  ok = (flag == good_when)
  word = "yes" if flag else "no"
  return f"[green]{word}[/]" if ok else f"[red]{word}[/]"


def show_host_status(st):
  from rich.table import Table
  t = Table.grid(padding=(0, 2))
  t.add_column(style="dim", justify="right")
  t.add_column()
  if st["present"]:
    t.add_row("adapter", f"[bold]{st['name'] or '(unnamed)'}[/]  [yellow]{st['mac']}[/]")
    t.add_row("powered", _yn(st["powered"]))
    pairable_cell = _yn(st["pairable"])
    if not st["pairable"] and st.get("pairable_why"):
      pairable_cell += f"  [dim]({st['pairable_why']})[/]"
    t.add_row("pairable", pairable_cell)
    t.add_row("discoverable", _yn(st["discoverable"], good_when=st["discoverable"]))
    t.add_row("scanning", _yn(st["discovering"], good_when=st["discovering"]))
  else:
    t.add_row("adapter", "[red]none found[/]")
    if not st["daemon"]:
      t.add_row("bluetoothd", "[red]not running[/]  (systemctl start bluetooth)")
  if st["hard_block"]:
    t.add_row("rfkill", "[red]HARD blocked[/]  (physical switch / BIOS)")
  elif st["soft_block"]:
    t.add_row("rfkill", "[red]soft blocked[/]  (rfkill unblock bluetooth)")
  else:
    t.add_row("rfkill", "[green]clear[/]")
  console.print(Panel(t, title="host bluetooth", border_style="blue"))


def ensure_adapter_on():
  st = host_bt_status()
  show_host_status(st)
  if not st["present"]:
    console.print(Panel(
        "no usable bluetooth adapter.\n"
        "checks: bluetoothd running? adapter plugged in? rfkill clear?",
        title="[red]cannot continue", border_style="red"))
    sys.exit(1)
  if st["hard_block"]:
    console.print("[red]adapter is hard-blocked — flip the physical "
                  "radio switch / check BIOS[/]")
    sys.exit(1)
  if st["soft_block"]:
    console.print("adapter is rfkill-soft-blocked — unblocking")
    subprocess.run(["rfkill", "unblock", "bluetooth"], capture_output=True)
  if not st["powered"]:
    console.print("adapter is off — powering on")
    rc, out = btctl("power", "on")
    if "succeeded" not in out:
      console.print(Panel(out.strip() or "(no output)",
                          title="[red]could not power on the adapter",
                          border_style="red"))
      sys.exit(1)
    show_host_status(host_bt_status())   # re-show now that it's up


def pair_trust_connect(mac):
  for verb, ok_marker in (("pair", "Pairing successful"),
                          ("trust", "trust succeeded"),
                          ("connect", "Connection successful")):
    with console.status(f"[cyan]{verb} {mac} ..."):
      _, out = btctl(verb, mac, timeout=30)
    if ok_marker.lower() not in out.lower():
      # pair fails with AlreadyExists when re-running on a paired pad —
      # that's fine, keep going toward connect
      if "AlreadyExists" in out:
        console.print(f"  {verb}: already done")
        continue
      console.print(Panel(out.strip() or "(no output)",
                          title=f"[red]{verb} failed", border_style="red"))
      return False
    console.print(f"  [green]{verb}: ok[/]")
  return True


def main():
  ap = argparse.ArgumentParser(
      description="pair a PS4 DualShock 4 over bluetooth")
  ap.add_argument("--forget", action="store_true",
                  help="remove any existing DS4 pairing before re-pairing")
  args = ap.parse_args()

  console.print(Panel.fit("[bold]PS4 pad bluetooth pairing[/]",
                          border_style="blue"))
  ensure_adapter_on()

  already = known_ds4s()

  if args.forget:
    for mac, name in already:
      console.print(f"forgetting [yellow]{name}[/] {mac}")
      btctl("remove", mac)
    already = []

  # fast path: a known pad that just needs (re)connecting. A DS4 pairing is
  # non-bonded, so it does NOT survive a bluetoothd restart — such pads show
  # up here as known-but-unpaired husks: drop those so the scan below sees
  # them as new and re-pairs cleanly.
  for mac, name in list(already):
    if not is_paired(mac):
      console.print(f"dropping stale unpaired entry [dim]{name} {mac}[/]")
      btctl("remove", mac)
      already.remove((mac, name))
      continue
    if is_connected(mac):
      console.print(Panel.fit(
          f"[bold green]{name}[/] {mac} is already connected",
          border_style="green"))
      return 0
    console.print(f"found paired pad [yellow]{name}[/] {mac} — trying to connect"
                  " (tap the PS button to wake it)")
    _, out = btctl("connect", mac, timeout=20)
    if "Connection successful" in out:
      console.print(Panel.fit(f"[bold green]connected[/] {name} {mac}",
                              border_style="green"))
      return 0
    console.print("  reconnect failed — falling through to a fresh pairing"
                  " (use --forget if it keeps failing)")

  console.print(Panel(
      "put the pad in pairing mode NOW:\n"
      "  with the pad [bold]off[/], hold [bold cyan]SHARE[/] then "
      "[bold cyan]PS[/] together ~5s\n"
      "  until the lightbar does quick white [bold]double-flashes[/]",
      title="controller side", border_style="cyan"))

  # `bluetoothctl scan on` only keeps discovering while attached to a tty
  # (it exits at once with piped/absent stdin, silently scanning 0s), so
  # loop bounded --timeout scans instead. The DS4 is classic-BT only —
  # `scan bredr` (bluez >= 5.65ish) skips LE noise; fall back to plain on.
  known_before = {mac for mac, _ in already}
  target = None
  scan_arg = "bredr"
  with console.status("[cyan]scanning for the pad ...") as status:
    deadline = time.time() + SCAN_TIMEOUT_SECS
    while time.time() < deadline and not target:
      status.update(
          f"[cyan]scanning for the pad ... {int(deadline - time.time())}s left")
      _, out = btctl("--timeout", "6", "scan", scan_arg, timeout=12)
      if scan_arg == "bredr" and "Invalid argument" in out:
        scan_arg = "on"       # older bluetoothctl: no transport arg
        continue
      for mac, name in known_ds4s():
        if mac not in known_before:
          target = (mac, name)
          break

  if not target:
    console.print(Panel(
        "no pad appeared within %ds.\n"
        "checks: lightbar actually double-flashing? pad charged?\n"
        "still paired to another host? (run again with --forget)"
        % int(SCAN_TIMEOUT_SECS),
        title="[red]not found", border_style="red"))
    return 1

  mac, name = target
  console.print(f"discovered [bold]{name}[/] [yellow]{mac}[/]")
  if not pair_trust_connect(mac):
    return 1

  console.print(Panel.fit(
      f"[bold green]paired + connected[/]  {name}  {mac}\n"
      "from now on a single PS-button tap reconnects it\n"
      "check it live with:  [bold]ork.ix.ps4pad.bt.status.py[/]",
      border_style="green"))
  return 0


if __name__ == "__main__":
  sys.exit(main())
