# Copyright (c) 2012, 2013 University of Cape Town
# Copyright (c) 2014 Bruce Merry
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import shutil
import os
import platform
import waflib.Errors
import waflib.Logs
from waflib.TaskGen import feature, after_method
from waflib import Task

APPNAME = 'clogs'
VERSION = '1.4.0'
out = 'build'

variants = {
    'debug':
    {
        'debuginfo': True,
        'symbols': True,
        'optimize': False,
        'assertions': True,
        'unit_tests': True,
    },
    'optimized':
    {
        'debuginfo': True,
        'symbols': True,
        'optimize': True,
        'assertions': True,
        'unit_tests': True,
    },
    'symbols':
    {
        'debuginfo': True,
        'symbols': True,
        'optimize': True,
        'assertions': False,
        'unit_tests': False,
    },
    'release':
    {
        'debuginfo': False,
        'symbols': False,
        'optimize': True,
        'assertions': False,
        'unit_tests': False,
    }
}

class split_debug_task(Task.Task):
    """Split debug information from a binary"""
    color = 'BLUE'
    run_str = 'objcopy --only-keep-debug ${SRC[0]} ${TGT[1]} && objcopy --strip-debug --strip-unneeded --add-gnu-debuglink=${TGT[1]} ${SRC[0]} ${TGT[0]}'

@after_method('apply_link')
@after_method('apply_vnum')
@feature('split_debug')
def split_debug(self):
    tgt = self.link_task.outputs[0]
    full = tgt.parent.find_or_declare(tgt.name + '.full')
    debug = tgt.parent.find_or_declare(tgt.name + '.debug')
    self.link_task.outputs[0] = full
    self.create_task('split_debug', [full], [tgt, debug])

def configure_variant(conf):
    if conf.env['assertions']:
        conf.define('DEBUG', '1', quote = False)
    else:
        conf.define('NDEBUG', '1', quote = False)
        conf.define('BOOST_DISABLE_ASSERTS', '1', quote = False)
    if conf.env['unit_tests']:
        conf.define('UNIT_TESTS', 1, quote = False)

def configure_variant_gcc(conf):
    ccflags = ['-Wall', '-Wextra']
    cxxflags = []
    if conf.env['CC_VERSION'][0] >= 4:
        ccflags.extend(['-fvisibility=hidden'])
        cxxflags.extend(['-fvisibility-inlines-hidden'])
        if conf.env['CC_VERSION'][0] > 4 or conf.env['CC_VERSION'][1] >= 8:
            cxxflags.extend(['-std=c++11'])
    if conf.env['optimize']:
        ccflags.append('-O2')
    if conf.env['debuginfo']:
        ccflags.append('-g')
    if not conf.env['symbols']:
        conf.env.append_value('LINKFLAGS', '-s')
    conf.env.append_value('CFLAGS', ccflags)
    conf.env.append_value('CXXFLAGS', ccflags)
    conf.env.append_value('CXXFLAGS', cxxflags)
    conf.env['LIB_PROGRAM_OPTIONS'] = ['boost_program_options']

def configure_platform_unix(conf):
    conf.define('CLOGS_FS_UNIX', 1, quote = False)
    conf.env['LIB_OS'] = ['pthread']  # For sqlite

def configure_platform_windows(conf):
    conf.define('CLOGS_FS_WINDOWS', 1, quote = False)
    conf.define('UNICODE', 1, quote = False)
    conf.env['LIB_OS'] = ['shlwapi', 'shell32']

def configure_variant_msvc(conf):
    ccflags = ['/EHsc', '/MD']
    linkflags = []
    if conf.env['optimize']:
        ccflags.append('/O2')
    if conf.env['debuginfo']:
        ccflags.append('/Zi')
        linkflags.append('/DEBUG')
    conf.env.append_value('CFLAGS', ccflags)
    conf.env.append_value('CXXFLAGS', ccflags)
    conf.env.append_value('LINKFLAGS', linkflags)
    if 'LIBPATH' in os.environ:
        for item in os.environ['LIBPATH'].split(os.pathsep):
            conf.env.append_value('LIBPATH', item)

