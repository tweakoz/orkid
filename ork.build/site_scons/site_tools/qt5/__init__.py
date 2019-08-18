
"""SCons.Tool.qt5

Tool-specific initialization for Qt5.

There normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.

"""

#
# Copyright (c) 2001-7,2010,2011,2012 The SCons Foundation
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

import os.path
import re

import SCons.Action
import SCons.Builder
import SCons.Defaults
import SCons.Scanner
import SCons.Tool
import SCons.Util

class ToolQt5Warning(SCons.Warnings.Warning):
    pass

class GeneratedMocFileNotIncluded(ToolQt5Warning):
    pass

class QtdirNotFound(ToolQt5Warning):
    pass

SCons.Warnings.enableWarningClass(ToolQt5Warning)

try:
    sorted
except NameError:
    # Pre-2.4 Python has no sorted() function.
    #
    # The pre-2.4 Python list.sort() method does not support
    # list.sort(key=) nor list.sort(reverse=) keyword arguments, so
    # we must implement the functionality of those keyword arguments
    # by hand instead of passing them to list.sort().
    def sorted(iterable, cmp=None, key=None, reverse=0):
        if key is not None:
            result = [(key(x), x) for x in iterable]
        else:
            result = iterable[:]
        if cmp is None:
            # Pre-2.3 Python does not support list.sort(None).
            result.sort()
        else:
            result.sort(cmp)
        if key is not None:
            result = [t1 for t0,t1 in result]
        if reverse:
            result.reverse()
        return result

qrcinclude_re = re.compile(r'<file[^>]*>([^<]*)</file>', re.M)

mocver_re = re.compile(r'.*(\d+)\.(\d+)\.(\d+).*')

def transformToWinePath(path) :
    return os.popen('winepath -w "%s"'%path).read().strip().replace('\\','/')

header_extensions = [".h", ".hxx", ".hpp", ".hh"]
if SCons.Util.case_sensitive_suffixes('.h', '.H'):
    header_extensions.append('.H')
# TODO: The following two lines will work when integrated back to SCons
# TODO: Meanwhile the third line will do the work
#cplusplus = __import__('c++', globals(), locals(), [])
#cxx_suffixes = cplusplus.CXXSuffixes
cxx_suffixes = [".c", ".cxx", ".cpp", ".cc"]

def checkMocIncluded(target, source, env):
    moc = target[0]
    cpp = source[0]
    # looks like cpp.includes is cleared before the build stage :-(
    # not really sure about the path transformations (moc.cwd? cpp.cwd?) :-/
    path = SCons.Defaults.CScan.path_function(env, moc.cwd)
    includes = SCons.Defaults.CScan(cpp, env, path)
    if not moc in includes:
        SCons.Warnings.warn(
            GeneratedMocFileNotIncluded,
            "Generated moc file '%s' is not included by '%s'" %
            (str(moc), str(cpp)))

def find_file(filename, paths, node_factory):
    for dir in paths:
        node = node_factory(filename, dir)
        if node.rexists():
            return node
    return None

def _detect(env):
    """Not really safe, but fast method to detect the Qt5 library"""
    try: return env['QT5DIR']
    except KeyError: pass

    try: return env['QTDIR']
    except KeyError: pass

    try: return os.environ['QT5DIR']
    except KeyError: pass

    try: return os.environ['QTDIR']
    except KeyError: pass

    moc = env.WhereIs('moc-qt5') or env.WhereIs('moc5') or env.WhereIs('moc')
    if moc:
        vernumber = os.popen3('%s -v' % moc)[2].read()
        vernumber = mocver_re.match(vernumber)
        if vernumber:
            vernumber = [ int(x) for x in vernumber.groups() ]
            if vernumber < [5, 0, 0]:
                vernumber = '.'.join([str(x) for x in vernumber])
                moc = None
                SCons.Warnings.warn(
                    QtdirNotFound,
                    "QT5DIR variable not defined, and detected moc is for Qt %s" % vernumber)

        QT5DIR = os.path.dirname(os.path.dirname(moc))
        SCons.Warnings.warn(
            QtdirNotFound,
            "QT5DIR variable is not defined, using moc executable as a hint (QT5DIR=%s)" % QT5DIR)
        return QT5DIR

    raise SCons.Errors.StopError(
        QtdirNotFound,
        "Could not detect Qt 5 installation")
    return None


