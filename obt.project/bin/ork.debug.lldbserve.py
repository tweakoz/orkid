#!/usr/bin/env python3

################################################################################
# ork.debug.lldbserve.py — lldb behind a zmq REQ/REP interface.
#
# Hosts an lldb session on a target (same exe/arg resolution as
# ork.debug.lldb.py, including python-script → ork.python rewriting) and
# serves lldb commands over ipc, so a NON-interactive caller (an agent, a
# script, another terminal) can drive the debugger conversationally:
#
#   ork.debug.lldbserve.py ork.ecs.player.exe scene.ecs --sessionid X &
#   ork.debug.lldbclient.py --sessionid X run          # blocks until first stop
#   ork.debug.lldbclient.py --sessionid X bt 20
#   ork.debug.lldbclient.py --sessionid X frame select 3
#   ork.debug.lldbclient.py --sessionid X p this->_items
#   ork.debug.lldbclient.py --sessionid X thread list
#   ork.debug.lldbclient.py --sessionid X --shutdown
#
# Protocol: one command string per request; the reply is everything lldb
# printed up to the post-command sentinel. A command that resumes the
# inferior (run / continue) replies when the process STOPS (crash /
# breakpoint / exit) — that's the point: "run" returns the crash report.
# The inferior's own stdout/stderr interleaves into the reply (lldb
# forwards it), which is usually what you want when crash-hunting.
#
# An abandoned client (timeout / ^C) is safe: the server's pending reply
# goes nowhere and the next request proceeds normally.
################################################################################

import sys, os, subprocess, threading, queue, argparse, signal

from obt import path as obt_path
this_dir = obt_path.fileOfInvokingModule()
sys.path.append(str(this_dir))
import _debug_helpers

import zmq

SENTINEL = "__ORK_LLDBSERVE_DONE__"

################################################################################

# --sessionid may appear anywhere (incl. after the target args, which REMAINDER
# would otherwise swallow) — extract it from argv before argparse sees it.
argv = sys.argv[1:]
session_id = None
for i, a in enumerate(argv):
  if a == "--sessionid" and (i + 1) < len(argv):
    session_id = argv[i + 1]
    argv = argv[:i] + argv[i + 2:]
    break
  if a.startswith("--sessionid="):
    session_id = a.split("=", 1)[1]
    argv = argv[:i] + argv[i + 1:]
    break
if session_id is None:
  print("ork.debug.lldbserve.py: --sessionid <id> is required")
  sys.exit(1)

parser = argparse.ArgumentParser(description="LLDB zmq server (pairs with ork.debug.lldbclient.py)")
parser.add_argument("executable_name", help="Name of the executable (without path).")
parser.add_argument("exec_args", nargs=argparse.REMAINDER, help="Arguments for the executable.")
args = parser.parse_args(argv)
args.sessionid = session_id

exe_path, exe_args, exe_name = _debug_helpers.get_exec_and_args(args)

ipc_path = f"ipc:///tmp/ork.lldbserve.{args.sessionid}.ipc"

################################################################################
# lldb subprocess — line-buffered pipes; stderr folded into stdout so crash
# reports and inferior output ride the same stream the sentinel scanner reads.
# The `run` alias matches ork.debug.lldb.py (-X 0: no shell expansion).
################################################################################

lldb_cmd = ["lldb",
            "-o", f"command alias run process launch -X 0 -- {' '.join(exe_args)}",
            "--",
            exe_path] + exe_args

proc = subprocess.Popen(
    lldb_cmd,
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
    bufsize=1)

out_q = queue.Queue()

def _reader():
  for line in proc.stdout:
    out_q.put(line)
  out_q.put(None)  # EOF marker

reader = threading.Thread(target=_reader, daemon=True)
reader.start()

def send_command(cmd):
  """Write one lldb command + the sentinel probe; collect output until the
  sentinel echoes (i.e. lldb has processed everything before it). Returns
  (output, eof)."""
  proc.stdin.write(cmd + "\n")
  proc.stdin.write(f"script print('{SENTINEL}')\n")
  proc.stdin.flush()
  lines = []
  while True:
    line = out_q.get()
    if line is None:
      return ("".join(lines) + "\n[lldbserve] lldb EXITED\n", True)
    if SENTINEL in line:
      # drop the echo of the script command itself if present
      return ("".join(l for l in lines if SENTINEL not in l), False)
    lines.append(line)

def drain_for(seconds):
  """__drain__ N — collect whatever lldb/the inferior prints for N seconds
  (async stop reports, inferior stdout). Pairs with async commands."""
  import time
  lines = []
  deadline = time.time() + seconds
  while True:
    remain = deadline - time.time()
    if remain <= 0:
      break
    try:
      line = out_q.get(timeout=remain)
    except queue.Empty:
      break
    if line is None:
      lines.append("\n[lldbserve] lldb EXITED\n")
      break
    lines.append(line)
  return "".join(lines)

# SYNCHRONOUS mode: process launch/continue BLOCK until the next stop, so the
# sentinel (and therefore the reply) carries the stop report — `run` returns
# the crash. Without this lldb is async and run's reply beats the stop output.
send_command("script lldb.debugger.SetAsync(False)")

# drain lldb's startup banter (target create etc.) so the first reply is clean
startup, _ = send_command("version")

################################################################################
# zmq REP loop
################################################################################

# pidfile — lets the client --kill this session OUT-OF-BAND (works even while
# the server is blocked mid-command, where a protocol-level shutdown would queue).
pid_path = f"/tmp/ork.lldbserve.{args.sessionid}.pid"
with open(pid_path, "w") as f:
  f.write(f"{os.getpid()} {proc.pid}\n")

ctxz = zmq.Context()
sock = ctxz.socket(zmq.REP)
sock.bind(ipc_path)
print(f"[lldbserve] session<{args.sessionid}> target<{exe_path}> at {ipc_path}", flush=True)
print(f"[lldbserve] args<{exe_args}>", flush=True)

try:
  while True:
    cmd = sock.recv_string()
    if cmd == "__shutdown__":
      sock.send_string("[lldbserve] shutting down\n")
      break
    if cmd == "__startup__":
      sock.send_string(startup)
      continue
    if cmd.startswith("__drain__"):
      try:
        secs = float(cmd.split()[1])
      except (IndexError, ValueError):
        secs = 5.0
      sock.send_string(drain_for(secs))
      continue
    output, eof = send_command(cmd)
    sock.send_string(output)
    if eof:
      break
finally:
  try:
    proc.stdin.write("process kill\nquit\n")
    proc.stdin.flush()
  except Exception:
    pass
  try:
    proc.wait(timeout=5)
  except Exception:
    proc.kill()
  sock.close(0)
  ctxz.term()
  for fp in (ipc_path.replace("ipc://", ""), pid_path):
    try:
      os.unlink(fp)
    except OSError:
      pass
  print("[lldbserve] done", flush=True)