def configure(conf):
    conf.load('gnu_dirs')
    conf.load('compiler_c')
    conf.load('compiler_cxx')

    if conf.options.with_doxygen is not False:
        conf.find_program('doxygen', mandatory = conf.options.with_doxygen)
    if conf.options.with_xsltproc is not False:
        conf.find_program('xsltproc', mandatory = conf.options.with_xsltproc)
    conf.env['split_debug'] = conf.options.split_debug

    for (key, value) in variants[conf.options.variant].items():
        conf.env[key] = value
    configure_variant(conf)
    if conf.env['CXX_NAME'] == 'gcc':
        configure_variant_gcc(conf)
    elif conf.env['CXX_NAME'] == 'msvc':
        configure_variant_msvc(conf)

    if os.name == 'nt' or platform.system() == 'Windows':
        configure_platform_windows(conf)
    elif os.name == 'posix' or platform.system() == 'Linux':
        configure_platform_unix(conf)
    else:
        waflib.Logs.warn('Unable to identify platform, assuming UNIX')
        configure_platform_unix(conf)

    if conf.options.cl_headers:
        conf.env.append_value('INCLUDES_OPENCL', [conf.options.cl_headers])
    conf.env.append_value('LIB_OPENCL', ['OpenCL'])
    conf.check_cxx(header_name = 'boost/foreach.hpp')
    conf.check_cxx(header_name = 'boost/program_options.hpp', use = 'PROGRAM_OPTIONS')
    conf.check_cxx(header_name = 'CL/cl.hpp', use = 'OPENCL')
    try:
        conf.check_cxx(header_name = 'cppunit/Test.h', lib = 'cppunit', uselib_store = 'CPPUNIT')
    except waflib.Errors.ConfigurationError:
        # Home-made builds of cppunit don't link against -ldl themselves
        conf.check_cxx(header_name = 'cppunit/Test.h', lib = ['cppunit', 'dl'], uselib_store = 'CPPUNIT', mandatory = False)
    for header in ['random', 'functional']:
        conf.check_cxx(header_name = header, mandatory = False)
        conf.check_cxx(header_name = 'tr1/' + header, mandatory = False)

    conf.check_cxx(
        function_name = 'QueryPerformanceCounter', header_name = 'windows.h',
        uselib_store = 'TIMER',
        mandatory = False)
    conf.check_cxx(
        function_name = 'clock_gettime', header_name = 'time.h', lib = 'rt',
        uselib_store = 'TIMER',
        mandatory = False)

    # Don't care about the defines, just insist the headers are there
    conf.undefine('HAVE_BOOST_FOREACH_HPP')
    conf.undefine('HAVE_BOOST_PROGRAM_OPTIONS_HPP')
    conf.undefine('HAVE_CL_CL_HPP')

def options(opt):
    opt.add_option('--variant', type = 'choice', dest = 'variant',
            default = 'release', action = 'store',
            help = 'build variant (release | debug | optimized)',
            choices = list(variants.keys()))
    opt.add_option('--with-doxygen', dest = 'with_doxygen', action = 'store_true', help = 'Build reference documentation')
    opt.add_option('--without-doxygen', dest = 'with_doxygen', action = 'store_false', help = 'Do not build reference documentation')
    opt.add_option('--with-xsltproc', dest = 'with_xsltproc', action = 'store_true', help = 'Build user manual')
    opt.add_option('--without-xsltproc', dest = 'with_xsltproc', action = 'store_false', help = 'Do not build user manual')
    opt.add_option('--cl-headers', action = 'store', default = None, help = 'Include path for OpenCL')
    opt.add_option('--split-debug', action = 'store_true', default = False, help = 'Put debug information into separate file (GCC only)')
    opt.load('compiler_c')
    opt.load('compiler_cxx')
    opt.load('gnu_dirs')

def post(bld):
    """
    This is a rather hacky way to ensure that 'waf install' will install the
    doxygen build.
    """
    from waflib import Utils
    if bld.env['DOXYGEN']:
        output_dir = bld.bldnode.find_dir('doc')
        outputs = output_dir.ant_glob('html/**/*', quiet = True)
        # The build rule doesn't know about the outputs other than index.html, so we
        # have to do this to prevent waf from complaining about missing build
        # signatures.
        for x in outputs:
            x.sig = Utils.h_file(x.abspath())
        # Do the install right now
        bld.install_files('${HTMLDIR}', outputs, cwd = output_dir, relative_trick = True, postpone = False, add = False)

    if bld.cmd == 'install':
        import waflib.Logs
        waflib.Logs.warn('If you installed to a standard library location, you should now run ldconfig.')

