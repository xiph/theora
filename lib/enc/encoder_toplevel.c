/********************************************************************
 *                                                                  *
 * THIS FILE IS PART OF THE OggTheora SOFTWARE CODEC SOURCE CODE.   *
 * USE, DISTRIBUTION AND REPRODUCTION OF THIS LIBRARY SOURCE IS     *
 * GOVERNED BY A BSD-STYLE SOURCE LICENSE INCLUDED WITH THIS SOURCE *
 * IN 'COPYING'. PLEASE READ THESE TERMS BEFORE DISTRIBUTING.       *
 *                                                                  *
 * THE Theora SOURCE CODE IS COPYRIGHT (C) 2002-2007                *
 * by the Xiph.Org Foundation http://www.xiph.org/                  *
 *                                                                  *
 ********************************************************************

  function:
  last mod: $Id$

 ********************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include "toplevel_lookup.h"
#include "../internal.h"
#include "dsp.h"
#include "codec_internal.h"

static void CompressKeyFrame(CP_INSTANCE *cpi){
  int j;

  oggpackB_reset(cpi->oggbuffer);
  cpi->FrameType = KEY_FRAME;
  cpi->LastKeyFrame = 0;

  /* code all blocks */
  for(j=0;j<cpi->frag_total;j++)
    cpi->frag_coded[j]=1;
  
  /* mark as video frame */
  oggpackB_write(cpi->oggbuffer,0,1);
  
  WriteFrameHeader(cpi);
  PickMVs(cpi);
  /* still need to go through mode selection to do MV/mode analysis
     that will be used by subsequent inter frames.  Mode will be
     special-forced to INTRA for each MB. */
  PickModes(cpi);
  
  EncodeData(cpi);
  
  cpi->LastKeyFrame = 1;
}

static int CompressFrame( CP_INSTANCE *cpi ) {
  ogg_uint32_t  i;

  oggpackB_reset(cpi->oggbuffer);
  cpi->FrameType = DELTA_FRAME;

  for ( i = 0; i < cpi->frag_total; i++ ) 
    cpi->frag_coded[i] = 1; /* TEMPORARY */
  
  /* mark as video frame */
  oggpackB_write(cpi->oggbuffer,0,1);

  WriteFrameHeader(cpi);
  PickMVs(cpi);
  if(PickModes( cpi )){
    /* mode analysis thinks this should have been a keyframe; start over and code as a keyframe instead */
    CompressKeyFrame(cpi);  /* Code a key frame */
    return 0;
  }
  
  cpi->LastKeyFrame++;
  EncodeData(cpi);
  
  return 0;
}

/********************** The toplevel: encode ***********************/

static int _ilog(unsigned int v){
  int ret=0;
  while(v){
    ret++;
    v>>=1;
  }
  return(ret);
}

static void theora_encode_dispatch_init(CP_INSTANCE *cpi);

