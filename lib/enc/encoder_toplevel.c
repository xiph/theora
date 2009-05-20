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
#include "codec_internal.h"
#include "mathops.h"
#include "../dec/ocintrin.h"
#if defined(OC_X86_ASM)
# include "x86/x86enc.h"
#endif



static void oc_enc_calc_lambda(CP_INSTANCE *cpi){
  ogg_int64_t l;
  int         qti;
  qti=cpi->FrameType!=KEY_FRAME;
  /*For now, lambda is fixed depending on the qi value and frame type:
      lambda=scale[qti]*(qavg[qti][qi]**1.5),
     where scale={2.25,1.125}.
    A more adaptive scheme might perform better, but Theora's behavior does not
     seem to conform to existing models in the literature.*/
  /*If rate control is active, use the lambda for the _target_ quantizer.
    This allows us to scale to rates slightly lower than we'd normally be able
     to reach, and give the rate control a semblance of "fractional QI"
     precision.*/
  if(cpi->info.target_bitrate>0)l=cpi->rc.log_qtarget;
  else l=cpi->log_qavg[qti][cpi->BaseQ];
  /*Raise to the 1.5 power.*/
  l+=l>>1;
  /*Multiply by 1.125.*/
  l+=0x00570068E7EF5A1ELL;
  /*And multiply by an extra factor of 2 for INTRA frames.*/
  if(!qti)l+=OC_Q57(1);
  /*The upper bound here is 0x48000.*/
  cpi->lambda=(int)oc_bexp64(l);
}



static void oc_rc_state_init(oc_rc_state *_rc,const theora_info *_info){
  ogg_int64_t npixels;
  ogg_int64_t ibpp;
  /*TODO: These parameters should be exposed in a th_enc_ctl() API.*/
  _rc->bits_per_frame=(_info->target_bitrate*
   (ogg_int64_t)_info->fps_denominator)/_info->fps_numerator;
  /*Insane framerates or frame sizes mean insane bitrates.
    Let's not get carried away.*/
  if(_rc->bits_per_frame>0x40000000000000LL){
    _rc->bits_per_frame=(ogg_int64_t)0x40000000000000LL;
  }
  else if(_rc->bits_per_frame<32)_rc->bits_per_frame=32;
  /*The buffer size is set equal to the keyframe interval, clamped to the range
     [8,256] frames.
    The 8 frame minimum gives us some chance to distribute bit estimation
     errors.
    The 256 frame maximum means we'll require 8-10 seconds of pre-buffering at
     24-30 fps, which is not unreasonable.*/
  _rc->buf_delay=_info->keyframe_frequency_force>256?
   256:_info->keyframe_frequency_force;
  _rc->buf_delay=OC_MAXI(_rc->buf_delay,12);
  _rc->max=_rc->bits_per_frame*_rc->buf_delay;
  /*Start with a buffer fullness of 75%.
    We can require fully half the buffer for a keyframe, and so this initial
     level gives us maximum flexibility for over/under-shooting in subsequent
     frames.*/
  _rc->target=_rc->fullness=(_rc->max+1>>1)+(_rc->max+2>>2);
  /*Pick exponents and initial scales for quantizer selection.
    TODO: These still need to be tuned.*/
  npixels=_info->width*(ogg_int64_t)_info->height;
  _rc->log_npixels=oc_blog64(npixels);
  ibpp=npixels/_rc->bits_per_frame;
  if(ibpp<1){
    _rc->exp[0]=59;
    _rc->log_scale[0]=oc_blog64(1997)-OC_Q57(8);
  }
  else if(ibpp<2){
    _rc->exp[0]=55;
    _rc->log_scale[0]=oc_blog64(1604)-OC_Q57(8);
  }
  else{
    _rc->exp[0]=48;
    _rc->log_scale[0]=oc_blog64(834)-OC_Q57(8);
  }
  if(ibpp<4){
    _rc->exp[1]=100;
    _rc->log_scale[1]=oc_blog64(2249)-OC_Q57(8);
  }
  else if(ibpp<8){
    _rc->exp[1]=95;
    _rc->log_scale[1]=oc_blog64(1751)-OC_Q57(8);
  }
  else{
    _rc->exp[1]=73;
    _rc->log_scale[1]=oc_blog64(1260)-OC_Q57(8);
  }
}