def __scanResources(node, env, path, arg):
    # Helper function for scanning .qrc resource files
    # I've been careful on providing names relative to the qrc file
    # If that was not needed this code could be simplified a lot
    def recursiveFiles(basepath, path) :
        result = []
        for item in os.listdir(os.path.join(basepath, path)) :
            itemPath = os.path.join(path, item)
            if os.path.isdir(os.path.join(basepath, itemPath)) :
                result += recursiveFiles(basepath, itemPath)
            else:
                result.append(itemPath)
        return result
    contents = node.get_contents()
    includes = qrcinclude_re.findall(contents)
    qrcpath = os.path.dirname(node.path)
    dirs = [included for included in includes if os.path.isdir(os.path.join(qrcpath,included))]
    # dirs need to include files recursively
    for dir in dirs :
        includes.remove(dir)
        includes+=recursiveFiles(qrcpath,dir)
    return includes

#
# Scanners
#
__qrcscanner = SCons.Scanner.Scanner(name = 'qrcfile',
    function = __scanResources,
    argument = None,
    skeys = ['.qrc'])

#
# Emitters
#
def __qrc_path(head, prefix, tail, suffix):
    if head:
        if tail:
            return os.path.join(head, "%s%s%s" % (prefix, tail, suffix))
        else:
            return "%s%s%s" % (prefix, head, suffix)
    else:
        return "%s%s%s" % (prefix, tail, suffix)
def __qrc_emitter(target, source, env):
    sourceBase, sourceExt = os.path.splitext(SCons.Util.to_String(source[0]))
    sHead = None
    sTail = sourceBase
    if sourceBase:
        sHead, sTail = os.path.split(sourceBase)

    t = __qrc_path(sHead, env.subst('$QT5_QRCCXXPREFIX'),
                   sTail, env.subst('$QT5_QRCCXXSUFFIX'))

    return t, source

#
# Action generators
#
def __moc_generator_from_h(source, target, env, for_signature):
    pass_defines = False
    try:
        if int(env.subst('$QT5_CPPDEFINES_PASSTOMOC')) == 1:
            pass_defines = True
    except ValueError:
        pass

    if pass_defines:
        return '$QT5_MOC $QT5_MOCDEFINES $QT5_MOCFROMHFLAGS $QT5_MOCINCFLAGS -o $TARGET $SOURCE'
    else:
        return '$QT5_MOC $QT5_MOCFROMHFLAGS $QT5_MOCINCFLAGS -o $TARGET $SOURCE'

def __moc_generator_from_cxx(source, target, env, for_signature):
    pass_defines = False
    try:
        if int(env.subst('$QT5_CPPDEFINES_PASSTOMOC')) == 1:
            pass_defines = True
    except ValueError:
        pass

    if pass_defines:
        return ['$QT5_MOC $QT5_MOCDEFINES $QT5_MOCFROMCXXFLAGS $QT5_MOCINCFLAGS -o $TARGET $SOURCE',
                SCons.Action.Action(checkMocIncluded,None)]
    else:
        return ['$QT5_MOC $QT5_MOCFROMCXXFLAGS $QT5_MOCINCFLAGS -o $TARGET $SOURCE',
                SCons.Action.Action(checkMocIncluded,None)]

def __mocx_generator_from_h(source, target, env, for_signature):
    pass_defines = False
    try:
        if int(env.subst('$QT5_CPPDEFINES_PASSTOMOC')) == 1:
            pass_defines = True
    except ValueError:
        pass

    if pass_defines:
        return '$QT5_MOC $QT5_MOCDEFINES $QT5_MOCFROMHFLAGS $QT5_MOCINCFLAGS -o $TARGET $SOURCE'
    else:
        return '$QT5_MOC $QT5_MOCFROMHFLAGS $QT5_MOCINCFLAGS -o $TARGET $SOURCE'