int theora_encode_init(theora_state *th, theora_info *c){
  CP_INSTANCE *cpi;

  memset(th, 0, sizeof(*th));
  /*Currently only the 4:2:0 format is supported.*/
  if(c->pixelformat!=OC_PF_420)return OC_IMPL;
  th->internal_encode=cpi=_ogg_calloc(1,sizeof(*cpi));
  theora_encode_dispatch_init(cpi);

  dsp_static_init (&cpi->dsp);

  c->version_major=TH_VERSION_MAJOR;
  c->version_minor=TH_VERSION_MINOR;
  c->version_subminor=TH_VERSION_SUB;

  if(c->quality>63)c->quality=63;
  if(c->quality<0)c->quality=32;
  if(c->target_bitrate<0)c->target_bitrate=0;
  cpi->BaseQ = c->quality;

  /* Set encoder flags. */
  /* if not AutoKeyframing cpi->ForceKeyFrameEvery = is frequency */
  if(!c->keyframe_auto_p)
    c->keyframe_frequency_force = c->keyframe_frequency;

  /* Set the frame rate variables. */
  if ( c->fps_numerator < 1 )
    c->fps_numerator = 1;
  if ( c->fps_denominator < 1 )
    c->fps_denominator = 1;

  /* don't go too nuts on keyframe spacing; impose a high limit to
     make certain the granulepos encoding strategy works */
  if(c->keyframe_frequency_force>32768)c->keyframe_frequency_force=32768;
  if(c->keyframe_mindistance>32768)c->keyframe_mindistance=32768;
  if(c->keyframe_mindistance>c->keyframe_frequency_force)
    c->keyframe_mindistance=c->keyframe_frequency_force;
  cpi->keyframe_granule_shift=_ilog(c->keyframe_frequency_force-1);

  /* clamp the target_bitrate to a maximum of 24 bits so we get a
     more meaningful value when we write this out in the header. */
  if(c->target_bitrate>(1<<24)-1)c->target_bitrate=(1<<24)-1;

  /* copy in config */
  memcpy(&cpi->info,c,sizeof(*c));
  th->i=&cpi->info;
  th->granulepos=-1;

  /* Set up an encode buffer */
#ifndef LIBOGG2
  cpi->oggbuffer = _ogg_malloc(sizeof(oggpack_buffer));
  oggpackB_writeinit(cpi->oggbuffer);
#else
  cpi->oggbuffer = _ogg_malloc(oggpack_buffersize());
  cpi->oggbufferstate = ogg_buffer_create();
  oggpackB_writeinit(cpi->oggbuffer, cpi->oggbufferstate);
#endif 

  InitFrameInfo(cpi);

  /* Initialise the compression process. */
  /* We always start at frame 1 */
  cpi->CurrentFrame = 1;

  InitHuffmanSet(cpi);

  /* This makes sure encoder version specific tables are initialised */
  memcpy(&cpi->quant_info, &TH_VP31_QUANT_INFO, sizeof(th_quant_info));
  InitQTables(cpi);

  /* Indicate that the next frame to be compressed is the first in the
     current clip. */
  cpi->LastKeyFrame = -1;
  cpi->readyflag = 1;
  
  cpi->HeadersWritten = 0;

  return 0;
}

int theora_encode_YUVin(theora_state *t,
                         yuv_buffer *yuv){
  int dropped = 0;
  ogg_int32_t i;
  unsigned char *LocalDataPtr;
  unsigned char *InputDataPtr;
  CP_INSTANCE *cpi=(CP_INSTANCE *)(t->internal_encode);

  if(!cpi->readyflag)return OC_EINVAL;
  if(cpi->doneflag)return OC_EINVAL;

  /* If frame size has changed, abort out for now */
  if (yuv->y_height != (int)cpi->info.height ||
      yuv->y_width != (int)cpi->info.width )
    return(-1);

  /* Copy over input YUV to internal YUV buffers. */
  /* we invert the image for backward compatibility with VP3 */
  /* First copy over the Y data */
  LocalDataPtr = cpi->frame + cpi->offset[0] + cpi->stride[0]*(yuv->y_height - 1);
  InputDataPtr = yuv->y;
  for ( i = 0; i < yuv->y_height; i++ ){
    memcpy( LocalDataPtr, InputDataPtr, yuv->y_width );
    LocalDataPtr -= cpi->stride[0];
    InputDataPtr += yuv->y_stride;
  }

  /* Now copy over the U data */
  LocalDataPtr = cpi->frame + cpi->offset[1] + cpi->stride[1]*(yuv->uv_height - 1);
  InputDataPtr = yuv->u;
  for ( i = 0; i < yuv->uv_height; i++ ){
    memcpy( LocalDataPtr, InputDataPtr, yuv->uv_width );
    LocalDataPtr -= cpi->stride[1];
    InputDataPtr += yuv->uv_stride;
  }

  /* Now copy over the V data */
  LocalDataPtr = cpi->frame + cpi->offset[2] + cpi->stride[2]*(yuv->uv_height - 1);
  InputDataPtr = yuv->v;
  for ( i = 0; i < yuv->uv_height; i++ ){
    memcpy( LocalDataPtr, InputDataPtr, yuv->uv_width );
    LocalDataPtr -= cpi->stride[2];
    InputDataPtr += yuv->uv_stride;
  }

  /* don't allow generating invalid files that overflow the p-frame
     shift, even if keyframe_auto_p is turned off */
  if(cpi->LastKeyFrame==-1 || cpi->LastKeyFrame >= (ogg_uint32_t)
     cpi->info.keyframe_frequency_force){
    CompressKeyFrame(cpi);
  } else  {
    /* Compress the frame. */
    dropped = CompressFrame(cpi);
  }
  
  /* Update stats variables. */
  cpi->CurrentFrame++;
  cpi->packetflag=1;

  t->granulepos=
    ((cpi->CurrentFrame - cpi->LastKeyFrame)<<cpi->keyframe_granule_shift)+
    cpi->LastKeyFrame - 1;

  return 0;
}

