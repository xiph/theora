#reformat long comments and replace tabs
#default tab spacing = 6 (for wordpad)

import fileinput
import sys

pagewidth = 118

if len(sys.argv) < 3:
  print "usage: python tab.py inputfile outputfile"
  print
  print
  x = 0/0

ofil = open(sys.argv[2], "w")

for o in fileinput.input(sys.argv[1]):
  if o[0] == '#':
    if o[1] == '#':
      o = '#' + o[2:]
    if o[1] == '#':
      o = '#' + o[2:]
    if o[1] == '*':					#remove formatting hints
      o = '#' + o[2:]
    if o[1] == '_':					#remove formatting hints
      o = '#' + o[2:]
    if o[1] == '/':
      o = '#' + o[2:]

  if (o[0] == '#') & (len(o) > pagewidth):
    w = o.split()						#create list of words in string
    x = 0
    while x < len(w):
      if x > 0:
        p = '#'
      else:
        p = ''
      while (len(p) + len(w[x])) < pagewidth:
        p += w[x]
        p += ' '
        x += 1
        if x >= len(w):
          break
      p = p.rstrip()					#remove last space
      ofil.write(p)
      ofil.write('\n')					#linefeed
  else:
    ofil.write(o)

ofil.close()