def __mocx_generator_from_cxx(source, target, env, for_signature):
    pass_defines = False
    try:
        if int(env.subst('$QT5_CPPDEFINES_PASSTOMOC')) == 1:
            pass_defines = True
    except ValueError:
        pass

    if pass_defines:
        return ['$QT5_MOC $QT5_MOCDEFINES $QT5_MOCFROMCXXFLAGS $QT5_MOCINCFLAGS -o $TARGET $SOURCE',
                SCons.Action.Action(checkMocIncluded,None)]
    else:
        return ['$QT5_MOC $QT5_MOCFROMCXXFLAGS $QT5_MOCINCFLAGS -o $TARGET $SOURCE',
                SCons.Action.Action(checkMocIncluded,None)]

def __qrc_generator(source, target, env, for_signature):
    name_defined = False
    try:
        if env.subst('$QT5_QRCFLAGS').find('-name') >= 0:
            name_defined = True
    except ValueError:
        pass

    if name_defined:
        return '$QT5_RCC $QT5_QRCFLAGS $SOURCE -o $TARGET'
    else:
        qrc_suffix = env.subst('$QT5_QRCSUFFIX')
        src = str(source[0])
        head, tail = os.path.split(src)
        if tail:
            src = tail
        qrc_suffix = env.subst('$QT5_QRCSUFFIX')
        if src.endswith(qrc_suffix):
            qrc_stem = src[:-len(qrc_suffix)]
        else:
            qrc_stem = src
        return '$QT5_RCC $QT5_QRCFLAGS -name %s $SOURCE -o $TARGET' % qrc_stem

#
# Builders
#
__ts_builder = SCons.Builder.Builder(
        action = SCons.Action.Action('$QT5_LUPDATECOM','$QT5_LUPDATECOMSTR'),
        suffix = '.ts',
        source_factory = SCons.Node.FS.Entry)
__qm_builder = SCons.Builder.Builder(
        action = SCons.Action.Action('$QT5_LRELEASECOM','$QT5_LRELEASECOMSTR'),
        src_suffix = '.ts',
        suffix = '.qm')
__qrc_builder = SCons.Builder.Builder(
        action = SCons.Action.CommandGeneratorAction(__qrc_generator, {'cmdstr':'$QT5_QRCCOMSTR'}),
        source_scanner = __qrcscanner,
        src_suffix = '$QT5_QRCSUFFIX',
        suffix = '$QT5_QRCCXXSUFFIX',
        prefix = '$QT5_QRCCXXPREFIX',
        single_source = 1)
__ex_moc_builder = SCons.Builder.Builder(
        action = SCons.Action.CommandGeneratorAction(__moc_generator_from_h, {'cmdstr':'$QT5_MOCCOMSTR'}))
__ex_uic_builder = SCons.Builder.Builder(
        action = SCons.Action.Action('$QT5_UICCOM', '$QT5_UICCOMSTR'),
        src_suffix = '.ui')


#
# Wrappers (pseudo-Builders)
#
def Ts5(env, target, source=None, *args, **kw):
    """
    A pseudo-Builder wrapper around the LUPDATE executable of Qt5.
        lupdate [options] [source-file|path]... -ts ts-files
    """
    if not SCons.Util.is_List(target):
        target = [target]
    if not source:
        source = target[:]
    if not SCons.Util.is_List(source):
        source = [source]

    # Check QT5_CLEAN_TS and use NoClean() function
    clean_ts = False
    try:
        if int(env.subst('$QT5_CLEAN_TS')) == 1:
            clean_ts = True
    except ValueError:
        pass

    result = []
    for t in target:
        obj = __ts_builder.__call__(env, t, source, **kw)
        # Prevent deletion of the .ts file, unless explicitly specified
        if not clean_ts:
            env.NoClean(obj)
        # Always make our target "precious", such that it is not deleted
        # prior to a rebuild
        env.Precious(obj)
        # Add to resulting target list
        result.extend(obj)

    return result

def Qm5(env, target, source=None, *args, **kw):
    """
    A pseudo-Builder wrapper around the LRELEASE executable of Qt5.
        lrelease [options] ts-files [-qm qm-file]
    """
    if not SCons.Util.is_List(target):
        target = [target]
    if not source:
        source = target[:]
    if not SCons.Util.is_List(source):
        source = [source]

    result = []
    for t in target:
        result.extend(__qm_builder.__call__(env, t, source, **kw))

    return result

