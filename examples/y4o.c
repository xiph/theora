/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2009                *
 * by the Xiph.Org Foundation and contributors http://www.xiph.org/ *
 *                                                                  *
 ********************************************************************

  function: yuv4ogg code to be used by example encoder application
  last mod: $Id: y4o.c 16421 2009-08-05 17:28:35Z gmaxwell $

 ********************************************************************/

#if !defined(_REENTRANT)
#define _REENTRANT
#endif
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
#if !defined(_WIN32)
#include <getopt.h>
#include <unistd.h>
#else
#include "getopt.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "theora/theoraenc.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisenc.h"
#include "y4o.h"

#define EPSILON 1e-6

#ifdef _WIN32
/*supply missing headers and functions to Win32. going to hell, I know*/
#include <fcntl.h>
#include <io.h>

static double rint(double x)
{
  if (x < 0.0)
    return (double)(int)(x - 0.5);
  else
    return (double)(int)(x + 0.5);
}
#endif

static char *chromaformat[]={
  "mono",      //0
  "411ntscdv", //1
  "420jpeg",   //2 chroma sample is centered vertically and horizontally between luma samples
  "420mpeg2",  //3 chroma sample is centered vertically between lines, cosited horizontally */
  "420paldv",  //4 chroma sample is cosited vertically and horizontally */
  "420unknown",//5
  "422jpeg",   //6 chroma sample is horizontally centered between luma samples */
  "422smpte",  //7 chroma sample is cosited horizontally */
  "422unknown",//8
  "444",       //9
  "444alpha",  //10
  NULL
};

static unsigned char chromabits[]={
  8,
  12,
  12,
  12,
  12,
  12,
  16,
  16,
  16,
  24,
  32
};

static int y4o_read_container_header(FILE *f, char *first, int *sync){
  char line[80];
  char *p;
  int n;
  size_t ret,len;

  len=sizeof("YUV4OGG");
  if(first){
    strncpy(line,first,len);
    line[len]='\0';
    first+=len;
    ret=strlen(line);
  }else{
    ret = fread(line, 1, len,f);
  }

  if (ret<len){
    if(feof(f))
      fprintf(stderr, "ERROR: EOF reading y4o file header\n");
    return 1;
  }

  if (strncmp(line, "YUV4OGG ", sizeof("YUV4OGG ")-1)){
    fprintf(stderr, "ERROR: cannot parse y4o file header\n");
    return 1;
  }

  /* proceed to get the tags... (overwrite the magic) */
  if(!first){
    for (n = 0, p = line; n < 80; n++, p++) {
      if ((ret=fread(p,1,1,f))<1){
        if(feof(f))
          fprintf(stderr,"ERROR: EOF reading y4o file header\n");
        return 1;
      }
      if (*p == '\n') {
        *p = '\0';
        break;
      }
    }
    if (n >= 80) {
      fprintf(stderr,"ERROR: line too long reading y4o file header\n");
      return 1;
    }
  }else{
    strncpy(line,first,80);
  }

  /* read tags */
  /* S%c = sync */

  {
    char *token, *value;
    char tag;

    *sync=-1;

    /* parse fields */
    for (token = strtok(line, " ");
         token != NULL;
         token = strtok(NULL, " ")) {
      if (token[0] == '\0') continue;   /* skip empty strings */
      tag = token[0];
      value = token + 1;
      switch (tag) {
      case 'S':
        switch(token[1]){
        case 'y':
        case 'Y':
          *sync = 1;
          break;
        case 'n':
        case 'N':
          *sync=0;
          break;
        default:
          fprintf(stderr,"ERROR: unknown sync tag setting in y4o header\n");
          return 1;
        }
        break;
      default:
        fprintf(stderr,"ERROR: unknown file tag in y4o header\n");
        return 1;
      }
    }
  }

  if(*sync==-1)return 1;

  return 0;
}

