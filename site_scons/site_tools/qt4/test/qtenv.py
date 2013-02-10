#
# Copyright (c) 2001-2010 The SCons Foundation
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
# KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
# NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
# LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
# OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
# WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#


import os, sys, glob
from SCons import Environment

def extendQtPath(qtpath):
    if os.path.exists(os.path.join(qtpath,'qt','bin')):
        # Looks like a binary install of the Qt4 SDK,
        # so we add the 'qt' folder to the path...
        return os.path.join(qtpath,'qt')

    return qtpath

def detectLatestQtVersion():
    if sys.platform.startswith("linux"):
        # Inspect '/usr/local/Trolltech' first...
        paths = glob.glob('/usr/local/Trolltech/*')
        if len(paths):
            paths.sort()
            return extendQtPath(paths[-1])
        else:
            # ...then try to find a binary SDK.
            paths = glob.glob('/opt/qtsdk-*')
            if len(paths):
                paths.sort()
                return extendQtPath(path[-1])
            
    else:
        # Simple check for Windows: inspect only 'C:\Qt'
        paths = glob.glob('C:\\Qt\\*')
        if len(paths):
            paths.sort()
            return paths[-1]
        
    return os.environ.get("QTDIR","")

def detectPkgconfigPath(qtdir):
    pkgpath = os.path.join(qtdir, 'lib', 'pkgconfig')
    if os.path.exists(os.path.join(pkgpath,'QtCore.pc')):
        return pkgpath
    pkgpath = os.path.join(qtdir, 'lib')
    if os.path.exists(os.path.join(pkgpath,'QtCore.pc')):
        return pkgpath

    return ""
    
def createQtEnvironment(qtdir=None, env=None):
    if not env:
        env = Environment.Environment(tools=['default'])
    if not qtdir:
        qtdir = detectLatestQtVersion()
    env['QT4DIR'] = qtdir
    if sys.platform.startswith("linux"):
        env['ENV']['PKG_CONFIG_PATH'] = detectPkgconfigPath(qtdir)
    env.Tool('qt4')

    return env