static void oc_enc_update_rc_state(CP_INSTANCE *cpi,
 long _bits,int _qti,int _qi,int _trial){
  /*Note, setting OC_SCALE_SMOOTHING[1] to 0x80 (0.5), which one might expect
     to be a reasonable value, actually causes a feedback loop with, e.g., 12
     fps content encoded at 24 fps; use values near 0 or near 1 for now.
    TODO: Should probably revisit using an exponential moving average in the
     first place at some point.*/
  static const unsigned OC_SCALE_SMOOTHING[2]={0x13,0x00};
  ogg_int64_t   log_scale;
  ogg_int64_t   log_bits;
  ogg_int64_t   log_qexp;
  /*Compute the estimated scale factor for this frame type.*/
  log_bits=oc_blog64(_bits);
  log_qexp=cpi->log_qavg[_qti][_qi]-OC_Q57(2);
  log_qexp=(log_qexp>>6)*(cpi->rc.exp[_qti]);
  log_scale=OC_MINI(log_bits-cpi->rc.log_npixels+log_qexp,OC_Q57(16));
  /*Use it to set that factor directly if this was a trial.*/
  if(_trial)cpi->rc.log_scale[_qti]=log_scale;
  else{
    /*Otherwise update an exponential moving average.*/
    cpi->rc.log_scale[_qti]=log_scale
     +(cpi->rc.log_scale[_qti]-log_scale+128>>8)*OC_SCALE_SMOOTHING[_qti];
    /*And update the buffer fullness level.*/
    cpi->rc.fullness+=cpi->rc.bits_per_frame*(1+cpi->dup_count)-_bits;
    /*If we're too quick filling the buffer, that rate is lost forever.*/
    if(cpi->rc.fullness>cpi->rc.max)cpi->rc.fullness=cpi->rc.max;
  }
}

