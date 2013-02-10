#!/usr/bin/env python
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
#
# create_scons_test.py
#
# Populates a Qt "examples" directory with files for the
# SCons test framework.
#
#
#
# Usage:
#
# Step 1: Copy the "examples" folder of your Qt source tree into
#         this directory.
# Step 2: Run "python create_scons_tests.py" to create all files
#         for the SCons test framework.
# Step 3: Execute "python runtest.py -a" to run all tests
#
# Additional options for this script are:
#
#   -local               Creates the test files in the local directory,
#                        also copies qtenv.py and qt4.py to their correct
#                        places.
#   -clean               Removes all intermediate test files.
#
#

import os, sys, re, glob, shutil

# cxx file extensions
cxx_ext = ['.c', '.cpp', '.cxx', '.cc']
# header file extensions
h_ext = ['.h', '.hpp', '.hxx']
# regex for includes
inc_re = re.compile(r'#include\s+<([^>]+)>')
# regex for qtLibraryTarget function
qtlib_re = re.compile(r'\$\$qtLibraryTarget\(([^\)]+)\)')
# we currently skip all .pro files that use these config values
complicated_configs = ['qdbus','phonon']
# for the following CONFIG values we have to provide default qt modules
config_modules = {'designer' : ['QtCore','QtGui','QtXml','QtScript','QtDesigner'],
                  'uitools' : ['QtCore','QtGui','QtUiTools'],
                  'assistant' : ['QtCore','QtGui','QtXml','QtScript','QtAssistant'],
                  'core' : ['QtCore'],
                  'qt' : ['QtCore','QtGui'],
                  'xml' : ['QtCore','QtGui','QtXml'],
                  'webkit' : ['QtCore','QtGui','QtXmlPatterns','QtWebKit'],
                  'network' : ['QtCore','QtGui','QtNetwork'],
                  'svg' : ['QtCore','QtGui','QtSvg'],
                  'script' : ['QtCore','QtGui','QtScript','QtScriptTools'],
                  'scripttools' : ['QtCore','QtGui','QtXml','QtScriptTools'],
                  'multimedia' : ['QtCore','QtGui','QtMultimedia'],
                  'script' : ['QtCore','QtGui','QtScript'],
                  'help' : ['QtCore','QtGui','QtXml','QtSql','QtNetwork','QtHelp'],
                  'qaxserver' : ['QtCore','QtGui','QAxFactory'],
                  'qaxcontainer' : ['QtCore','QtGui','QAxFactory'],
                  'qtestlib' : ['QtCore','QtGui','QtTest'],
                  'qt3support' : ['QtCore','QtGui','Qt3Support'],
                  'opengl' : ['QtCore','QtGui','QtOpenGL']
                  }
# for the following CONFIG values we have to provide additional CPP defines
config_defines = {'plugin' : ['QT_PLUGIN'],
                  'designer' : ['QDESIGNER_EXPORT_WIDGETS']
                 }

# dictionary of special Qt Environment settings for all single tests/pro files
qtenv_flags = {'QT4_GOBBLECOMMENTS' : '1'
              }

# available qt modules
valid_qt_modules = [
        'QtCore',
        'QtGui',
        'QtOpenGL',
        'Qt3Support',
        'QtAssistant', # deprecated
        'QtAssistantClient',
        'QtScript',
        'QtDBus',
        'QtSql',
        'QtSvg',
        # The next modules have not been tested yet so, please
        # maybe they require additional work on non Linux platforms
        'QtNetwork',
        'QtTest',
        'QtXml',
        'QtXmlPatterns',
        'QtUiTools',
        'QtDesigner',
        'QtDesignerComponents',
        'QtWebKit',
        'QtHelp',
        'QtScript',
        'QAxFactory',
        'QtScriptTools',
        'QtMultimedia']

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

