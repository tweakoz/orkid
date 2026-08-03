#!/usr/bin/env python3

import argparse
import getpass
import glob
import grp
import platform
import pwd
import resource
import subprocess
import sys

TAG = "[ork.ix.setup.audiortpri]"
DROPIN = "/etc/security/limits.d/40-ork-audio.conf"
DROPIN_RULE = "@audio - rtprio 95"
PREFERRED = ["pipewire", "audio"]

###############################################################################

def verdict(ok, msg):
  print("%s %s %s" % (TAG, "ok" if ok else "FAIL", msg))
  sys.exit(0 if ok else 1)

def softlimit():
  return resource.getrlimit(resource.RLIMIT_RTPRIO)

def limitsfiles():
  return ["/etc/security/limits.conf"] + sorted(glob.glob("/etc/security/limits.d/*.conf"))

def scanrules():
  rules = []
  for path in limitsfiles():
    try:
      lines = open(path, "r").read().splitlines()
    except OSError:
      continue
    for line in lines:
      line = line.split("#")[0].strip()
      items = line.split()
      if len(items) != 4:
        continue
      domain, ltype, item, value = items
      if not domain.startswith("@"):
        continue
      if item.lower() != "rtprio":
        continue
      if ltype.lower() not in ("-", "soft", "hard"):
        continue
      try:
        value = int(value)
      except ValueError:
        continue
      if value > 0:
        rules.append((domain[1:], value, path))
  return rules

def groupexists(name):
  try:
    grp.getgrnam(name)
    return True
  except KeyError:
    return False

def usergroups(user):
  names = set()
  for g in grp.getgrall():
    if user in g.gr_mem:
      names.add(g.gr_name)
  try:
    names.add(grp.getgrgid(pwd.getpwnam(user).pw_gid).gr_name)
  except KeyError:
    pass
  return names

def pick(rules):
  present = [r for r in rules if groupexists(r[0])]
  for want in PREFERRED:
    for r in present:
      if r[0] == want:
        return r
  return present[0] if present else None

def sudorun(cmd, stdin=None):
  print("  running: sudo %s" % " ".join(cmd))
  rc = subprocess.run(["sudo"] + cmd, input=stdin, text=True).returncode
  if rc != 0:
    verdict(False, "sudo %s failed (rc %d)" % (cmd[0], rc))

def relogin(value):
  print("")
  print("  ###########################################################################")
  print("  # the rtprio grant takes effect at your NEXT LOGIN - not in this shell.")
  print("  # any obtnet node daemon started from an older login must be restarted")
  print("  # from a fresh login, or jobs routed through it still run with rtprio 0.")
  print("  # verify after relogin with:  ulimit -r   (expect %s)" % value)
  print("  ###########################################################################")
  print("")

###############################################################################

def docheck(user):
  soft, hard = softlimit()
  rules = scanrules()
  mine = usergroups(user)
  print("user: %s" % user)
  print("rtprio rlimit: soft=%s hard=%s" % (soft, hard))
  if len(rules) == 0:
    print("granting rules: none")
  for group, value, path in rules:
    print("granting rule: @%s rtprio %d  [%s]  exists=%s member=%s"
          % (group, value, path, groupexists(group), group in mine))
  granting = [r for r in rules if r[0] in mine]
  if soft > 0:
    verdict(True, "armed - rtprio soft limit %d in this session" % soft)
  if len(granting) > 0:
    group, value, path = granting[0]
    verdict(True, "pending relogin - member of @%s (rtprio %d) but this session has soft limit 0"
                  % (group, value))
  verdict(False, "not armed - rtprio soft limit 0 and %s is in no granting group" % user)

def doapply(user):
  soft, hard = softlimit()
  print("user: %s" % user)
  print("rtprio rlimit: soft=%s hard=%s" % (soft, hard))
  if soft > 0:
    verdict(True, "already armed - rtprio soft limit %d, nothing to do" % soft)

  rules = scanrules()
  mine = usergroups(user)
  granting = [r for r in rules if r[0] in mine]
  if len(granting) > 0:
    group, value, path = granting[0]
    print("granting rule: @%s rtprio %d  [%s] - already a member" % (group, value, path))
    relogin(value)
    verdict(True, "pending relogin - already member of @%s (rtprio %d)" % (group, value))

  chosen = pick(rules)
  if chosen is not None:
    group, value, path = chosen
    print("granting rule: @%s rtprio %d  [%s] - adding %s" % (group, value, path, user))
    sudorun(["usermod", "-aG", group, user])
    relogin(value)
    verdict(True, "added %s to @%s (rtprio %d from %s)" % (user, group, value, path))

  print("no rtprio grant found in %s" % " ".join(limitsfiles()))
  print("installing %s: %s" % (DROPIN, DROPIN_RULE))
  sudorun(["tee", DROPIN], stdin=DROPIN_RULE + "\n")
  sudorun(["usermod", "-aG", "audio", user])
  relogin(95)
  verdict(True, "installed %s and added %s to @audio (rtprio 95)" % (DROPIN, user))

###############################################################################

parser = argparse.ArgumentParser(
  description="grant realtime scheduling priority (rtprio) for audio to the current user")
parser.add_argument("--check", action="store_true",
                    help="report state only, change nothing (exit 0 if grant armed or pending relogin)")
args = parser.parse_args()

if platform.system() != "Linux":
  print("this op configures linux PAM rlimits (/etc/security/limits.d) - %s is not supported"
        % platform.system())
  verdict(False, "not linux (%s)" % platform.system())

user = getpass.getuser()

if args.check:
  docheck(user)
else:
  doapply(user)