static int oc_enc_select_qi(CP_INSTANCE *cpi,int _qti,int _trial){
  ogg_int64_t  rate_total;
  ogg_uint32_t next_key_frame;
  int          nframes[2];
  int          buf_delay;
  ogg_int64_t  log_qtarget;
  int          best_qi;
  ogg_int64_t  best_qdiff;
  int          qi;
  /*Figure out how to re-distribute bits so that we hit our fullness target
     before the last keyframe in our current buffer window (after the current
     frame), or the end of the buffer window, whichever comes first.*/
  next_key_frame=_qti?cpi->info.keyframe_frequency_force-cpi->LastKeyFrame:0;
  nframes[0]=(cpi->rc.buf_delay-OC_MINI(next_key_frame,cpi->rc.buf_delay)
   +cpi->info.keyframe_frequency_force-1)/cpi->info.keyframe_frequency_force;
  if(nframes[0]+_qti>1){
    buf_delay=next_key_frame+(nframes[0]-1)*cpi->info.keyframe_frequency_force;
    nframes[0]--;
  }
  else buf_delay=cpi->rc.buf_delay;
  nframes[1]=buf_delay-nframes[0];
  rate_total=cpi->rc.fullness-cpi->rc.target
   +buf_delay*cpi->rc.bits_per_frame;
  /*Downgrade the delta frame rate to correspond to the current dup count.
    This will way over-estimate the bits to use for an occasional dup (as
     opposed to a consistent dup count, as used with VFR input), but the
     hysteresis on the quantizer below will keep us from going out of control,
     and we _do_ have more bits to spend after all.*/
  if(cpi->dup_count>0)nframes[1]=(nframes[1]+cpi->dup_count)/(cpi->dup_count+1);
  /*If there aren't enough bits to achieve our desired fullness level, use the
     minimum quality permitted.*/
  if(rate_total<=0)log_qtarget=OC_QUANT_MAX_LOG;
  else{
    static const unsigned char KEY_RATIO[2]={29,32};
    ogg_int64_t   log_scale0;
    ogg_int64_t   log_scale1;
    ogg_int64_t   prevr;
    ogg_int64_t   curr;
    ogg_int64_t   realr;
    int           i;
    log_scale0=cpi->rc.log_scale[_qti]+cpi->rc.log_npixels;
    log_scale1=cpi->rc.log_scale[1-_qti]+cpi->rc.log_npixels;
    curr=(rate_total+(buf_delay>>1))/buf_delay;
    realr=curr*KEY_RATIO[_qti]+16>>5;
    for(i=0;i<10;i++){
      ogg_int64_t rdiff;
      ogg_int64_t rderiv;
      ogg_int64_t log_rpow;
      ogg_int64_t rscale;
      ogg_int64_t drscale;
      ogg_int64_t bias;
      prevr=curr;
      log_rpow=oc_blog64(prevr)-log_scale0;
      log_rpow=(log_rpow+(cpi->rc.exp[_qti]>>1))/cpi->rc.exp[_qti]*
       cpi->rc.exp[1-_qti];
      rscale=nframes[1-_qti]*KEY_RATIO[1-_qti]*
       oc_bexp64(log_scale1+log_rpow);
      rdiff=nframes[_qti]*KEY_RATIO[_qti]*prevr+rscale-(rate_total<<5);
      drscale=(rscale+(cpi->rc.exp[_qti]>>1))/cpi->rc.exp[_qti]*
       cpi->rc.exp[1-_qti]/prevr;
      rderiv=nframes[_qti]*KEY_RATIO[_qti]+drscale;
      if(rderiv==0)break;
      bias=rderiv+OC_SIGNMASK(rdiff^rderiv)^OC_SIGNMASK(rdiff^rderiv);
      curr=prevr-((rdiff<<1)+bias)/(rderiv<<1);
      realr=curr*KEY_RATIO[_qti]+16>>5;
      if(curr<=0||realr>rate_total||prevr==curr)break;
    }
    log_qtarget=OC_Q57(2)-((oc_blog64(realr)-log_scale0+(cpi->rc.exp[_qti]>>1))/
     cpi->rc.exp[_qti]<<6);
    log_qtarget=OC_MINI(log_qtarget,OC_QUANT_MAX_LOG);
  }
  /*If this was not one of the initial frames, limit the change in quality.*/
  if(!_trial){
    ogg_int64_t log_qmin;
    ogg_int64_t log_qmax;
    /*Clamp the target quantizer to within [0.8*Q,1.2*Q], where Q is the
       current quantizer.
      TODO: With user-specified quant matrices, we need to enlarge these limits
       if they don't actually let us change qi values.*/
    log_qmin=cpi->log_qavg[_qti][cpi->BaseQ]-0x00A4D3C25E68DC58LL;
    log_qmax=cpi->log_qavg[_qti][cpi->BaseQ]+0x00A4D3C25E68DC58LL;
    log_qtarget=OC_CLAMPI(log_qmin,log_qtarget,log_qmax);
  }
  /*Search for the quantizer that matches the target most closely.
    We don't assume a linear ordering, but when there are ties we do pick the
     quantizer closest to the current one.*/
  best_qi=cpi->info.quality;
  best_qdiff=cpi->log_qavg[_qti][best_qi]-log_qtarget;
  best_qdiff=best_qdiff+OC_SIGNMASK(best_qdiff)^OC_SIGNMASK(best_qdiff);
  for(qi=cpi->info.quality+1;qi<64;qi++){
    ogg_int64_t qdiff;
    qdiff=cpi->log_qavg[_qti][qi]-log_qtarget;
    qdiff=qdiff+OC_SIGNMASK(qdiff)^OC_SIGNMASK(qdiff);
    if(qdiff<best_qdiff||
     qdiff==best_qdiff&&abs(qi-cpi->BaseQ)<abs(best_qi-cpi->BaseQ)){
      best_qi=qi;
      best_qdiff=qdiff;
    }
  }
  /*Save the quantizer target for lambda calculations.*/
  cpi->rc.log_qtarget=log_qtarget;
  return best_qi;
}



