# SCons build specification
# see http://www.scons.org if you do not have this tool
from os.path import join
import SCons

# TODO: should use lamda and map to work on python 1.5
def path(prefix, list): return [join(prefix, x) for x in list]

libtheora_Sources = Split("""
  dct_encode.c encode.c encoder_toplevel.c
  blockmap.c
  comment.c
  cpu.c
  dct.c
  dct_decode.c
  decode.c
  dsp.c
  frarray.c
  frinit.c
  huffman.c
  idct.c
  mcomp.c
  misc_common.c
  pb.c
  pp.c
  quant.c
  reconstruct.c
  scan.c
  toplevel.c
""")

env = Environment()
if env['CC'] == 'gcc':
  env.Append(CCFLAGS=["-g", "-O2", "-Wall"])
#  env.Append(CCFLAGS=["-g", "-Wall"])

def CheckPKGConfig(context, version): 
  context.Message( 'Checking for pkg-config... ' ) 
  ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0] 
  context.Result( ret ) 
  return ret 

def CheckPKG(context, name): 
  context.Message( 'Checking for %s... ' % name )
  ret = context.TryAction('pkg-config --exists %s' % name)[0]
  context.Result( ret ) 
  return ret
     
def CheckSDL(context):
  name = "sdl-config"
  context.Message( 'Checking for %s... ' % name )
  ret = SCons.Util.WhereIs('sdl-config')
  context.Result( ret ) 
  return ret

# check for appropriate inline asm support
host_x86_32_test = """
    int main(int argc, char **argv) {
#if !defined(__i386__)
  #error __i386__ not defined
#endif
  return 0;
    }
    """
def CheckHost_x86_32(context):
  context.Message('Checking for an x86_32 host...')
  result = context.TryCompile(host_x86_32_test, '.c')
  context.Result(result)
  return result

host_x86_64_test = """
    int main(int argc, char **argv) {
#if !defined(__x86_64__)
  #error __x86_64__ not defined
#endif
  return 0;
    }
    """
def CheckHost_x86_64(context):
  context.Message('Checking for an x86_64 host...')
  result = context.TryCompile(host_x86_64_test, '.c')
  context.Result(result)
  return result

conf = Configure(env, custom_tests = {
  'CheckPKGConfig' : CheckPKGConfig,
  'CheckPKG' : CheckPKG,
  'CheckSDL' : CheckSDL,
  'CheckHost_x86_32' : CheckHost_x86_32,
  'CheckHost_x86_64' : CheckHost_x86_64,
  })
  
if not conf.CheckPKGConfig('0.15.0'): 
   print 'pkg-config >= 0.15.0 not found.' 
   Exit(1)

if not conf.CheckPKG('ogg'): 
  print 'libogg not found.' 
  Exit(1) 

if conf.CheckPKG('vorbis vorbisenc'):
  have_vorbis=True
else:
  have_vorbis=False
  
build_player_example=True
if not conf.CheckHeader('sys/soundcard.h'):
  build_player_example=False
if build_player_example and not conf.CheckSDL():
  build_player_example=False

if conf.CheckHost_x86_32():
  libtheora_Sources += Split("""
    x86_32/dsp_mmx.c
    x86_32/dsp_mmxext.c
    x86_32/recon_mmx.c
    x86_32/fdct_mmx.c
  """)
elif conf.CheckHost_x86_64():
  libtheora_Sources += Split("""
    x86_64/dsp_mmx.c
    x86_64/dsp_mmxext.c
    x86_64/recon_mmx.c
    x86_64/fdct_mmx.c
  """)
env = conf.Finish()

env.Append(CPPPATH=['lib', 'include'])
env.ParseConfig('pkg-config --cflags --libs ogg')

libtheora_a = env.Library('lib/theora', path('lib', libtheora_Sources))
libtheora_so = env.SharedLibrary('lib/theora', path('lib', libtheora_Sources))

#installing
prefix='/usr'
lib_dir = prefix + '/lib'
env.Alias('install', prefix)
env.Install(lib_dir, [libtheora_a, libtheora_so])

# example programs
dump_video = env.Copy()
dump_video.Append(LIBS=['theora'], LIBPATH=['./lib'])
dump_video_Sources = Split("""dump_video.c""")
dump_video.Program('examples/dump_video', path('examples', dump_video_Sources))

if have_vorbis:
  encex = dump_video.Copy()
  encex.ParseConfig('pkg-config --cflags --libs vorbisenc vorbis')
  encex_Sources = Split("""encoder_example.c""")
  encex.Program('examples/encoder_example', path('examples', encex_Sources))

  if build_player_example:
    plyex = encex.Copy()
    plyex_Sources = Split("""player_example.c""")
    plyex.ParseConfig('sdl-config --cflags --libs')
    plyex.Program('examples/player_example', path('examples', plyex_Sources))
