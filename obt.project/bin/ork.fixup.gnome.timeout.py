#!/usr/bin/env python3 

from ork.command import Command

cmd = ["gsettings","set","org.gnome.mutter","check-alive-timeout","60000"]
Command(cmd).exec()
