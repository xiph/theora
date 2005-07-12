# SCons build specification
# see http://www.scons.org if you do not have this tool

from os.path import join

# TODO: should use lamda and map to work on python 1.5
def path(prefix, list): return [join(prefix, x) for x in list]

libtheora_Sources = Split("""
  dct_encode.c encode.c encoder_toplevel.c
  blockmap.c
  comment.c
  dct.c
  dct_decode.c
  decode.c
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

env.Append(CPPPATH=['lib', 'include'])

env.Library('theora', path('lib', libtheora_Sources))

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

