# SCons build specification
# see http://www.scons.org if you do not have this tool

from os.path import join

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
	'CheckHost_x86_32' : CheckHost_x86_32,
	'CheckHost_x86_64' : CheckHost_x86_64
	})
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

env.Library('theora', path('lib', libtheora_Sources))


# example programs

examples = env.Copy()

examples.Append(LIBPATH=['.'])
examples.Append(LIBS=['theora','vorbisenc','vorbis','ogg']);

encex_Sources = Split("""encoder_example.c""")
examples.Program('examples/encoder_example', 
	path('examples', encex_Sources))

plyex_Sources = Split("""player_example.c""")
examples.Append(CPPFLAGS=[Split('-I/usr/include/SDL -D_REENTRANT')])
examples.Append(LINKFLAGS=[Split('-L/usr/lib -lSDL -lpthread')])
examples.Program('examples/player_example',
	path('examples', plyex_Sources))