static y4o_stream_t *y4o_read_stream_header(FILE *f){
  char line[80];
  char *p;
  int n;
  size_t ret;
  int af=0,vf=0;
  y4o_stream_t *s;

  /* Check for past the stream headers and at first frame */
  line[0]=getc(f);
  ungetc(line[0],f);
  if(line[0]=='F'){
    return NULL;
  }

  s=calloc(1,sizeof(*s));

  ret = fread(line, 1, sizeof("AUDIO"), f);
  if (ret<sizeof("AUDIO")){
    if(feof(f))
      fprintf(stderr,"ERROR: EOF reading y4o stream header\n");
    goto err;
  }

  if (!strncmp(line, "VIDEO ", sizeof("VIDEO ")-1)) vf=1;
  else if (!strncmp(line, "AUDIO ", sizeof("AUDIO ")-1)) af=1;

  if(!(af||vf)){
    fprintf(stderr,"ERROR: unknown y4o stream header type\n");
    goto err;
  }

  if(vf){
    /* video stream! */
    /* proceed to get the tags... (overwrite the magic) */
    for (n = 0, p = line; n < 80; n++, p++) {
      if ((ret=fread(p, 1, 1, f))<1){
        if(feof(f))
          fprintf(stderr,"ERROR: EOF reading y4o video stream header\n");
        goto err;
      }
      if (*p == '\n') {
        *p = '\0';
        break;
      }
    }
    if (n >= 80) {
      fprintf(stderr,"ERROR: line too long reading y4o video stream header\n");
      goto err;
    }

    /* read tags */
    /* W%d = width
       H%d = height
       F%d:%d = fps
       I%c = interlace
       A%d:%d = pixel aspect
       C%s = chroma format
    */

    {
      char *token, *value;
      char tag;
      int i;
      s->m.video.format=-1;
      s->m.video.i=-1;

      /* parse fields */
      for (token = strtok(line, " ");
           token != NULL;
           token = strtok(NULL, " ")) {
        if (token[0] == '\0') continue;   /* skip empty strings */
        tag = token[0];
        value = token + 1;
        switch (tag) {
        case 'W':
          s->m.video.w = atoi(token+1);
          break;
        case 'H':
          s->m.video.h = atoi(token+1);
          break;
        case 'F':
          {
            char *pos=strchr(token+1,':');
            if(pos){
              *pos='\0';
              s->m.video.fps_n = atoi(token+1);
              s->m.video.fps_d = atoi(pos+1);
              *pos=':';
            }else{
              s->m.video.fps_n = atoi(token+1);
              s->m.video.fps_d = 1;
            }
          }
          break;
        case 'I':
          switch(token[1]){
          case 'p':
          case 'P':
            s->m.video.i=Y4O_I_PROGRESSIVE;
            break;
          case 't':
          case 'T':
            s->m.video.i=Y4O_I_TOP_FIRST;
            break;
          case 'b':
          case 'B':
            s->m.video.i=Y4O_I_BOTTOM_FIRST;
            break;
          default:
            fprintf(stderr,"ERROR: unknown y4o video interlace setting\n");
            goto err;
          }
        case 'A':
          {
            char *pos=strchr(token+1,':');
            if(pos){
              *pos='\0';
              s->m.video.pa_n = atoi(token+1);
              s->m.video.pa_d = atoi(pos+1);
              *pos=':';
            }else{
              s->m.video.pa_n = atoi(token+1);
              s->m.video.pa_d = 1;
            }
          }
          break;
        case 'C':
          for(i=0;chromaformat[i];i++)
            if(!strcasecmp(chromaformat[i],token+1))break;
          if(!chromaformat[i]){
            fprintf(stderr,"ERROR: unknown y4o video chroma format\n");
            goto err;
          }
          s->m.video.format=i;
          break;
        default:
          fprintf(stderr,"ERROR: unknown y4o video stream tag\n");
          goto err;
        }
      }
    }

    if(s->m.video.fps_n>0 &&
       s->m.video.fps_d>0 &&
       s->m.video.pa_n>0 &&
       s->m.video.pa_d>0 &&
       s->m.video.format>=0 &&
       s->m.video.w>0 &&
       s->m.video.h>0 &&
       s->m.video.i>=0)
      s->type=Y4O_STREAM_VIDEO;
    else{
      fprintf(stderr,"ERROR: missing flags in y4o video stream header\n");
      goto err;
    }

    {
      int dw = s->m.video.w*s->m.video.pa_n;
      int dh = s->m.video.h*s->m.video.pa_d;
      int d;
      for(d=1;d<10000;d++)
        if(fabs(rint(dw/(double)dh*d) - dw/(double)dh*d)<EPSILON)
          break;
      s->m.video.frame_n=rint(dw/(double)dh*d);
      s->m.video.frame_d=d;
    }

    return s;

  }

  if(af){
    /* audio stream! */
    /* proceed to get the tags... (overwrite the magic) */
    for (n = 0, p = line; n < 80; n++, p++) {
      if ((ret=fread(p, 1, 1, f))<1){
        if(feof(f))
          fprintf(stderr,"ERROR: EOF reading y4o audio stream header\n");
        goto err;
      }
      if (*p == '\n') {
        *p = '\0';
        break;
      }
    }
    if (n >= 80) {
      fprintf(stderr,"ERROR: line too long reading y4o audio stream header\n");
      goto err;
    }

    /* read tags */
    /* R%d = rate
       C%d = channels
       all interchange audio is 24 bit signed LE
    */

    {
      char *token, *value;
      char tag;

      /* parse fields */
      for (token = strtok(line, " ");
           token != NULL;
           token = strtok(NULL, " ")) {
        if (token[0] == '\0') continue;   /* skip empty strings */
        tag = token[0];
        value = token + 1;
        switch (tag) {
        case 'R':
          s->m.audio.rate = atoi(token+1);
          break;
        case 'C':
          s->m.audio.ch = atoi(token+1);
          break;
        default:
          fprintf(stderr,"ERROR: unknown y4o audio stream tag\n");
          goto err;
        }
      }
    }

    if(s->m.audio.rate>0 &&
       s->m.audio.ch>0)
      s->type=Y4O_STREAM_AUDIO;
    else{
      fprintf(stderr,"ERROR: missing flags in y4o stream header\n");
      goto err;
    }

    return s;
  }

 err:
  s->type = Y4O_STREAM_INVALID;
  return s;
}