def Qrc5(env, target, source=None, *args, **kw):
    """
    A pseudo-Builder wrapper around the RCC executable of Qt5.
        rcc [options] qrc-files -o out-file
    """
    if not SCons.Util.is_List(target):
        target = [target]
    if not source:
        source = target[:]
    if not SCons.Util.is_List(source):
        source = [source]

    result = []
    for t, s in zip(target, source):
        result.extend(__qrc_builder.__call__(env, t, s, **kw))

    return result

def ExplicitMoc5(env, target, source, *args, **kw):
    """
    A pseudo-Builder wrapper around the MOC executable of Qt5.
        moc [options] <header-file>
    """
    if not SCons.Util.is_List(target):
        target = [target]
    if not SCons.Util.is_List(source):
        source = [source]

    result = []
    for t in target:
        # Is it a header or a cxx file?
        result.extend(__ex_moc_builder.__call__(env, t, source, **kw))

    return result

def ExplicitUic5(env, target, source, *args, **kw):
    """
    A pseudo-Builder wrapper around the UIC executable of Qt5.
        uic [options] <uifile>
    """
    if not SCons.Util.is_List(target):
        target = [target]
    if not SCons.Util.is_List(source):
        source = [source]

    result = []
    for t in target:
        result.extend(__ex_uic_builder.__call__(env, t, source, **kw))

    return result

def generate(env):
    """Add Builders and construction variables for qt5 to an Environment."""

    suffixes = [
        '-qt5',
        '-qt5.exe',
        '5',
        '5.exe',
        '',
        '.exe',
    ]
    command_suffixes = ['-qt5', '5', '']

    def locateQt5Command(env, command, qtdir) :
        triedPaths = []
        for suffix in suffixes :
            fullpath = os.path.join(qtdir,'bin',command + suffix)
            if os.access(fullpath, os.X_OK) :
                return fullpath
            triedPaths.append(fullpath)

        fullpath = env.Detect([command+s for s in command_suffixes])
        if not (fullpath is None) : return fullpath

        raise Exception("Qt5 command '" + command + "' not found. Tried: " + ', '.join(triedPaths))

    CLVar = SCons.Util.CLVar
    Action = SCons.Action.Action
    Builder = SCons.Builder.Builder

    env['QT5DIR']  = _detect(env)
    # TODO: 'Replace' should be 'SetDefault'
