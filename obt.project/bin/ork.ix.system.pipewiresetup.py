#!/usr/bin/env python3

from obt import path, pathtools, command

share = path.stage()/"share"

pwconf_src = path.orkid()/"ork.data"/"misc"/"pipewire.conf"
pwconf_dst = share / "pipewire" / "ORKPW.conf"

pathtools.copyfile(pwconf_src, pwconf_dst)

command.run(["ln","-s","/lib/x86_64-linux-gnu/libncursesw.so.6", path.libs()/"libtinfow.so.6"])