static void CompressKeyFrame(CP_INSTANCE *cpi, int recode){
  oggpackB_reset(cpi->oggbuffer);
  cpi->FrameType = KEY_FRAME;
  if(cpi->info.target_bitrate>0){
    cpi->BaseQ=oc_enc_select_qi(cpi,0,cpi->CurrentFrame==1);
  }
  oc_enc_calc_lambda(cpi);
  cpi->LastKeyFrame = 0;

  /* mark as video frame */
  oggpackB_write(cpi->oggbuffer,0,1);

  WriteFrameHeader(cpi);
  PickModes(cpi,recode);
  EncodeData(cpi);

  cpi->LastKeyFrame = 1;
}

static int CompressFrame( CP_INSTANCE *cpi, int recode ) {
  oggpackB_reset(cpi->oggbuffer);
  cpi->FrameType = DELTA_FRAME;
  if(cpi->info.target_bitrate>0){
    cpi->BaseQ=oc_enc_select_qi(cpi,1,0);
  }
  oc_enc_calc_lambda(cpi);

  /* mark as video frame */
  oggpackB_write(cpi->oggbuffer,0,1);

  WriteFrameHeader(cpi);
  if(PickModes(cpi,recode)){
    /* mode analysis thinks this should have been a keyframe; start over and code as a keyframe instead */

    oggpackB_reset(cpi->oggbuffer);
    cpi->FrameType = KEY_FRAME;
    if(cpi->info.target_bitrate>0)cpi->BaseQ=oc_enc_select_qi(cpi,0,0);
    oc_enc_calc_lambda(cpi);
    cpi->LastKeyFrame = 0;

    /* mark as video frame */
    oggpackB_write(cpi->oggbuffer,0,1);

    WriteFrameHeader(cpi);

    PickModes(cpi,1);
    EncodeData(cpi);

    cpi->LastKeyFrame = 1;

    return 0;
  }

  if(cpi->first_inter_frame == 0){
    cpi->first_inter_frame = 1;
    EncodeData(cpi);
    if(cpi->info.target_bitrate>0){
      oc_enc_update_rc_state(cpi,oggpackB_bytes(cpi->oggbuffer)<<3,
       1,cpi->BaseQ,1);
    }
    CompressFrame(cpi,1);
    return 0;
  }

  cpi->LastKeyFrame++;
  EncodeData(cpi);

  return 0;
}

/********************** The toplevel: encode ***********************/

static void theora_encode_dispatch_init(CP_INSTANCE *cpi);