#    env.SetDefault(
    env.Replace(
        QT5DIR  = _detect(env),
        QT5_BINPATH = os.path.join('$QT5DIR', 'bin'),
        # TODO: This is not reliable to QT5DIR value changes but needed in order to support '-qt5' variants
        QT5_MOC = locateQt5Command(env,'moc', env['QT5DIR']),
        QT5_UIC = locateQt5Command(env,'uic', env['QT5DIR']),
        QT5_RCC = locateQt5Command(env,'rcc', env['QT5DIR']),
        QT5_LUPDATE = locateQt5Command(env,'lupdate', env['QT5DIR']),
        QT5_LRELEASE = locateQt5Command(env,'lrelease', env['QT5DIR']),

        QT5_GOBBLECOMMENTS = 0, # If set to 1, comments are removed before scanning cxx/h files.
        QT5_CPPDEFINES_PASSTOMOC = 1, # If set to 1, all CPPDEFINES get passed to the moc executable.
        QT5_CLEAN_TS = 0, # If set to 1, translation files (.ts) get cleaned on 'scons -c'

        # Some Qt5 specific flags. I don't expect someone wants to
        # manipulate those ...
        QT5_UICFLAGS = CLVar(''),
        QT5_MOCFROMHFLAGS = CLVar(''),
        QT5_MOCFROMCXXFLAGS = CLVar('-i'),
        QT5_QRCFLAGS = '',
        QT5_LUPDATEFLAGS = '',
        QT5_LRELEASEFLAGS = '',

        # suffixes/prefixes for the headers / sources to generate
        QT5_UISUFFIX = '.ui',
        QT5_UICDECLPREFIX = 'ui_',
        QT5_UICDECLSUFFIX = '.h',
        QT5_MOCINCPREFIX = '-I',
        QT5_MOCHPREFIX = 'moc_',
        QT5_MOCHSUFFIX = '$CXXFILESUFFIX',
        QT5_MOCCXXPREFIX = '',
        QT5_MOCCXXSUFFIX = '.moc',
        QT5_QRCSUFFIX = '.qrc',
        QT5_QRCCXXSUFFIX = '$CXXFILESUFFIX',
        QT5_QRCCXXPREFIX = 'qrc_',
        QT5_MOCDEFPREFIX = '-D',
        QT5_MOCDEFSUFFIX = '',
        QT5_MOCDEFINES = '${_defines(QT5_MOCDEFPREFIX, CPPDEFINES, QT5_MOCDEFSUFFIX, __env__)}',
        QT5_MOCCPPPATH = [],
        QT5_MOCINCFLAGS = '$( ${_concat(QT5_MOCINCPREFIX, QT5_MOCCPPPATH, INCSUFFIX, __env__, RDirs)} $)',

        # Commands for the qt5 support ...
        QT5_UICCOM = '$QT5_UIC $QT5_UICFLAGS -o $TARGET $SOURCE',
        QT5_LUPDATECOM = '$QT5_LUPDATE $QT5_LUPDATEFLAGS $SOURCES -ts $TARGET',
        QT5_LRELEASECOM = '$QT5_LRELEASE $QT5_LRELEASEFLAGS -qm $TARGET $SOURCES',

        # Specialized variables for the Extended Automoc support
        # (Strategy #1 for qtsolutions)
        QT5_XMOCHPREFIX = 'moc_',
        QT5_XMOCHSUFFIX = '.cpp',
        QT5_XMOCCXXPREFIX = '',
        QT5_XMOCCXXSUFFIX = '.moc',

        )

    try:
        env.AddMethod(Ts5, "Ts5")
        env.AddMethod(Qm5, "Qm5")
        env.AddMethod(Qrc5, "Qrc5")
        env.AddMethod(ExplicitMoc5, "ExplicitMoc5")
        env.AddMethod(ExplicitUic5, "ExplicitUic5")
    except AttributeError:
        # Looks like we use a pre-0.98 version of SCons...
        from SCons.Script.SConscript import SConsEnvironment
        SConsEnvironment.Ts5 = Ts5
        SConsEnvironment.Qm5 = Qm5
        SConsEnvironment.Qrc5 = Qrc5
        SConsEnvironment.ExplicitMoc5 = ExplicitMoc5
        SConsEnvironment.ExplicitUic5 = ExplicitUic5

    # Interface builder
    uic5builder = Builder(
        action = SCons.Action.Action('$QT5_UICCOM', '$QT5_UICCOMSTR'),
        src_suffix='$QT5_UISUFFIX',
        suffix='$QT5_UICDECLSUFFIX',
        prefix='$QT5_UICDECLPREFIX',
        single_source = True
        #TODO: Consider the uiscanner on new scons version
        )
    env['BUILDERS']['Uic5'] = uic5builder


    def repathMocs(target, source, env):
        for each in target:
            print each.get_abspath()
        print source, env
        assert(False)

    # Metaobject builder
    mocBld = Builder(action={}, prefix={}, suffix={}, emitter=repathMocs )
    for h in header_extensions:
        act = SCons.Action.CommandGeneratorAction(__moc_generator_from_h, {'cmdstr':'$QT5_MOCCOMSTR'})
        mocBld.add_action(h, act)
        mocBld.prefix[h] = '$QT5_MOCHPREFIX'
        mocBld.suffix[h] = '$QT5_MOCHSUFFIX'
    for cxx in cxx_suffixes:
        act = SCons.Action.CommandGeneratorAction(__moc_generator_from_cxx, {'cmdstr':'$QT5_MOCCOMSTR'})
        mocBld.add_action(cxx, act)
        mocBld.prefix[cxx] = '$QT5_MOCCXXPREFIX'
        mocBld.suffix[cxx] = '$QT5_MOCCXXSUFFIX'
    env['BUILDERS']['Moc5'] = mocBld

    # Metaobject builder for the extended auto scan feature
    # (Strategy #1 for qtsolutions)
    xMocBld = Builder(action={}, prefix={}, suffix={})
    for h in header_extensions:
        act = SCons.Action.CommandGeneratorAction(__mocx_generator_from_h, {'cmdstr':'$QT5_MOCCOMSTR'})
        xMocBld.add_action(h, act)
        xMocBld.prefix[h] = '$QT5_XMOCHPREFIX'
        xMocBld.suffix[h] = '$QT5_XMOCHSUFFIX'
    for cxx in cxx_suffixes:
        act = SCons.Action.CommandGeneratorAction(__mocx_generator_from_cxx, {'cmdstr':'$QT5_MOCCOMSTR'})
        xMocBld.add_action(cxx, act)
        xMocBld.prefix[cxx] = '$QT5_XMOCCXXPREFIX'
        xMocBld.suffix[cxx] = '$QT5_XMOCCXXSUFFIX'
    env['BUILDERS']['XMoc5'] = xMocBld

    # Add the Qrc5 action to the CXX file builder (registers the
    # *.qrc extension with the Environment)
    cfile_builder, cxxfile_builder = SCons.Tool.createCFileBuilders(env)
    qrc_act = SCons.Action.CommandGeneratorAction(__qrc_generator, {'cmdstr':'$QT5_QRCCOMSTR'})
    cxxfile_builder.add_action('$QT5_QRCSUFFIX', qrc_act)
    cxxfile_builder.add_emitter('$QT5_QRCSUFFIX', __qrc_emitter)

    # TODO: Does dbusxml2cpp need an adapter
    try:
        env.AddMethod(enable_modules, "EnableQt5Modules")
    except AttributeError:
        # Looks like we use a pre-0.98 version of SCons...
        from SCons.Script.SConscript import SConsEnvironment
        SConsEnvironment.EnableQt5Modules = enable_modules