def simple_copy(task):
    """
    Wrapper around shutils.copy2 that allows it to be used as rule function.
    This is more portable than calling cp via the shell.
    """
    src = task.inputs[0].abspath()
    tgt = task.outputs[0].abspath()
    shutil.copy2(src, tgt)

def build(bld):
    bld(
            rule = 'python ${SRC} ${TGT}',
            source = ['clc2cpp.py'] + bld.path.ant_glob('kernels/*.cl'),
            target = 'src/kernels.cpp')
    if bld.env['XSLTPROC']:
        bld(
                rule = '${XSLTPROC} --xinclude --stringparam clogs.version ' + VERSION + ' -o ${TGT} ${SRC}',
                source = ['doc/clogs-user-xml.xsl', 'doc/clogs-user.xml'],
                target = 'doc/clogs-user.html')
        bld(
                rule = simple_copy,
                source = 'doc/clogs-user.in.css',
                target = 'doc/clogs-user.css'
            )
        bld(
                rule = simple_copy,
                source = 'doc/benchmark/' + VERSION + '/clogs-benchmark.svg',
                target = 'doc/images/clogs-benchmark.svg'
            )
        output_dir = bld.bldnode.find_or_declare('doc')
        output_dir.mkdir()
        bld.install_files('${HTMLDIR}',
                output_dir.ant_glob('clogs-user.*') + output_dir.ant_glob('images/**/*'),
                cwd = bld.bldnode.find_dir('doc'),
                relative_trick = True)
    if bld.env['DOXYGEN']:
        bld(
                rule = '${DOXYGEN} ${SRC[0].abspath()}',
                source = ['Doxyfile'] + bld.path.ant_glob('include/clogs/*.h'),
                target = 'doc/html/index.html')
        # We need to defer setting up the installation until after
        # the build is complete, because we don't yet know which
        # files to produce.
        bld.add_post_fun(post)
    bld.define('SQLITE_THREADSAFE', '1', quote = False)
    bld.define('SQLITE_OMIT_LOAD_EXTENSION', '1', quote = False)
    lib_sources = bld.path.ant_glob('src/*.cpp') + bld.path.ant_glob('src/*.c') + ['src/kernels.cpp']
    clogs_stlib = bld.stlib(
            source = lib_sources,
            defines = ['CLOGS_DLL_DO_STATIC'],
            target = 'clogs',
            includes = 'include',
            export_includes = 'include',
            install_path = bld.env['LIBDIR'],
            name = 'CLOGS-ST',
            use = 'OPENCL OS')
    features = ['cxxshlib']
    if bld.env['split_debug'] and bld.env['CXX_NAME'] == 'gcc':
        features += ['split_debug']
    clogs_shlib = bld.shlib(
            features = features,
            source = lib_sources,
            defines = ['CLOGS_DLL_DO_EXPORT'],
            target = 'clogs',
            includes = 'include',
            export_includes = 'include',
            name = 'CLOGS-SH',
            use = 'OPENCL OS',
            vnum = VERSION)
    if 'HAVE_CPPUNIT_TEST_H=1' in bld.env['DEFINES']:
        bld.program(
                source = bld.path.ant_glob('test/*.cpp') + ['tools/options.cpp', 'tools/timer.cpp'],
                defines = ['CLOGS_DLL_DO_STATIC'],
                target = 'clogs-test',
                use = 'PROGRAM_OPTIONS CPPUNIT OPENCL CLOGS-ST TIMER',
                install_path = None)
    bld.program(
            source = ['tools/options.cpp', 'tools/timer.cpp', 'tools/clogs-benchmark.cpp'],
            target = 'clogs-benchmark',
            use = 'PROGRAM_OPTIONS OPENCL CLOGS-SH TIMER')
    bld.install_files('${INCLUDEDIR}/clogs', bld.path.ant_glob('include/clogs/*.h'))
