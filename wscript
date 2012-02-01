APPNAME = 'clogs'
VERSION = '1.0.0'
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
    'release':
    {
        'debuginfo': False,
        'symbols': False,
        'optimize': True,
        'assertions': False,
        'unit_tests': False,
    }
}

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
    if conf.env['CC_VERSION'][0] >= 4:
        ccflags.extend(['-fvisibility=hidden', '-fvisibility-inlines-hidden'])
    if conf.env['optimize']:
        ccflags.append('-O2')
    if conf.env['debuginfo']:
        ccflags.append('-g')
    if not conf.env['symbols']:
        conf.env.append_value('LINKFLAGS', '-s')
    conf.env.append_value('CFLAGS', ccflags)
    conf.env.append_value('CXXFLAGS', ccflags)

def configure(conf):
    conf.load('gnu_dirs')
    conf.load('compiler_cxx')

    if conf.options.with_doxygen is not False:
        conf.find_program('doxygen', mandatory = conf.options.with_doxygen)
    if conf.options.with_xsltproc is not False:
        conf.find_program('xsltproc', mandatory = conf.options.with_xsltproc)

    if conf.options.cl_headers:
        conf.env.append_value('INCLUDES_OPENCL', [conf.options.cl_headers])
    conf.env.append_value('LIB_OPENCL', ['OpenCL'])
    conf.check_cxx(header_name = 'boost/foreach.hpp')
    conf.check_cxx(header_name = 'boost/program_options.hpp', lib = 'boost_program_options-mt')
    conf.check_cxx(header_name = 'CL/cl.hpp', use = 'OPENCL')
    # Don't care about the defines, just insist the headers are there
    conf.undefine('HAVE_BOOST_FOREACH_HPP')
    conf.undefine('HAVE_BOOST_PROGRAM_OPTIONS_HPP')
    conf.undefine('HAVE_CL_CL_HPP')

    for (key, value) in variants[conf.options.variant].items():
        conf.env[key] = value
    configure_variant(conf)
    if conf.env['CXX_NAME'] == 'gcc':
        configure_variant_gcc(conf)

def options(opt):
    opt.add_option('--variant', type = 'choice', dest = 'variant', default = 'release', action = 'store', help = 'build variant', choices = variants.keys())
    opt.add_option('--with-doxygen', dest = 'with_doxygen', action = 'store_true', help = 'Build reference documentation')
    opt.add_option('--without-doxygen', dest = 'with_doxygen', action = 'store_false', help = 'Do not build reference documentation')
    opt.add_option('--with-xsltproc', dest = 'with_xsltproc', action = 'store_true', help = 'Build user manual')
    opt.add_option('--without-xsltproc', dest = 'with_xsltproc', action = 'store_false', help = 'Do not build user manual')
    opt.add_option('--cl-headers', action = 'store', default = None, help = 'Include path for OpenCL')
    opt.load('compiler_cxx')
    opt.load('gnu_dirs')

def post(bld):
    # This is a rather hacky way to ensure that 'waf install' will install the
    # doxygen build. 
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
        bld.install_files('${DOCDIR}', outputs, cwd = output_dir, relative_trick = True, postpone = False, add = False)

    if bld.cmd == 'install':
        import waflib.Logs
        waflib.Logs.warn('If you installed to a standard library location, you should now run ldconfig.')

def build(bld):
    bld(
            rule = 'python ${SRC} ${TGT}',
            source = ['clc2cpp.py'] + bld.path.ant_glob('kernels/*.cl'),
            target = 'src/kernels.cpp')
    if bld.env['XSLTPROC']:
        bld(
                rule = '${XSLTPROC} --stringparam clogs.version ' + VERSION + ' -o ${TGT} ${SRC}',
                source = ['doc/xhtml-single.xsl', 'doc/clogs.xml'],
                target = 'doc/clogs.html')
        bld(
                rule = 'cp ${SRC} ${TGT}',
                source = 'doc/clogs.in.css',
                target = 'doc/clogs.css'
            )
        bld.install_files('${DOCDIR}', ['doc/clogs.html', 'doc/clogs.css'])
    if bld.env['DOXYGEN']:
        bld(
                rule = '${DOXYGEN} ${SRC[0].abspath()}',
                source = ['Doxyfile'] + bld.path.ant_glob('include/clogs/*.h'),
                target = 'doc/html/index.html')
        # We need to defer setting up the installation until after
        # the build is complete, because we don't yet know which
        # files to produce.
        bld.add_post_fun(post)
    clogs_stlib = bld.stlib(
            source = bld.path.ant_glob('src/*.cpp') + ['src/kernels.cpp'],
            target = 'clogs',
            includes = 'include',
            export_includes = 'include',
            install_path = bld.env['LIBDIR'],
            name = 'CLOGS-ST',
            use = 'OPENCL')
    clogs_shlib = bld.shlib(
            source = bld.path.ant_glob('src/*.cpp') + ['src/kernels.cpp'],
            defines = ['CLOGS_DLL_DO_EXPORTS'],
            target = 'clogs',
            includes = 'include',
            export_includes = 'include',
            name = 'CLOGS-SH',
            use = 'OPENCL',
            vnum = VERSION)
    bld.program(
            source = bld.path.ant_glob('test/*.cpp') + ['tools/options.cpp', 'tools/timer.cpp'],
            target = 'clogs-test',
            lib = ['boost_program_options-mt', 'cppunit', 'rt'],
            use = 'OPENCL CLOGS-ST',
            install_path = None)
    bld.program(
            source = bld.path.ant_glob('tools/*.cpp'),
            target = 'clogs-benchmark',
            lib = ['boost_program_options-mt', 'cppunit', 'rt'],
            use = 'OPENCL CLOGS-SH')
    bld.install_files('${INCLUDEDIR}/clogs', bld.path.ant_glob('include/clogs/*.h'))