def enable_modules(self, modules, debug=False, crosscompiling=False) :
    import sys

    validModules = [
        # Qt Essentials
        'QtCore',
        'QtGui',
        'QtMultimedia',
        'QtMultimediaQuick_p',
        'QtMultimediaWidgets',
        'QtNetwork',
        'QtPlatformSupport',
        'QtQml',
        'QtQmlDevTools',
        'QtQuick',
        'QtQuickParticles',
        'QtSql',
        'QtQuickTest',
        'QtTest',
        'QtWebKit',
        'QtWebKitWidgets',
        'QtWidgets',
        # Qt Add-Ons
        'QtConcurrent',
        'QtDBus',
        'QtOpenGL',
        'QtPrintSupport',
        'QtDeclarative',
        'QtScript',
        'QtScriptTools',
        'QtSvg',
        'QtUiTools',
        'QtXml',
        'QtXmlPatterns',
        # Qt Tools
        'QtHelp',
        'QtDesigner',
        'QtDesignerComponents',
        # Other
        'QtCLucene',
        'QtConcurrent',
        'QtV8',
        "QtX11Extras"
        ]
    pclessModules = [
    ]
    staticModules = [
    ]
    invalidModules=[]
    for module in modules:
        if module not in validModules :
            invalidModules.append(module)
    if invalidModules :
        raise Exception("Modules %s are not Qt5 modules. Valid Qt5 modules are: %s"% (
            str(invalidModules),str(validModules)))

    moduleDefines = {
        'QtScript'   : ['QT_SCRIPT_LIB'],
        'QtSvg'      : ['QT_SVG_LIB'],
        'QtSql'      : ['QT_SQL_LIB'],
        'QtXml'      : ['QT_XML_LIB'],
        'QtOpenGL'   : ['QT_OPENGL_LIB'],
        'QtGui'      : ['QT_GUI_LIB'],
        'QtNetwork'  : ['QT_NETWORK_LIB'],
        'QtCore'     : ['QT_CORE_LIB'],
        'QtWidgets'  : ['QT_WIDGETS_LIB'],
    }
    for module in modules :
        try : self.AppendUnique(CPPDEFINES=moduleDefines[module])
        except: pass
    debugSuffix = ''
    if sys.platform in ["darwin", "linux2"] and not crosscompiling :
        if debug : debugSuffix = '_debug'
        for module in modules :
            if module not in pclessModules : continue
            self.AppendUnique(LIBS=[module.replace('Qt','Qt5')+debugSuffix])
            self.AppendUnique(LIBPATH=[os.path.join("$QT5DIR","lib")])
            self.AppendUnique(CPPPATH=[os.path.join("$QT5DIR","include")])
            self.AppendUnique(CPPPATH=[os.path.join("$QT5DIR","include",module)])
        pcmodules = [module.replace('Qt','Qt5')+debugSuffix for module in modules if module not in pclessModules ]
        if 'Qt5DBus' in pcmodules:
            self.AppendUnique(CPPPATH=[os.path.join("$QT5DIR","include","Qt5DBus")])
        if "Qt5Assistant" in pcmodules:
            self.AppendUnique(CPPPATH=[os.path.join("$QT5DIR","include","Qt5Assistant")])
            pcmodules.remove("Qt5Assistant")
            pcmodules.append("Qt5AssistantClient")
        self.AppendUnique(RPATH=[os.path.join("$QT5DIR","lib")])
        self.ParseConfig('pkg-config %s --libs --cflags'% ' '.join(pcmodules))
        self["QT5_MOCCPPPATH"] = self["CPPPATH"]
        return
    if sys.platform == "win32" or crosscompiling :
        if crosscompiling:
            transformedQtdir = transformToWinePath(self['QT5DIR'])
            self['QT5_MOC'] = "QT5DIR=%s %s"%( transformedQtdir, self['QT5_MOC'])
        self.AppendUnique(CPPPATH=[os.path.join("$QT5DIR","include")])
        try: modules.remove("QtDBus")
        except: pass
        if debug : debugSuffix = 'd'
        if "QtAssistant" in modules:
            self.AppendUnique(CPPPATH=[os.path.join("$QT5DIR","include","QtAssistant")])
            modules.remove("QtAssistant")
            modules.append("QtAssistantClient")
        self.AppendUnique(LIBS=['qtmain'+debugSuffix])
        self.AppendUnique(LIBS=[lib.replace("Qt","Qt5")+debugSuffix for lib in modules if lib not in staticModules])
        self.PrependUnique(LIBS=[lib+debugSuffix for lib in modules if lib in staticModules])
        if 'QtOpenGL' in modules:
            self.AppendUnique(LIBS=['opengl32'])
        self.AppendUnique(CPPPATH=[ '$QT5DIR/include/'])
        self.AppendUnique(CPPPATH=[ '$QT5DIR/include/'+module for module in modules])
        if crosscompiling :
            self["QT5_MOCCPPPATH"] = [
                path.replace('$QT5DIR', transformedQtdir)
                    for path in self['CPPPATH'] ]
        else :
            self["QT5_MOCCPPPATH"] = self["CPPPATH"]
        self.AppendUnique(LIBPATH=[os.path.join('$QT5DIR','lib')])
        return

    """
    if sys.platform=="darwin" :
        # TODO: Test debug version on Mac
        self.AppendUnique(LIBPATH=[os.path.join('$QT5DIR','lib')])
        self.AppendUnique(LINKFLAGS="-F$QT5DIR/lib")
        self.AppendUnique(LINKFLAGS="-L$QT5DIR/lib") #TODO clean!
        if debug : debugSuffix = 'd'
        for module in modules :
#            self.AppendUnique(CPPPATH=[os.path.join("$QT5DIR","include")])
#            self.AppendUnique(CPPPATH=[os.path.join("$QT5DIR","include",module)])
# port qt5-mac:
            self.AppendUnique(CPPPATH=[os.path.join("$QT5DIR","include", "qt5")])
            self.AppendUnique(CPPPATH=[os.path.join("$QT5DIR","include", "qt5", module)])
            if module in staticModules :
                self.AppendUnique(LIBS=[module+debugSuffix]) # TODO: Add the debug suffix
                self.AppendUnique(LIBPATH=[os.path.join("$QT5DIR","lib")])
            else :
#                self.Append(LINKFLAGS=['-framework', module])
# port qt5-mac:
                self.Append(LIBS=module)
        if 'QtOpenGL' in modules:
            self.AppendUnique(LINKFLAGS="-F/System/Library/Frameworks")
            self.Append(LINKFLAGS=['-framework', 'AGL']) #TODO ughly kludge to avoid quotes
            self.Append(LINKFLAGS=['-framework', 'OpenGL'])
        self["QT5_MOCCPPPATH"] = self["CPPPATH"]
        return
# This should work for mac but doesn't
#    env.AppendUnique(FRAMEWORKPATH=[os.path.join(env['QT5DIR'],'lib')])
#    env.AppendUnique(FRAMEWORKS=['QtCore','QtGui','QtOpenGL', 'AGL'])
    """

def exists(env):
    return _detect(env)