def parseProFile(fpath):
    """ Parse the .pro file fpath and return the defined
    variables in a dictionary.
    """
    keys = {}
    f = open(fpath,'r')
    curkey = None
    curlist = []
    for l in f.readlines():
        l = l.rstrip('\n')
        l = l.rstrip()
        kl = l.split('=')
        if len(kl) > 1:
            # Close old key
            if curkey:
                if keys.has_key(curkey):
                    keys[curkey].extend(curlist)
                else:
                    keys[curkey] = curlist
                
            # Split off optional leading part with "contains():"
            cl = kl[0].split(':')
            if (l.find('lesock') < 0) and ((len(cl) < 2) or (cl[0].find('msvc') < 0)):
                nkey = cl[-1]
                # Open new key
                curkey = nkey.split()[0]
                value = kl[1].lstrip()
                if value.endswith('\\'):
                    # Key is continued on next line
                    value = value[:-1]
                    curlist = value.split()
                else:
                    # Single line key
                    if keys.has_key(curkey):
                        keys[curkey].extend(value.split())
                    else:
                        keys[curkey] = value.split()
                    curkey = None
                    curlist = []
        else:
            if l.endswith('\\'):
                # Continue current key
                curlist.extend(l[:-1].split())
            else:
                # Unknown, so go back to VOID state
                if curkey:
                    # Append last item for current key...
                    curlist.extend(l.split())
                    if keys.has_key(curkey):
                        keys[curkey].extend(curlist)
                    else:
                        keys[curkey] = curlist
                        
                    # ... and reset parse state.
                    curkey = None
                    curlist = []
    f.close()
    
    return keys

def writeSConstruct(dirpath):
    """ Create a SConstruct file in dirpath.
    """
    sc = open(os.path.join(dirpath,'SConstruct'),'w')
    sc.write("""import qtenv

qtEnv = qtenv.createQtEnvironment()
Export('qtEnv')
SConscript('SConscript')
    """)
    sc.close()

def collectModulesFromFiles(fpath):
    """ Scan source files in dirpath for included
    Qt4 modules, and return the used modules in a list.
    """
    mods = []
    try:
        f = open(fpath,'r')
        content = f.read()
        f.close()
    except:
        return mods
    
    for m in inc_re.finditer(content):
        mod = m.group(1)
        if (mod in valid_qt_modules) and (mod not in mods):
            mods.append(mod)
    return mods

def validKey(key, pkeys):
    """ Helper function
    """
    if pkeys.has_key(key) and len(pkeys[key]) > 0:
        return True
    
    return False

def collectModules(dirpath, pkeys):
    """ Scan source files in dirpath for included
    Qt4 modules, and return the used modules in a list.
    """
    mods = []
    defines = []
    # Scan subdirs
    if validKey('SUBDIRS', pkeys):
        for s in pkeys['SUBDIRS']:
            flist = glob.glob(os.path.join(dirpath, s, '*.*'))
            for f in flist:
                root, ext = os.path.splitext(f)
                if (ext and ((ext in cxx_ext) or
                             (ext in h_ext))):
                    mods.extend(collectModulesFromFiles(f))
                    
    # Scan sources
    if validKey('SOURCES', pkeys):
        for s in pkeys['SOURCES']:
            f = os.path.join(dirpath,s)
            mods.extend(collectModulesFromFiles(f))
    
    # Scan headers
    if validKey('HEADERS', pkeys):
        for s in pkeys['HEADERS']:
            f = os.path.join(dirpath,s)
            mods.extend(collectModulesFromFiles(f))
            
    # Check CONFIG keyword
    if validKey('CONFIG', pkeys):
        for k in pkeys['CONFIG']:
            if config_modules.has_key(k):
                mods.extend(config_modules[k])
            if config_defines.has_key(k):
                defines.extend(config_defines[k])

    # Check QT keyword
    if validKey('QT', pkeys):
        for k in pkeys['QT']:
            if config_modules.has_key(k):
                mods.extend(config_modules[k])

    # Make lists unique
    unique_mods = []
    for m in mods:
        if m not in unique_mods:
            unique_mods.append(m)
    unique_defines = []
    for d in defines:
        if d not in unique_defines:
            unique_defines.append(d)

    # Safety hack, if no modules are found so far
    # assume that this is a normal Qt GUI application...
    if len(unique_mods) == 0:
        unique_mods = ['QtCore','QtGui']

    return (unique_mods, unique_defines)

