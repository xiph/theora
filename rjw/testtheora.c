/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function: example dumpvid application; dumps  Theora streams
  last mod: $Id: dump_video.c 15400 2008-10-15 12:10:58Z tterribe $

 ********************************************************************/

/* By Robin Watts (robin@wss.co.uk). Actually, this was originally
 * dump_video.c by Mauricio Piacentini (mauricio at xiph.org), but I've
 * hacked it around horribly, so it's probably not nice to blame him
 * for it in any way! */

/* Do *not* use this file as an example of how to correctly sync audio
 * and video. It's designed to be just good enough so I know that audio
 * and video decode are actually working. */

/* Enable/disable the following to determine if we post process */
//#define POST_PROCESS

/* Enable the following if your source file is coming from a slow device
 * to get more realistic timings */
#define PRE_READ_FILE

/* Enable the following to set the screen depth */
#define SCREEN_DEPTH 16
//#define OVERLAY

/* Ideally we should get details of fb and audio ioctls from the correct
 * linux headers. If, like me, you're crosscompiling on a system that
 * doesn't have these headers available, you'll need to use a hideous
 * embarrassing hack. */
#define MISSING_LINUX_HEADERS

#define STATIC

#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#if !defined(_LARGEFILE_SOURCE)
#define _LARGEFILE_SOURCE
#endif
#if !defined(_LARGEFILE64_SOURCE)
#define _LARGEFILE64_SOURCE
#endif
#if !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS 64
#endif

#include <stdio.h>
//#if !defined(_WIN32)
//#include <getopt.h>
//#include <unistd.h>
//#else
//#include "getopt.h"
//#endif
#include <stdlib.h>
#include <string.h>
//#include <sys/timeb.h>
//#include <sys/types.h>
//#include <sys/stat.h>
/*Yes, yes, we're going to hell.*/
//#if defined(_WIN32)
//#include <io.h>
//#endif
//#include <fcntl.h>
#include <math.h>
//#include <signal.h>
#include <stdarg.h>
#include "theora/theora.h"
#include "tremolo/ivorbiscodec.h"
#include "tremolo/codec_internal.h"
#ifdef HAVE_YUVLIB
#include "yuv2rgb.h"
#endif

#ifdef _WIN32_WCE
#include <windows.h>

#define clock GetTickCount
#define CLOCKS_PER_SEC 1000
typedef long clock_t;

#else
#include <time.h>
#endif

char text[4096];

