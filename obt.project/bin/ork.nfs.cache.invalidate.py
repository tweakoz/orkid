#!/usr/bin/env python3


from obt import command

cmd_list = [
  "sudo",
  "sh",
  "-c",
  "'echo 3 > /proc/sys/vm/drop_caches'"
]

command.system(cmd_list,do_log=True)