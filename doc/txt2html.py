#reformat python script as HTML

comface = "Times New Roman"
codeface = "Courier New"

import fileinput
import sys

pagewidth = 118

if len(sys.argv) < 3:
  print "usage: python tab.py inputfile outputfile"
  print
  print
  x = 0/0

ofil = open(sys.argv[2], "w")
#ofil = open("2.txt", "w")

ofil.write("<html><body>")

state = "code"

#note that formatting characters must be in sequence: #, _, /

for o in fileinput.input(sys.argv[1]):

  oldstate = state
  if (o[0] == '#') and (o[1] != '*'):				#if this line is comment-only
    state = "comment"
    top = ""
    cap = ""
    if (o[1] == '#') and (o[2] == '#') and (o[3] == '#'):	# '####' means big header
      top += "<h2>"
      cap += "</h2>"
      o = o[3:]
    elif (o[1] == '#') and (o[2] == '#'):				# '###' means header
      top += "<h3>"
      cap += "</h3>"
      o = o[2:]
    if o[1] == '#':							# '##' means emphasis on this line
      top += "<b>"
      cap += "</b>"
      o = o[1:]
    if o[1] == '_':							# '_' means underline
      top += "<u>"
      cap += "</u>"
      o = o[1:]
    if o[1] == '/':							# '/" means italics
      top += "<i>"
      cap += "</i>"
      o = o[1:]
    if state != oldstate:
      ofil.write('</pre><font face="' + comface + '">')
    ofil.write(top + o[1:] + cap + "<br>")
  else:
    state = "code"
    if (o[0] == '#') and (o[1] == '*'):				# '#*' is preformatted comment
      o = o[2:]								# remove them
    if state != oldstate:
      ofil.write('<font face="' + codeface + '"><pre>')
    ofil.write(o)

ofil.write("</body></html>")
ofil.close()
