05/23/03

Very simple port of the sample player for Windows, for testing.
Uses SDL and portaudio, and provides basic synchronization and correction
for audio latency. This version uses WMME as the audio output device. The
required SDL and portaudio libs/headers are supplied in the tree, as well
as some modified files (circular stream io for example.)

It can be invoked from the command line, or you can drop a Theora file over
the app icon for basic playback.

This example will be updated to a true Win32 app sometime in the future,
hope it is useful for basic testing now.

KNOWN ISSUE- Playback will drift out of synch if there is audio starvation,
and blank samples are written to the output ring buffer. 

mauricio@xiph.org