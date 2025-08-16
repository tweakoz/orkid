#!/usr/bin/env python3

from obt import command

# fetch all singularity namespace assets
command.run(["ork.asset.catalog.fetch.py", 
             "-p", "singularity|casiocz",
             "-p", "singularity|tx81z",
             "-p", "singularity|irs",
             "-p", "singularity|wavs",
             "-p", "singularity|kurzweil",
             "-p", "singularity|midifiles",
             "--parallel", "6",
             ], do_log=True)