y4o_in_t *y4o_init(FILE *f, char *first){
  int i;
  y4o_in_t *y;

  if(y4o_read_container_header(f,first,&i))
    return NULL;

  y=calloc(1,sizeof(*y));
  y->synced=i;

  // read stream headers
  while(1){
    y4o_stream_t *s=y4o_read_stream_header(f);
    if(!s)
      break;
    if(s->type == Y4O_STREAM_INVALID)
      fprintf(stderr,"ERROR: Stream #%d unreadable; trying to continue\n",y->num_streams);
    s->stream_num=y->num_streams++;

    if(y->streams){
      y->streams=realloc(y->streams,y->num_streams*sizeof(*y->streams));
    }else{
      y->streams=calloc(1,sizeof(*y->streams));
    }
    y->streams[s->stream_num]=s;
  }

  y->f = f;

  return y;
}

void y4o_free(y4o_in_t *y){
  int i;
  if(y){
    if(y->streams){
      for(i=0;i<y->num_streams;i++){
        y4o_stream_t *s=y->streams[i];
        free(s);
      }
      free(y->streams);
    }
    free(y);
  }
}

y4o_frame_t *y4o_read_frame_header(y4o_in_t *y){
  FILE *f=y->f;
  int streamno;
  int length;
  double pts;
  char line[80];
  char *p;
  int n;
  size_t ret;

  ret = fread(line, 1, sizeof("FRAME"),f);
  if (ret<sizeof("FRAME"))
  {
    /* A clean EOF should end exactly at a frame-boundary */
    if( ret != 0 && feof(f) )
      fprintf(stderr,"ERROR: EOF reading y4o frame\n");
    y->eof=1;
    return NULL;
  }

  if (strncmp(line, "FRAME ", sizeof("FRAME ")-1)){
    fprintf(stderr,"ERROR: loss of y4o framing\n");
    exit(1);
  }

  /* proceed to get the tags... (overwrite the magic) */
  for (n = 0, p = line; n < 80; n++, p++) {
    if ((ret=fread(p, 1, 1, f))<1){
      if(feof(f))
        fprintf(stderr,"ERROR: EOF reading y4o frame\n");
      return NULL;
    }
    if (*p == '\n') {
      *p = '\0';           /* Replace linefeed by end of string */
      break;
    }
  }
  if (n >= 80) {
    fprintf(stderr,"ERROR: line too long reading y4o frame header\n");
    return NULL;
  }

  /* read tags */
  /* S%d = streamno
     L%d = length
     P%g = pts */

  {
    char *token, *value;
    char tag;

    streamno=-1;
    length=-1;
    pts=-1;

    /* parse fields */
    for (token = strtok(line, " ");
         token != NULL;
         token = strtok(NULL, " ")) {
      if (token[0] == '\0') continue;   /* skip empty strings */
      tag = token[0];
      value = token + 1;
      switch (tag) {
      case 'S':
        streamno = atoi(token+1);
        break;
      case 'L':
        length = atoi(token+1);
        break;
      case 'P':
        pts = atof(token+1);
        break;
      default:
        fprintf(stderr,"ERROR: unknown y4o frame tag\n");
        return NULL;
      }
    }
  }

  if(streamno>=y->num_streams){
    fprintf(stderr,"ERROR: error reading frame; streamno out of range\n");
    return NULL;
  }

  if(streamno==-1 || length==-1 || pts==-1){
    fprintf(stderr,"ERROR: missing y4o frame tags; frame unreadable\n");
    return NULL;
  }

  {
    y4o_frame_t *p=calloc(1,sizeof(*p));

    if(!p){
      fprintf(stderr,"ERROR: unable to allocate memory for frame\n");
      return NULL;
    }

    p->pts=pts;
    p->len=length;
    p->streamno=streamno;

    return p;
  }
}

int y4o_read_frame_data(y4o_in_t *y, y4o_frame_t *p){
  int ret;
  char *data=malloc(p->len);

  if(!data){
    fprintf(stderr,"ERROR: unable to allocate memory for frame\n");
    return -1;
  }
  p->data=data;
  ret=fread(data,1,p->len,y->f);
  if(ret<p->len){
    free(data);
    p->len=0;
    p->data=NULL;
    if(feof(y->f)){
      fprintf(stderr,"ERROR: unable to read frame; EOF\n");
    }else{
      fprintf(stderr,"ERROR: IO error reading frame\n");
    }
    return -1;
  }
  return 0;
}

y4o_frame_t *y4o_read_frame(y4o_in_t *y){
  y4o_frame_t *p = y4o_read_frame_header(y);
  if(!p)return NULL;
  if(y4o_read_frame_data(y,p)){
    y4o_free_frame(p);
    return NULL;
  }
  return p;
}

void y4o_free_frame(y4o_frame_t *p){
  if(p->data)free(p->data);
  memset(p,0,sizeof(*p));
  free(p);
}
