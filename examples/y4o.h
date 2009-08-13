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

static char *y4o_chromaformat_long[]={
  "monochrome",            //0
  "4:1:1 [ntscdv chroma]", //1
  "4:2:0 [jpeg chroma]",   //2 chroma sample is centered vertically and horizontally between luma samples
  "4:2:0 [mpeg2 chroma]",  //3 chroma sample is centered vertically between lines, cosited horizontally */
  "4:2:0 [paldv chroma]",  //4 chroma sample is cosited vertically and horizontally */
  "4:2:0 [unknown chroma]",//5
  "4:2:2 [jpeg chroma]",   //6 chroma sample is horizontally centered between luma samples */
  "4:2:2 [smpte chroma]",  //7 chroma sample is cosited horizontally */
  "4:2:2 [unknown chroma]",//8
  "4:4:4",                 //9
  "4:4:4+alpha",           //10
  NULL
};

typedef enum {
  Y4O_Cmono=0,
  Y4O_C411ntscdv=1,
  Y4O_C420jpeg=2,
  Y4O_C420mpeg2=3,
  Y4O_C420paldv=4,
  Y4O_C420unknown=5,
  Y4O_C422jpeg=6,
  Y4O_C422smpte=7,
  Y4O_C422unknown=8,
  Y4O_C444=9,
  Y4O_C444alpha=10,
} y4o_chromafmt;

typedef enum {
  Y4O_STREAM_INVALID=0,
  Y4O_STREAM_VIDEO=1,
  Y4O_STREAM_AUDIO=2
} y4o_stream_type;

typedef enum {
  Y4O_I_INVALID=-1,
  Y4O_I_PROGRESSIVE=0,
  Y4O_I_TOP_FIRST=1,
  Y4O_I_BOTTOM_FIRST=2
} y4o_interlace_type;

typedef struct {
  double pts;
  size_t len;
  int streamno;
  char *data;
} y4o_frame_t;

typedef struct {
  y4o_stream_type type;
  int stream_num;

  union stream_t {
    struct {
      int rate;
      int ch;
    } audio;
    struct {
      int fps_n;
      int fps_d;
      int pa_n;
      int pa_d;
      int frame_n;
      int frame_d;
      int format;
      int w;
      int h;
      y4o_interlace_type i;
    } video;
  }m;

} y4o_stream_t;

typedef struct {
  FILE *f;
  int eof;
  y4o_stream_t **streams;
  int num_streams;
  int synced;
  int seekable;
} y4o_in_t;

y4o_in_t    *y4o_init(FILE *f, char *first);
void         y4o_free(y4o_in_t *y);
y4o_frame_t *y4o_read_frame_header(y4o_in_t *y);
int          y4o_read_frame_data(y4o_in_t *y, y4o_frame_t *p);
y4o_frame_t *y4o_read_frame(y4o_in_t *y);
void         y4o_free_frame(y4o_frame_t *p);
