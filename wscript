# encoding: utf-8
out = '_build'

def build(bld):
  bld.recurse('src')

def configure(conf):
  conf.load('gnu_dirs')
  conf.load('compiler_c')

  flags = ('-Wall',)
  flags += conf.options.debug and ('-g', '-O0', '-pedantic',) or ('-O2',)
  conf.env.append_unique('CFLAGS', flags)

  conf.recurse('src')

def options(opt):
  opt.load('gnu_dirs')
  opt.load('compiler_c')

  opt.add_option('--release', action='store_false', default=False, dest='debug',
      help="Set compilation release flags (default).")
  opt.add_option('--debug', action='store_true', dest='debug',
      help="Set compilation debug flags.")

  opt.recurse('src')