def writeSConscript(dirpath, profile, pkeys):
    """ Create a SConscript file in dirpath.
    """

    # Activate modules
    mods, defines = collectModules(dirpath, pkeys)
    if validKey('CONFIG', pkeys) and isComplicated(pkeys['CONFIG'][0]):
        return False
    allmods = True
    for m in mods:
        if m not in pkeys['qtmodules']:
            print "   no module %s" % m
            allmods = False
    if not allmods:
        return False

    sc = open(os.path.join(dirpath,'SConscript'),'w')
    sc.write("""Import('qtEnv')

env = qtEnv.Clone()
""")

    if len(mods):
        sc.write('env.EnableQt4Modules([\n')
        for m in mods[:-1]:
            sc.write("'%s',\n" % m)
        sc.write("'%s'\n" % mods[-1])
        sc.write('])\n\n')

    # Add CPPDEFINEs
    if len(defines):
        sc.write('env.AppendUnique(CPPDEFINES=[\n')
        for d in defines[:-1]:
            sc.write("'%s',\n" % d)
        sc.write("'%s'\n" % defines[-1])
        sc.write('])\n\n')
    
    # Add LIBS
    if validKey('LIBS', pkeys):
        sc.write('env.AppendUnique(LIBS=[\n')
        for d in pkeys['LIBS'][:-1]:
            sc.write("'%s',\n" % d)
        sc.write("'%s'\n" % pkeys['LIBS'][-1])
        sc.write('])\n\n')
 
    # Add special environment flags
    if len(qtenv_flags):
        for key, value in qtenv_flags.iteritems():    
            sc.write("env['%s']=%s\n" % (key, value))
    
    
    # Write source files
    if validKey('SOURCES', pkeys):
        sc.write('source_files = [\n')
        for s in pkeys['SOURCES'][:-1]:
            sc.write("'%s',\n" % s)
        sc.write("'%s'\n" % pkeys['SOURCES'][-1])
        sc.write(']\n\n')
    
    # Write .ui files
    if validKey('FORMS', pkeys):
        sc.write('ui_files = [\n')
        for s in pkeys['FORMS'][:-1]:
            sc.write("'%s',\n" % s)
        sc.write("'%s'\n" % pkeys['FORMS'][-1])
        sc.write(']\n')
        sc.write('env.Uic4(ui_files)\n\n')
    
    # Write .qrc files
    if validKey('RESOURCES', pkeys):
        qrc_name = pkeys['RESOURCES'][0]
        if not qrc_name.endswith('.qrc'):
            qrc_name += ".qrc"
        sc.write("source_files.append(['%s'])\n" % qrc_name)
    
    # Select module
    type = 'Program'
    if validKey('TEMPLATE', pkeys):
        if pkeys['TEMPLATE'][0] == 'lib':
            type = 'StaticLibrary'
        if pkeys['TEMPLATE'][0] == 'dll':
            type = 'SharedLibrary'

    # TARGET may be wrapped by qtLibraryTarget function...    
    target = profile
    if validKey('TARGET', pkeys):
        t = pkeys['TARGET'][0]
        m = qtlib_re.search(t)
        if m:
            t = "Qt" + m.group(1)
        target = t.replace("$$TARGET", profile)
        
    # Create program/lib/dll
    if validKey('SOURCES', pkeys):    
        sc.write("env.%s('%s', source_files)\n\n" % (type, target))
    else:
        sc.write("env.%s('%s', Glob('*.cpp'))\n\n" % (type, target))
        
    sc.close()

    return True

