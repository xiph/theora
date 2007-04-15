/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2006                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function: example dumpvid application; dumps  Theora streams
  last mod: $Id$

 ********************************************************************/

/* By Mauricio Piacentini (mauricio at xiph.org) */
/*  simply dump decoded YUV data, for verification of theora bitstream */

#define _GNU_SOURCE
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
/*Yes, yes, we're going to hell.*/
#if defined(_WIN32)
#include <io.h>
#endif
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include "getopt.h"
#include "theora/theoradec.h"

const char *optstring = "o:rf";
struct option options [] = {
  {"output",required_argument,NULL,'o'},
  {"raw",no_argument, NULL,'r'}, /*Disable YUV4MPEG2 headers:*/
  {"fps-only",no_argument, NULL, 'f'}, /* Only interested in fps of decode loop */
  {NULL,0,NULL,0}
};

/* Helper; just grab some more compressed bitstream and sync it for
   page extraction */
int buffer_data(FILE *in,ogg_sync_state *oy){
  char *buffer=ogg_sync_buffer(oy,4096);
  int bytes=fread(buffer,1,4096,in);
  ogg_sync_wrote(oy,bytes);
  return(bytes);
}

/* never forget that globals are a one-way ticket to Hell */
/* Ogg and codec state for demux/decode */
ogg_sync_state     oy;
ogg_page           og;
ogg_stream_state   vo;
ogg_stream_state   to;
th_info        ti;
th_comment     tc;
th_setup_info *ts;
th_dec_ctx    *td;

int              theora_p=0;
int              theora_processing_headers;
int              stateflag=0;

/* single frame video buffering */
int          videobuf_ready=0;
ogg_int64_t  videobuf_granulepos=-1;
double       videobuf_time=0;
int          raw=0;

FILE* outfile = NULL;

int got_sigint=0;
static void sigint_handler (int signal) {
  got_sigint = 1;
}

static th_ycbcr_buffer ycbcr;

static void stripe_decoded(th_ycbcr_buffer _dst,th_ycbcr_buffer _src,
 int _fragy0,int _fragy_end){
  int pli;
  for(pli=0;pli<3;pli++){
    int yshift;
    int y_end;
    int y;
    yshift=pli!=0&&!(ti.pixel_fmt&2);
    y_end=_fragy_end<<3-yshift;
    for(y=_fragy0<<3-yshift;y<y_end;y++){
      memcpy(_dst[pli].data+y*_dst[pli].ystride,
       _src[pli].data+y*_src[pli].ystride,_src[pli].width);
    }
  }
}

static void open_video(void){
  th_stripe_callback cb;
  int                    pli;
  /*Here we allocate a buffer so we can use the striped decode feature.
    There's no real reason to do this in this application, because we want to
     write to the file top-down, but the frame gets decoded bottom up, so we
     have to buffer it all anyway.
    But this illustrates how the API works.*/
  for(pli=0;pli<3;pli++){
    int xshift;
    int yshift;
    xshift=pli!=0&&!(ti.pixel_fmt&1);
    yshift=pli!=0&&!(ti.pixel_fmt&2);
    ycbcr[pli].data=(unsigned char *)malloc(
     (ti.frame_width>>xshift)*(ti.frame_height>>yshift)*sizeof(char));
    ycbcr[pli].ystride=ti.frame_width>>xshift;
    ycbcr[pli].width=ti.frame_width>>xshift;
    ycbcr[pli].height=ti.frame_height>>yshift;
  }
  /*Similarly, since ycbcr is a global, there's no real reason to pass it as
     the context.
    In a more object-oriented decoder, we could pass the "this" pointer
     instead (though in C++ platform-dependent calling convention differences
     prevent us from using a real member function pointer).*/
  cb.ctx=ycbcr;
  cb.stripe_decoded=(th_stripe_decoded_func)stripe_decoded;
  th_decode_ctl(td,TH_DECCTL_SET_STRIPE_CB,&cb,sizeof(cb));
}

