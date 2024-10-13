#!/usr/bin/env python3
################################################################################
from obt import path, command
from obt.pathtools import ensureDirectoryExists
from obt.wget import batch_wget
from yarl import URL
################################################################################
command.run([
  "ork.assetpak.fetch.py","-p","std"
])
