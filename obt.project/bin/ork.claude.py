#!/usr/bin/env python

import os, argparse
from ork import path as ork_path
from obt import command, host

os.chdir(str(ork_path.root))

HOME = os.environ["HOME"]

cmd = ["claude"]
if host.IsOsx:
  cmd = [f"{HOME}/.local/bin/claude"]
prompt  = ["read <orkid>/ork.data/misc/session_notes.md.\n"]
prompt += ["And report back.\n"]

joined_prompt = "".join(prompt)
cmd += [f'{joined_prompt}']

command.run(cmd,do_log=True)
