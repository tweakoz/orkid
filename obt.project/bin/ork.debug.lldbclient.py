#!/usr/bin/env python3

################################################################################
# ork.debug.lldbclient.py — send one lldb command to an ork.debug.lldbserve.py
# session and print the reply. Non-interactive lldb browsing:
#
#   ork.debug.lldbclient.py --sessionid X run
#   ork.debug.lldbclient.py --sessionid X bt 20
#   ork.debug.lldbclient.py --sessionid X frame select 3
#   ork.debug.lldbclient.py --sessionid X p this->_items
#   ork.debug.lldbclient.py --sessionid X --timeout 30 continue
#   ork.debug.lldbclient.py --sessionid X --shutdown
#
# No --timeout = block until the server replies (a `run`/`continue` replies
# when the inferior STOPS — crash, breakpoint, or exit). A timed-out client
# is safe to abandon; the server finishes the command and serves the next
# request normally (the orphaned reply is dropped).
################################################################################

import sys, os, signal, time, argparse
import zmq

parser = argparse.ArgumentParser(description="client for ork.debug.lldbserve.py")
parser.add_argument("--sessionid", required=True, help="Session id (must match the server).")
parser.add_argument("--timeout", type=float, default=None, help="Seconds to wait for the reply (default: forever).")
parser.add_argument("--shutdown", action="store_true", help="Graceful shutdown (waits for the current command).")
parser.add_argument("--kill", action="store_true", help="KILL the session out-of-band (works mid-command).")
parser.add_argument("--debugserver", action="store_true",
                    help="talk to an in-process ork::debugserver (ORKID_DEBUG_SERVER=<sid>) instead of an lldb session.")
parser.add_argument("command", nargs=argparse.REMAINDER, help="The lldb command to run.")
args = parser.parse_args()

if args.kill:
  pid_path = f"/tmp/ork.lldbserve.{args.sessionid}.pid"
  ipc_file = f"/tmp/ork.lldbserve.{args.sessionid}.ipc"
  try:
    with open(pid_path) as f:
      pids = [int(x) for x in f.read().split()]
  except (OSError, ValueError):
    print(f"[lldbclient] no live session {args.sessionid!r} (no pidfile)")
    sys.exit(1)
  for pid in reversed(pids):  # lldb (and its inferior) first, then the server
    try:
      os.kill(pid, signal.SIGTERM)
    except ProcessLookupError:
      pass
  time.sleep(0.5)
  for pid in reversed(pids):
    try:
      os.kill(pid, signal.SIGKILL)
    except ProcessLookupError:
      pass
  for fp in (pid_path, ipc_file):
    try:
      os.unlink(fp)
    except OSError:
      pass
  print(f"[lldbclient] session {args.sessionid!r} KILLED (pids {pids})")
  sys.exit(0)

cmd = "__shutdown__" if args.shutdown else " ".join(args.command).strip()
if not cmd:
  print("no command given (or pass --shutdown / --kill)")
  sys.exit(1)

servkind = "debugserve" if args.debugserver else "lldbserve"
ipc_path = f"ipc:///tmp/ork.{servkind}.{args.sessionid}.ipc"

ctxz = zmq.Context()
sock = ctxz.socket(zmq.REQ)
sock.setsockopt(zmq.LINGER, 0)
sock.connect(ipc_path)
sock.send_string(cmd)

if args.timeout is not None:
  poller = zmq.Poller()
  poller.register(sock, zmq.POLLIN)
  if not poller.poll(int(args.timeout * 1000)):
    print(f"[lldbclient] TIMEOUT after {args.timeout}s — the server is still executing "
          f"{cmd!r}; retry later with a fresh client (the orphaned reply is dropped).")
    sock.close(0)
    ctxz.term()
    sys.exit(2)

print(sock.recv_string(), end="")
sock.close(0)
ctxz.term()