int theora_encode_packetout( theora_state *t, int last_p, ogg_packet *op){
  CP_INSTANCE *cpi=(CP_INSTANCE *)(t->internal_encode);
  long bytes=oggpackB_bytes(cpi->oggbuffer);

  if(!bytes)return(0);
  if(!cpi->packetflag)return(0);
  if(cpi->doneflag)return(-1);

#ifndef LIBOGG2
  op->packet=oggpackB_get_buffer(cpi->oggbuffer);
#else
  op->packet=oggpackB_writebuffer(cpi->oggbuffer);
#endif
  op->bytes=bytes;
  op->b_o_s=0;
  op->e_o_s=last_p;

  op->packetno=cpi->CurrentFrame;
  op->granulepos=t->granulepos;

  cpi->packetflag=0;
  if(last_p)cpi->doneflag=1;

  return 1;
}

static void _tp_writebuffer(oggpack_buffer *opb, const char *buf, const long len)
{
  long i;

  for (i = 0; i < len; i++)
    oggpackB_write(opb, *buf++, 8);
}

static void _tp_writelsbint(oggpack_buffer *opb, long value)
{
  oggpackB_write(opb, value&0xFF, 8); 
  oggpackB_write(opb, value>>8&0xFF, 8);
  oggpackB_write(opb, value>>16&0xFF, 8);
  oggpackB_write(opb, value>>24&0xFF, 8);
}

/* build the initial short header for stream recognition and format */
int theora_encode_header(theora_state *t, ogg_packet *op){
  CP_INSTANCE *cpi=(CP_INSTANCE *)(t->internal_encode);
  int offset_y;

#ifndef LIBOGG2
  oggpackB_reset(cpi->oggbuffer);
#else
  oggpackB_writeinit(cpi->oggbuffer, cpi->oggbufferstate);
#endif
  oggpackB_write(cpi->oggbuffer,0x80,8);
  _tp_writebuffer(cpi->oggbuffer, "theora", 6);

  oggpackB_write(cpi->oggbuffer,TH_VERSION_MAJOR,8);
  oggpackB_write(cpi->oggbuffer,TH_VERSION_MINOR,8);
  oggpackB_write(cpi->oggbuffer,TH_VERSION_SUB,8);

  oggpackB_write(cpi->oggbuffer,cpi->info.width>>4,16);
  oggpackB_write(cpi->oggbuffer,cpi->info.height>>4,16);
  oggpackB_write(cpi->oggbuffer,cpi->info.frame_width,24);
  oggpackB_write(cpi->oggbuffer,cpi->info.frame_height,24);
  oggpackB_write(cpi->oggbuffer,cpi->info.offset_x,8);
  /* Applications use offset_y to mean offset from the top of the image; the
   * meaning in the bitstream is the opposite (from the bottom). Transform.
   */
  offset_y = cpi->info.height - cpi->info.frame_height - 
    cpi->info.offset_y;
  oggpackB_write(cpi->oggbuffer,offset_y,8);

  oggpackB_write(cpi->oggbuffer,cpi->info.fps_numerator,32);
  oggpackB_write(cpi->oggbuffer,cpi->info.fps_denominator,32);
  oggpackB_write(cpi->oggbuffer,cpi->info.aspect_numerator,24);
  oggpackB_write(cpi->oggbuffer,cpi->info.aspect_denominator,24);

  oggpackB_write(cpi->oggbuffer,cpi->info.colorspace,8);
  oggpackB_write(cpi->oggbuffer,cpi->info.target_bitrate,24);
  oggpackB_write(cpi->oggbuffer,cpi->info.quality,6);

  oggpackB_write(cpi->oggbuffer,cpi->keyframe_granule_shift,5);

  oggpackB_write(cpi->oggbuffer,cpi->info.pixelformat,2);

  oggpackB_write(cpi->oggbuffer,0,3); /* spare config bits */

#ifndef LIBOGG2
  op->packet=oggpackB_get_buffer(cpi->oggbuffer);
#else
  op->packet=oggpackB_writebuffer(cpi->oggbuffer);
#endif
  op->bytes=oggpackB_bytes(cpi->oggbuffer);

  op->b_o_s=1;
  op->e_o_s=0;

  op->packetno=0;

  op->granulepos=0;
  cpi->packetflag=0;

  return(0);
}