int theora_encode_init(theora_state *th, theora_info *c){
  CP_INSTANCE *cpi;

  memset(th, 0, sizeof(*th));
  /*Currently only the 4:2:0 format is supported.*/
  if(c->pixelformat!=OC_PF_420)return OC_IMPL;
  th->internal_encode=cpi=_ogg_calloc(1,sizeof(*cpi));
  theora_encode_dispatch_init(cpi);
  oc_mode_scheme_chooser_init(&cpi->chooser);
#if defined(OC_X86_ASM)
  oc_enc_vtable_init_x86(cpi);
#else
  oc_enc_vtable_init_c(cpi);
#endif

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
  cpi->keyframe_granule_shift=OC_ILOG_32(c->keyframe_frequency_force-1);


  /* copy in config */
  memcpy(&cpi->info,c,sizeof(*c));
  th->i=&cpi->info;
  th->granulepos=-1;

  /* Set up an encode buffer */
  cpi->oggbuffer = _ogg_malloc(sizeof(oggpack_buffer));
  oggpackB_writeinit(cpi->oggbuffer);
  cpi->dup_count=0;
  cpi->nqueued_dups=0;
  cpi->packetflag=0;

  InitFrameInfo(cpi);

  /* Initialise the compression process. */
  /* We always start at frame 1 */
  cpi->CurrentFrame = 1;

  memcpy(cpi->huff_codes,TH_VP31_HUFF_CODES,sizeof(cpi->huff_codes));

  /* This makes sure encoder version specific tables are initialised */
  memcpy(&cpi->quant_info, &TH_VP31_QUANT_INFO, sizeof(th_quant_info));
  InitQTables(cpi);
  if(cpi->info.target_bitrate>0)oc_rc_state_init(&cpi->rc,&cpi->info);

  /* Indicate that the next frame to be compressed is the first in the
     current clip. */
  cpi->LastKeyFrame = -1;
  cpi->readyflag = 1;

  cpi->HeadersWritten = 0;
  /*We overload this flag to track header output.*/
  cpi->doneflag=-3;

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
  if(cpi->doneflag>0)return OC_EINVAL;

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
  if(cpi->LastKeyFrame==-1 || cpi->LastKeyFrame+cpi->dup_count>= (ogg_uint32_t)
     cpi->info.keyframe_frequency_force){

    CompressKeyFrame(cpi,0);
    if(cpi->info.target_bitrate>0){
      oc_enc_update_rc_state(cpi,oggpackB_bytes(cpi->oggbuffer)<<3,
       0,cpi->BaseQ,1);
    }

    /* On first frame, the previous was a initial dry-run to prime
       feed-forward statistics */
    if(cpi->CurrentFrame==1)CompressKeyFrame(cpi,1);

  }
  else{
    /*Compress the frame.*/
    dropped=CompressFrame(cpi,0);
  }
  oc_enc_restore_fpu(cpi);


  /* Update stats variables. */
  {
    /* swap */
    unsigned char *temp;
    temp=cpi->lastrecon;
    cpi->lastrecon=cpi->recon;
    cpi->recon=temp;
  }
  if(cpi->FrameType==KEY_FRAME){
    memcpy(cpi->golden,cpi->lastrecon,sizeof(*cpi->lastrecon)*cpi->frame_size);
  }
  cpi->CurrentFrame++;
  cpi->packetflag=1;
  if(cpi->info.target_bitrate>0){
    oc_enc_update_rc_state(cpi,oggpackB_bytes(cpi->oggbuffer)<<3,
     cpi->FrameType!=KEY_FRAME,cpi->BaseQ,0);
  }

  t->granulepos=
    ((cpi->CurrentFrame - cpi->LastKeyFrame)<<cpi->keyframe_granule_shift)+
    cpi->LastKeyFrame - 1;
  cpi->nqueued_dups=cpi->dup_count;
  cpi->dup_count=0;

  return 0;
}

