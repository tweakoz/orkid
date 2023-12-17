#!/usr/bin/env python3

from obt import path, command

command.system([
    "rm",
    "-f",
    "%s/*"%path.dblockcache()
])