/* build the comment header packet from the passed metadata */
int theora_encode_comment(theora_comment *tc, ogg_packet *op)
{
  const char *vendor = theora_version_string();
  const int vendor_length = strlen(vendor);
  oggpack_buffer *opb;

#ifndef LIBOGG2
  opb = _ogg_malloc(sizeof(oggpack_buffer));
  oggpackB_writeinit(opb);
#else
  opb = _ogg_malloc(oggpack_buffersize());
  oggpackB_writeinit(opb, ogg_buffer_create());
#endif 
  oggpackB_write(opb, 0x81, 8);
  _tp_writebuffer(opb, "theora", 6);

  _tp_writelsbint(opb, vendor_length);
  _tp_writebuffer(opb, vendor, vendor_length);

  _tp_writelsbint(opb, tc->comments);
  if(tc->comments){
    int i;
    for(i=0;i<tc->comments;i++){
      if(tc->user_comments[i]){
        _tp_writelsbint(opb,tc->comment_lengths[i]);
        _tp_writebuffer(opb,tc->user_comments[i],tc->comment_lengths[i]);
      }else{
        oggpackB_write(opb,0,32);
      }
    }
  }
  op->bytes=oggpack_bytes(opb);

#ifndef LIBOGG2
  /* So we're expecting the application will free this? */
  op->packet=_ogg_malloc(oggpack_bytes(opb));
  memcpy(op->packet, oggpack_get_buffer(opb), oggpack_bytes(opb));
  oggpack_writeclear(opb);
#else
  op->packet = oggpack_writebuffer(opb);
  /* When the application puts op->packet into a stream_state object,
     it becomes the property of libogg2's internal memory management. */
#endif

  _ogg_free(opb);

  op->b_o_s=0;
  op->e_o_s=0;

  op->packetno=0;
  op->granulepos=0;

  return (0);
}

/* build the final header packet with the tables required
   for decode */
int theora_encode_tables(theora_state *t, ogg_packet *op){
  CP_INSTANCE *cpi=(CP_INSTANCE *)(t->internal_encode);

#ifndef LIBOGG2
  oggpackB_reset(cpi->oggbuffer);
#else
  oggpackB_writeinit(cpi->oggbuffer, cpi->oggbufferstate);
#endif
  oggpackB_write(cpi->oggbuffer,0x82,8);
  _tp_writebuffer(cpi->oggbuffer,"theora",6);

  WriteQTables(cpi,cpi->oggbuffer);
  WriteHuffmanTrees(cpi->HuffRoot_VP3x,cpi->oggbuffer);

#ifndef LIBOGG2
  op->packet=oggpackB_get_buffer(cpi->oggbuffer);
#else
  op->packet=oggpackB_writebuffer(cpi->oggbuffer);
#endif
  op->bytes=oggpackB_bytes(cpi->oggbuffer);

  op->b_o_s=0;
  op->e_o_s=0;

  op->packetno=0;

  op->granulepos=0;
  cpi->packetflag=0;

  cpi->HeadersWritten = 1;

  return(0);
}