static void video_write(void){
  int pli;
  int i;

  /*Uncomment the following to do normal, non-striped decoding.
  th_ycbcr_buffer ycbcr;
  th_decode_ycbcr_out(td,ycbcr);*/
  fprintf(outfile, "FRAME\n");

  for(pli=0;pli<3;pli++){
    for(i=0;i<ycbcr[pli].height;i++){
      fwrite(ycbcr[pli].data+ycbcr[pli].ystride*i, 1,
       ycbcr[pli].width, outfile);
    }
  }
}

/* helper: push a page into the appropriate steam */
/* this can be done blindly; a stream won't accept a page
                that doesn't belong to it */
static int queue_page(ogg_page *page){
  if(theora_p)ogg_stream_pagein(&to,page);
  return 0;
}

static void usage(void){
  fprintf(stderr,
          "Usage: dumpvid <file.ogg> > outfile\n"
          "input is read from stdin if no file is passed on the command line\n"
          "\n"
  );
}

int main(int argc,char *argv[]){

  ogg_packet op;

  int long_option_index;
  int c;

  int frames = 0;

  FILE *infile = stdin;
  outfile = stdout;

  struct timeb start;
  struct timeb after;
  struct timeb last;
  int fps_only=0;

#ifdef _WIN32 /* We need to set stdin/stdout to binary mode on windows. */
  /* Beware the evil ifdef. We avoid these where we can, but this one we
     cannot. Don't add any more, you'll probably go to hell if you do. */
  _setmode( _fileno( stdin ), _O_BINARY );
  _setmode( _fileno( stdout ), _O_BINARY );
#endif

  /* Process option arguments. */
  while((c=getopt_long(argc,argv,optstring,options,&long_option_index))!=EOF){
    switch(c){
    case 'o':
      if(!strcmp(optarg,"-")){
	outfile=fopen(optarg,"wb");
	if(outfile==NULL){
	  fprintf(stderr,"Unable to open output file '%s'\n", optarg);
	  exit(1);
	}
      }else{
	outfile=stdout;
      }
      break;
      
    case 'r':
	raw=1;
	break;
	
    case 'f':
      fps_only = 1;
      outfile = NULL;
      break;
      
    default:
      usage();
    }
  }
  if(optind<argc){
    infile=fopen(argv[optind],"rb");
    if(infile==NULL){
      fprintf(stderr,"Unable to open '%s' for extraction.\n", argv[optind]);
      exit(1);
    }
    if(++optind<argc){
      usage();
      exit(1);
    }
  }

  /* start up Ogg stream synchronization layer */
  ogg_sync_init(&oy);

  /* init supporting Vorbis structures needed in header parsing */
  /*vorbis_info_init(&vi);*/
  /*vorbis_comment_init(&vc);*/

  /* init supporting Theora structures needed in header parsing */
  th_comment_init(&tc);
  th_info_init(&ti);

  /* Ogg file open; parse the headers */
  /* Only interested in Vorbis/Theora streams */
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
      ogg_stream_packetpeek(&test,&op);

      /* identify the codec: try theora */
      if(!theora_p && (theora_processing_headers=
       th_decode_headerin(&ti,&tc,&ts,&op))>=0){
        /* it is theora */
        memcpy(&to,&test,sizeof(test));
        theora_p=1;
        /*Advance past the successfully processed header.*/
        if(theora_processing_headers)ogg_stream_packetout(&to,NULL);
      }else{
        /* whatever it is, we don't care about it */
        ogg_stream_clear(&test);
      }
    }
    /* fall through to non-bos page parsing */
  }

  /* we're expecting more header packets. */
  while(theora_p && theora_processing_headers){
    int ret;

    /* look for further theora headers */
    while(theora_processing_headers&&(ret=ogg_stream_packetpeek(&to,&op))){
      if(ret<0)continue;
      theora_processing_headers=th_decode_headerin(&ti,&tc,&ts,&op);
      if(theora_processing_headers<0){
        printf("Error parsing Theora stream headers; corrupt stream?\n");
        exit(1);
      }
      else if(theora_processing_headers>0){
        /*Advance past the successfully processed header.*/
        ogg_stream_packetout(&to,NULL);
      }
      theora_p++;
    }

    /*Stop now so we don't fail if there aren't enough pages in a short
       stream.*/
    if(!(theora_p && theora_processing_headers))break;

    /* The header pages/packets will arrive before anything else we
       care about, or the stream is not obeying spec */

    if(ogg_sync_pageout(&oy,&og)>0){
      queue_page(&og); /* demux into the appropriate stream */
    }else{
      int ret=buffer_data(infile,&oy); /* someone needs more data */
      if(ret==0){
        fprintf(stderr,"End of file while searching for codec headers.\n");
        exit(1);
      }
    }
  }

  /* and now we have it all.  initialize decoders */
  if(theora_p){
    td=th_decode_alloc(&ti,ts);
    fprintf(stderr,"Ogg logical stream %lx is Theora %dx%d %.02f fps video\nEncoded frame content is %dx%d with %dx%d offset\n",
            to.serialno,ti.frame_width,ti.frame_height,
            (double)ti.fps_numerator/ti.fps_denominator,
            ti.pic_width, ti.pic_height, ti.pic_x, ti.pic_y);
  }else{
    /* tear down the partial theora setup */
    th_info_clear(&ti);
    th_comment_clear(&tc);
  }
  /*Either way, we're done with the codec setup data.*/
  th_setup_free(ts);

  /* open video */
  if(theora_p)open_video();

  if(!raw && outfile){
    fprintf(outfile, "YUV4MPEG2 W%d H%d F%d:%d I%c A%d:%d\n",
     ti.pic_width,ti.pic_height,ti.fps_numerator,ti.fps_denominator,'p',
     ti.aspect_numerator,ti.aspect_denominator);
  }

  /* install signal handler */
  signal (SIGINT, sigint_handler);

  /* on to the main decode loop.*/

  stateflag=0; /* playback has not begun */
  /* queue any remaining pages from data we buffered but that did not
      contain headers */
  while(ogg_sync_pageout(&oy,&og)>0){
    queue_page(&og);
  }

  if(fps_only){
    ftime(&start);
    ftime(&last);
  }

  while(!got_sigint){

    while(theora_p && !videobuf_ready){
      /* theora is one in, one out... */
      if(ogg_stream_packetout(&to,&op)>0){

        if(th_decode_packetin(td,&op,&videobuf_granulepos)>=0){
          videobuf_time=th_granule_time(td,videobuf_granulepos);
          videobuf_ready=1;
          frames++;
	  if(fps_only)
	    ftime(&after);
        }
	
      }else
        break;
    }

    if(fps_only && (videobuf_ready || fps_only==2)){
      long ms = 
	after.time*1000.+after.millitm-
	(last.time*1000.+last.millitm);
      
      if(ms>500 || fps_only==1 || 
	 (feof(infile) && !videobuf_ready)){
	float file_fps = (float)ti.fps_numerator/ti.fps_denominator;
	fps_only=2;
	
	ms = after.time*1000.+after.millitm-
	  (start.time*1000.+start.millitm);
	
	fprintf(stderr,"\rframe:%d rate:%.2fx           ",
		frames, 
		frames*1000./(ms*file_fps));
	memcpy(&last,&after,sizeof(last));
      }
    }
    
    if(!videobuf_ready  && feof(infile))break;

    if(!videobuf_ready ){
      /* no data yet for somebody.  Grab another page */
      buffer_data(infile,&oy);
      while(ogg_sync_pageout(&oy,&og)>0){
        queue_page(&og);
      }
    }
    /* dumpvideo frame, and get new one */
    else 
      if(outfile)video_write();

    videobuf_ready=0;
  }

  /* close everything */

  if(theora_p){
    ogg_stream_clear(&to);
    th_decode_free(td);
    th_comment_clear(&tc);
    th_info_clear(&ti);
  }
  ogg_sync_clear(&oy);

  if(infile && infile!=stdin)fclose(infile);
  if(outfile && outfile!=stdout)fclose(outfile);

  fprintf(stderr, "\n\n%d frames\n", frames);
  fprintf(stderr, "\nDone.\n");

  return(0);

}