def writeSConsTestFile(dirpath, folder):
    updirs = dirpath.count('/')+1
    f = open(os.path.join(dirpath, "sconstest-%s.py" % folder),'w')
    f.write("""
import TestSCons

test = TestSCons.TestSCons()
test.dir_fixture("%s")
test.file_fixture('%sqtenv.py')
test.file_fixture('%s__init__.py','site_scons/site_tools/qt4/__init__.py')
test.run()

test.pass_test()
    """ % (folder, '../'*updirs, '../'*(updirs+1)))
    f.close()

def installLocalFiles(dirpath):
    updirs = dirpath.count('/')+1
    shutil.copy(os.path.join(dirpath,'../'*updirs+'qtenv.py'),
                os.path.join(dirpath,'qtenv.py'))
    toolpath = os.path.join(dirpath,'site_scons','site_tools','qt4')
    if not os.path.exists(toolpath):
        os.makedirs(toolpath)
    shutil.copy(os.path.join(dirpath,'../'*(updirs+1)+'__init__.py'),
                os.path.join(dirpath,'site_scons','site_tools','qt4','__init__.py'))
    
def isComplicated(keyvalues):
    for s in keyvalues:
        if s in complicated_configs:
            return True
    
    return False

def createSConsTest(dirpath, profile, options):
    """ Create files for the SCons test framework in dirpath.
    """
    pkeys = parseProFile(os.path.join(dirpath, profile))
    if validKey('TEMPLATE', pkeys) and pkeys['TEMPLATE'][0] == 'subdirs':
        return
    if validKey('CONFIG', pkeys) and isComplicated(pkeys['CONFIG']):
        return
    if validKey('QT', pkeys) and isComplicated(pkeys['QT']):
        return

    head, tail = os.path.split(dirpath)
    if head and tail:
        print os.path.join(dirpath, profile)
        pkeys['qtmodules'] = options['qtmodules']
        if not writeSConscript(dirpath, profile[:-4], pkeys):
            return
        writeSConstruct(dirpath)
        writeSConsTestFile(head, tail)
        if options['local']:
            installLocalFiles(dirpath)

def cleanSConsTest(dirpath, profile, options):
    """ Remove files for the SCons test framework in dirpath.
    """
    try:
        os.remove(os.path.join(dirpath,'SConstruct'))
    except:
        pass
    try:
        os.remove(os.path.join(dirpath,'SConscript'))
    except:
        pass
    try:
        os.remove(os.path.join(dirpath,'qtenv.py'))
    except:
        pass
    try:
        shutil.rmtree(os.path.join(dirpath,'site_scons'),
                      ignore_errors=True)
    except:
        pass
    head, tail = os.path.split(dirpath)
    if head and tail:
        try:
            os.remove(os.path.join(head, "sconstest-%s.py" % tail))
        except:
            pass
        
def main():
    """ The main program.
    """

    # Parse command line options
    options = {'local' : False, # Install qtenv.py and qt4.py in local folder
               'qtpath' : None,
               'pkgconfig' : None
               }
    clean = False
    qtpath = None
    for o in sys.argv[1:]:
        if o == "-local":
            options['local'] = True
        elif o == "-clean":
            clean = True
        else:
            options['qtpath'] = o
    
    if not options['qtpath']:
        qtpath = detectLatestQtVersion()
        if qtpath == "":
            print "No Qt installation found!"
            sys.exit(1)

        options['pkgconfig'] = detectPkgconfigPath(qtpath)
        if options['pkgconfig'] == "":
            print "No pkgconfig files found!"
            sys.exit(1)

        options['qtpath'] = qtpath
        options['qtmodules'] = []
        for v in valid_qt_modules:
            if os.path.exists(os.path.join(options['pkgconfig'],v+'.pc')):
                options['qtmodules'].append(v)

    if not clean:
        doWork = createSConsTest
    else:
        doWork = cleanSConsTest
        
    # Detect .pro files
    for path, dirs, files in os.walk('.'):
        for f in files:
            if f.endswith('.pro'):
                doWork(path, f, options)

if __name__ == "__main__":
    main()