static void theora_encode_clear (theora_state  *th){
  CP_INSTANCE *cpi;
  cpi=(CP_INSTANCE *)th->internal_encode;
  if(cpi){

    ClearHuffmanSet(cpi);
    ClearFrameInfo(cpi);

    oggpackB_writeclear(cpi->oggbuffer);
    _ogg_free(cpi->oggbuffer);
    _ogg_free(cpi);
  }

  memset(th,0,sizeof(*th));
}


/* returns, in seconds, absolute time of current packet in given
   logical stream */
static double theora_encode_granule_time(theora_state *th,
 ogg_int64_t granulepos){
#ifndef THEORA_DISABLE_FLOAT
  CP_INSTANCE *cpi=(CP_INSTANCE *)(th->internal_encode);

  if(granulepos>=0){
    ogg_int64_t iframe=granulepos>>cpi->keyframe_granule_shift;
    ogg_int64_t pframe=granulepos-(iframe<<cpi->keyframe_granule_shift);

    return (iframe+pframe)*
      ((double)cpi->info.fps_denominator/cpi->info.fps_numerator);

  }
#endif

  return(-1); /* negative granulepos or float calculations disabled */
}

/* returns frame number of current packet in given logical stream */
static ogg_int64_t theora_encode_granule_frame(theora_state *th,
 ogg_int64_t granulepos){
  CP_INSTANCE *cpi=(CP_INSTANCE *)(th->internal_encode);

  if(granulepos>=0){
    ogg_int64_t iframe=granulepos>>cpi->keyframe_granule_shift;
    ogg_int64_t pframe=granulepos-(iframe<<cpi->keyframe_granule_shift);

    return (iframe+pframe);
  }

  return(-1);
}


static int theora_encode_control(theora_state *th,int req,
 void *buf,size_t buf_sz) {
  CP_INSTANCE *cpi;
  int value;
  
  if(th == NULL)
    return TH_EFAULT;

  cpi = th->internal_encode;
  
  switch(req) {
    case TH_ENCCTL_SET_QUANT_PARAMS:
      if( ( buf==NULL&&buf_sz!=0 )
  	   || ( buf!=NULL&&buf_sz!=sizeof(th_quant_info) )
  	   || cpi->HeadersWritten ){
        return TH_EINVAL;
      }
      
      memcpy(&cpi->quant_info, buf, sizeof(th_quant_info));
      InitQTables(cpi);
      
      return 0;
    case TH_ENCCTL_SET_VP3_COMPATIBLE:
      if(cpi->HeadersWritten)
        return TH_EINVAL;
      
      memcpy(&cpi->quant_info, &TH_VP31_QUANT_INFO, sizeof(th_quant_info));
      InitQTables(cpi);
      
      return 0;
    case TH_ENCCTL_SET_SPLEVEL:
      if(buf == NULL || buf_sz != sizeof(int))
        return TH_EINVAL;
      
      memcpy(&value, buf, sizeof(int));
            
      switch(value) {
        case 0:
          cpi->MotionCompensation = 1;
          cpi->info.quick_p = 0;
        break;
        
        case 1:
          cpi->MotionCompensation = 1;
          cpi->info.quick_p = 1;
        break;
        
        case 2:
          cpi->MotionCompensation = 0;
          cpi->info.quick_p = 1;
        break;
        
        default:
          return TH_EINVAL;    
      }
      
      return 0;
    case TH_ENCCTL_GET_SPLEVEL_MAX:
      value = 2;
      memcpy(buf, &value, sizeof(int));
      return 0;
    default:
      return TH_EIMPL;
  }
}

static void theora_encode_dispatch_init(CP_INSTANCE *cpi){
  cpi->dispatch_vtbl.clear=theora_encode_clear;
  cpi->dispatch_vtbl.control=theora_encode_control;
  cpi->dispatch_vtbl.granule_frame=theora_encode_granule_frame;
  cpi->dispatch_vtbl.granule_time=theora_encode_granule_time;
}
