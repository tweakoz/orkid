#!/usr/bin/env python3

from obt import command

# fetch all singularity namespace assets
command.run(["ork.asset.catalog.fetch.py", "-n", "singularity"], do_log=True)
command.run(["ork.asset.catalog.fetch.py", "-n", "ork_envmaps"], do_log=True)