int theora_encode_packetout(theora_state *_t,int _last_p,ogg_packet *_op){
  CP_INSTANCE *cpi;
  cpi=(CP_INSTANCE *)_t->internal_encode;
  if(cpi->doneflag>0)return -1;
  if(cpi->packetflag){
    cpi->packetflag=0;
    _op->packet=oggpackB_get_buffer(cpi->oggbuffer);
    _op->bytes=oggpackB_bytes(cpi->oggbuffer);
  }
  else if(cpi->nqueued_dups>0){
    cpi->nqueued_dups--;
    cpi->CurrentFrame++;
    cpi->LastKeyFrame++;
    _t->granulepos=cpi->LastKeyFrame-1
     +(cpi->CurrentFrame-cpi->LastKeyFrame<<cpi->keyframe_granule_shift);
    _op->packet=NULL;
    _op->bytes=0;
  }
  else{
    if(_last_p){
      cpi->doneflag=1;
#if defined(OC_COLLECT_METRICS)
      oc_enc_mode_metrics_dump(cpi);
#endif
    }
    return 0;
  }
  _last_p=_last_p&&cpi->nqueued_dups<=0;
  _op->b_o_s=0;
  _op->e_o_s=_last_p;
  _op->packetno=cpi->CurrentFrame;
  _op->granulepos=_t->granulepos;
  return 1+cpi->nqueued_dups;
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

  oggpackB_reset(cpi->oggbuffer);
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

  /* The header target_bitrate is limited to 24 bits, so we clamp here */
  oggpackB_write(cpi->oggbuffer,(cpi->info.target_bitrate>(1<<24)-1) ? ((1<<24)-1) : cpi->info.target_bitrate ,24);

  oggpackB_write(cpi->oggbuffer,cpi->info.quality,6);

  oggpackB_write(cpi->oggbuffer,cpi->keyframe_granule_shift,5);

  oggpackB_write(cpi->oggbuffer,cpi->info.pixelformat,2);

  oggpackB_write(cpi->oggbuffer,0,3); /* spare config bits */

  op->packet=oggpackB_get_buffer(cpi->oggbuffer);
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

  opb = _ogg_malloc(sizeof(oggpack_buffer));
  oggpackB_writeinit(opb);
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

  /* So we're expecting the application will free this? */
  op->packet=_ogg_malloc(oggpack_bytes(opb));
  memcpy(op->packet, oggpack_get_buffer(opb), oggpack_bytes(opb));
  oggpack_writeclear(opb);

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

  oggpackB_reset(cpi->oggbuffer);
  oggpackB_write(cpi->oggbuffer,0x82,8);
  _tp_writebuffer(cpi->oggbuffer,"theora",6);

  oc_quant_params_pack(cpi->oggbuffer,&cpi->quant_info);
  oc_huff_codes_pack(cpi->oggbuffer,(const th_huff_table *)cpi->huff_codes);

  op->packet=oggpackB_get_buffer(cpi->oggbuffer);
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

    ClearFrameInfo(cpi);

    oggpackB_writeclear(cpi->oggbuffer);
    _ogg_free(cpi->oggbuffer);

    memset(cpi,0,sizeof(cpi));
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

    return (iframe+pframe-1);
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
    case TH_ENCCTL_SET_KEYFRAME_FREQUENCY_FORCE:{
      ogg_uint32_t keyframe_frequency_force;
      if(buf==NULL)return TH_EFAULT;
      if(buf_sz!=sizeof(keyframe_frequency_force))return TH_EINVAL;
      keyframe_frequency_force=*(ogg_uint32_t *)buf;
      if(cpi->HeadersWritten){
        /*It's still early enough to enlarge keyframe_granule_shift.*/
        cpi->keyframe_granule_shift=OC_CLAMPI(cpi->keyframe_granule_shift,
         OC_ILOG_32(keyframe_frequency_force-1),31);
      }
      cpi->info.keyframe_frequency_force=OC_MINI(keyframe_frequency_force,
       (ogg_uint32_t)1U<<cpi->keyframe_granule_shift);
      *(ogg_uint32_t *)buf=cpi->info.keyframe_frequency_force;
      return 0;
    }
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
    case TH_ENCCTL_SET_DUP_COUNT:{
      int dup_count;
      if(buf==NULL)return TH_EFAULT;
      if(buf_sz!=sizeof(int))return TH_EINVAL;
      dup_count=*(int *)buf;
      if(dup_count>=cpi->info.keyframe_frequency_force)return TH_EINVAL;
      cpi->dup_count=OC_MAXI(dup_count,0);
      return 0;
    }break;
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
