#!/usr/bin/env python3

from ork import path, command

command.system([
    "rm",
    "-f",
    "%s/*"%path.dblockcache()
])