void Output(const char *fmt, ...)
{
  va_list  ap;
#ifdef _WIN32_WCE
  char    *t = text;
  WCHAR    uni[4096];
  WCHAR   *u = uni;

  va_start(ap,fmt);
  vsprintf(text, fmt, ap);
  va_end(ap);

  while (*t != 0)
  {
      *u++ = (WCHAR)(*t++);
  }
  *u++ = 0;
  OutputDebugString(uni);
#else
  va_start(ap,fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
#endif
}

#ifdef _WIN32_WCE
typedef struct _RawFrameBufferInfo
{
   WORD wFormat;
   WORD wBPP;
   VOID *pFramePointer;
   int  cxStride;
   int  cyStride;
   int  cxPixels;
   int  cyPixels;
}
RawFrameBufferInfo;

#define GETRAWFRAMEBUFFER   0x00020001
#define FORMAT_565 1
#define FORMAT_555 2
#define FORMAT_OTHER 3

struct GXDisplayProperties
{
    DWORD cxWidth;
    DWORD cyHeight; /* notice lack of 'th' in the word height */
    long cbxPitch;  /* number of bytes to move right one x pixel - can be negative */
    long cbyPitch;  /* number of bytes to move down one y pixel - can be negative */
    long cBPP;      /* # of bits in each pixel */
    DWORD ffFormat; /* format flags */
};
typedef struct GXDisplayProperties(*getGXProperties)(void);

void *find_screen_mem(int *width,
                      int *height)
{
    HDC                hdc;
    int                result;
    RawFrameBufferInfo frameBufferInfo;

    /* First, we'll try using GETRAWFRAMEBUFFER. This should work on all
     * modern WinCEs, but some 2003's get it wrong. */
    hdc = GetDC(NULL);
    result = ExtEscape(hdc, GETRAWFRAMEBUFFER, 0, NULL,
                       sizeof(RawFrameBufferInfo), (char *)&frameBufferInfo);

    if (result > 0)
    {
        *width  = frameBufferInfo.cxPixels;
        *height = frameBufferInfo.cyPixels;
        return frameBufferInfo.pFramePointer;
    }

    return NULL;
}
#endif

static int screen_width  = 0;
static int screen_height = 0;
static int screen_depth  = 16;
static int dither = 0;

#ifdef MISSING_LINUX_HEADERS
/* Massively gross hack alert! */

/* Stuff from ioctl.h/mmap.h */
#define PROT_READ  1
#define PROT_WRITE 2
#define MAP_SHARED 1
#define O_WRONLY 1
#define O_RDWR   2
extern void*mmap(void*addr,size_t len, int prot, int flags, int fd, int off);
extern int ioctl(int fd, unsigned long request, ...);

typedef unsigned int __u32;
typedef unsigned short __u16;

/* Stuff from fb.h */
#define FBIOGET_VSCREENINFO 0x4600
#define FBIOPUT_VSCREENINFO 0x4601
#define FBIOGET_FSCREENINFO 0x4602

struct fb_fix_screeninfo {
    char id[16];
    unsigned long smem_start;
    __u32 smem_len;
    __u32 type;
    __u32 type_aux;
    __u32 visual;
    __u16 xpanstep;
    __u16 ypanstep;
    __u16 ywrapstep;
    __u32 line_length;
    unsigned long mmio_start;
    __u32 mmio_len;
    __u32 mmio_accel;
    __u16 reserved[3];
};

struct fb_bitfield {
    __u32 offset;
    __u32 length;
    __u32 msb_right;
};

struct fb_var_screeninfo {
    __u32 xres;
    __u32 yres;
    __u32 xres_virtual;
    __u32 yres_virtual;
    __u32 xoffset;
    __u32 yoffset;
    __u32 bits_per_pixel;
    __u32 grayscale;

    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;
    struct fb_bitfield transp;

    __u32 nonstd;
    __u32 activate;
    __u32 height;
    __u32 width;
    __u32 accel_flags;

    __u32 pixclock;
    __u32 left_margin;
    __u32 right_margin;
    __u32 upper_margin;
    __u32 lower_margin;
    __u32 hsync_len;
    __u32 vsync_len;
    __u32 sync;
    __u32 vmode;
    __u32 rotate;
    __u32 reserved[5];
};

/* Stuff lifted from soundcard.h */
#define _IOC_NRBITS      8
#define _IOC_TYPEBITS    8
#define _IOC_SIZEBITS    14
#define _IOC_DIRBITS     2
#define _IOC_NRSHIFT     0
#define _IOC_TYPESHIFT   (_IOC_NRSHIFT+_IOC_NRBITS)
#define _IOC_SIZESHIFT   (_IOC_TYPESHIFT+_IOC_TYPEBITS)
#define _IOC_DIRSHIFT    (_IOC_SIZESHIFT+_IOC_SIZEBITS)
#define _IOC(dir,type,nr,size)   \
   (((dir)  << _IOC_DIRSHIFT)  | \
    ((type) << _IOC_TYPESHIFT) | \
    ((nr)   << _IOC_NRSHIFT)   | \
    ((size) << _IOC_SIZESHIFT))
extern unsigned int __invalid_size_argument_for_IOC;
#define _IOC_NONE  0U
#define _IOC_WRITE 1U
#define _IOC_READ  2U
#define _IOC_TYPECHECK(t) \
        ((sizeof(t) == sizeof(t[1]) && \
          sizeof(t) < (1 << _IOC_SIZEBITS)) ? \
          sizeof(t) : __invalid_size_argument_for_IOC)
#define _IOWR(type, nr, size) _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOR(type, nr, size) _IOC(_IOC_READ,(type),(nr),(_IOC_TYPECHECK(size)))
#define _IOW(type, nr, size) _IOC(_IOC_WRITE,(type),(nr),(_IOC_TYPECHECK(size)))
#define _SIOWR _IOWR
#define _SIOR _IOR
#define _SIOW _IOW
#define SNDCTL_DSP_SPEED      _SIOWR('P', 2, int)
#define SNDCTL_DSP_CHANNELS   _SIOWR('P', 6, int)
#define SNDCTL_DSP_SETFMT     _SIOWR('P', 5, int)
#define SNDCTL_DSP_GETOSPACE  _SIOR('P', 12, audio_buf_info)
#define SNDCTL_DSP_SETTRIGGER _SIOW('P', 16, int)
#define SNDCTL_DSP_GETTRIGGER _SIOR('P', 16, int)
#define AFMT_S16_LE          (0x00000010)
#define PCM_ENABLE_OUTPUT    2

typedef struct audio_buf_info {
  int fragments;
  int fragstotal;
  int fragsize;
  int bytes;
} audio_buf_info;


#else
#include <linux/fb.h>
#include <sys/mman.h>
#endif

void *open_screen(int width, int height)
{
  char *buff = NULL;

#ifdef _WIN32_WCE
  buff = find_screen_mem(&screen_width, &screen_height);

  Output("Screen %d x %d @ %x\n", screen_width, screen_height, buff);
#else
#ifdef OVERLAY
  struct fb_var_screeninfo fbvar;
  int fbdev = open("/dev/fb1", O_RDWR);

  if (fbdev == 0)
  {
      Output("Failed to open /dev/fb1\n");
      exit(1);
  }

  {
      int i;

      if (ioctl(fbdev, FBIOGET_VSCREENINFO, &fbvar) != 0)
          Output("Failed to get info on screen\n");

      for(i=0; i<20; i++)
        Output("%d\n", ((int*)&fbvar)[i]);

      screen_width =fbvar.xres;
      screen_height=fbvar.yres;
      screen_depth =fbvar.bits_per_pixel;
      screen_depth=2;
#else
  int fbdev = open("/dev/fb0", O_RDWR);

  if (fbdev == 0)
  {
      Output("Failed to open /dev/fb0\n");
      exit(1);
  }

  {
      int i;
#define FB_SCREEN_WIDTH  720
#define FB_SCREEN_HEIGHT 574
#if SCREEN_DEPTH == 32
      struct fb_var_screeninfo fbvar =
      {
          FB_SCREEN_WIDTH, FB_SCREEN_HEIGHT,
          FB_SCREEN_WIDTH, FB_SCREEN_HEIGHT,
          0, 0, 32, 0,
          {16,8,0},{8,8,0},{0,8,0},{0,0,0},
          0,0,-1,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
      };
#elif SCREEN_DEPTH == 24
      struct fb_var_screeninfo fbvar =
      {
          FB_SCREEN_WIDTH, FB_SCREEN_HEIGHT,
          FB_SCREEN_WIDTH, FB_SCREEN_HEIGHT,
          0, 0, 24, 0,
          {16,8,0},{8,8,0},{0,8,0},{0,0,0},
          0,0,-1,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
      };
#else
      struct fb_var_screeninfo fbvar =
      {
          FB_SCREEN_WIDTH, FB_SCREEN_HEIGHT,
          FB_SCREEN_WIDTH, FB_SCREEN_HEIGHT,
          0, 0, 16, 0,
          {11,5,0},{5,6,0},{0,5,0},{0,0,0},
          0,0,-1,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
      };
#endif
      if (ioctl(fbdev, FBIOPUT_VSCREENINFO, &fbvar) != 0)
          Output("Failed to set screen mode\n");
      if (ioctl(fbdev, FBIOGET_VSCREENINFO, &fbvar) != 0)
          Output("Failed to get info on screen\n");

      screen_width =fbvar.xres;
      screen_height=fbvar.yres;
      screen_depth =fbvar.bits_per_pixel;
#endif
  }

  buff = (char    *)mmap(0,
                         screen_width*screen_height*(screen_depth>>3),
                         PROT_WRITE, MAP_SHARED, fbdev, 0);
#endif

  if (buff != NULL)
  {
    memset(buff, 0, screen_width*screen_height*(screen_depth>>3));

    /* Centre the output */
    if (width < screen_width)
    {
      buff += ((screen_width-width)>>1)*(screen_depth>>3);
    }
    if (height < screen_height)
    {
      buff += ((screen_height-height)>>1)*screen_width*(screen_depth>>3);
    }

    return buff;
  }

  return NULL;
}

#ifdef PRE_READ_FILE
static char *pre_read_buffer = NULL;
static char *pre_read_ptr    = NULL;
static int   pre_read_left   = 0;

STATIC int pre_read_file(FILE *infile)
{
    long len;

    Output("Pre-reading file...");

    fseek(infile, 0, SEEK_END);
    len = ftell(infile);
    Output(" %d bytes\n", len);
    fseek(infile, 0, SEEK_SET);
    pre_read_buffer = malloc(len);
    if (pre_read_buffer == NULL)
        return 1;
    len=fread(pre_read_buffer,1,len,infile);
    fclose(infile);
    Output("Read %d bytes\n", len);
    pre_read_ptr  = pre_read_buffer;
    pre_read_left = len;
    return 0;
}

STATIC void pre_read_dump(void)
{
    free(pre_read_buffer);
    pre_read_buffer = NULL;
    pre_read_ptr    = NULL;
    pre_read_left   = 0;
}

#define FEOF(infile) ((infile == NULL) ? (pre_read_left==0) : (feof(infile)))

#else
#define FEOF(infile) feof(infile)
#endif

/* Helper; just grab some more compressed bitstream and sync it for
   page extraction */
int buffer_data(FILE *in,ogg_sync_state *oy){
  char *buffer=ogg_sync_bufferin(oy,4096);
  int bytes;

#ifdef PRE_READ_FILE
  if (in == NULL){
    bytes = pre_read_left;
    if (bytes > 4096)
        bytes = 4096;
    pre_read_left -= bytes;
    memcpy(buffer, pre_read_ptr, bytes);
    pre_read_ptr += bytes;
  }
  else
#endif
  {
    bytes=fread(buffer,1,4096,in);
  }
  ogg_sync_wrote(oy,bytes);
  return(bytes);
}

/* never forget that globals are a one-way ticket to Hell */
/* Ogg and codec state for demux/decode */
ogg_sync_state    oy;
ogg_page          og;
ogg_stream_state  vo;
ogg_stream_state  to;
theora_info       ti;
theora_comment    tc;
theora_state      td;
vorbis_info       vi;
vorbis_dsp_state  vd;
vorbis_comment    vc;

int              theora_p;
int              vorbis_p;
int              stateflag;

/* single frame video buffering */
int          videobuf_ready;
ogg_int64_t  videobuf_granulepos;
double       videobuf_time;

/* single audio fragment audio buffering */
int          audiobuf_ready;
ogg_int16_t *audiobuf;
ogg_int64_t  audiobuf_granulepos; /* time position of last sample */
#define PCM_SIZE (1<<15)
ogg_int16_t  pcm[PCM_SIZE];
int          pcmstart; /* In samples */
int          pcmsent;  /* In samples */
int          pcmend;   /* In samples */
ogg_int64_t  audio_samples;
int          audio_sendtime;

typedef enum
{
    Playback_DecodeVideo = 1,
    Playback_DecodeAudio = 2,
    Playback_PlayVideo   = 4,
    Playback_PlayAudio   = 8,
    Playback_Sync        = 16,
    Playback_CRC         = 32,
    Playback_ShowCRC     = 64
} Playback;

Playback        playback;

#ifdef _WIN32_WCE
static HWAVEOUT wince_audio_dev;
static WAVEHDR whdr1;
static WAVEHDR whdr2;
#else
static int audio_dev = 0;
static int audio_started = 0;
#endif

void open_audio(void)
{
#ifdef _WIN32_WCE
  WAVEFORMATEX wfx;
  MMRESULT     res;

  wince_audio_dev = NULL;
  wfx.wFormatTag      = WAVE_FORMAT_PCM;
  wfx.nChannels       = vi.channels;
  wfx.nSamplesPerSec  = vi.rate;
  wfx.wBitsPerSample  = 16;
  wfx.nBlockAlign     = (wfx.wBitsPerSample*vi.channels)>>3;
  wfx.nAvgBytesPerSec = vi.rate*wfx.nBlockAlign;
  wfx.cbSize          = 0;
  res = waveOutOpen(&wince_audio_dev,
                    0,
                    &wfx,
                    0,
                    0,
                    CALLBACK_NULL);
  if (res!= MMSYSERR_NOERROR)
  {
    /* Failed */
    wince_audio_dev = NULL;
  }
  whdr1.dwBufferLength = 0;
  whdr1.dwFlags = WHDR_DONE;
  whdr2.dwBufferLength = 0;
  whdr2.dwFlags = WHDR_DONE;
#else
  audio_started = 0;
  audio_dev = open("/dev/audio", O_WRONLY);
  if (audio_dev == 0)
      return;
  {
    int format   = AFMT_S16_LE;
    int channels = vi.channels;
    int speed    = vi.rate;
    int enable   = ~PCM_ENABLE_OUTPUT;

    if (ioctl(audio_dev, SNDCTL_DSP_SETFMT, &format) != 0)
    {
      Output("Failed to set 16bit PCM\n");
      close(audio_dev);
      audio_dev = 0;
      return;
    }
    if (ioctl(audio_dev, SNDCTL_DSP_CHANNELS, &channels) != 0)
    {
      Output("Failed to get number of channels\n");
      close(audio_dev);
      audio_dev = 0;
      return;
    }
    if (ioctl(audio_dev, SNDCTL_DSP_SPEED, &speed) != 0)
    {
      Output("Failed to set sample speed\n");
      close(audio_dev);
      audio_dev = 0;
      return;
    }
    if (ioctl(audio_dev, SNDCTL_DSP_SETTRIGGER, &enable) != 0)
    {
      Output("Failed to disable PCM\n");
      close(audio_dev);
      audio_dev = 0;
      return;
    }
  }
#endif
  pcmstart = 0;
  pcmsent  = 0;
  pcmend   = 0;
  audio_samples = 0;
  audio_sendtime = clock();
}

#if 1
#define AUDIO_CHECK()
#else
#define AUDIO_CHECK() do {audio_check();}while(0==1)
void audio_check(void)
{
    if ((pcmstart <0) || (pcmstart >= PCM_SIZE))
    {
        Output("pcmstart out of range! %04x\n", pcmstart);
        pcmsent = (pcmsent>>1)<<1;
    }
    if ((pcmsent <0) || (pcmsent >= PCM_SIZE))
    {
        Output("pcmsent out of range! %04x\n", pcmsent);
        pcmsent = (pcmsent>>1)<<1;
    }
    if ((pcmend <0) || (pcmend >= PCM_SIZE))
    {
        Output("pcmend out of range! %04x\n", pcmend);
        pcmend = (pcmend>>1)<<1;
    }
    Output("start=%04x sent=%04x end=%04x\n", pcmstart, pcmsent, pcmend);
    if (pcmstart == pcmend)
    {
        if (pcmsent != pcmstart)
        {
            Output("pcmsent Broken!\n");
            pcmsent = (pcmsent>>1)<<1;
        }
    }
    if (pcmstart < pcmend)
    {
        if ((pcmsent < pcmstart) || (pcmsent > pcmend))
        {
            Output("pcmsent Broken! (2)\n");
            pcmsent = (pcmsent>>1)<<1;
        }
    }
    if (pcmstart > pcmend)
    {
        if ((pcmsent < pcmstart) && (pcmsent > pcmend))
        {
            Output("pcmsent Broken! (3) start=%04x sent=%04x end=%04x\n", pcmstart, pcmsent, pcmend);
            pcmsent = (pcmsent>>1)<<1;
        }
    }
}
#endif

void audio_close(void)
{
#ifdef _WIN32_WCE
  if (wince_audio_dev != NULL)
  {
    waveOutClose(wince_audio_dev);
    wince_audio_dev = NULL;
  }
#else
  if (audio_dev != 0)
  {
    close(audio_dev);
    audio_dev = 0;
  }
#endif
}

int audio_fillblock(ogg_int16_t **pos)
{
    *pos = &pcm[pcmend];
    if (pcmstart == pcmend)
    {
        /* Empty buffer */
        //Output("Sound buffer empty, requesting %d samples\n", PCM_SIZE/4);
        return PCM_SIZE/4;
    }
    else if ((pcmstart - pcmend) > (PCM_SIZE/4))
    {
        /* [***.......#########*****] */
        /*     ^end   ^start   ^sent */
        //Output("Sound buffer wrapped, requesting %d samples\n", PCM_SIZE/4);
        return PCM_SIZE/4;
    }
    else if ((pcmstart == 0) && (pcmend >= 3*PCM_SIZE/4))
    {
        return 0;
    }
    else if (pcmend > pcmstart)
    {
        int max;
        /* [...##########******.....] */
        /*     ^start    ^sent ^end   */

        max = PCM_SIZE-pcmend;
        if (max > PCM_SIZE/4)
            max = PCM_SIZE/4;
        //Output("Sound buffer unwrapped, requesting %d samples\n", max);
        return max;
    }
    //Output("Sound buffer full\n");
    return 0;
}

double audio_time()
{
    int extratime;

    extratime = clock()-audio_sendtime;
    if (!vorbis_p)
      return (double)extratime/CLOCKS_PER_SEC;
    return ((double)audio_samples/vi.channels/vi.rate +
            (double)extratime/CLOCKS_PER_SEC);
}

void audio_filled(int samples)
{
  int fill;

  if ((playback & Playback_PlayAudio) == 0)
  {
      audiobuf_ready = 1;
      return;
  }

  //Output("Sound buffer filled with %d samples\n", samples);
  pcmend += samples;
  if (pcmend == PCM_SIZE)
    pcmend = 0;
  AUDIO_CHECK();

  fill = pcmend - pcmstart;
  if (fill < 0)
    fill = PCM_SIZE - fill;
  audiobuf_ready = (fill >= PCM_SIZE/2);
}

STATIC void kick_audio(void)
{
  int enable = PCM_ENABLE_OUTPUT;

  ioctl(audio_dev, SNDCTL_DSP_SETTRIGGER, &enable);
  audio_started = 1;
}

STATIC void audio_write_nonblocking(void)
{
  int      len;
#ifdef _WIN32_WCE
  MMRESULT res;
  DWORD    flags1;
  DWORD    flags2;

  if (wince_audio_dev != NULL)
  {
    flags1 = whdr1.dwFlags;
    if ((flags1 & WHDR_DONE) != 0)
    {
      pcmstart += whdr1.dwBufferLength>>1;
      if (pcmstart >= PCM_SIZE)
        pcmstart -= PCM_SIZE;
      whdr1.dwBufferLength = 0;
      AUDIO_CHECK();
    }
    flags2 = whdr2.dwFlags;
    if ((flags1 & WHDR_DONE) != 0)
    {
      pcmstart += whdr2.dwBufferLength>>1;
      if (pcmstart >= PCM_SIZE)
        pcmstart -= PCM_SIZE;
      whdr2.dwBufferLength = 0;
      AUDIO_CHECK();
    }
    len = (pcmend-pcmsent);
    if (len < 0)
      len = PCM_SIZE-pcmsent;
    if (len == 0)
    {
        /* Nothing to do, as everything sent */
    }
    else if ((flags1 & WHDR_DONE) != 0)
    {
      whdr1.lpData         = (LPSTR)&pcm[pcmsent];
      whdr1.dwBufferLength = len*2; /* Bytes */
      whdr1.dwFlags        = 0;
      res = waveOutPrepareHeader(wince_audio_dev, &whdr1, sizeof(whdr1));
      res = waveOutWrite(wince_audio_dev, &whdr1, sizeof(whdr1));
      audio_samples += len;
      audio_sendtime = clock();
      //Output("Sending %d samples\n", len);
      pcmsent += len;
      if (pcmsent == PCM_SIZE)
        pcmsent = 0;
      AUDIO_CHECK();
    }
    else if ((flags2 & WHDR_DONE) != 0)
    {
      whdr2.lpData         = (LPSTR)&pcm[pcmsent];
      whdr2.dwBufferLength = len*2; /* Bytes */
      whdr2.dwFlags        = 0;
      res = waveOutPrepareHeader(wince_audio_dev, &whdr2, sizeof(whdr2));
      res = waveOutWrite(wince_audio_dev, &whdr2, sizeof(whdr2));
      audio_samples += len;
      audio_sendtime = clock();
      //Output("Sending %d samples\n", len);
      pcmsent += len;
      if (pcmsent == PCM_SIZE)
        pcmsent = 0;
      AUDIO_CHECK();
    }
  }
#else
  if (audio_dev != 0)
  {
    len = (pcmend-pcmsent);
    if (len < 0)
      len = PCM_SIZE-pcmsent;
    len &= ~4093;
    if (len > 0)
    {
      audio_buf_info info;

      if (!audio_started)
      {
        /* We mustn't start until we have at least half a buffer full */
        if (len < PCM_SIZE/2)
            return;
      }

      ioctl(audio_dev, SNDCTL_DSP_GETOSPACE, &info);
      /* Don't get too far ahead of ourselves */
      //if ((info.fragstotal - info.fragments)*info.fragsize > PCM_SIZE*2)
      //  return;
      if (info.bytes < len*2)
      {
        if (!audio_started)
          kick_audio();
        return;
      }

      //printf("Sending %d samples from offset %d\n", len, pcmsent);
      write(audio_dev, &pcm[pcmsent], len*2);
      pcmsent += len;
      audio_samples += len;
      if (pcmsent == PCM_SIZE)
        pcmsent = 0;
      pcmstart += len;
      if (pcmstart == PCM_SIZE)
        pcmstart = 0;
      audio_sendtime = clock();
      info.bytes -= len*2;

      if ((!audio_started) && (info.bytes < info.fragsize))
      {
        kick_audio();
      }

      AUDIO_CHECK();
    }
  }
#endif
  audiobuf_ready = 0;
}

int   raw;
char *rgb_frame;

FILE *outfile;

void display_video(void *base)
{
#ifdef HAVE_YUVLIB
  int w, h;

  yuv_buffer yuv;
  theora_decode_YUVout(&td,&yuv);

  w = yuv.y_width;
  if (w > screen_width)
      w = screen_width;
  h = yuv.y_height;
  if (h > screen_height)
      h = screen_height;
  switch (screen_depth)
  {
    case 16:
      if ((yuv.y_width == yuv.uv_width) && (yuv.y_height == yuv.uv_height))
      {
        yuv444_2_rgb565(base,
                        yuv.y,
                        yuv.u,
                        yuv.v,
                        w,
                        h,
                        yuv.y_stride,
                        yuv.uv_stride,
                        screen_width*2,
                        yuv2rgb565_table,
                        dither++);
      }
      else if (yuv.y_height == yuv.uv_height)
      {
        yuv422_2_rgb565(base,
                        yuv.y,
                        yuv.u,
                        yuv.v,
                        w,
                        h,
                        yuv.y_stride,
                        yuv.uv_stride,
                        screen_width*2,
                        yuv2rgb565_table,
                        dither++);
      }
      else
      {
        yuv420_2_rgb565(base,
                        yuv.y,
                        yuv.u,
                        yuv.v,
                        w,
                        h,
                        yuv.y_stride,
                        yuv.uv_stride,
                        screen_width*2,
                        yuv2rgb565_table,
                        dither++);
      }
      break;
    case 24:
      if ((yuv.y_width == yuv.uv_width) && (yuv.y_height == yuv.uv_height))
      {
        yuv444_2_rgb888(base,
                        yuv.y,
                        yuv.u,
                        yuv.v,
                        w,
                        h,
                        yuv.y_stride,
                        yuv.uv_stride,
                        screen_width*3,
                        yuv2rgb565_table,
                        dither++);
      }
      else if (yuv.y_height == yuv.uv_height)
      {
        yuv422_2_rgb888(base,
                        yuv.y,
                        yuv.u,
                        yuv.v,
                        w,
                        h,
                        yuv.y_stride,
                        yuv.uv_stride,
                        screen_width*3,
                        yuv2rgb565_table,
                        dither++);
      }
      else
      {
        yuv420_2_rgb888(base,
                        yuv.y,
                        yuv.u,
                        yuv.v,
                        w,
                        h,
                        yuv.y_stride,
                        yuv.uv_stride,
                        screen_width*3,
                        yuv2rgb565_table,
                        dither++);
      }
      break;
    case 32:
      if ((yuv.y_width == yuv.uv_width) && (yuv.y_height == yuv.uv_height))
      {
        yuv444_2_rgb8888(base,
                         yuv.y,
                         yuv.u,
                         yuv.v,
                         w,
                         h,
                         yuv.y_stride,
                         yuv.uv_stride,
                         screen_width*4,
                         yuv2rgb565_table,
                         dither++);
      }
      else if (yuv.y_height == yuv.uv_height)
      {
        yuv422_2_rgb8888(base,
                         yuv.y,
                         yuv.u,
                         yuv.v,
                         w,
                         h,
                         yuv.y_stride,
                         yuv.uv_stride,
                         screen_width*4,
                         yuv2rgb565_table,
                         dither++);
      }
      else
      {
        yuv420_2_rgb8888(base,
                         yuv.y,
                         yuv.u,
                         yuv.v,
                         w,
                         h,
                         yuv.y_stride,
                         yuv.uv_stride,
                         screen_width*4,
                         yuv2rgb565_table,
                         dither++);
      }
      break;
    default:
      break;
  }
#endif
}

#if 0
int got_sigint=0;
static void sigint_handler (int signal) {
  got_sigint = 1;
}
#endif

/* this is a nop in the current implementation. we could
   open a file here or something if so moved. */
STATIC void open_video(void){
  return;
}

/* write out the planar YUV frame, uncropped */
STATIC void video_write(void){
  yuv_buffer yuv;
  theora_decode_YUVout(&td,&yuv);

#ifdef HAVE_YUVLIB
  if (rgb_frame == NULL)
  {
    rgb_frame = malloc(yuv.y_height*yuv.y_width*sizeof(short));
    if (rgb_frame == NULL)
      Output("Failed to malloc output rgb buffer!\n");
  }
  if (rgb_frame != NULL)
  {
      yuv420_2_rgb565(rgb_frame,
                      yuv.y,
                      yuv.u,
                      yuv.v,
                      yuv.y_width,
                      yuv.y_height,
                      yuv.y_stride,
                      yuv.uv_stride,
                      yuv.y_height*2,
                      yuv2rgb565_table,
                      dither++);
    Output("Saving RGB Frame\n");
    fwrite(rgb_frame, 1, yuv.y_height*yuv.y_width*sizeof(short), outfile);
  }

#else
  if(outfile){
    int i;
    Output("Saving Frame\n");
    if(!raw)
      fprintf(outfile, "FRAME\n");
    for(i=0;i<yuv.y_height;i++)
      fwrite(yuv.y+yuv.y_stride*i, 1, yuv.y_width, outfile);
    for(i=0;i<yuv.uv_height;i++)
      fwrite(yuv.u+yuv.uv_stride*i, 1, yuv.uv_width, outfile);
    for(i=0;i<yuv.uv_height;i++)
      fwrite(yuv.v+yuv.uv_stride*i, 1, yuv.uv_width, outfile);
  }
#endif
}

STATIC void do_crc(unsigned char *in, int width, int *crc_ref)
{
    int crc = *crc_ref;

    while (--width > 0)
    {
      int i = *in++;

      if (--width > 0)
          i |= (*in++)<<8;

      crc = ((crc<<1) | (crc>>15)) ^ i;
      crc &= 65535;
    }

    *crc_ref = crc;
}

STATIC void video_crc(int *crcref){
  int i;

  yuv_buffer yuv;
  theora_decode_YUVout(&td,&yuv);

  for(i=0;i<yuv.y_height;i++)
    do_crc(yuv.y+yuv.y_stride*i, yuv.y_width, crcref);
  for(i=0;i<yuv.uv_height;i++)
    do_crc(yuv.u+yuv.uv_stride*i, yuv.uv_width, crcref);
  for(i=0;i<yuv.uv_height;i++)
    do_crc(yuv.v+yuv.uv_stride*i, yuv.uv_width, crcref);

  if ((playback & Playback_ShowCRC) != 0)
      Output("Cumulative CRC = %04x\n", *crcref);
}

/* dump the theora comment header */
STATIC int dump_comments(theora_comment *tc){
  int i, len;
  char *value;

  Output("Encoded by %s\n",tc->vendor);
  if(tc->comments){
    Output("theora comment header:\n");
    for(i=0;i<tc->comments;i++){
      if(tc->user_comments[i]){
        len=tc->comment_lengths[i];
        value=malloc(len+1);
        memcpy(value,tc->user_comments[i],len);
        value[len]='\0';
        Output("\t%s\n", value);
        free(value);
      }
    }
  }
  return(0);
}



/* helper: push a page into the appropriate steam */
/* this can be done blindly; a stream won't accept a page
                that doesn't belong to it */
STATIC int queue_page(ogg_page *page){
  if(theora_p)ogg_stream_pagein(&to,page);
  if(vorbis_p)ogg_stream_pagein(&vo,page);
  return 0;
}

STATIC void syntax(void){
  Output( "Usage: testtheora <infile.ogv> {[<crc_or_outfile>] [maxframes]\n"
          "where <crc_or_outfile> is:\n"
          " -<xxxx>    play A/V unsynced displaying crcs\n"
          " :<xxxx>    play A/V unsynced and display final crc\n"
          " ;          decode A/V (no playback or crcs) (for speed/timings)\n"
          " +          play A/V synced (badly)\n"
          " *          play A/V unsynced\n"
          " @<xxxx>    decode A/V, but only play video\n"
          " <filename> output file\n"
          "\n"
          "In the above, xxxx is the expected 4 digit hex CRC of the decoded frame."
          "\n"
  );
  exit(1);
}

int main(int argc,char *const *argv){

  ogg_packet op;

  void *framebuffer = NULL;
  int max_frames = 0x7FFFFFFF;
  int frames     = 0;
  int crc        = 0;
  int refcrc     = -1;
#ifdef POST_PROCESS
  int pp_level_max;
  int pp_level;
  int pp_inc;
#endif

  clock_t start_clock, end_clock;
  FILE *infile   = stdin;
  outfile        = NULL;
  rgb_frame      = NULL;

  /* Process option arguments. */
  if (argc < 2)
    syntax();

  infile=fopen(argv[1],"rb");
  if(infile==NULL){
    Output("Unable to open '%s' for extraction.\n", argv[1]);
    syntax();
  }

#ifdef PRE_READ_FILE
  if (pre_read_file(infile) == 0)
  {
      infile = NULL;
  }
#endif

  playback = Playback_DecodeVideo;
  if (argc >= 3)
  {
    char *p = argv[2];
    if (p[0] == '-')
    {
      playback |= Playback_DecodeAudio | Playback_PlayVideo | Playback_PlayAudio | Playback_ShowCRC;
    }
    else if (p[0] == ':')
    {
      playback |= Playback_DecodeAudio | Playback_PlayVideo | Playback_PlayAudio;
    }
    else if (p[0] == ';')
    {
      playback |= Playback_DecodeAudio;
    }
    else if (p[0] == '+')
    {
      playback |= Playback_DecodeAudio | Playback_PlayVideo | Playback_PlayAudio | Playback_CRC | Playback_Sync; /* Really crap attempt at syncing */
    }
    else if (p[0] == '*')
    {
      playback |= Playback_DecodeAudio | Playback_PlayVideo | Playback_PlayAudio;
    }
    else if (p[0] == '@')
    {
      playback |= Playback_DecodeAudio | Playback_PlayVideo;
    }
    else
    {
      outfile=fopen(argv[2],"wb");
      if(outfile==NULL){
        Output("Unable to open output file '%s'\n", argv[2]);
        exit(1);
      }
      p = NULL;
    }
    if ((p != NULL) && (p[1] != '0'))
    {
        sscanf(&p[1], "%x", &refcrc);
        playback |= Playback_CRC;
    }
  }

  if (argc >= 4)
  {
    max_frames=atoi(argv[3]);
  }

  theora_p = 0;
  vorbis_p = 0;
  stateflag = 0;
  videobuf_ready=0;
  videobuf_granulepos=-1;
  videobuf_time=0;
  audiobuf_ready=0;
  audiobuf_granulepos=0; /* time position of last sample */
  raw=0;

  /*
     Ok, Ogg parsing. The idea here is we have a bitstream
     that is made up of Ogg pages. The libogg sync layer will
     find them for us. There may be pages from several logical
     streams interleaved; we find the first theora stream and
     ignore any others.

     Then we pass the pages for our stream to the libogg stream
     layer which assembles our original set of packets out of
     them. It's the packets that libtheora actually knows how
     to handle.
  */

  /* start up Ogg stream synchronization layer */
  ogg_sync_init(&oy);

  /* init supporting Vorbis structures needed in header parsing */
  vorbis_info_init(&vi);
  vorbis_comment_init(&vc);

  /* init supporting Theora structures needed in header parsing */
  theora_comment_init(&tc);
  theora_info_init(&ti);

  memset(&op, 0, sizeof(op));

  /* Ogg file open; parse the headers */

  /* Vorbis and Theora both depend on some initial header packets
     for decoder setup and initialization. We retrieve these first
     before entering the main decode loop. */

  /* Only interested in Theora streams */
  while(!stateflag){
    int ret=buffer_data(infile,&oy);
    if(ret==0)break;
    while(ogg_sync_pageout(&oy,&og)>0){
      ogg_stream_state test;

      /* is this a mandated initial header? If not, stop parsing */
      if(!ogg_page_bos(&og)){
        /* don't leak the page; get it into the appropriate stream */
        queue_page(&og);
        stateflag=1;
        break;
      }

      ogg_stream_init(&test,ogg_page_serialno(&og));
      ogg_stream_pagein(&test,&og);
      ogg_stream_packetout(&test,&op);

      /* identify the codec: try theora */
      if(!theora_p && theora_decode_header(&ti,&tc,&op)>=0){
        /* it is theora -- save this stream state */
        memcpy(&to,&test,sizeof(test));
        theora_p=1;
      }else if(((playback & Playback_DecodeAudio) != 0) &&
               (!vorbis_p && vorbis_dsp_headerin(&vi,&vc,&op)>=0)){
        /* it is vorbis */
        memcpy(&vo,&test,sizeof(test));
        vorbis_p=1;
      }else{
        /* whatever it is, we don't care about it */
        ogg_stream_clear(&test);
      }
    }
    /* fall through to non-bos page parsing */
  }

  /* we're expecting more header packets. */
  while((theora_p && theora_p<3) || (vorbis_p && vorbis_p<3)){
    int ret;

    /* look for further theora headers */
    while(theora_p && (theora_p<3) && (ret=ogg_stream_packetout(&to,&op))){
      if(ret<0){
        Output("Error parsing Theora stream headers; corrupt stream?\n");
        exit(1);
      }
      if(theora_decode_header(&ti,&tc,&op)){
        Output("Error parsing Theora stream headers; corrupt stream?\n");
        exit(1);
      }
      theora_p++;
    }

    /* look for more vorbis header packets */
    while(vorbis_p && (vorbis_p<3) && (ret=ogg_stream_packetout(&vo,&op))){
      if(ret<0){
        fprintf(stderr,"Error parsing Vorbis stream headers; corrupt stream?\n");
        exit(1);
      }
      if(vorbis_dsp_headerin(&vi,&vc,&op)){
        fprintf(stderr,"Error parsing Vorbis stream headers; corrupt stream?\n");
        exit(1);
      }
      vorbis_p++;
    }

    /* The header pages/packets will arrive before anything else we
       care about, or the stream is not obeying spec */

    if(ogg_sync_pageout(&oy,&og)>0){
      queue_page(&og); /* demux into the appropriate stream */
    }else{
      int ret=buffer_data(infile,&oy); /* someone needs more data */
      if(ret==0){
        Output("End of file while searching for codec headers.\n");
        exit(1);
      }
    }
  }

  /* and now we have it all.  initialize decoders */
  if(theora_p){
    theora_decode_init(&td,&ti);
    Output("Ogg logical stream %lx is Theora %dx%d %.02f fps video\n"
           "Encoded frame content is %dx%d with %dx%d offset\n",
           to.serialno,ti.width,ti.height,
           (double)ti.fps_numerator/ti.fps_denominator,
           ti.frame_width,ti.frame_height,ti.offset_x,ti.offset_y);
    if ((playback & Playback_PlayVideo) != 0)
      framebuffer = open_screen(ti.width, ti.height);
    dump_comments(&tc);
#ifdef POST_PROCESS
    theora_control(&td,TH_DECCTL_GET_PPLEVEL_MAX,&pp_level_max,
     sizeof(pp_level_max));
    pp_level=pp_level_max;
    theora_control(&td,TH_DECCTL_SET_PPLEVEL,&pp_level,sizeof(pp_level));
    pp_inc=0;
#endif
  }else{
    /* tear down the partial theora setup */
    theora_info_clear(&ti);
    theora_comment_clear(&tc);
  }

  if(vorbis_p){
    vorbis_dsp_init(&vd,&vi);
    Output("Ogg logical stream %lx is Vorbis %d channel %ld Hz audio.\n",
           vo.serialno,vi.channels,vi.rate);
  }else{
    /* tear down the partial vorbis setup */
    vorbis_info_clear(&vi);
    vorbis_comment_clear(&vc);
  }

  /* open audio */
  if(vorbis_p)open_audio();

  /* open video */
  if(theora_p)open_video();

  if(!raw && outfile){
    Output("YUV4MPEG2 W%d H%d F%d:%d I%c A%d:%d\n",
           ti.width,ti.height,ti.fps_numerator,ti.fps_denominator,'p',
           ti.aspect_numerator,ti.aspect_denominator);
  }

#if 0
  /* install signal handler */
  signal (SIGINT, sigint_handler);
#endif

  start_clock = clock();

  /* Finally the main decode loop.

     It's one Theora packet per frame, so this is pretty
     straightforward if we're not trying to maintain sync
     with other multiplexed streams.

     the videobuf_ready flag is used to maintain the input
     buffer in the libogg stream state. If there's no output
     frame available at the end of the decode step, we must
     need more input data. We could simplify this by just
     using the return code on ogg_page_packetout(), but the
     flag system extends easily to the case were you care
     about more than one multiplexed stream (like with audio
     playback). In that case, just maintain a flag for each
     decoder you care about, and pull data when any one of
     them stalls.

     videobuf_time holds the presentation time of the currently
     buffered video frame. We ignore this value.
  */

  stateflag=0; /* playback has not begun */
  /* queue any remaining pages from data we buffered but that did not
      contain headers */
  while(ogg_sync_pageout(&oy,&og)>0){
    queue_page(&og);
  }

  //if(fps_only){
  //  ftime(&start);
  //  ftime(&last);
  //}

  //while(!got_sigint){
  while(max_frames > 0){

    /* we want a video and audio frame ready to go at all times.  If
       we have to buffer incoming, buffer the compressed data (ie, let
       ogg do the buffering) */
    while(vorbis_p && !audiobuf_ready){
      int ret;
      ogg_int16_t *fillbase;

      /* if there's pending, decoded audio, grab it */
      ret=audio_fillblock(&fillbase);
      if (ret==0)
          break;
      ret=vorbis_dsp_pcmout(&vd,fillbase,ret/vi.channels);
      if(ret>0){
        vorbis_dsp_read(&vd,ret);
        if(vd.granulepos>=0)
          audiobuf_granulepos=vd.granulepos;
        else
          audiobuf_granulepos+=ret;
        ret *= vi.channels;
        audio_filled(ret);

      }else{

        /* no pending audio; is there a pending packet to decode? */
        if(ogg_stream_packetout(&vo,&op)>0){
          vorbis_dsp_synthesis(&vd,&op,1);//==0) /* test for success! */
            //vorbis_synthesis_blockin(&vd,&vb);
        }else   /* we need more data; break out to suck in another page */
          break;
      }
    }

    while(theora_p && !videobuf_ready){
      /* theora is one in, one out... */
      if(ogg_stream_packetout(&to,&op)>0){
        int ret;

#ifdef POST_PROCESS
        if(pp_inc){
          pp_level+=pp_inc;
          theora_control(&td,TH_DECCTL_SET_PPLEVEL,&pp_level,
           sizeof(pp_level));
          pp_inc=0;
        }
#endif

        ret=theora_decode_packetin(&td,&op);
        if (ret<0)
        {
          Output("Decode returned %d\n",ret);
        }
        videobuf_granulepos=td.granulepos;
        videobuf_time=theora_granule_time(&td,videobuf_granulepos);
        videobuf_ready=1;
        frames++;
        max_frames--;
        //if(fps_only)
        //  ftime(&after);

      }else
        break;
    }

    if(!videobuf_ready && !audiobuf_ready && FEOF(infile))break;

    if((theora_p && !videobuf_ready) || (vorbis_p && !audiobuf_ready)){
      /* no data yet for somebody.  Grab another page */
      buffer_data(infile,&oy);
      while(ogg_sync_pageout(&oy,&og)>0){
        queue_page(&og);
      }
    }

    /* If playback has begun, top audio buffer off immediately. */
    if(stateflag) audio_write_nonblocking();

    /* dumpvideo frame, and get new one */
    if(stateflag && videobuf_ready &&
       (((playback & Playback_Sync) == 0) || (videobuf_time < audio_time())))
    {
      if (outfile)
      {
        video_write();
      }
      else
      {
        if ((playback & Playback_CRC) != 0)
        {
          video_crc(&crc);
        }
        if ((videobuf_ready) &&
            (framebuffer != NULL) &&
            ((playback & Playback_PlayVideo) != 0))
        {
          display_video(framebuffer);
        }
      }
      videobuf_ready=0;
    }

    /* if our buffers either don't exist or are ready to go,
       we can begin playback */
    if((!theora_p || videobuf_ready) &&
       (!vorbis_p || audiobuf_ready))stateflag=1;
    /* same if we've run out of input */
    if(FEOF(infile))stateflag=1;

  }

  end_clock = clock();

  /* end of decoder loop -- close everything */
  audio_close();

  if(vorbis_p){
    ogg_stream_clear(&vo);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
  }
  if(theora_p){
    ogg_stream_clear(&to);
    theora_clear(&td);
    theora_comment_clear(&tc);
    theora_info_clear(&ti);
  }
  ogg_sync_clear(&oy);

  free(rgb_frame);

  if(infile && infile!=stdin)fclose(infile);
  if(outfile) fclose(outfile);
  if((playback & Playback_CRC) != 0)
  {
    if (refcrc == -1)
        Output("CRC %04x\n", crc);
    else if (crc == refcrc)
      Output("CRC good!\n");
    else
      Output("CRC bad! %04x, but expected %04x\n", crc, refcrc);
  }

  end_clock -= start_clock;
  Output("\n\n%d frames in %f ms  (%f fps)\n",
         frames, ((float)end_clock)/CLOCKS_PER_SEC,
         ((float)frames)*CLOCKS_PER_SEC/end_clock);
  Output("\nDone.\n");

#ifdef PRE_READ_FILE
  pre_read_dump();
#endif

  return(0);

}

#ifdef _WIN32_WCE

typedef struct
{
    int    argc;
    char **argv;
}
thread_args;

DWORD WINAPI thread_starter(void *args_)
{
    thread_args *args = (thread_args *)args_;

    return main(args->argc, args->argv);
}

unsigned __int64 FILETIME_to_uint64(FILETIME ft)
{
    unsigned __int64 ret;
    ret = ft.dwHighDateTime;
    ret <<= 32;
    ret |= ft.dwLowDateTime;
    return ret;
}

int profile(int argc, char *argv[])
{
    HANDLE      thread;
    thread_args args;
    DWORD       threadId;
    DWORD       exitcode = 1;
    //FILETIME    create, exit, kernel, user;
    int         total_time = 0;
    unsigned __int64 oldTime = 0;
    CONTEXT     context;
    int        *profile;
    int         bin;
    int         tickcount = 0;

#define PROFILE_START (0x11000)
#define PROFILE_STOP  (0x39900)
#define PROFILE_SIZE  (PROFILE_STOP-PROFILE_START)
#define PROFILE_QUANT (4)

    args.argc = argc;
    args.argv = argv;

    profile = calloc(PROFILE_SIZE>>PROFILE_QUANT, 4);
    if (profile == NULL)
    {
        Output("Failed to allocate profile space!\n");
        return 1;
    }

    thread = CreateThread(NULL,
                          0,
                          (LPTHREAD_START_ROUTINE)&thread_starter,
                          &args,
                          0,
                          &threadId);

    while (1 == 1)
    {
        BOOL  done;

        done = GetExitCodeThread(thread, &exitcode);
        if ((!done) || (exitcode != STILL_ACTIVE))
            break;
#if 0
        done = GetThreadTimes(thread,
                              &create,
                              &exit,
                              &kernel,
                              &user);
        if (done)
        {
            unsigned __int64 t64;

            t64 = FILETIME_to_uint64(user) + FILETIME_to_uint64(kernel);
            oldTime = (t64-oldTime)/10000;
            total_time += (int)oldTime;
            oldTime = t64;
        }
#endif
        context.ContextFlags = CONTEXT_FULL;
        done = GetThreadContext(thread, &context);

        if (done)
        {
            bin = (context.Pc & 0x7FFFFFFF)>>(PROFILE_QUANT+2);
            if (bin != 0)
                bin -= PROFILE_START>>(PROFILE_QUANT+2);
            if ((bin < 0) || (bin >= PROFILE_SIZE))
            {
                /* Try again, but with r14 */
                bin = (context.Lr & 0x7FFFFFFF)>>(PROFILE_QUANT+2);
                if (bin != 0)
                    bin -= PROFILE_START>>(PROFILE_QUANT+2);
            }
            if ((bin < 0) || (bin >= PROFILE_SIZE))
            {
                bin = 0;
                //Output("PC=%x r14=%x PSR=%x\n", context.Pc, context.Lr, context.Psr);
            }
            profile[bin]++;
            tickcount++;
        }

        Sleep(1);
    }

    Output("Profiler shutting down: %x\n", total_time);

    {
        FILE *file;

        file = fopen("\\Storage Card\\profile.out", "wb");
        if (file != NULL)
        {
            int i;

            fputc('P',file);
            fputc('R',file);
            fputc('O',file);
            fputc('F',file);
            i = 1; /* File version */
            fwrite(&i, sizeof(int), 1, file);
            i = 1000; /* Frequency */
            fwrite(&i, sizeof(int), 1, file);
            fwrite(&tickcount, sizeof(int), 1, file);
            i = 1; /* Sections */
            fwrite(&i, sizeof(int), 1, file);
            i = PROFILE_QUANT; /* Granularity */
            fwrite(&i, sizeof(int), 1, file);
            i = PROFILE_START; /* Start */
            fwrite(&i, sizeof(int), 1, file);
            i = PROFILE_SIZE>>PROFILE_QUANT; /* Size */
            fwrite(&i, sizeof(int), 1, file);
            fwrite(profile, sizeof(int), PROFILE_SIZE>>PROFILE_QUANT, file);
            fclose(file);
        }
    }

    return exitcode;
}


#define TESTFILE 0

#include "ddraw.h"

int WinMain(HINSTANCE h,HINSTANCE i,LPWSTR l,int n)
{
    int ret;
    char *argv0[] = { "testtheorarm",
                     "\\Storage Card\\theorarm\\matrix-55.ogg",
#ifdef POST_PROCESS
                     "-47e5", // Full PP
#else
                     "-662c", // No PP
#endif
                     "10",
                     NULL
                   };
    char *argv1[]= { "testtheorarm",
                     "\\Storage Card\\theorarm\\matrix-300-varAQ.ogg",
                     //"\\Storage Card\\theorarm\\matrix-300-varAQ.out",
#ifdef POST_PROCESS
                     "-3ddf", // Full PP
#else
                     "-90d6", // No PP
#endif
                     "10",
                     NULL
                   };
    char *argv0v[] = { "testtheorarm",
                     "\\Storage Card\\theorarm\\matrix-55.ogg",
                     "+",
                     "100",
                     NULL
                   };
    char *argv1v[]= { "testtheorarm",
                     "\\Storage Card\\theorarm\\matrix-300-varAQ.ogg",
                     //"\\Storage Card\\theorarm\\matrix-300-varAQ.out",
                     "+",
                     "100",
                     NULL
                   };
    char *argv2v[]= { "testtheorarm",
                     //"\\Storage Card\\theorarm\\my-matrix.ogg",
                     "\\my-matrix2.ogg",
                     //"*",
#if 1
                     ";",
#else
#ifdef POST_PROCESS
                     //":0000",
#else
                     //":4a08",
#endif
#endif
                     "100000000",
                     NULL
                   };
    char *argv3v[]= { "testtheorarm",
                     "\\Storage Card\\theorarm\\ducks_take_off_444_720p25.ogg",
                     "*",
                     "30",
                     NULL
                   };
    char *argv4v[]= { "testtheorarm",
                     "\\Storage Card\\theorarm\\mobile_422_ntsc.ogg",
                     "*",
                     "30",
                     NULL
                   };

    ret = main(4, argv0);
    if (ret == 0)
        ret = main(4, argv1);
    if (ret == 0)
        ret = main(4, argv4v);
    if (ret == 0)
        ret = main(4, argv0v);
    if (ret == 0)
        ret = main(4, argv1v);
    if (ret == 0)
        ret = main(4, argv3v);
    if (ret == 0)
        ret = profile(4, argv2v);
    return ret;
}

int wmain(int argc,char *const *argv)
{
    return WinMain(0,0,0,0);
}

#endif
